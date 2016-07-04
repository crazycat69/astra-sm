/*
 * Astra Core (Child process)
 *
 * Copyright (C) 2015-2016, Artem Kharitonov <artem@3phase.pw>
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
#include <core/child.h>
#include <core/spawn.h>
#include <core/timer.h>

#define MSG(_msg) "[child/%s] " _msg, child->name

#define IO_BUFFER_SIZE (64UL * 1024UL) /* 64 KiB */
#define IO_BUFFER_TS_PACKETS (IO_BUFFER_SIZE / TS_PACKET_SIZE)

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
    char name[128];
    asc_process_t proc;

    asc_timer_t *kill_timer;
    unsigned int kill_ticks;

    child_io_t sin;
    child_io_t sout;
    child_io_t serr;

    event_callback_t on_ready;
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

#define EVENT_CALLBACK(__io, __event) \
    static void EVENT_##__io##_##__event(void *arg) \
    { \
        asc_child_t *const child = (asc_child_t *)arg; \
        on_stdio_##__event(child, &child->__io); \
    }

/*
 * reading from child
 */

static
void recv_text(const asc_child_t *child, child_io_t *io)
{
    /* line-buffered input */
    const size_t space = sizeof(io->data) - io->pos_write - 1;

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
            if (len > 0 && io->on_flush != NULL)
                io->on_flush(child->arg, str, len);

            io->pos_read = i + 1;
        }
    }

    if (space == 0 && io->pos_read == 0)
    {
        /* buffered line is too long; dump what we got */
        const size_t len = strnlen((char *)io->data, sizeof(io->data));
        if (len > 0 && io->on_flush != NULL)
            io->on_flush(child->arg, io->data, len);

        io->pos_write = 0;
    }
}

static
void recv_mpegts(const asc_child_t *child, child_io_t *io)
{
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
                if (io->on_flush != NULL)
                    io->on_flush(child->arg, ts, 1);
                    // FIXME: flush more than 1

                break;
            }
        }
    }
}

static
ssize_t recv_buffer(asc_child_t *child, child_io_t *io)
{
    uint8_t *const dst = &io->data[io->pos_write];
    const size_t space = sizeof(io->data) - io->pos_write - 1;

    /* buffer incoming data */
    const ssize_t ret = recv(io->fd, (char *)dst, space, 0);
    if (ret <= 0)
        return ret;
    else if ((size_t)ret > space)
        return 0;
    else if (io == &child->sin)
        return ret;

    io->pos_write += ret;

    /* pass data to callbacks according to configured I/O mode */
    switch (io->mode)
    {
        case CHILD_IO_MPEGTS:
            recv_mpegts(child, io);
            break;

        case CHILD_IO_TEXT:
            recv_text(child, io);
            break;

        case CHILD_IO_RAW:
            /* raw mode: pass every read to callback */
            if (io->on_flush != NULL)
                io->on_flush(child->arg, dst, ret);

            /* fallthrough */

        case CHILD_IO_NONE:
        default:
            io->pos_write = 0;
            break;
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

    return ret;
}

static
void on_stdio_close(asc_child_t *child, const child_io_t *io)
{
    const char *name = NULL;
    if (io == &child->sin)
        name = "stdin";
    else if (io == &child->sout)
        name = "stdout";
    else if (io == &child->serr)
        name = "stderr";

    asc_log_debug(MSG("%s pipe got closed on far side"), name);
    asc_child_close(child);
}

static
void on_stdio_read(asc_child_t *child, child_io_t *io)
{
    const ssize_t ret = recv_buffer(child, io);
    if (ret == -1)
    {
        if (asc_socket_would_block())
            return;

        asc_log_debug(MSG("recv(): %s"), asc_error_msg());
    }

    if (ret <= 0)
        on_stdio_close(child, io);
}

EVENT_CALLBACK(sin, close)
EVENT_CALLBACK(sout, close)
EVENT_CALLBACK(serr, close)

EVENT_CALLBACK(sin, read)
EVENT_CALLBACK(sout, read)
EVENT_CALLBACK(serr, read)

/*
 * writing to child
 */

static
ssize_t send_raw(int fd, const uint8_t *buf, size_t len)
{
    size_t written = 0;

    while (len > 0)
    {
        const ssize_t ret = send(fd, (char *)&buf[written], len, 0);
        if (ret < 0 || (size_t)ret > len)
            return -1;

        written += ret;
        len -= ret;
    }

    return written;
}

static
ssize_t send_mpegts(child_io_t *io, const uint8_t *buf, size_t npkts)
{
    size_t left = npkts;
    size_t pos = 0;

    while (left > 0)
    {
        size_t slots = (sizeof(io->data) - io->pos_write) / TS_PACKET_SIZE;
        if (slots == 0 || (left > IO_BUFFER_TS_PACKETS && io->pos_write > 0))
        {
            const ssize_t ret = send_raw(io->fd, io->data, io->pos_write);
            io->pos_write = 0;

            if (ret < 0)
                return -1;

            slots = IO_BUFFER_TS_PACKETS;
        }

        size_t bytes = slots * TS_PACKET_SIZE;
        if (left <= IO_BUFFER_TS_PACKETS)
        {
            if (slots > left)
            {
                slots = left;
                bytes = left * TS_PACKET_SIZE;
            }

            memcpy(&io->data[io->pos_write], &buf[pos], bytes);
            io->pos_write += bytes;
        }
        else
        {
            /* send large chunks without copying them to the buffer */
            const ssize_t ret = send_raw(io->fd, &buf[pos], bytes);
            if (ret < 0)
                return -1;
        }

        pos += bytes;
        left -= slots;
    }

    return npkts;
}

ssize_t asc_child_send(asc_child_t *child, const void *buf, size_t len)
{
    switch (child->sin.mode)
    {
        case CHILD_IO_MPEGTS:
            return send_mpegts(&child->sin, (uint8_t *)buf, len);

        case CHILD_IO_TEXT:
        case CHILD_IO_RAW:
            return send_raw(child->sin.fd, (uint8_t *)buf, len);

        default:
            return len;
    }
}

static
void on_sin_write(void *arg)
{
    asc_child_t *const child = (asc_child_t *)arg;

    if (child->on_ready != NULL)
        child->on_ready(child->arg);
}

/*
 * create and destroy
 */

asc_child_t *asc_child_init(const asc_child_cfg_t *cfg)
{
    asc_child_t *const child = ASC_ALLOC(1, asc_child_t);
    snprintf(child->name, sizeof(child->name), "%s", cfg->name);

    asc_assert(cfg->sin.on_flush == NULL && cfg->sin.ignore_read == false
               , MSG("cannot set read callback on standard input"));

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

    asc_child_set_on_close(child, cfg->on_close);
    asc_child_set_on_ready(child, cfg->on_ready);
    child->arg = cfg->arg;

    return child;
}

static
void io_drain(asc_child_t *child)
{
    while (recv_buffer(child, &child->sin) > 0)
        ; /* nothing */

    while (recv_buffer(child, &child->sout) > 0)
        ; /* nothing */

    while (recv_buffer(child, &child->serr) > 0)
        ; /* nothing */
}

static
void io_cleanup(child_io_t *io)
{
    ASC_FREE(io->ev, asc_event_close);

    if (io->fd != -1)
    {
        asc_pipe_close(io->fd);
        io->fd = -1;
    }
}

void asc_child_close(asc_child_t *child)
{
    ASC_FREE(child->kill_timer, asc_timer_destroy);
    child->kill_ticks++;

    /* shutdown stdio pipes on first call */
    if (child->kill_ticks == 1)
    {
        io_drain(child);

        io_cleanup(&child->sin);
        io_cleanup(&child->sout);
        io_cleanup(&child->serr);
    }

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
            if (child->kill_ticks == 1)
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
                /* schedule next status check and return */
                const timer_callback_t fn = (timer_callback_t)asc_child_close;
                child->kill_timer = asc_timer_init(KILL_TICK_MSEC, fn, child);

                return;
            }

            /* time's up; euthanize the bastard, wait until it dies */
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

            /* fallthrough */

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
    /* `destroy' is similar to `close', except it always blocks */
    ASC_FREE(child->kill_timer, asc_timer_destroy);

    /* if close is in progress, don't resend termination signal */
    bool waitquit = true;
    if (child->kill_ticks == 0)
    {
        io_cleanup(&child->sin);
        io_cleanup(&child->sout);
        io_cleanup(&child->serr);

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
        pid_t status = -1;
        for (size_t i = 0; i < 150; i++)
        {
            status = asc_process_wait(&child->proc, NULL, false);
            if (status != 0)
                break;

            asc_usleep(10 * 1000);
        }

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

void asc_child_set_on_close(asc_child_t *child
                            , child_close_callback_t on_close)
{
    child->on_close = on_close;
}

void asc_child_set_on_ready(asc_child_t *child, event_callback_t on_ready)
{
    event_callback_t cb = NULL;
    if (on_ready != NULL)
        cb = on_sin_write;

    asc_event_set_on_write(child->sin.ev, cb);
    child->on_ready = on_ready;
}

static inline
child_io_t *get_io_by_fd(asc_child_t *child, int child_fd)
{
    switch (child_fd)
    {
        case STDIN_FILENO:  return &child->sin;
        case STDOUT_FILENO: return &child->sout;
        case STDERR_FILENO: return &child->serr;
    }

    return NULL;
}

void asc_child_set_on_flush(asc_child_t *child, int child_fd
                            , child_io_callback_t on_flush)
{
    child_io_t *const io = get_io_by_fd(child, child_fd);
    asc_assert(io != &child->sin, MSG("can't change read events on stdin"));

    io->on_flush = on_flush;
}

void asc_child_set_mode(asc_child_t *child, int child_fd
                        , child_io_mode_t mode)
{
    child_io_t *const io = get_io_by_fd(child, child_fd);

    io->pos_read = io->pos_write = 0;
    io->mode = mode;
}

void asc_child_toggle_input(asc_child_t *child, int child_fd
                            , bool enable)
{
    const child_io_t *const io = get_io_by_fd(child, child_fd);
    asc_assert(io != &child->sin, MSG("can't change read events on stdin"));

    if (enable)
        asc_event_set_on_read(io->ev, io->on_read);
    else
        asc_event_set_on_read(io->ev, NULL);
}

pid_t asc_child_pid(const asc_child_t *child)
{
    return asc_process_id(&child->proc);
}
