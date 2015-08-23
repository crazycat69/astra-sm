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

#ifndef _WIN32
#   include <sys/wait.h>
#endif /* !_WIN32 */

#ifndef _REMOVE_ME_
    /*
     * TODO: move `spawn' and `child' to core
     */
#   include "spawn.h"
#   include "child.h"
#endif /* _REMOVE_ME_ */

#define MSG(_msg) "[child/%s] " _msg, child->name

#define IO_BUFFER_SIZE 8192

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

    uint8_t buffer[IO_BUFFER_SIZE * 2];
    size_t pos_read;
    size_t pos_write;
} child_io_t;

struct asc_child_t
{
    const char *name;
    asc_pid_t pid;

    asc_timer_t *kill_timer;
    unsigned kill_ticks;

    child_io_t sin;
    child_io_t sout;
    child_io_t serr;

    child_close_callback_t on_close;
    void *arg;
};

/*
 * NOTE: parent end of the child's stdin is normally
 *       write-only. therefore, assume all read events are
 *       disconnect notifications.
 */
#define EVENT_sin_read EVENT_sin_close

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
    {
        return "input";
    }
    else if (io == &child->sout)
    {
        return "output";
    }
    else if (io == &child->serr)
    {
        return "error";
    }
    else
        return NULL;
}

/*
 * redirected IO callbacks
 */
static void on_stdio_close(asc_child_t *child, child_io_t *io)
{
    asc_log_debug(MSG("child closed standard %s, cleaning up")
                  , get_io_name(child, io));

    asc_child_close(child);
}

static void on_stdio_read(asc_child_t *child, child_io_t *io)
{
#ifndef _REMOVE_ME_
    /* placeholder code */
    asc_log_info("got read from standard %s", get_io_name(child, io));

    char buf[4096];
    const ssize_t ret = read(io->fd, buf, sizeof(buf));
    if (ret <= 0)
    {
        if (errno != EAGAIN)
            on_stdio_close(child, io);

        return;
    }
#endif /* _REMOVE_ME_ */
}

EVENT_CALLBACK(sin, close)
EVENT_CALLBACK(sout, close)
EVENT_CALLBACK(serr, close)

EVENT_CALLBACK(sout, read)
EVENT_CALLBACK(serr, read)

// XXX: writing

/*
 * setters
 */
void asc_child_set_on_close(asc_child_t *child
                            , child_close_callback_t on_close)
{
    child->on_close = on_close;
}

// XXX: set on ready

void asc_child_toggle_input(asc_child_t *child, int fd, bool enable)
{
    const child_io_t *const io = get_child_io(child, fd);
    asc_event_set_on_read(io->ev, (enable ? io->on_read : NULL));
}

/*
 * create and destroy
 */
asc_child_t *asc_child_init(const asc_child_cfg_t *cfg)
{
    asc_child_t *const child = (asc_child_t *)calloc(1, sizeof(*child));
    asc_assert(child != NULL, "[child] calloc() failed");

    /* start a child process */
    child->pid = asc_child_spawn(cfg->command
                                 , &child->sin.fd, &child->sout.fd
                                 , &child->serr.fd);

    if (child->pid == -1)
    {
        free(child);
        return NULL;
    }

    child->name = cfg->name;

    /* register event callbacks */
    CHILD_IO_SETUP(sin);
    CHILD_IO_SETUP(sout);
    CHILD_IO_SETUP(serr);

    child->on_close = cfg->on_close;
    child->arg = cfg->arg;

    return child;
}

static bool kill_process(const asc_child_t *child, bool force)
{
#ifndef _WIN32
    asc_log_debug(MSG("sending %s signal to pid %d")
                  , (force ? "KILL" : "TERM"), child->pid);

    const int sig = (force ? SIGKILL : SIGTERM);
    if (kill(child->pid, sig) != 0 && errno != ESRCH)
    {
        asc_log_error(MSG("kill() failed: %s"), strerror(errno));
        return false;
    }
#else /* !_WIN32 */
#   error "FIXME: add Win32 support"
#endif /* !_WIN32 */

    return true;
}

void asc_child_close(asc_child_t *child)
{
    /* shutdown stdio pipes */
    CHILD_IO_CLEANUP(sin);
    CHILD_IO_CLEANUP(sout);
    CHILD_IO_CLEANUP(serr);

    /* check process state */
#ifndef _WIN32
    int exit_code = EXIT_ABORT;

    int status;
    const pid_t ret = waitpid(child->pid, &status, WNOHANG);
    switch (ret)
    {
        case -1:
            /* error; shouldn't happen */
            asc_log_error(MSG("waitpid() failed: %s"), strerror(errno));
            break;

        case 0:
            /* process is still around */
            if (child->kill_ticks == 0)
            {
                /* try `kill -TERM' first */
                if (!kill_process(child, false))
                    break;
            }

            if (++child->kill_ticks <= KILL_MAX_TICKS)
            {
                /* schedule next status check */
                child->kill_timer = asc_timer_one_shot(KILL_TICK_MSEC
                                                       , (timer_callback_t)asc_child_close
                                                       , child);

                return;
            }

            /* last resort: `kill -KILL', block until it dies */
            if (!kill_process(child, true))
                break;

            if (waitpid(child->pid, &status, 0) != 0)
                break;

        default:
            /* process reaped */
            exit_code = WEXITSTATUS(status);
    }
#else /* !_WIN32 */
#   error "FIXME: add Win32 support"
#endif /* !_WIN32 */

    /* shutdown complete */
    if (child->on_close != NULL)
        child->on_close(child->arg, (int)exit_code);

    free(child);
}

void asc_child_destroy(asc_child_t *child)
{
    ASC_FREE(child->kill_timer, asc_timer_destroy);
    child->kill_ticks = KILL_MAX_TICKS;
    child->on_close = NULL;

    asc_child_close(child);
}

asc_pid_t asc_child_pid(const asc_child_t *child)
{
#ifndef _WIN32
    return child->pid;
#else
#   error "FIXME"
#endif
}
