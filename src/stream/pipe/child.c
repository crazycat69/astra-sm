/*
 * Astra Core (Child process)
 *
 * Copyright (C) 2015, Artem Kharitonov <artem@sysert.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <astra.h>
#include "child.h" // TODO: move to core/child.h
#include <core/spawn.h>
#include <core/socket.h>
#include <core/timer.h>

#define MSG(_msg) "[child/%s] " _msg, child->name

#define IO_BUFFER_SIZE (32 * 1024) /* 32 KiB */

#define KILL_TICK_MSEC 100
#define KILL_MAX_TICKS 15

typedef struct
{
    int fd;

    asc_event_t *ev;
    event_callback_t on_read;
    event_callback_t on_close;

    child_io_mode_t mode;
    child_io_callback_t on_flush;

    uint8_t data[IO_BUFFER_SIZE];
    size_t pos_read;
    size_t pos_write;
} child_io_t;

struct asc_child_t
{
    const char *name;
    asc_process_t proc;

    asc_timer_t *kill_timer;
    unsigned kill_ticks;

    child_io_t sin;
    child_io_t sout;
    child_io_t serr;

    child_close_callback_t on_close;
    void *arg;
};

#define CHILD_IO_SETUP(__io) \
    do { \
        child->__io.mode = cfg->__io.mode; \
        child->__io.on_flush = cfg->__io.on_flush; \
        child->__io.on_read = EVENT_##__io##_read; \
        child->__io.on_close = EVENT_##__io##_close; \
        child->__io.ev = asc_event_init(child->__io.fd, child); \
        asc_event_set_on_error(child->__io.ev, child->__io.on_close); \
        if (!cfg->__io.ignore_read) \
            asc_event_set_on_read(child->__io.ev, child->__io.on_read); \
    } while (0)

#define CHILD_IO_CLEANUP(__io) \
    do { \
        ASC_FREE(child->__io.ev, asc_event_close); \
        if (child->__io.fd != -1) \
        { \
            asc_pipe_close(child->__io.fd); \
            child->__io.fd = -1; \
        } \
    } while (0)

#define EVENT_CALLBACK(__io, __event) \
    static void EVENT_##__io##_##__event(void *arg) \
    { \
        asc_child_t *const child = (asc_child_t *)arg; \
        on_stdio_##__event(child, &child->__io); \
    }

#define asc_child_close_tick \
    (timer_callback_t)asc_child_close

/*
 * helper functions
 */
static inline
child_io_t *get_child_io(asc_child_t *child, int fd)
{
    switch (fd)
    {
        case STDIN_FILENO:  return &child->sin;
        case STDOUT_FILENO: return &child->sout;
        case STDERR_FILENO: return &child->serr;
        default:
            return NULL;
    }
}

static inline
const char *get_io_name(const asc_child_t *child, const child_io_t *io)
{
    if (io == &child->sin)
        return "stdin";
    else if (io == &child->sout)
        return "stdout";
    else if (io == &child->serr)
        return "stderr";
    else
        return NULL;
}
// XXX: these are only used once!

/*
 * reading from child
 */
static inline
void on_stdio_close(asc_child_t *child, child_io_t *io)
{
    asc_log_debug(MSG("%s pipe got closed on far side")
                  , get_io_name(child, io));

    asc_child_close(child);
}

static
void on_stdio_read(asc_child_t *child, child_io_t *io)
{
    uint8_t *const dst = &io->data[io->pos_write];
    size_t space = sizeof(io->data) - io->pos_write - 1;

    const ssize_t ret = recv(io->fd, (char *)dst, space, 0);
    switch (ret)
    {
        case -1:
            if (asc_socket_would_block())
                return;

            asc_log_debug(MSG("recv(): %s"), asc_error_msg());

        case 0:
            on_stdio_close(child, io);
            return;

        default:
            break;
    }

    if (io->on_flush == NULL)
        return;

    /* buffer incoming data */
    asc_assert(ret > 0 && ret <= (ssize_t)space, MSG("recv() went haywire!"));

    space -= ret;
    io->pos_write += ret;

    switch (io->mode)
    {
        case CHILD_IO_MPEGTS:
            /* 188-byte TS packets */
            for (; io->pos_write >= io->pos_read + (TS_PACKET_SIZE * 2)
                 ; io->pos_read += TS_PACKET_SIZE)
            {
                /* look for sync byte */
                for (size_t i = 0; i < TS_PACKET_SIZE; i++)
                {
                    const uint8_t *const ts = &io->data[io->pos_read + i];
                    if (TS_IS_SYNC(ts))
                    {
                        io->pos_read += i;
                        io->on_flush(child->arg, ts, TS_PACKET_SIZE);
                        break;
                    }
                }
            }

            break;

        case CHILD_IO_TEXT:
            /* line-buffered input */
            for (size_t i = io->pos_read; i < io->pos_write; i++)
            {
                uint8_t *const c = &io->data[i];
                if (*c == '\n' || *c == '\r' || *c == '\0')
                {
                    /* terminate line and feed it to callback */
                    *c = '\0';

                    const uint8_t *const str = &io->data[io->pos_read];
                    const size_t max = sizeof(io->data) - io->pos_read;
                    const size_t len = strnlen((char *)str, max);
                    if (len > 0)
                        io->on_flush(child->arg, str, len);

                    io->pos_read = i + 1;
                }
            }

            if (space == 0 && io->pos_read == 0)
            {
                /* buffered line is too long; dump what we got */
                const size_t len = strnlen((char *)io->data, sizeof(io->data));
                if (len > 0)
                    io->on_flush(child->arg, io->data, len);

                io->pos_write = 0;
            }

            break;

        case CHILD_IO_RAW:
            /* pass every read to callback */
            io->on_flush(child->arg, dst, ret);

        default:
            io->pos_write = 0;
            return;
    }

    if (io->pos_read > 0)
    {
        /* move remaining fragment to beginning of the buffer */
        const size_t frag = io->pos_write - io->pos_read;
        if (frag > 0)
            memmove(io->data, &io->data[io->pos_read], frag);

        io->pos_write = frag;
        io->pos_read = 0;
    }
}

EVENT_CALLBACK(sin, close)
EVENT_CALLBACK(sout, close)
EVENT_CALLBACK(serr, close)

EVENT_CALLBACK(sin, read)
EVENT_CALLBACK(sout, read)
EVENT_CALLBACK(serr, read)

/*
 * TODO: writing to child
 */

/*
 * create and destroy
 */
asc_child_t *asc_child_init(const asc_child_cfg_t *cfg)
{
    asc_child_t *const child = (asc_child_t *)calloc(1, sizeof(*child));
    asc_assert(child != NULL, "[child] calloc() failed");

    child->name = cfg->name;

    /* start the process */
    asc_log_debug(MSG("attempting to execute `%s'"), cfg->command);
    const int ret = asc_process_spawn(cfg->command, &child->proc
                                      , &child->sin.fd, &child->sout.fd
                                      , &child->serr.fd);

    if (ret != 0)
    {
        asc_log_debug(MSG("couldn't spawn process: %s"), asc_error_msg());

        free(child);
        return NULL;
    }

    /* register event callbacks */
    CHILD_IO_SETUP(sin);
    CHILD_IO_SETUP(sout);
    CHILD_IO_SETUP(serr);

    child->on_close = cfg->on_close;
    child->arg = cfg->arg;

    return child;
}

void asc_child_close(asc_child_t *child)
{
    /* shutdown stdio pipes */
    CHILD_IO_CLEANUP(sin);
    CHILD_IO_CLEANUP(sout);
    CHILD_IO_CLEANUP(serr);

    /* check process state */
    int status = -1;
    const pid_t ret = asc_process_wait(&child->proc, &status, false);

    switch (ret)
    {
        case -1:
            /* query fail; clean up and hope it dies on its own */
            asc_log_error(MSG("couldn't get status: %s"), asc_error_msg());
            break;

        case 0:
            /* still active; give it some time to exit */
            if (child->kill_ticks++ == 0)
            {
                /* ask nicely on first tick */
                asc_log_debug(MSG("sending termination signal"));
                if (asc_process_kill(&child->proc, false) != 0)
                {
                    asc_log_error(MSG("couldn't terminate child: %s")
                                  , asc_error_msg());
                    break;
                }
            }

            if (child->kill_ticks <= KILL_MAX_TICKS)
            {
                child->kill_timer = asc_timer_one_shot(KILL_TICK_MSEC
                                                       , asc_child_close_tick
                                                       , child);
                return;
            }

            /* euthanize the bastard, wait until it dies */
            asc_log_warning(MSG("sending kill signal"));
            if (asc_process_kill(&child->proc, true) != 0)
            {
                asc_log_error(MSG("couldn't kill child: %s"), asc_error_msg());
                break;
            }

            if (asc_process_wait(&child->proc, &status, true) == -1)
            {
                asc_log_error(MSG("couldn't get status: %s"), asc_error_msg());
                break;
            }

        default:
            /* process exited or killed */
#ifndef _WIN32
            if (WIFSIGNALED(status))
            {
                const int signum = WTERMSIG(status);
                asc_log_debug(MSG("caught signal %d (%s)")
                              , signum, sys_siglist[signum]);

                status = 128 + signum;
            }
            else if (WIFEXITED(status))
                status = WEXITSTATUS(status);
#endif /* !_WIN32 */
            break;
    }

    /* shutdown complete */
    if (child->on_close != NULL)
        child->on_close(child->arg, status);

    asc_process_free(&child->proc);
    free(child);
}

void asc_child_destroy(asc_child_t *child)
{
    /* `destroy' is similar to `close', except it blocks */
    ASC_FREE(child->kill_timer, asc_timer_destroy);

    CHILD_IO_CLEANUP(sin);
    CHILD_IO_CLEANUP(sout);
    CHILD_IO_CLEANUP(serr);

    /* if close is in progress, don't resend termination signal */
    bool waitquit = true;
    if (child->kill_ticks == 0)
    {
        asc_log_debug(MSG("sending termination signal"));
        if (asc_process_kill(&child->proc, false) != 0)
        {
            asc_log_error(MSG("couldn't terminate child: %s"), asc_error_msg());
            waitquit = false;
        }
    }

    if (waitquit)
    {
        /* wait up to 1.5s */
        pid_t status;
        for (size_t i = 0; i < 150; i++)
        {
            status = asc_process_wait(&child->proc, NULL, false);
            if (status != 0)
                break;

            asc_usleep(10 * 1000);
        }
        /* XXX: check asc_utime */

        if (status == 0)
        {
            /* process is still around; force it to quit */
            asc_log_warning(MSG("sending kill signal"));
            if (asc_process_kill(&child->proc, true) == 0)
                status = asc_process_wait(&child->proc, NULL, true);
            else
                asc_log_error(MSG("couldn't kill child: %s"), asc_error_msg());
        }

        /* report final status */
        if (status > 0)
            asc_log_debug(MSG("child exited (pid = %lld)"), (long long)status);
        else if (status == -1)
            asc_log_error(MSG("couldn't get status: %s"), asc_error_msg());
    }

    asc_process_free(&child->proc);
    free(child);
}

/*
 * setters and getters
 */
__asc_inline
void asc_child_set_on_close(asc_child_t *child
                            , child_close_callback_t on_close)
{
    child->on_close = on_close;
}

/*
__asc_inline
void asc_child_set_on_ready(asc_child_t *child, event_callback_t on_ready)
{
    TODO
}
*/

// XXX: put __asc_inline back in
void asc_child_toggle_input(asc_child_t *child, int fd, bool enable)
{
    const child_io_t *const io = get_child_io(child, fd);
    asc_event_set_on_read(io->ev, (enable ? io->on_read : NULL));
}

__asc_inline
pid_t asc_child_pid(const asc_child_t *child)
{
    return asc_process_id(&child->proc);
}
