/*
 * Child process test program
 *
 * Copyright (C) 2016, Artem Kharitonov <artem@3phase.pw>
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

#define do_nothing() \
    do { \
        while (true) { sleep(86400); } \
    } while (0)

#ifdef _WIN32
static BOOL WINAPI console_handler(DWORD type)
{
    __uarg(type);
    fprintf(stderr, "peep\n");
    return TRUE;
}
#else /* _WIN32 */
#include <signal.h>
#include <sys/socket.h>

static void signal_handler(int signum)
{
    __uarg(signum);
    fprintf(stderr, "peep\n");
}
#endif /* !_WIN32 */

/* ignore termination signals */
static void __dead cmd_bandit(void)
{
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, true);
#else /* _WIN32 */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    signal(SIGPIPE, SIG_IGN);
#endif /* !_WIN32 */

    fprintf(stderr, "peep\n");
    do_nothing();
}

/* read from fd and write to another fd */
static void cmd_cat(int rfd, int wfd, bool stdio)
{
#ifdef _WIN32
    /* NOTE: read() and write() don't work on sockets on Win32 */
    if (!stdio)
    {
        WSADATA wsaData;
        const int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (err != 0)
        {
            fprintf(stderr, "WSAStartup() failed\n");
            exit(EXIT_FAILURE);
        }
    }
#endif

    while (true)
    {
        char buf[512];

        ssize_t in;
        if (stdio)
            in = read(rfd, buf, sizeof(buf));
        else
            in = recv(rfd, buf, sizeof(buf), 0);

        if (in < 0)
        {
            fprintf(stderr, "read from fd returned %zd!\n", in);
            exit(EXIT_FAILURE);
        }
        else if (in > 0)
        {
            ssize_t out;
            if (stdio)
                out = write(wfd, buf, (size_t)in);
            else
                out = send(wfd, buf, (size_t)in, 0);

            if (out != in)
            {
                fprintf(stderr, "write to fd returned %zd!\n", out);
                exit(EXIT_FAILURE);
            }
        }
        else
            return;
    }
}

/* close all stdio fds and do nothing */
static void __dead cmd_close(void)
{
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    do_nothing();
}

/* exit with a given return value */
static void __dead cmd_exit(int rc)
{
    exit(rc);
}

/* report my pid to stdout and exit */
static void cmd_pid(void)
{
    char buf[512] = { 0 };
    const int len = snprintf(buf, sizeof(buf), "%lld\n", (long long)getpid());
    write(STDOUT_FILENO, buf, len);

    do_nothing();
}

/* report current date to stdout once per second */
static void __dead cmd_ticker(void)
{
    while (true)
    {
        char buf[512] = { 0 };

        const time_t t = time(NULL);
        const int len = snprintf(buf, sizeof(buf), "%s", ctime(&t));
        const int ret = write(STDOUT_FILENO, buf, len);
        if (ret <= 0)
            exit(EXIT_FAILURE);

        sleep(1);
    }
}

/* output TS packets interspersed with some random bytes */
static void trash(void)
{
    char buf[32];

    const size_t writes = 1 + (rand() % 32);
    for (size_t i = 0; i < writes; i++)
    {
        const size_t len = 1 + (rand() % sizeof(buf));
        for (size_t j = 0; j < len;)
        {
            buf[j] = rand();
            if (buf[j] != 0x47)
                j++;
        }

        const int ret = write(STDOUT_FILENO, buf, len);
        if (ret <= 0)
            exit(EXIT_FAILURE);
    }
}

static void cmd_unaligned(unsigned int cnt)
{
    const unsigned int pid = 0x100;
    unsigned int cc = 15;

    while (cnt-- > 0)
    {
        trash();

        const size_t pkts = rand() % 100;
        for (size_t i = 0; i < pkts; i++)
        {
            uint8_t ts[TS_PACKET_SIZE] = { 0x47 };

            cc = (cc + 1) & 0xf;
            TS_SET_PID(ts, pid);
            TS_SET_CC(ts, cc);

            const int ret = write(STDOUT_FILENO, ts, sizeof(ts));
            if (ret <= 0)
                exit(EXIT_FAILURE);
        }

        trash();
    }
}

static void __dead usage(const char *argv0)
{
    fprintf(stderr, "usage: %s <cmd> [args]\n", argv0);
    exit(EXIT_FAILURE);
}

int main(int argc, const char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

#ifdef _WIN32
    setmode(STDIN_FILENO, O_BINARY);
    setmode(STDOUT_FILENO, O_BINARY);
    setmode(STDERR_FILENO, O_BINARY);
#endif /* _WIN32 */

    if (argc <= 1)
        usage(argv[0]);

    if (!strcmp(argv[1], "bandit"))
        cmd_bandit();
    else if (!strcmp(argv[1], "cat") && argc >= 3)
        cmd_cat(STDIN_FILENO, atoi(argv[2]), true);
    else if (!strcmp(argv[1], "close"))
        cmd_close();
    else if (!strcmp(argv[1], "exit") && argc >= 3)
        cmd_exit(atoi(argv[2]));
    else if (!strcmp(argv[1], "pid"))
        cmd_pid();
    else if (!strcmp(argv[1], "pipefd") && argc >= 3)
    {
        const int fd = atoi(argv[2]);
        cmd_cat(fd, fd, false);
    }
    else if (!strcmp(argv[1], "ticker"))
        cmd_ticker();
    else if (!strcmp(argv[1], "unaligned") && argc >= 3)
        cmd_unaligned(atoi(argv[2]));
    else
        usage(argv[0]);

    return EXIT_SUCCESS;
}
