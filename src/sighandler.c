/*
 * Astra Main App (OS signal handling)
 * http://cesbo.com/astra
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
#include "sighandler.h"

#ifndef _WIN32
#include <signal.h>
#include <pthread.h>

/* TODO: move these to core/thread.h */
#define mutex_lock(__mutex) pthread_mutex_lock(&__mutex)
#define mutex_unlock(__mutex) pthread_mutex_unlock(&__mutex)

struct signal_setup
{
    const int signum;
    const bool ignore;
    sighandler_t oldhandler;
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
static pthread_mutex_t signal_lock = PTHREAD_MUTEX_INITIALIZER;
static bool quit_thread = true;

static void perror_exit(int errnum, const char *str)
{
    fprintf(stderr, "%s: %s\n", str, strerror(errnum));
    exit(EXIT_FAILURE);
}

static void *thread_loop(void *arg)
{
    __uarg(arg);

    while (true)
    {
        int signum;
        const int ret = sigwait(&block_mask, &signum);
        if (ret != 0)
            perror_exit(ret, "sigwait()");

        pthread_mutex_lock(&signal_lock);
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
                astra_shutdown();
                break;

            case SIGUSR1:
                astra_reload();
                break;

            case SIGHUP:
                astra_sighup();
                break;

            case SIGQUIT:
                astra_abort();
                break;

            default:
                break;
        }

        pthread_mutex_unlock(&signal_lock);
    }

    return NULL;
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
                perror_exit(errno, "signal()");
        }
        else
        {
            sigaddset(&block_mask, ss->signum);
        }
    }

    int ret = pthread_sigmask(SIG_BLOCK, &block_mask, &old_mask);
    if (ret != 0)
        perror_exit(ret, "pthread_sigmask()");

    /* delay any actions until main thread finishes initialization */
    ret = pthread_mutex_lock(&signal_lock);
    if (ret != 0)
        perror_exit(ret, "pthread_mutex_lock()");

    /* start our signal handling thread */
    quit_thread = false;
    ret = pthread_create(&signal_thread, NULL, &thread_loop, NULL);
    if (ret != 0)
        perror_exit(ret, "pthread_create()");
}

void signal_cleanup(void)
{
    /* ask signal thread to quit */
    quit_thread = true;
    pthread_mutex_unlock(&signal_lock);
    if (pthread_kill(signal_thread, SIGTERM) == 0)
        pthread_join(signal_thread, NULL);

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
        perror_exit(ret, "pthread_sigmask()");
}
#else /* !_WIN32 */
/* TODO: move these to core/thread.h */
#define mutex_lock(__mutex) WaitForSingleObject(__mutex, INFINITE)
#define mutex_unlock(__mutex) while (ReleaseMutex(__mutex))

static HANDLE signal_lock;
static bool ignore_ctrl = true;

static void perror_exit(DWORD errnum, const char *str)
{
    LPTSTR msg = NULL;
    FormatMessage((FORMAT_MESSAGE_FROM_SYSTEM
                   | FORMAT_MESSAGE_ALLOCATE_BUFFER
                   | FORMAT_MESSAGE_IGNORE_INSERTS)
                  , NULL, errnum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)
                  , (LPTSTR)&msg, 0, NULL);

    /* NOTE: FormatMessage() appends a newline to error message */ \
    fprintf(stderr, "%s: %s", str, msg);
    exit(EXIT_FAILURE);
}

static BOOL WINAPI ctrl_handler(DWORD type)
{
    /* handlers are run in separate threads */
    mutex_lock(signal_lock);
    if (ignore_ctrl)
    {
        mutex_unlock(signal_lock);
        return true;
    }

    bool ret = true;
    switch (type)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            astra_shutdown();
            break;

        default:
            ret = false;
    }

    mutex_unlock(signal_lock);
    return ret;
}

void signal_setup(void)
{
    /* create and lock signal handling mutex */
    signal_lock = CreateMutex(NULL, true, NULL);
    if (signal_lock == NULL)
        perror_exit(GetLastError(), "CreateMutex()");

    /* register console event handler */
    ignore_ctrl = false;
    if (!SetConsoleCtrlHandler(ctrl_handler, true))
        perror_exit(GetLastError(), "SetConsoleCtrlHandler()");
}

void signal_cleanup(void)
{
    /* dismiss any threads waiting for lock */
    ignore_ctrl = true;
    mutex_unlock(signal_lock);

    /* remove ctrl handler; this also joins handler threads */
    if (!SetConsoleCtrlHandler(ctrl_handler, false))
        perror_exit(GetLastError(), "SetConsoleCtrlHandler()");

    /* free mutex */
    if (!CloseHandle(signal_lock))
        perror_exit(GetLastError(), "CloseHandle()");
}
#endif /* !_WIN32 */

void signal_enable(bool running)
{
    if (running)
        mutex_unlock(signal_lock);
    else
        mutex_lock(signal_lock);
}
