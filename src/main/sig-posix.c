/*
 * Astra (OS signal handling)
 * http://cesbo.com/astra
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

#include <astra/astra.h>
#include <astra/core/mainloop.h>
#include <astra/core/mutex.h>

#include <signal.h>

#include "sig.h"

struct signal_setup
{
    const int signum;
    const bool ignore;
    void (*oldhandler)(int);
};

static struct signal_setup siglist[] = {
    { SIGHUP,  false, NULL },
    { SIGINT,  false, NULL },
    { SIGQUIT, false, NULL },
    { SIGUSR1, false, NULL },
    { SIGTERM, false, NULL },
    { SIGPIPE, true, NULL },
};

static sigset_t block_mask;
static sigset_t old_mask;

static pthread_t signal_thread;
static pthread_mutex_t signal_lock;
static bool quit_thread = true;

static
void *thread_loop(void *arg)
{
    __uarg(arg);

    while (true)
    {
        int signum;
        const int ret = sigwait(&block_mask, &signum);
        if (ret != 0)
            signal_perror(ret, "sigwait()");

        if (!asc_mutex_timedlock(&signal_lock, SIGNAL_LOCK_WAIT))
        {
            /*
             * looks like the main thread disabled signal handling and
             * then got blocked during cleanup or some other routine.
             */
            if (signum == SIGINT || signum == SIGTERM)
                signal_timeout();

            continue;
        }

        if (quit_thread)
        {
            /* signal handling is being shut down */
            pthread_mutex_unlock(&signal_lock);
            pthread_exit(NULL);
        }

        switch (signum)
        {
            case SIGINT:
            case SIGTERM:
                asc_main_loop_shutdown();
                break;

            case SIGUSR1:
                asc_main_loop_reload();
                break;

            case SIGHUP:
                asc_main_loop_sighup();
                break;

            case SIGQUIT:
                asc_lib_abort();
                break;

            default:
                break;
        }

        pthread_mutex_unlock(&signal_lock);
    }

    return NULL;
}

static
void signal_cleanup(void)
{
    /* ask signal thread to quit */
    if (!pthread_equal(pthread_self(), signal_thread))
    {
        pthread_mutex_lock(&signal_lock);
        quit_thread = true;
        pthread_mutex_unlock(&signal_lock);

        if (pthread_kill(signal_thread, SIGTERM) == 0)
            pthread_join(signal_thread, NULL);

        pthread_mutex_destroy(&signal_lock);
    }

    /* restore old handlers for ignored signals */
    for (size_t i = 0; i < ASC_ARRAY_SIZE(siglist); i++)
    {
        const struct signal_setup *const ss = &siglist[i];
        if (ss->ignore)
            signal(ss->signum, ss->oldhandler);
    }

    /* restore old signal mask */
    const int ret = pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
    if (ret != 0)
        signal_perror(ret, "pthread_sigmask()");
}

void signal_setup(void)
{
    /* block signals of interest before starting any threads */
    sigemptyset(&block_mask);

    for (size_t i = 0; i < ASC_ARRAY_SIZE(siglist); i++)
    {
        struct signal_setup *const ss = &siglist[i];
        if (ss->ignore)
        {
            /* doesn't matter which thread gets to ignore it */
            ss->oldhandler = signal(ss->signum, SIG_IGN);
            if (ss->oldhandler == SIG_ERR)
                signal_perror(errno, "signal()");
        }
        else
        {
            sigaddset(&block_mask, ss->signum);
        }
    }

    int ret = pthread_sigmask(SIG_BLOCK, &block_mask, &old_mask);
    if (ret != 0)
        signal_perror(ret, "pthread_sigmask()");

    /* initialize signal lock */
    pthread_mutexattr_t attr;
    ret = pthread_mutexattr_init(&attr);
    if (ret != 0)
        signal_perror(ret, "pthread_mutexattr_init()");

    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if (ret != 0)
        signal_perror(ret, "pthread_mutexattr_settype()");

    ret = pthread_mutex_init(&signal_lock, &attr);
    if (ret != 0)
        signal_perror(ret, "pthread_mutex_init()");

    pthread_mutexattr_destroy(&attr);

    /* delay any actions until main thread finishes initialization */
    ret = pthread_mutex_lock(&signal_lock);
    if (ret != 0)
        signal_perror(ret, "pthread_mutex_lock()");

    /* start our signal handling thread */
    quit_thread = false;
    ret = pthread_create(&signal_thread, NULL, &thread_loop, NULL);
    if (ret != 0)
        signal_perror(ret, "pthread_create()");

    atexit(signal_cleanup);
}

void signal_enable(bool running)
{
    pthread_mutex_lock(&signal_lock);

    if (running)
        pthread_mutex_unlock(&signal_lock);
}
