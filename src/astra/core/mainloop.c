/*
 * Astra Core (Main loop)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *               2015-2017, Artem Kharitonov <artem@3phase.pw>
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

#include <astra/astra.h>
#include <astra/core/mainloop.h>
#include <astra/core/event.h>
#include <astra/core/mutex.h>
#include <astra/core/timer.h>
#include <astra/core/socket.h>
#include <astra/core/spawn.h>
#include <astra/luaapi/luaapi.h>
#include <astra/luaapi/state.h>

#define MSG(_msg) "[core/mainloop] " _msg

/* garbage collector interval, usecs */
#define LUA_GC_TIMEOUT (1 * 1000 * 1000)

/* maximum number of jobs queued */
#define JOB_QUEUE_SIZE 256

enum
{
    MAIN_LOOP_SIGHUP   = 0x00000001,
    MAIN_LOOP_RELOAD   = 0x00000002,
    MAIN_LOOP_SHUTDOWN = 0x00000004,
};

typedef struct
{
    loop_callback_t proc;
    void *arg;
    void *owner;
} loop_job_t;

typedef struct
{
    uint32_t flags;
    unsigned int stop_cnt;

    int wake_fd[2];
    asc_event_t *wake_ev;
    unsigned int wake_cnt;

    loop_job_t jobs[JOB_QUEUE_SIZE];
    unsigned int job_cnt;
    asc_mutex_t job_mutex;
} asc_main_loop_t;

static
asc_main_loop_t *main_loop = NULL;

/*
 * main thread wake up mechanism
 */

static
void on_wake_read(void *arg);

static
void on_wake_error(void *arg);

static
bool wake_open(void)
{
    int fds[2] = { -1, -1 };

    if (asc_pipe_open(fds, NULL, PIPE_BOTH) != 0)
        return false;

    main_loop->wake_fd[0] = fds[0];
    main_loop->wake_fd[1] = fds[1];

    main_loop->wake_ev = asc_event_init(fds[PIPE_RD], NULL);
    asc_event_set_on_read(main_loop->wake_ev, on_wake_read);
    asc_event_set_on_error(main_loop->wake_ev, on_wake_error);

    return true;
}

static
void wake_close(void)
{
    ASC_FREE(main_loop->wake_ev, asc_event_close);

    const int fds[2] =
    {
        main_loop->wake_fd[0],
        main_loop->wake_fd[1],
    };

    if (fds[0] != -1)
    {
        main_loop->wake_fd[0] = -1;
        asc_pipe_close(fds[0]);
    }

    if (fds[1] != -1)
    {
        main_loop->wake_fd[1] = -1;
        asc_pipe_close(fds[1]);
    }
}

/* read event handler: discard incoming data, reopen pipe on errors */
static
void on_wake_read(void *arg)
{
    ASC_UNUSED(arg);

    char buf[32];
    const int ret = recv(main_loop->wake_fd[PIPE_RD], buf, sizeof(buf), 0);
    switch (ret)
    {
        case -1:
            /* error that may or may not be EWOULDBLOCK */
            if (asc_socket_would_block())
                return;

            asc_log_error(MSG("wake up recv(): %s"), asc_error_msg());
            break;

        case 0:
            /* connection closed from the other side */
            asc_log_error(MSG("wake up pipe closed unexpectedly"));
            break;

        default:
            /* successful read */
            return;
    }

    /* this code is highly unlikely to be reached */
    asc_log_warning(MSG("reopening wake up pipe"));

    wake_close();
    if (!wake_open())
       asc_log_error(MSG("couldn't reopen pipe: %s"), asc_error_msg());
}

/* error event handler: emit error message and close pipe */
static
void on_wake_error(void *arg)
{
    ASC_UNUSED(arg);

    /* shouldn't happen, ever */
    asc_log_error(MSG("BUG: error event on wake up pipe"));
    wake_close();
}

/* increase pipe refcount, opening it if necessary */
void asc_wake_open(void)
{
    if (main_loop->wake_cnt == 0)
    {
        asc_log_debug(MSG("opening main loop wake up pipe"));
        if (!wake_open())
            asc_log_error(MSG("couldn't open pipe: %s"), asc_error_msg());
    }

    ++main_loop->wake_cnt;
}

/* decrease pipe refcount, closing it when it's no longer needed */
void asc_wake_close(void)
{
    ASC_ASSERT(main_loop->wake_cnt > 0, MSG("wake up pipe already closed"));
    --main_loop->wake_cnt;

    if (main_loop->wake_cnt == 0)
    {
        asc_log_debug(MSG("closing main loop wake up pipe"));
        wake_close();
    }
}

/* signal event polling function to return */
void asc_wake(void)
{
    const int fd = main_loop->wake_fd[PIPE_WR];
    static const char byte = '\0';

    if (fd != -1 && send(fd, &byte, 1, 0) == -1)
        asc_log_error(MSG("wake up send(): %s"), asc_error_msg());
}

/*
 * callback queue
 */

/* add a procedure to main loop's job list */
void asc_job_queue(void *owner, loop_callback_t proc, void *arg)
{
    bool overflow = false;

    asc_mutex_lock(&main_loop->job_mutex);
    if (main_loop->job_cnt < JOB_QUEUE_SIZE)
    {
        loop_job_t *const job = &main_loop->jobs[main_loop->job_cnt++];

        job->proc = proc;
        job->arg = arg;
        job->owner = owner;
    }
    else
    {
        main_loop->job_cnt = 0;
        overflow = true;
    }
    asc_mutex_unlock(&main_loop->job_mutex);

    if (overflow)
        asc_log_error(MSG("job queue overflow, list flushed"));
}

/* remove jobs belonging to a specific module or object */
void asc_job_prune(void *owner)
{
    unsigned int i = 0;

    asc_mutex_lock(&main_loop->job_mutex);
    while (i < main_loop->job_cnt)
    {
        loop_job_t *const job = &main_loop->jobs[i];

        if (job->owner == owner)
        {
            main_loop->job_cnt--;
            memmove(job, &job[1], (main_loop->job_cnt - i) * sizeof(*job));
        }
        else
        {
            i++;
        }
    }
    asc_mutex_unlock(&main_loop->job_mutex);
}

/* run all queued callbacks */
static
void run_jobs(void)
{
    loop_job_t *const first = &main_loop->jobs[0];
    loop_job_t job;

    asc_mutex_lock(&main_loop->job_mutex);
    while (main_loop->job_cnt > 0)
    {
        /* pull first job in queue */
        main_loop->job_cnt--;
        job = *first;
        memmove(first, &first[1], main_loop->job_cnt * sizeof(*first));

        /* run it with mutex unlocked */
        asc_mutex_unlock(&main_loop->job_mutex);
        job.proc(job.arg);
        asc_mutex_lock(&main_loop->job_mutex);
    }
    asc_mutex_unlock(&main_loop->job_mutex);
}

/*
 * event loop
 */

void asc_main_loop_init(void)
{
    main_loop = ASC_ALLOC(1, asc_main_loop_t);

    main_loop->wake_fd[0] = main_loop->wake_fd[1] = -1;
    asc_mutex_init(&main_loop->job_mutex);
}

void asc_main_loop_destroy(void)
{
    if (main_loop == NULL)
        return;

    wake_close();
    asc_mutex_destroy(&main_loop->job_mutex);

    ASC_FREE(main_loop, free);
}

/* process events, return when a shutdown or reload is requested */
bool asc_main_loop_run(void)
{
    uint64_t current_time = asc_utime();
    uint64_t gc_check_timeout = current_time;
    unsigned int ev_sleep = 0;

    while (true)
    {
        if (!asc_event_core_loop(ev_sleep))
            return true; /* polling failed, restart instance */

        if (main_loop->flags != 0)
        {
            const uint32_t flags = main_loop->flags;
            main_loop->flags = 0;

            if (flags & MAIN_LOOP_SHUTDOWN)
            {
                main_loop->stop_cnt = 0;
                return false;
            }
            else if (flags & MAIN_LOOP_RELOAD)
            {
                return true;
            }
            else if (flags & MAIN_LOOP_SIGHUP)
            {
                asc_log_reopen();

                lua_getglobal(lua, "on_sighup");
                if (lua_isfunction(lua, -1))
                {
                    if (lua_tr_call(lua, 0, 0) != 0)
                        lua_err_log(lua);
                }
                else
                {
                    lua_pop(lua, 1);
                }
            }
        }

        current_time = asc_utime();
        if ((current_time - gc_check_timeout) >= LUA_GC_TIMEOUT)
        {
            gc_check_timeout = current_time;
            lua_gc(lua, LUA_GCCOLLECT, 0);
        }

        run_jobs();
        ev_sleep = asc_timer_core_loop();
    }
}

/*
 * loop controls
 */

/* request graceful shutdown, abort if called multiple times */
void asc_main_loop_shutdown(void)
{
    if (main_loop->flags & MAIN_LOOP_SHUTDOWN)
    {
        if (++main_loop->stop_cnt >= 3)
        {
            /*
             * NOTE: can't use regular exit() here as this is usually
             *       run by a signal handler thread. cleanup will try to
             *       join the thread on itself, possibly resulting in
             *       a deadlock.
             */
            _exit(EXIT_MAINLOOP);
        }
        else if (main_loop->stop_cnt >= 2)
        {
            asc_log_error(MSG("main thread appears to be blocked; "
                              "will abort on next shutdown request"));
        }
    }

    main_loop->flags |= MAIN_LOOP_SHUTDOWN;
}

/* ask loader program (i.e. main.c) to restart the instance */
void asc_main_loop_reload(void)
{
    main_loop->flags |= MAIN_LOOP_RELOAD;
}

/* reopen logs and run `on_sighup` Lua function if defined */
void asc_main_loop_sighup(void)
{
    main_loop->flags |= MAIN_LOOP_SIGHUP;
}
