/*
 * Astra: Unit tests
 * http://cesbo.com/astra
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

#include "../test_libastra.h"
#include <core/spawn.h>
#include <core/event.h>
#include <core/mainloop.h>
#include <core/socket.h>
#include <core/thread.h>
#include <utils/crc32b.h>

#ifndef _WIN32
#   include <signal.h>
#   ifdef HAVE_SYS_SELECT_H
#       include <sys/select.h>
#   endif /* HAVE_SYS_SELECT_H */
#endif /* !_WIN32 */

#define TEST_SLAVE "./misc/slave"

static void pipe_check_nb(int fd, bool expect)
{
#ifdef _WIN32
    /* NOTE: on Win32, there's no way to test if a socket is non-blocking */
    __uarg(fd);
    __uarg(expect);
#else /* _WIN32 */
    const int ret = fcntl(fd, F_GETFL);
    ck_assert(ret != -1);
    ck_assert(expect == ((ret & O_NONBLOCK) != 0));
#endif /* !_WIN32 */
}

#define MIN_BUF_SIZE (64 * 1024) /* 64 KiB */

static void pipe_check_buf(int fd)
{
    static const int optlist[] =
    {
        SO_SNDBUF,
#ifdef _WIN32
        SO_RCVBUF,
#endif
    };

    for (size_t i = 0; i < ASC_ARRAY_SIZE(optlist); i++)
    {
        int val = 0;
        socklen_t optlen = sizeof(val);

        const int ret = getsockopt(fd, SOL_SOCKET, optlist[i]
                                   , (char *)&val, &optlen);
        ck_assert(ret == 0);
        ck_assert(val >= MIN_BUF_SIZE);
    }
}

static bool pipe_get_inherit(int fd)
{
#ifdef _WIN32
    DWORD flags = 0;
    const bool ret = GetHandleInformation(ASC_TO_HANDLE(fd), &flags);
    ck_assert(ret == true);

    return flags & HANDLE_FLAG_INHERIT;
#else /* _WIN32 */
    const int ret = fcntl(fd, F_GETFD);
    ck_assert(ret != -1);

    return (!(ret & FD_CLOEXEC));
#endif /* !_WIN32 */
}

/* open and close pipe */
static void pipe_open_test(unsigned int nb_side)
{
    int fds[2] = { -1, -1 };
    int nb_fd = -1;

    int ret = asc_pipe_open(fds, &nb_fd, nb_side);
    ck_assert(ret == 0 && fds[PIPE_RD] != -1 && fds[PIPE_WR] != -1);

    /* check inheritability and socket buffers */
    ck_assert(pipe_get_inherit(fds[PIPE_RD]) == false);
    ck_assert(pipe_get_inherit(fds[PIPE_WR]) == false);
    pipe_check_buf(fds[PIPE_RD]);
    pipe_check_buf(fds[PIPE_WR]);

    /* verify non-blocking flag */
    switch (nb_side)
    {
        case PIPE_RD:
            ck_assert(nb_fd == fds[PIPE_RD]);
            pipe_check_nb(fds[PIPE_RD], true);
            pipe_check_nb(fds[PIPE_WR], false);
            break;

        case PIPE_WR:
            ck_assert(nb_fd == fds[PIPE_WR]);
            pipe_check_nb(fds[PIPE_RD], false);
            pipe_check_nb(fds[PIPE_WR], true);
            break;

        case PIPE_BOTH:
            /* nb_fd is irrelevant here */
            pipe_check_nb(fds[PIPE_RD], true);
            pipe_check_nb(fds[PIPE_WR], true);
            break;

        case PIPE_NONE:
        default:
            ck_assert(nb_fd == -1);
            pipe_check_nb(fds[PIPE_RD], false);
            pipe_check_nb(fds[PIPE_WR], false);
    }

    /* try using pipe with select() */
    fd_set rs, ws, es;

    FD_ZERO(&rs);
    FD_SET((unsigned)fds[PIPE_RD], &rs);
    FD_SET((unsigned)fds[PIPE_WR], &rs);

    FD_ZERO(&ws);
    FD_SET((unsigned)fds[PIPE_RD], &ws);
    FD_SET((unsigned)fds[PIPE_WR], &ws);

    FD_ZERO(&es);
    FD_SET((unsigned)fds[PIPE_RD], &es);
    FD_SET((unsigned)fds[PIPE_WR], &es);

    int nfds;
    if (fds[PIPE_RD] > fds[PIPE_WR])
        nfds = fds[PIPE_RD] + 1;
    else
        nfds = fds[PIPE_WR] + 1;

    struct timeval tv;

    memset(&tv, 0, sizeof(tv));
    ret = select(nfds, &rs, NULL, NULL, &tv);
    ck_assert(ret == 0); /* no read events */

    memset(&tv, 0, sizeof(tv));
    ret = select(nfds, NULL, &ws, NULL, &tv);
    ck_assert(ret == 2); /* expect both ends to be writable */

    memset(&tv, 0, sizeof(tv));
    ret = select(nfds, NULL, NULL, &es, &tv);
    ck_assert(ret == 0); /* no exception events */

    ck_assert(asc_pipe_close(fds[PIPE_RD]) == 0);
    ck_assert(asc_pipe_close(fds[PIPE_WR]) == 0);
}

START_TEST(pipe_open)
{
    static const unsigned int cases[] = {
        PIPE_RD, PIPE_WR, PIPE_BOTH, PIPE_NONE
    };
    for (unsigned int i = 0; i < ASC_ARRAY_SIZE(cases); i++)
        pipe_open_test(cases[i]);
}
END_TEST

/* set child process inheritance on pipe */
START_TEST(pipe_inherit)
{
    int fds[2] = { -1, -1 };

    const int ret = asc_pipe_open(fds, NULL, PIPE_NONE);
    ck_assert(ret == 0 && fds[0] != -1 && fds[1] != -1);

    /* check inheritability and socket buffers */
    ck_assert(pipe_get_inherit(fds[0]) == false);
    ck_assert(pipe_get_inherit(fds[1]) == false);
    pipe_check_buf(fds[0]);
    pipe_check_buf(fds[1]);

    for (unsigned int i = 0; i < ASC_ARRAY_SIZE(fds); i++)
    {
        /* enable */
        ck_assert(asc_pipe_inherit(fds[i], true) == 0);
        ck_assert(pipe_get_inherit(fds[i]) == true);

        /* disable */
        ck_assert(asc_pipe_inherit(fds[i], false) == 0);
        ck_assert(pipe_get_inherit(fds[i]) == false);
    }
}
END_TEST

/* write to pipe, read from the other end */
#define BUF_SIZE (128 * 1024) /* 128 KiB */

START_TEST(pipe_write)
{
    int fds[2] = { -1, -1 };
    uint8_t data[BUF_SIZE];

    const int ret = asc_pipe_open(fds, NULL, PIPE_NONE);
    ck_assert(ret == 0 && fds[0] != -1 && fds[1] != -1);

    /* both ends must be blocking and non-inheritable */
    ck_assert(pipe_get_inherit(fds[0]) == false);
    ck_assert(pipe_get_inherit(fds[1]) == false);
    pipe_check_nb(fds[0], false);
    pipe_check_nb(fds[1], false);
    pipe_check_buf(fds[0]);
    pipe_check_buf(fds[1]);

    for (unsigned int i = 0; i < 1000; i++)
    {
        const int rfd = fds[(i % 2)];
        const int wfd = fds[(i % 2) ^ 1];

        /* generate data packet */
        const size_t data_size = 1 + (rand() % (BUF_SIZE - 1));
        for (size_t j = 0; j < data_size; j++)
            data[j] = rand();

        /* send it through pipe */
        const ssize_t out = send(wfd, (char *)data, data_size, 0);
        ck_assert(out == (ssize_t)data_size);

        /* read and compare CRC */
        uint8_t buf[BUF_SIZE];
        size_t rpos = 0;
        while (rpos < data_size)
        {
            const size_t left = sizeof(buf) - rpos;
            const ssize_t in = recv(rfd, (char *)&buf[rpos], left, 0);
            ck_assert(in > 0 && (size_t)in <= left);
            rpos += in;
        }
        ck_assert(rpos == data_size);

        const uint32_t crc1 = au_crc32b(data, data_size);
        const uint32_t crc2 = au_crc32b(buf, rpos);
        ck_assert(crc1 == crc2);
    }

    ck_assert(asc_pipe_close(fds[PIPE_RD]) == 0);
    ck_assert(asc_pipe_close(fds[PIPE_WR]) == 0);
}
END_TEST

/* pipe event notification test */
#define SEND_SIZE (BUF_SIZE / 128)

static uint32_t tx_crc = 0;
static uint32_t rx_crc = 0;
static size_t rx_pos = 0;
static uint8_t rx_buf[BUF_SIZE] = { 0 };

static void thread_proc(void *arg)
{
    const int wfd = *((int *)arg);

    /* generate random data */
    uint8_t tx_buf[BUF_SIZE];
    for (size_t i = 0; i < sizeof(tx_buf); i++)
        tx_buf[i] = rand();

    tx_crc = au_crc32b(tx_buf, sizeof(tx_buf));

    /* send data and close socket */
    size_t tx_pos = 0;

    while (tx_pos < sizeof(tx_buf))
    {
        const ssize_t ret = send(wfd, (char *)&tx_buf[tx_pos], SEND_SIZE, 0);
        ck_assert(ret == SEND_SIZE);

        tx_pos += SEND_SIZE;
    }

    asc_pipe_close(wfd);
}

static void pipe_on_read(void *arg)
{
    const int rfd = *((int *)arg);

    while (true)
    {
        const ssize_t left = sizeof(rx_buf) - rx_pos;
        ck_assert(left >= 0);

        const ssize_t ret = recv(rfd, (char *)&rx_buf[rx_pos], left, 0);
        switch (ret)
        {
            case -1:
                if (asc_socket_would_block())
                    return;

                ck_abort_msg("recv(): %s", asc_error_msg());
                break;

            case 0:
                /* closed on far side */
                ck_assert(rx_pos == sizeof(rx_buf));

                rx_crc = au_crc32b(rx_buf, sizeof(rx_buf));
                ck_assert(rx_crc == tx_crc);

                asc_main_loop_shutdown();
                return;

            default:
                ck_assert(ret > 0);
                rx_pos += ret;
        }
    }
}

START_TEST(pipe_event)
{
    int fds[2] = { -1, -1 };

    const int ret = asc_pipe_open(fds, NULL, PIPE_RD);
    ck_assert(ret == 0 && fds[PIPE_RD] != -1 && fds[PIPE_WR] != -1);

    /* verify our end is non-blocking */
    pipe_check_nb(fds[PIPE_RD], true);
    pipe_check_nb(fds[PIPE_WR], false);

    /* check socket buffers */
    pipe_check_buf(fds[0]);
    pipe_check_buf(fds[1]);

    /* set up event handler */
    asc_event_t *const ev = asc_event_init(fds[PIPE_RD], &fds[PIPE_RD]);
    asc_event_set_on_read(ev, pipe_on_read);

    /* start writing thread */
    asc_thread_t *const thr = asc_thread_init();
    asc_thread_start(thr, &fds[PIPE_WR], thread_proc, NULL);

    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    asc_event_close(ev);
    asc_pipe_close(fds[PIPE_RD]);
}
END_TEST

/* test process spawning and stdio redirection */
START_TEST(process_spawn)
{
    for (unsigned int j = 0; j < 16; j++)
    {
        int sin = -1, sout = -1, serr = -1;
        asc_process_t proc;

        const char *cmd;
        if (j % 2)
            cmd = TEST_SLAVE " cat 1"; /* stdout */
        else
            cmd = TEST_SLAVE " cat 2"; /* stderr */

        /* start slave process */
        memset(&proc, 0, sizeof(proc));
        int ret = asc_process_spawn(cmd, &proc, &sin, &sout, &serr);
        ck_assert_msg(ret == 0, "couldn't spawn child: %s", asc_error_msg());

        pid_t pid = asc_process_id(&proc);
        ck_assert(pid > 0 && sin != -1 && sout != -1 && serr != -1);

        const int echofd = (j % 2) ? sout : serr;

        /* make sure it hasn't quit */
        ret = asc_process_wait(&proc, NULL, false);
        ck_assert(ret == 0);

        /* test echo */
        for (unsigned int k = 0; k < 16; k++)
        {
            /* write to stdin */
            uint8_t data[BUF_SIZE];
            const size_t data_size = 1 + (rand() % (BUF_SIZE - 1));
            for (size_t i = 0; i < data_size; i++)
                data[i] = rand();

            const ssize_t out = send(sin, (char *)data, data_size, 0);
            ck_assert(out == (ssize_t)data_size);

            /* expect echo on stdout/stderr */
            uint8_t buf[BUF_SIZE];
            size_t rpos = 0;
            fd_set rs;

            while (rpos < data_size)
            {
                FD_ZERO(&rs);
                FD_SET((unsigned)echofd, &rs);

                ret = select(echofd + 1, &rs, NULL, NULL, NULL);
                ck_assert(ret == 1 && FD_ISSET((unsigned)echofd, &rs));

                const size_t left = sizeof(buf) - rpos;
                const ssize_t in = recv(echofd, (char *)&buf[rpos], left, 0);
                ck_assert(in > 0 && (size_t)in <= left);

                rpos += in;
            }

            /* verify data integrity */
            const uint32_t crc1 = au_crc32b(data, data_size);
            const uint32_t crc2 = au_crc32b(buf, rpos);
            ck_assert(crc1 == crc2);
        }

        /* signal slave to quit by closing its stdio pipes */
        ck_assert(asc_pipe_close(sin) == 0);
        ck_assert(asc_pipe_close(sout) == 0);
        ck_assert(asc_pipe_close(serr) == 0);

        /* wait until it quits and clean up */
        int rc = -1;
        const pid_t waited = asc_process_wait(&proc, &rc, true);
#ifndef _WIN32
        ck_assert(WIFEXITED(rc) && !WIFSIGNALED(rc));
        rc = WEXITSTATUS(rc);
#endif /* !_WIN32 */
        ck_assert(waited == pid);
        ck_assert(rc == 0);

        asc_process_free(&proc);
    }
}
END_TEST

/* close our end of child's stdout, wait until it catches SIGPIPE */
START_TEST(process_near_close)
{
    int ret, sin = -1, serr = -1, sout = -1;
    asc_process_t proc;

    /* start slave */
    memset(&proc, 0, sizeof(proc));
    ret = asc_process_spawn(TEST_SLAVE " ticker", &proc, &sin, &sout, &serr);
    ck_assert(ret == 0 && sin != -1 && sout != -1 && serr != -1);

    const pid_t pid = asc_process_id(&proc);
    ck_assert(pid > 0);

    /* close its stdout after first read */
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET((unsigned)sout, &rs);

    ret = select(sout + 1, &rs, NULL, NULL, NULL);
    ck_assert(ret == 1 && FD_ISSET((unsigned)sout, &rs));

    while (true)
    {
        char buf[512];
        ret = recv(sout, buf, sizeof(buf), 0);
        if (ret == -1 && asc_socket_would_block())
            break;

        ck_assert(ret > 0);
    }

    ck_assert(asc_pipe_close(sout) == 0);

    /* check exit status */
    int rc = -1;
    const pid_t waited = asc_process_wait(&proc, &rc, true);
    ck_assert(waited == pid);
#ifdef _WIN32
    ck_assert(rc == EXIT_FAILURE);
#else
    ck_assert(WIFSIGNALED(rc) && WTERMSIG(rc) == SIGPIPE);
#endif

    /* clean up */
    ck_assert(asc_pipe_close(sin) == 0);
    ck_assert(asc_pipe_close(serr) == 0);
    asc_process_free(&proc);
}
END_TEST

/* wait until child closes stdio */
START_TEST(process_far_close)
{
    int ret, sin = -1, serr = -1, sout = -1;
    asc_process_t proc;

    /* start slave */
    memset(&proc, 0, sizeof(proc));
    ret = asc_process_spawn(TEST_SLAVE " close", &proc, &sin, &sout, &serr);
    ck_assert(ret == 0 && sin != -1 && sout != -1 && serr != -1);

    const pid_t pid = asc_process_id(&proc);
    ck_assert(pid > 0);

    /* wait for close event on each pipe */
    int fds[] = { sin, sout, serr };
    for (size_t i = 0; i < ASC_ARRAY_SIZE(fds); i++)
    {
        const int fd = fds[i];

        fd_set rs;
        FD_ZERO(&rs);
        FD_SET((unsigned)fd, &rs);

        ret = select(fd + 1, &rs, NULL, NULL, NULL);
        ck_assert(ret == 1 && FD_ISSET((unsigned)fd, &rs));

        char buf[32];
        ret = recv(fd, buf, sizeof(buf), 0);
        ck_assert(ret <= 0);
        ck_assert(asc_pipe_close(fd) == 0);
    }

    /* kill and clean up */
    int rc = -1;
    ck_assert(asc_process_kill(&proc, false) == 0);
    ck_assert(asc_process_wait(&proc, &rc, true) == pid);
#ifdef _WIN32
    ck_assert(rc == (int)STATUS_CONTROL_C_EXIT);
#else /* _WIN32 */
    ck_assert(WIFSIGNALED(rc) && WTERMSIG(rc) == SIGTERM);
#endif /* !_WIN32 */
    asc_process_free(&proc);
}
END_TEST

/* test pid getter */
START_TEST(process_id)
{
    int ret, sin = -1, serr = -1, sout = -1;
    asc_process_t proc;

    /* start slave */
    memset(&proc, 0, sizeof(proc));
    ret = asc_process_spawn(TEST_SLAVE " pid", &proc, &sin, &sout, &serr);
    ck_assert(ret == 0 && sin != -1 && sout != -1 && serr != -1);

    const pid_t pid = asc_process_id(&proc);
    ck_assert(pid > 0);

    /* read pid from child */
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET((unsigned)sout, &rs);

    ret = select(sout + 1, &rs, NULL, NULL, NULL);
    ck_assert(ret == 1 && FD_ISSET((unsigned)sout, &rs));

    char buf[512];
    size_t rpos = 0;

    while (true)
    {
        const size_t left = sizeof(buf) - rpos;
        const ssize_t in = recv(sout, (char *)&buf[rpos], left, 0);
        if (in <= 0)
        {
            buf[rpos] = '\0';
            break;
        }

        ck_assert(in > 0 && (size_t)in <= left);
        rpos += in;
    }

    const pid_t expect = strtoll(buf, NULL, 10);
    ck_assert(expect == pid);

    /* clean up */
    int rc = -1;
    ck_assert(asc_pipe_close(sin) == 0);
    ck_assert(asc_pipe_close(sout) == 0);
    ck_assert(asc_pipe_close(serr) == 0);
    ck_assert(asc_process_kill(&proc, false) == 0);
    ck_assert(asc_process_wait(&proc, &rc, true) == pid);
#ifdef _WIN32
    ck_assert(rc == (int)STATUS_CONTROL_C_EXIT);
#else /* _WIN32 */
    ck_assert(WIFSIGNALED(rc) && WTERMSIG(rc) == SIGTERM);
#endif /* !_WIN32 */
    asc_process_free(&proc);
}
END_TEST

/* terminate a misbehaving process */
START_TEST(process_kill)
{
    int ret, sin = -1, sout = -1, serr = -1;
    asc_process_t proc;

    /* start slave */
    ret = asc_process_spawn(TEST_SLAVE " bandit", &proc, &sin, &sout, &serr);
    ck_assert(ret == 0 && sin != -1 && sout != -1 && serr != -1);

    const pid_t pid = asc_process_id(&proc);
    ck_assert(pid > 0);

    /* make sure termination signals are received */
    char buf[64] = { 0 };
    unsigned int reqs = 0;
    size_t rpos = 0;

    while (true)
    {
        fd_set rs;
        FD_ZERO(&rs);
        FD_SET((unsigned)serr, &rs);

        ret = select(serr + 1, &rs, NULL, NULL, NULL);
        ck_assert(ret == 1 && FD_ISSET((unsigned)serr, &rs));

        const size_t left = sizeof(buf) - rpos;
        const ssize_t in = recv(serr, (char *)&buf[rpos], left, 0);
        ck_assert(in > 0 && (size_t)in < left);
        rpos += in;

        if (!strcmp(buf, "peep\n"))
        {
            if (reqs++ >= 32)
                break;

            memset(buf, 0, sizeof(buf));
            rpos = 0;

            ret = asc_process_kill(&proc, false);
            ck_assert(ret == 0);
        }
    }

    /* clean up */
    int rc = -1;
    ck_assert(asc_process_kill(&proc, true) == 0);
    ck_assert(asc_process_wait(&proc, &rc, true) == pid);
#ifdef _WIN32
    ck_assert(rc == EXIT_FAILURE); /* set by asc_process_kill() on Win32 */
#else /* _WIN32 */
    ck_assert(WIFSIGNALED(rc) && WTERMSIG(rc) == SIGKILL);
#endif /* !_WIN32 */
    ck_assert(asc_pipe_close(sin) == 0);
    ck_assert(asc_pipe_close(sout) == 0);
    ck_assert(asc_pipe_close(serr) == 0);
    asc_process_free(&proc);
}
END_TEST

/* try to kill a zombie process */
START_TEST(process_zombie)
{
    int ret, sin = -1, sout = -1, serr = -1;
    asc_process_t proc;

    ret = asc_process_spawn(TEST_SLAVE " exit 0", &proc, &sin, &sout, &serr);
    ck_assert(ret == 0 && sin != -1 && sout != -1 && serr != -1);

    ck_assert(asc_pipe_close(sin) == 0);
    ck_assert(asc_pipe_close(sout) == 0);
    ck_assert(asc_pipe_close(serr) == 0);

    for (unsigned int i = 0; i < 16; i++)
    {
        asc_usleep(10 * 1000); /* 10ms */

        const bool force = (i % 2) ? true : false;
        ck_assert(asc_process_kill(&proc, force) == 0);
    }

    int rc = -1;
    ck_assert(asc_process_wait(&proc, &rc, false) > 0);
#ifndef _WIN32
    ck_assert(WIFEXITED(rc) && !WIFSIGNALED(rc));
    rc = WEXITSTATUS(rc);
#endif /* !_WIN32 */
    ck_assert(rc == 0);
    asc_process_free(&proc);
}
END_TEST

/* retrieve child's exit code */
START_TEST(process_exit)
{
    for (unsigned int i = 32; i < 128; i++)
    {
        int ret, sin = -1, sout = -1, serr = -1;
        asc_process_t proc;

        char cmd[128];
        ret = snprintf(cmd, sizeof(cmd), "%s exit %u", TEST_SLAVE, i);
        ck_assert(ret > 0);

        memset(&proc, 0, sizeof(proc));
        ret = asc_process_spawn(cmd, &proc, &sin, &sout, &serr);
        ck_assert(ret == 0 && sin != -1 && sout != -1 && serr != -1);

        const pid_t pid = asc_process_id(&proc);
        ck_assert(pid > 0);

        int rc = -1;
        ret = asc_process_wait(&proc, &rc, true);
        ck_assert(ret == pid);
#ifndef _WIN32
        ck_assert(WIFEXITED(rc) && !WIFSIGNALED(rc));
        rc = WEXITSTATUS(rc);
#endif
        ck_assert(rc == (int)i);

        ck_assert(asc_pipe_close(sin) == 0);
        ck_assert(asc_pipe_close(sout) == 0);
        ck_assert(asc_pipe_close(serr) == 0);
        asc_process_free(&proc);
    }
}
END_TEST

/* test fd inheritance */
START_TEST(process_inherit)
{
    /* open pipe with inheritance enabled on far end */
    int tether[2] = { -1, -1 };

    int ret = asc_pipe_open(tether, NULL, 0); /* our end is non-blocking */
    ck_assert(ret == 0 && tether[0] != -1 && tether[1] != -1);

    pipe_check_nb(tether[0], true);  /* our end */
    pipe_check_nb(tether[1], false); /* far end */

    ck_assert(pipe_get_inherit(tether[0]) == false);
    ck_assert(pipe_get_inherit(tether[1]) == false);

    ck_assert(asc_pipe_inherit(tether[1], true) == 0);
    ck_assert(pipe_get_inherit(tether[1]) == true);

    /* start slave */
    int sin = -1, sout = -1, serr = -1;
    char cmd[64] = { 0 };
    asc_process_t proc;

    ret = snprintf(cmd, sizeof(cmd), "%s pipefd %d", TEST_SLAVE, tether[1]);
    ck_assert(ret > 0);

    ret = asc_process_spawn(cmd, &proc, &sin, &sout, &serr);
    ck_assert(ret == 0);

    ck_assert(asc_pipe_close(tether[1]) == 0); /* don't need this anymore */

    /* test echo on inherited handle */
    for (uint8_t i = 0; i < 255; i++)
    {
        ret = send(tether[0], (char *)&i, sizeof(i), 0);
        ck_assert(ret == sizeof(i));

        fd_set rs;
        FD_ZERO(&rs);
        FD_SET((unsigned)tether[0], &rs);

        ret = select(tether[0] + 1, &rs, NULL, NULL, NULL);
        ck_assert(ret == 1 && FD_ISSET((unsigned)tether[0], &rs));

        uint8_t buf[16];
        ret = recv(tether[0], (char *)buf, sizeof(buf), 0);
        ck_assert(ret == sizeof(i));
        ck_assert(buf[0] == i);
    }

    /* close pipe and wait until it quits */
    int rc = -1;
    ck_assert(asc_pipe_close(tether[0]) == 0);
    ck_assert(asc_process_wait(&proc, &rc, true) > 0);
#ifndef _WIN32
    ck_assert(WIFEXITED(rc) && !WIFSIGNALED(rc));
    rc = WEXITSTATUS(rc);
#endif
    ck_assert(rc == 0);

    /* clean up */
    ck_assert(asc_pipe_close(sin) == 0);
    ck_assert(asc_pipe_close(sout) == 0);
    ck_assert(asc_pipe_close(serr) == 0);
    asc_process_free(&proc);
}
END_TEST

Suite *core_spawn(void)
{
    Suite *const s = suite_create("core/spawn");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);

#ifndef _WIN32
    if (can_fork != CK_NOFORK)
        tcase_set_timeout(tc, 10);
#endif /* !_WIN32 */

    tcase_add_test(tc, pipe_open);
    tcase_add_test(tc, pipe_inherit);
    tcase_add_test(tc, pipe_write);
    tcase_add_test(tc, pipe_event);

    tcase_add_test(tc, process_spawn);
    tcase_add_test(tc, process_near_close);
    tcase_add_test(tc, process_far_close);
    tcase_add_test(tc, process_id);
    tcase_add_test(tc, process_kill);
    tcase_add_test(tc, process_zombie);
    tcase_add_test(tc, process_exit);
    tcase_add_test(tc, process_inherit);

    suite_add_tcase(s, tc);

    return s;
}
