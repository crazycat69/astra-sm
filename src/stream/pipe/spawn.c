/*
 * Astra Core (Process spawning)
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

#ifndef _REMOVE_ME_
    /* TODO: add Win32 support and put this under core/spawn */
#   include "spawn.h"
#endif /* _REMOVE_ME_ */

#ifndef _WIN32
#include <signal.h>

/* maximum signal number */
#ifndef NSIG
#   ifdef _NSIG
#       define NSIG _NSIG
#   else
#       define NSIG 64
#   endif /* _NSIG */
#endif /* !NSIG */

#define PIPE_RD 0
#define PIPE_WR 1

/* async-signal-safe functions for child process */
static inline __func_pure
size_t strlen_s(const char *s)
{
    size_t n = 0;
    while (*(s++))
        n++;

    return n;
}

static inline
void perror_s(const char *s)
{
    const char *msg = "Unknown error";
    if (errno < sys_nerr && sys_errlist[errno] != NULL)
        msg = sys_errlist[errno];

    size_t slen;
    if (s != NULL && (slen = strlen_s(s)))
    {
        write(STDERR_FILENO, s, slen);
        write(STDERR_FILENO, ": ", 2);
    }

    write(STDERR_FILENO, msg, strlen_s(msg));
    write(STDERR_FILENO, "\n", 1);
}

/* safety wrapper for pipe() to set the close-on-exec flag */
#ifndef HAVE_PIPE2
static inline
int __pipe(int fds[2])
{
    if (pipe(fds) != 0)
        return -1;

    if (fcntl(fds[PIPE_RD], F_SETFD, FD_CLOEXEC) != 0)
        goto fail;

    if (fcntl(fds[PIPE_WR], F_SETFD, FD_CLOEXEC) != 0)
        goto fail;

    return 0;

fail:
    close(fds[PIPE_RD]);
    close(fds[PIPE_WR]);

    return -1;
}
#else
#   define __pipe(__fds) pipe2(__fds, O_CLOEXEC)
#endif /* !HAVE_PIPE2 */

/* create a pipe with an optional non-blocking side and return its fd */
int asc_pipe_open(int fds[2], int *parent_fd, int parent_side)
{
    if (__pipe(fds) != 0)
        return -1;

    if (parent_fd != NULL)
    {
        if (fcntl(fds[parent_side], F_SETFL, O_NONBLOCK) != 0)
        {
            /* couldn't set non-blocking mode; this shouldn't happen */
            close(fds[0]);
            close(fds[1]);
            fds[0] = fds[1] = -1;

            return -1;
        }

        *parent_fd = fds[parent_side];
    }

    return 0;
}

__asc_inline
int asc_pipe_close(int fd)
{
    return close(fd);
}

/* create a child process with redirected stdio */
pid_t asc_child_spawn(const char *command, int *sin, int *sout, int *serr)
{
    pid_t pid = -1;
    int pipes[6] = { -1, -1, -1, -1, -1, -1 };

    /* make stdio pipes */
    int *const to_child = &pipes[0];
    int *const from_child = &pipes[2];
    int *const err_pipe = &pipes[4];

    if (asc_pipe_open(to_child, sin, PIPE_WR) != 0)
        goto fail;

    if (asc_pipe_open(from_child, sout, PIPE_RD) != 0)
        goto fail;

    if (asc_pipe_open(err_pipe, serr, PIPE_RD) != 0)
        goto fail;

    /* fork and exec */
    pid = fork();
    if (pid == 0)
    {
        /* we're the child; redirect stdio */
        dup2(to_child[PIPE_RD], STDIN_FILENO);
        fcntl(STDIN_FILENO, F_SETFD, 0);

        dup2(from_child[PIPE_WR], STDOUT_FILENO);
        fcntl(STDOUT_FILENO, F_SETFD, 0);

        dup2(err_pipe[PIPE_WR], STDERR_FILENO);
        fcntl(STDERR_FILENO, F_SETFD, 0);

        /* reset signal handlers and masks */
        for (size_t sig = 1; sig <= NSIG; sig++)
        {
            struct sigaction sa;
            if (sigaction(sig, NULL, &sa) == 0 && sa.sa_handler != SIG_DFL)
                signal(sig, SIG_DFL);
        }

        sigset_t mask;
        sigemptyset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);

        /* detach from terminal */
        setsid();

        /* go to root directory */
        chdir("/");

        /* try to run command */
        execle("/bin/sh", "sh", "-c", command, NULL, environ);
        perror_s("execl()");
        _exit(127);
    }
    else if (pid > 0)
    {
        /* we're the parent; close unused pipe ends and return */
        close(to_child[PIPE_RD]);
        close(from_child[PIPE_WR]);
        close(err_pipe[PIPE_WR]);

        return pid;
    }

fail:
    for (size_t i = 0; i < ASC_ARRAY_SIZE(pipes); i++)
    {
        if (pipes[i] != -1)
            close(pipes[i]);
    }

    return -1;
}
#else /* !_WIN32 */
    // XXX: asc_child_spawn() notes
    //      close hThread right away, return hProcess (typedef'd as pid_t)
#   error "FIXME: support creating child processes under Win32"
#endif /* !_WIN32 */
