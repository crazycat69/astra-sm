/*
 * Astra Core (Process spawning)
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
#include <core/spawn.h>

/*
 * spawning routines
 */

#ifdef _WIN32

/* create job object with kill-on-close limit */
static
HANDLE create_kill_job(void)
{
    HANDLE jo = CreateJobObject(NULL, NULL);
    if (jo == NULL)
        return NULL;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
    memset(&jeli, 0, sizeof(jeli));
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    const JOBOBJECTINFOCLASS joic = JobObjectExtendedLimitInformation;
    if (!SetInformationJobObject(jo, joic, &jeli, sizeof(jeli)))
        ASC_FREE(jo, CloseHandle);

    return jo;
}

/* create child process with redirected stdio */
static
int create_redirected(const char *command
                      , HANDLE *job, PROCESS_INFORMATION *pi
                      , HANDLE sin, HANDLE sout, HANDLE serr)
{
    /* enable inheritance on stdio handles */
    const DWORD h_flags = HANDLE_FLAG_INHERIT;
    if (!SetHandleInformation(sin, h_flags, h_flags))
        return -1;

    if (!SetHandleInformation(sout, h_flags, h_flags))
        return -1;

    if (!SetHandleInformation(serr, h_flags, h_flags))
        return -1;

    /* try to run command */
    STARTUPINFO si;
    memset(&si, 0, sizeof(si));

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = sin;
    si.hStdOutput = sout;
    si.hStdError = serr;

    char *const buf = strdup(command);
    const bool ret = CreateProcess(NULL, buf, NULL, NULL, true
                                   , (CREATE_NEW_PROCESS_GROUP
                                      /*
                                       * | CREATE_NO_WINDOW
                                       *
                                       * FIXME: sending ctrl+break doesn't work
                                       */
                                      | CREATE_SUSPENDED
                                      | CREATE_BREAKAWAY_FROM_JOB)
                                   , NULL, NULL, &si, pi);
    free(buf);

    if (!ret)
        return -1;

    /* if possible, set child to terminate when parent quits */
    BOOL in_job;
    if (IsProcessInJob(pi->hProcess, NULL, &in_job) && !in_job)
    {
        HANDLE new_job = create_kill_job();
        if (new_job == NULL
            || !AssignProcessToJobObject(new_job, pi->hProcess))
        {
            TerminateProcess(pi->hProcess, 0);

            ASC_FREE(pi->hProcess, CloseHandle);
            ASC_FREE(pi->hThread, CloseHandle);
            ASC_FREE(new_job, CloseHandle);

            return -1;
        }

        *job = new_job;
    }
    else
    {
        *job = NULL;
    }

    /* begin execution */
    ResumeThread(pi->hThread);

    return 0;
}

#else /* _WIN32 */

/* user environment */
#if !HAVE_DECL_ENVIRON
extern char **environ;
#endif /* !HAVE_DECL_ENVIRON */

/* maximum signal number */
#ifndef NSIG
#   ifdef _NSIG
#       define NSIG _NSIG
#   else
#       define NSIG 64
#   endif /* _NSIG */
#endif /* !NSIG */

/* async-signal-safe functions for child process */
static __func_pure
size_t strlen_s(const char *s)
{
    size_t n = 0;
    while (*(s++))
        n++;

    return n;
}

static
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

/* fork, redirect stdio and execute `command' */
static
int fork_and_exec(const char *command, pid_t *out_pid
                  , int sin, int sout, int serr)
{
    static const char pfx[] = "exec ";
    const size_t bufsiz = sizeof(pfx) + strlen(command);
    char *const buf = (char *)calloc(1, bufsiz);

    if (buf != NULL)
    {
        memcpy(buf, pfx, sizeof(pfx));
        strncat(buf, command, bufsiz - 1 - strlen(buf));
        command = buf;
    }

    *out_pid = fork();
    if (*out_pid == 0)
    {
        /* we're the child; redirect stdio */
        dup2(sin, STDIN_FILENO);
        fcntl(STDIN_FILENO, F_SETFD, 0);

        dup2(sout, STDOUT_FILENO);
        fcntl(STDOUT_FILENO, F_SETFD, 0);

        dup2(serr, STDERR_FILENO);
        fcntl(STDERR_FILENO, F_SETFD, 0);

        /* reset signal handlers and masks */
        for (size_t sig = 1; sig < NSIG; sig++)
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

        /* try to run command */
        execle("/bin/sh", "sh", "-c", command, NULL, environ);
        perror_s("execle()");
        _exit(127);
    }

    free(buf);
    return (*out_pid > 0) ? 0 : -1;
}

#endif /* !_WIN32 */

/*
 * process helper functions
 */

#ifdef _WIN32

pid_t asc_process_wait(const asc_process_t *proc, int *status, bool block)
{
    if (block)
    {
        const DWORD ret = WaitForSingleObject(proc->pi.hProcess, INFINITE);
        if (ret != WAIT_OBJECT_0)
            return -1;
    }

    DWORD code = STILL_ACTIVE;
    if (!GetExitCodeProcess(proc->pi.hProcess, &code))
        return -1;
    else if (code == STILL_ACTIVE)
        return 0;

    if (status != NULL)
        *status = code;

    return proc->pi.dwProcessId;
}

static BOOL CALLBACK enum_proc(HWND hwnd, LPARAM lparam)
{
    const asc_process_t *const proc = (asc_process_t *)lparam;

    DWORD pid = 0;
    if (GetWindowThreadProcessId(hwnd, &pid) != 0)
    {
        if (pid == proc->pi.dwProcessId)
            SendMessage(hwnd, WM_CLOSE, 0, 0);
    }

    return true;
}

int asc_process_kill(const asc_process_t *proc, bool forced)
{
    /* check process status before sending signals */
    const DWORD ret = WaitForSingleObject(proc->pi.hProcess, 0);
    if (ret == WAIT_OBJECT_0)
        return 0; /* process dead, handle still open */
    else if (ret == WAIT_FAILED)
        return -1; /* invalid handle or some other error */

    if (forced)
    {
        if (!TerminateProcess(proc->pi.hProcess, EXIT_FAILURE))
            return -1;
    }
    else
    {
        if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, proc->pi.dwProcessId))
            return -1;

        if (!EnumWindows(enum_proc, (LPARAM)proc))
            return -1;
    }

    return 0;
}

#else /* _WIN32 */

int asc_process_kill(const asc_process_t *proc, bool forced)
{
    const int olderr = errno;
    const int ret = kill(*proc, (forced ? SIGKILL : SIGTERM));

    /* on some systems, kill() fails when used on a zombie process */
    if (ret == -1 && errno == ESRCH)
    {
        errno = olderr;
        return 0;
    }

    return ret;
}

#endif /* !_WIN32 */

/*
 * pipe-related functions
 */

/* close `n' sockets starting from `first' */
static
void closeall(int *first, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        asc_pipe_close(*first);
        *(first++) = -1;
    }
}

#ifdef _WIN32

#ifndef SIO_LOOPBACK_FAST_PATH
#   define SIO_LOOPBACK_FAST_PATH _WSAIOW(IOC_VENDOR, 16)
#endif /* !SIO_LOOPBACK_FAST_PATH */

#define PIPE_BUFFER (256 * 1024) /* 256 KiB */

static
int prepare_socket(SOCKET s)
{
    int one = 1;

    /* set SO_REUSEADDR */
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR
                   , (char *)&one, sizeof(one)) != 0)
        return -1;

    /* set TCP_NODELAY */
    if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY
                   , (char *)&one, sizeof(one)) != 0)
        return -1;

    /* set socket buffer size */
    const int bufsiz = PIPE_BUFFER;
    if (setsockopt(s, SOL_SOCKET, SO_SNDBUF
                   , (const char *)&bufsiz, sizeof(bufsiz)) != 0)
        return -1;

    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF
                   , (const char *)&bufsiz, sizeof(bufsiz)) != 0)
        return -1;

    /* enable TCP Fast Loopback if available */
    DWORD bytes = 0;
    WSAIoctl(s, SIO_LOOPBACK_FAST_PATH, &one, sizeof(one)
             , NULL, 0, &bytes, NULL, NULL);

    return 0;
}

/* close socket without clobbering last-error */
static
int closesocket_s(SOCKET s)
{
    const int olderr = GetLastError();
    const int ret = closesocket(s);
    if (ret == 0)
        SetLastError(olderr);

    return ret;
}

/* make selectable pipe by connecting two TCP sockets */
static
int socketpipe(int fds[2])
{
    union
    {
        struct sockaddr_in in;
        struct sockaddr addr;
    } sa_listen, sa_client, sa_req;

    SOCKET listener = INVALID_SOCKET;
    SOCKET client = INVALID_SOCKET;
    SOCKET server = INVALID_SOCKET;

    int addrlen = sizeof(sa_listen.in);

    memset(&sa_listen, 0, sizeof(sa_listen));
    sa_listen.in.sin_family = AF_INET;
    sa_listen.in.sin_port = 0;
    sa_listen.in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    memcpy(&sa_client, &sa_listen, sizeof(sa_listen));

    /* establish listening socket */
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET)
        goto fail;

    if (prepare_socket(listener) != 0)
        goto listen_fail;

    if (bind(listener, &sa_listen.addr, addrlen) != 0)
        goto listen_fail;

    if (getsockname(listener, &sa_listen.addr, &addrlen) != 0)
        goto listen_fail;

    if (listen(listener, SOMAXCONN) != 0)
        goto listen_fail;

    /* open first pipe end, connect it to listener */
    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client == INVALID_SOCKET)
        goto listen_fail;

    if (prepare_socket(client) != 0)
        goto client_fail;

    if (bind(client, &sa_client.addr, addrlen) != 0)
        goto client_fail;

    if (getsockname(client, &sa_client.addr, &addrlen) != 0)
        goto client_fail;

    if (connect(client, &sa_listen.addr, addrlen) != 0)
        goto client_fail;

    /* accept the connection request */
    while (true)
    {
        server = accept(listener, &sa_req.addr, &addrlen);
        if (server == INVALID_SOCKET)
            goto client_fail;

        if (sa_req.in.sin_port == sa_client.in.sin_port
            && sa_req.in.sin_addr.s_addr == sa_client.in.sin_addr.s_addr)
        {
            closesocket_s(listener);
            break;
        }

        /* discard stray connection */
        closesocket_s(server);
        server = INVALID_SOCKET;
    }

    fds[0] = client;
    fds[1] = server;

    return 0;

client_fail:
    closesocket_s(client);
listen_fail:
    closesocket_s(listener);
fail:
    return -1;
}

int asc_pipe_inherit(int fd, bool inherit)
{
    const HANDLE sock = ASC_TO_HANDLE(fd);
    const DWORD val = inherit ? 1 : 0;

    if (SetHandleInformation(sock, HANDLE_FLAG_INHERIT, val))
        return 0;
    else
        return -1;
}

int asc_pipe_close(int fd)
{
    return closesocket_s(fd);
}

#else /* _WIN32 */

#define PIPE_BUFFER (256 * 1024) /* 256 KiB */

/* call socketpair(), set close-on-exec and increase buffer size */
static
int socketpipe(int fds[2])
{
#ifdef SOCK_CLOEXEC
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, PF_UNSPEC, fds) != 0)
#endif /* SOCK_CLOEXEC */
    {
        if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fds) != 0)
            return -1;

        if (fcntl(fds[PIPE_RD], F_SETFD, FD_CLOEXEC) != 0
            || fcntl(fds[PIPE_WR], F_SETFD, FD_CLOEXEC) != 0)
        {
            closeall(fds, 2);
            return -1;
        }
    }

    for (unsigned int i = 0; i < 2; i++)
    {
        int val = 0;
        socklen_t len = sizeof(val);

        const int ret = getsockopt(fds[i], SOL_SOCKET, SO_SNDBUF
                                   , (char *)&val, &len);

        if (ret == 0 && val < PIPE_BUFFER)
        {
            val = PIPE_BUFFER;
            setsockopt(fds[i], SOL_SOCKET, SO_SNDBUF
                       , (char *)&val, sizeof(val));
        }
    }

    return 0;
}

int asc_pipe_inherit(int fd, bool inherit)
{
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1)
        return -1;

    if (inherit)
        flags &= ~FD_CLOEXEC;
    else
        flags |= FD_CLOEXEC;

    return fcntl(fd, F_SETFD, flags);
}

int asc_pipe_close(int fd)
{
    return close(fd);
}

#endif /* !_WIN32 */

/*
 * public interface
 */

/* create a pipe with an optional non-blocking side and return its fd */
int asc_pipe_open(int fds[2], int *nb_fd, unsigned int nb_side)
{
    if (socketpipe(fds) != 0)
        return -1;

    for (unsigned int i = 0; i < 2; i++)
    {
        if (nb_side == i || nb_side == PIPE_BOTH)
        {
#ifdef _WIN32
            unsigned long nonblock = 1;
            const int ret = ioctlsocket(fds[i], FIONBIO, &nonblock);
#else /* _WIN32 */
            const int ret = fcntl(fds[i], F_SETFL, O_NONBLOCK);
#endif /* !_WIN32 */

            if (ret != 0)
            {
                /* couldn't set non-blocking mode; this shouldn't happen */
                closeall(fds, 2);
                return -1;
            }

            if (nb_fd != NULL)
                *nb_fd = fds[i];
        }
    }

    return 0;
}

/* create a child process with redirected stdio */
int asc_process_spawn(const char *command, asc_process_t *proc
                      , int *parent_sin, int *parent_sout, int *parent_serr)
{
    /* make stdio pipes */
    int pipes[6] = { -1, -1, -1, -1, -1, -1 };

    if (asc_pipe_open(&pipes[0], parent_sin, PIPE_WR) != 0)
    {
        /* nothing to close */
        return -1;
    }

    if (asc_pipe_open(&pipes[2], parent_sout, PIPE_RD) != 0)
    {
        closeall(pipes, 2);
        return -1;
    }

    if (asc_pipe_open(&pipes[4], parent_serr, PIPE_RD) != 0)
    {
        closeall(pipes, 4);
        return -1;
    }

    /* call OS-specific spawning function */
    const int child_sin = pipes[0 + PIPE_RD];
    const int child_sout = pipes[2 + PIPE_WR];
    const int child_serr = pipes[4 + PIPE_WR];

#ifdef _WIN32
    if (create_redirected(command, &proc->job, &proc->pi
                          , ASC_TO_HANDLE(child_sin)
                          , ASC_TO_HANDLE(child_sout)
                          , ASC_TO_HANDLE(child_serr)) != 0)
#else /* _WIN32 */
    if (fork_and_exec(command, proc
                      , child_sin, child_sout, child_serr) != 0)
#endif /* !_WIN32 */
    {
        closeall(pipes, 6);
        return -1;
    }

    /* close far side pipe ends */
    asc_pipe_close(child_sin);
    asc_pipe_close(child_sout);
    asc_pipe_close(child_serr);

    return 0;
}
