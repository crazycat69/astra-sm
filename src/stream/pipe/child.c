/*
 * Astra Module: Pipe (Process spawning)
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

#include "pipe.h"

#ifndef _WIN32
#include <signal.h>

#define PIPE_RD 0
#define PIPE_WR 1

/* async-signal-safe functions for child process */
static inline
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

/* create a pipe with a non-blocking side and write its fd to parent_fd */
static inline
int make_pipe(int fds[2], int *parent_fd, int parent_side)
{
    if (__pipe(fds) != 0)
        return -1;

    if (fcntl(fds[parent_side], F_SETFL, O_NONBLOCK) != 0)
    {
        /* couldn't set non-blocking mode; this shouldn't happen */
        close(fds[PIPE_RD]);
        close(fds[PIPE_WR]);
        fds[0] = fds[1] = -1;

        return -1;
    }

    *parent_fd = fds[parent_side];

    return 0;
}

/* create a child process with redirected stdio */
pid_t pipe_spawn(const char *command
                 , int *readfd, int *writefd, int *errfd) // swap readfd/writefd
{
    pid_t pid = -1;
    int pipes[6] = { -1, -1, -1, -1, -1, -1 };

    /* make stdio pipes */
    int *to_child = &pipes[0];
    if (make_pipe(to_child, writefd, PIPE_WR) != 0)
        goto fail;

    int *from_child = &pipes[2];
    if (make_pipe(from_child, readfd, PIPE_RD) != 0)
        goto fail;

    int *err_pipe = &pipes[4];
    if (make_pipe(err_pipe, errfd, PIPE_RD) != 0)
        goto fail;

    /* fork and exec */
    pid = fork();
    if (pid == 0)
    {
        /* we're the child; redirect stdio */
        if (to_child[PIPE_RD] != STDIN_FILENO)
            dup2(to_child[PIPE_RD], STDIN_FILENO);

        if (from_child[PIPE_WR] != STDOUT_FILENO)
            dup2(from_child[PIPE_WR], STDOUT_FILENO);

        if (err_pipe[PIPE_WR] != STDERR_FILENO)
            dup2(err_pipe[PIPE_WR], STDERR_FILENO);

        // XXX: does an FD retain O_NONBLOCK after dup2()? after exec()?

        // XXX: if SIGPIPE is ignored in parent,
        //      does the child retain SIG_IGN after exec()?

        /* unmask all signals */
        sigset_t mask;
        sigemptyset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);

        /* try to run command */
        execle("/bin/sh", "sh", "-c", command, NULL, environ);
        perror_s("execl()");
        _exit(127);
    }
    else if (pid > 0)
    {
        /* we're the parent; close unused pipe ends */
        close(to_child[PIPE_RD]);
        close(from_child[PIPE_WR]);
        close(err_pipe[PIPE_WR]);
    }
    else
        goto fail;

    return pid;

fail:
    for (size_t i = 0; i < ASC_ARRAY_SIZE(pipes); i++)
    {
        if (pipes[i] != -1)
            close(pipes[i]);
    }

    return -1;
}

#else /* !_WIN32 */
#   error "FIXME: support creating child processes under Win32"
#endif /* !_WIN32 */
