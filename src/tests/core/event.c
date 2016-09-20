/*
 * Astra Unit Tests
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

#include "../libastra.h"
#include <astra/core/event.h>
#include <astra/core/spawn.h>
#include <astra/core/timer.h>
#include <astra/core/mainloop.h>

#ifndef _WIN32
#   include <arpa/inet.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#   include <netdb.h>
#   include <sys/ioctl.h>
#endif

typedef union
{
    struct sockaddr_in in;
    struct sockaddr addr;
} test_sa_t;

static int sock_erropt(int fd)
{
    int err = 0;
    socklen_t optlen = sizeof(err);

    ck_assert(getsockopt(fd, SOL_SOCKET, SO_ERROR
                         , (char *)&err, &optlen) == 0);

    return err;
}

static int sock_err(void)
{
#ifdef _WIN32
    return GetLastError();
#else
    return errno;
#endif
}

static bool sock_blocked(int err)
{
#ifdef _WIN32
    return (err == WSAEWOULDBLOCK);
#else
    return (err == EAGAIN || err == EWOULDBLOCK);
#endif
}

static void sock_nonblock(int fd)
{
#ifdef _WIN32
    unsigned long nonblock = 1;
    ck_assert(ioctlsocket(fd, FIONBIO, &nonblock) == 0);
#else /* _WIN32 */
    const int flags = fcntl(fd, F_GETFL);
    ck_assert(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
#endif /* !_WIN32 */
}

static int sock_open(int type, unsigned short *port)
{
    int fd = socket(AF_INET, type, 0);
    ck_assert(fd != -1);
    sock_nonblock(fd);

    /* bind to loopback */
    test_sa_t sa;

    memset(&sa, 0, sizeof(sa));
    sa.in.sin_family = AF_INET;
    sa.in.sin_port = 0;
    sa.in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    socklen_t addrlen = sizeof(sa);
    ck_assert(bind(fd, &sa.addr, addrlen) == 0);

    /* get port */
    addrlen = sizeof(sa);
    ck_assert(getsockname(fd, &sa.addr, &addrlen) == 0);
    ck_assert(sa.in.sin_port > 0);

    if (port != NULL)
        *port = sa.in.sin_port;

    return fd;
}

static int sock_connect(int fd, unsigned short port)
{
    test_sa_t sa;

    memset(&sa, 0, sizeof(sa));
    sa.in.sin_family = AF_INET;
    sa.in.sin_port = port;
    sa.in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    const int ret = connect(fd, &sa.addr, sizeof(sa));
    if (ret == 0)
        return 0;
    else
        return sock_err();
}

static void sock_close(int s)
{
#ifdef _WIN32
    ck_assert(closesocket(s) == 0);
#else
    ck_assert(close(s) == 0);
#endif
}

static void sock_shutdown(int s)
{
    int ret;
#ifdef _WIN32
    ret = shutdown(s, SD_SEND);
#else
    ret = shutdown(s, SHUT_WR);
#endif
    ck_assert(ret == 0);
}

static void on_fail_event(void *arg)
{
    __uarg(arg);
    ck_abort_msg("didn't expect to reach this code");
}

/* switch between reading and writing */
typedef struct
{
    int fd;
    asc_event_t *ev;

    int peer_fd;
    asc_event_t *peer_ev;

    unsigned int cnt;
    size_t rx;
    size_t tx;
} pp_test_t;

static asc_event_t *pp_ev_a = NULL;
static asc_event_t *pp_ev_b = NULL;
static asc_timer_t *pp_timer = NULL;
static char pp_buf[1024] = { 0 };

static void pp_on_write(void *arg);

static void pp_on_read(void *arg)
{
    pp_test_t *const t = (pp_test_t *)arg;

    while (true)
    {
        const ssize_t ret = recv(t->fd, pp_buf, sizeof(pp_buf), 0);
        if (ret <= 0)
        {
            if (sock_blocked(sock_err()))
            {
                asc_event_set_on_read(t->ev, NULL);
                asc_event_set_on_write(t->ev, pp_on_write);
                t->cnt++;

                break;
            }

            ck_abort_msg("recv(): %s", asc_error_msg());
        }

        t->rx += ret;
    }
}

static void pp_on_write(void *arg)
{
    pp_test_t *const t = (pp_test_t *)arg;

    while (true)
    {
        const ssize_t ret = send(t->fd, pp_buf, sizeof(pp_buf), 0);
        if (ret <= 0)
        {
            if (sock_blocked(sock_err()))
            {
                asc_event_set_on_read(t->peer_ev, pp_on_read);
                asc_event_set_on_write(t->ev, NULL);
                t->cnt++;

                break;
            }

            ck_abort_msg("send(): %s", asc_error_msg());
        }

        t->tx += ret;
    }
}

static void pp_on_timer(void *arg)
{
    __uarg(arg);

    pp_timer = NULL;
    asc_main_loop_shutdown();
}

START_TEST(push_pull)
{
    int fds[2] = { -1, -1 };

    pp_test_t *const tests = ASC_ALLOC(2, pp_test_t);
    ck_assert(tests != NULL);

    const int ret = asc_pipe_open(fds, NULL, PIPE_BOTH);
    ck_assert(ret == 0 && fds[0] != -1 && fds[1] != -1);

    pp_ev_a = asc_event_init(fds[0], &tests[0]);
    pp_ev_b = asc_event_init(fds[1], &tests[1]);
    ck_assert(pp_ev_a != NULL && pp_ev_b != NULL);

    pp_timer = asc_timer_one_shot(1000, pp_on_timer, NULL); /* 1s */
    ck_assert(pp_timer != NULL);

    tests[0].fd = fds[0];
    tests[0].ev = pp_ev_a;
    tests[0].peer_fd = fds[1];
    tests[0].peer_ev = pp_ev_b;

    tests[1].fd = fds[1];
    tests[1].ev = pp_ev_b;
    tests[1].peer_fd = fds[0];
    tests[1].peer_ev = pp_ev_a;

    asc_event_set_on_error(pp_ev_a, on_fail_event);
    asc_event_set_on_error(pp_ev_b, on_fail_event);

    asc_event_set_on_write(pp_ev_a, pp_on_write);

    ck_assert(asc_main_loop_run() == false);
    for (unsigned int i = 0; i < 2; i++)
    {
        ck_assert(tests[i].cnt > 0);
        ck_assert(tests[i].rx > 0);
        ck_assert(tests[i].tx > 0);

        asc_log_info("event push-pull test %u: cnt=%u rx=%zu tx=%zu"
                     , i, tests[i].cnt, tests[i].rx, tests[i].tx);
    }

    ASC_FREE(pp_ev_a, asc_event_close);
    ASC_FREE(pp_ev_b, asc_event_close);

    ck_assert(asc_pipe_close(fds[0]) == 0);
    ck_assert(asc_pipe_close(fds[1]) == 0);
    ck_assert(pp_timer == NULL);

    free(tests);
}
END_TEST

/* test TCP shutdown notification */
enum
{
    TC_SERVER_CLOSE_GRACEFUL = 0,
    TC_CLIENT_CLOSE_ABORTIVE = 1,
    TC_CLIENT_CLOSE_GRACEFUL = 2,
    TC_SERVER_CLOSE_ABORTIVE = 3,
    TC_CASE_COUNT,
};

static int tc_case = -1;

static int tc_ear_fd = -1;
static asc_event_t *tc_ear_ev = NULL;

static int tc_clnt_fd = -1;
static asc_event_t *tc_clnt_ev = NULL;

static int tc_svr_fd = -1;
static asc_event_t *tc_svr_ev = NULL;

#define TC_SOCK_KILL(_fd, _event, _soft) \
    do { \
        if (_soft) \
            sock_shutdown(_fd); \
        ASC_FREE(_event, asc_event_close); \
        sock_close(_fd); \
        _fd = -1; \
    } while (0)

#define TC_GRACEFUL true
#define TC_ABORTIVE false
#define TC_CLEANUP false

#define TC_ITERATIONS 512

static void tc_send(int fd)
{
    static const char buf[32] = { 0 };
    ck_assert(send(fd, buf, sizeof(buf), 0) > 0);
}

static ssize_t tc_recv(int fd)
{
    char buf[128];

    while (true)
    {
        const ssize_t ret = recv(fd, buf, sizeof(buf), 0);
        if (ret == 0)
            return 0;
        else if (ret == -1)
            return sock_err();
        else if (ret != 32)
            ck_abort();
    }
}

static void tc_svr_on_read(void *arg);

static void tc_svr_on_write(void *arg);

static void tc_ear_on_accept(void *arg)
{
    __uarg(arg);

    struct sockaddr sa;
    socklen_t sl = sizeof(sa);
    tc_svr_fd = accept(tc_ear_fd, &sa, &sl);
    ck_assert(tc_svr_fd >= 0);
    sock_nonblock(tc_svr_fd);

    tc_svr_ev = asc_event_init(tc_svr_fd, NULL);
    asc_event_set_on_error(tc_svr_ev, on_fail_event);

    switch (tc_case)
    {
        case TC_SERVER_CLOSE_GRACEFUL:
            asc_event_set_on_read(tc_svr_ev, on_fail_event);
            asc_event_set_on_write(tc_svr_ev, tc_svr_on_write);
            break;

        case TC_CLIENT_CLOSE_ABORTIVE:
            asc_event_set_on_read(tc_svr_ev, tc_svr_on_read);
            break;

        case TC_CLIENT_CLOSE_GRACEFUL:
            asc_event_set_on_read(tc_svr_ev, on_fail_event);
            asc_event_set_on_write(tc_svr_ev, tc_svr_on_write);
            break;

        case TC_SERVER_CLOSE_ABORTIVE:
            asc_event_set_on_read(tc_svr_ev, tc_svr_on_read);
            break;

        default:
            ck_abort();
    }
}

static void tc_clnt_on_read(void *arg);

static void tc_clnt_on_write(void *arg);

static void tc_clnt_on_connect(void *arg)
{
    __uarg(arg);

    ck_assert(sock_erropt(tc_clnt_fd) == 0);

    switch (tc_case)
    {
        case TC_SERVER_CLOSE_GRACEFUL:
            asc_event_set_on_read(tc_clnt_ev, tc_clnt_on_read);
            break;

        case TC_CLIENT_CLOSE_ABORTIVE:
            asc_event_set_on_read(tc_clnt_ev, on_fail_event);
            asc_event_set_on_write(tc_clnt_ev, tc_clnt_on_write);
            break;

        case TC_CLIENT_CLOSE_GRACEFUL:
            asc_event_set_on_read(tc_clnt_ev, tc_clnt_on_read);
            break;

        case TC_SERVER_CLOSE_ABORTIVE:
            asc_event_set_on_read(tc_clnt_ev, on_fail_event);
            asc_event_set_on_write(tc_clnt_ev, tc_clnt_on_write);
            break;

        default:
            ck_abort();
    }
}

static void tc_clnt_on_read(void *arg)
{
    __uarg(arg);
    ssize_t ret;

    switch (tc_case)
    {
        case TC_SERVER_CLOSE_GRACEFUL:
            /* expect graceful shutdown by server */
            ret = tc_recv(tc_clnt_fd);
            if (sock_blocked(ret))
                return;

            ck_assert(ret == 0);

            /* clean up client side socket */
            TC_SOCK_KILL(tc_clnt_fd, tc_clnt_ev, TC_CLEANUP);
            asc_main_loop_shutdown();
            break;

        case TC_CLIENT_CLOSE_GRACEFUL:
            /* drain socket buffers */
            ck_assert(sock_blocked(tc_recv(tc_clnt_fd)));

            /* initiate shutdown as soon as fd is writable */
            asc_event_set_on_write(tc_clnt_ev, tc_clnt_on_write);
            asc_event_set_on_read(tc_clnt_ev, on_fail_event);
            break;

        case TC_SERVER_CLOSE_ABORTIVE:
            /* expect abortive shutdown by server */
            ret = tc_recv(tc_clnt_fd);
            if (sock_blocked(ret))
                return;

            ck_assert(ret == 0);

            /* clean up client side socket */
            TC_SOCK_KILL(tc_clnt_fd, tc_clnt_ev, TC_CLEANUP);
            asc_main_loop_shutdown();
            break;

        default:
            ck_abort();
    }
}

static void tc_clnt_on_write(void *arg)
{
    __uarg(arg);

    switch (tc_case)
    {
        case TC_CLIENT_CLOSE_ABORTIVE:
            tc_send(tc_clnt_fd);

            /* close client side socket */
            TC_SOCK_KILL(tc_clnt_fd, tc_clnt_ev, TC_ABORTIVE);
            break;

        case TC_CLIENT_CLOSE_GRACEFUL:
            /* close client side socket */
            TC_SOCK_KILL(tc_clnt_fd, tc_clnt_ev, TC_GRACEFUL);
            break;

        case TC_SERVER_CLOSE_ABORTIVE:
            tc_send(tc_clnt_fd);

            asc_event_set_on_read(tc_clnt_ev, tc_clnt_on_read);
            asc_event_set_on_write(tc_clnt_ev, NULL);
            break;

        default:
            ck_abort();
    }
}

static void tc_svr_on_read(void *arg)
{
    __uarg(arg);
    ssize_t ret;

    switch (tc_case)
    {
        case TC_CLIENT_CLOSE_ABORTIVE:
            /* expect abortive shutdown by client */
            ret = tc_recv(tc_svr_fd);
            if (sock_blocked(ret))
                return;

            ck_assert(ret == 0);

            /* clean up server side socket */
            TC_SOCK_KILL(tc_svr_fd, tc_svr_ev, TC_CLEANUP);
            asc_main_loop_shutdown();
            break;

        case TC_CLIENT_CLOSE_GRACEFUL:
            /* expect graceful shutdown by client */
            ret = tc_recv(tc_svr_fd);
            if (sock_blocked(ret))
                return;

            ck_assert(ret == 0);

            /* clean up server side socket */
            TC_SOCK_KILL(tc_svr_fd, tc_svr_ev, TC_CLEANUP);
            asc_main_loop_shutdown();
            break;

        case TC_SERVER_CLOSE_ABORTIVE:
            /* drain socket buffers */
            ck_assert(sock_blocked(tc_recv(tc_svr_fd)));

            /* close server side socket */
            TC_SOCK_KILL(tc_svr_fd, tc_svr_ev, TC_ABORTIVE);
            break;

        default:
            ck_abort();
    }
}

static void tc_svr_on_write(void *arg)
{
    __uarg(arg);

    switch (tc_case)
    {
        case TC_SERVER_CLOSE_GRACEFUL:
            tc_send(tc_svr_fd);

            /* close server side socket */
            TC_SOCK_KILL(tc_svr_fd, tc_svr_ev, TC_GRACEFUL);
            break;

        case TC_CLIENT_CLOSE_GRACEFUL:
            tc_send(tc_svr_fd);

            /* wait until client closes connection */
            asc_event_set_on_read(tc_svr_ev, tc_svr_on_read);
            asc_event_set_on_write(tc_svr_ev, NULL);
            break;

        default:
            ck_abort();
    }
}

START_TEST(tcp_connect)
{
    for (unsigned int i = 0; i < TC_ITERATIONS; i++)
    {
        tc_case = i % TC_CASE_COUNT;

        /* open up listening socket */
        short unsigned listen_port = 0;
        tc_ear_fd = sock_open(SOCK_STREAM, &listen_port);
        ck_assert(listen(tc_ear_fd, SOMAXCONN) == 0);

        tc_ear_ev = asc_event_init(tc_ear_fd, NULL);
        asc_event_set_on_read(tc_ear_ev, tc_ear_on_accept);
        asc_event_set_on_write(tc_ear_ev, on_fail_event);
        asc_event_set_on_error(tc_ear_ev, on_fail_event);

        /* initiate client connection */
        tc_clnt_fd = sock_open(SOCK_STREAM, NULL);
        const int ret = sock_connect(tc_clnt_fd, listen_port);
#ifdef _WIN32
        ck_assert(ret == 0 || ret == WSAEINPROGRESS || ret == WSAEWOULDBLOCK);
#else
        ck_assert(ret == 0 || ret == EINPROGRESS);
#endif

        tc_clnt_ev = asc_event_init(tc_clnt_fd, NULL);
        asc_event_set_on_write(tc_clnt_ev, tc_clnt_on_connect);
        asc_event_set_on_error(tc_clnt_ev, on_fail_event);

        /* run test case */
        ck_assert(asc_main_loop_run() == false);

        /* clean up listener */
        TC_SOCK_KILL(tc_ear_fd, tc_ear_ev, TC_CLEANUP);

        ck_assert(tc_ear_fd == -1 && tc_ear_ev == NULL);
        ck_assert(tc_clnt_fd == -1 && tc_clnt_ev == NULL);
        ck_assert(tc_svr_fd == -1 && tc_svr_ev == NULL);
    }
}
END_TEST

/* attempt TCP connection to a closed port */
static asc_event_t *tr_ev = NULL;
static int tr_fd = -1;
static int tr_err = -1;

static void tr_on_connect(void *arg)
{
    __uarg(arg);

    if (tr_err == -1)
        tr_err = sock_erropt(tr_fd);

#ifdef _WIN32
    ck_assert(tr_err == WSAECONNREFUSED);
#else
    ck_assert(tr_err == ECONNREFUSED);
#endif

    ASC_FREE(tr_ev, asc_event_close);
    sock_close(tr_fd);
    tr_fd = tr_err = -1;
    asc_main_loop_shutdown();
}

static void tr_on_read(void *arg)
{
    __uarg(arg);

    /*
     * Many OS/event backend combinations trigger on_read on connection
     * failure. "Real" code should not handle read events on connect()'ing
     * sockets; this here is for testing purposes only.
     */
    char buf[32];
    const ssize_t ret = recv(tr_fd, buf, sizeof(buf), 0);

    if (tr_err == -1)
    {
        /* in case SO_ERROR is cleared on recv() */
        tr_err = sock_err();
        ck_assert(ret == -1);
#ifdef _WIN32
        ck_assert(tr_err == WSAECONNREFUSED);
#else
        ck_assert(tr_err == ECONNREFUSED);
#endif
    }

    asc_log_warning("connect error triggered on_read!");
    /* don't clean up, we still want to see on_error or on_write */
}

static void tr_on_write(void *arg)
{
    __uarg(arg);

    tr_on_connect(NULL);
    asc_log_info("connect error triggered on_write");
}

static void tr_on_error(void *arg)
{
    __uarg(arg);

    tr_on_connect(NULL);
    asc_log_info("connect error triggered on_error");
}

START_TEST(tcp_refused)
{
    int i = 1;
//#ifndef __FreeBSD__
    i++;
//#endif

    while (i--)
    {
        unsigned short port = 0;
        const int spoiler = sock_open(SOCK_STREAM, &port);
        tr_fd = sock_open(SOCK_STREAM, NULL);
        sock_close(spoiler);

        tr_ev = asc_event_init(tr_fd, NULL);

        /* handle read event on first iteration */
        if (i == 1)
            asc_event_set_on_read(tr_ev, tr_on_read);

        asc_event_set_on_write(tr_ev, tr_on_write);
        asc_event_set_on_error(tr_ev, tr_on_error);

        int ret = sock_connect(tr_fd, port);
#ifdef _WIN32
        ck_assert(ret == WSAEINPROGRESS || ret == WSAEWOULDBLOCK);
#else
        ck_assert(ret == EINPROGRESS || ret == ECONNREFUSED);
#endif

#ifdef _WIN32
        if (ret == WSAECONNREFUSED)
#else
        if (ret == ECONNREFUSED)
#endif
        {
            /* completed right away, but we still want on_error event */
            asc_log_warning("connect() completed right away");
            tr_err = ret;
        }

        ck_assert(asc_main_loop_run() == false);
        ck_assert(tr_fd == -1 && tr_err == -1 && tr_ev == NULL);
    }
}
END_TEST

#ifndef HAVE_EVENT_KQUEUE
/* send out-of-band data */
typedef struct
{
    asc_event_t *ev;
    int fd;
    unsigned int rx;
    unsigned int tx;
} oob_test_t;

#define OOB_MAX_BYTES 1024
#define OOB_DATA 0x10

static void oob_pipe(int fds[2])
{
    /* open sockets */
    unsigned short port = 0;
    const int listener = sock_open(SOCK_STREAM, &port);
    ck_assert(listen(listener, SOMAXCONN) == 0);

    const int client = sock_open(SOCK_STREAM, NULL);
    const int ret = sock_connect(client, port);
#ifdef _WIN32
    ck_assert(ret == 0 || ret == WSAEINPROGRESS || ret == WSAEWOULDBLOCK);
#else
    ck_assert(ret == 0 || ret == EINPROGRESS);
#endif

    /* wait for connection */
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(listener, &rset);
    ck_assert(select(listener + 1, &rset, NULL, NULL, NULL) == 1);
    ck_assert(FD_ISSET(listener, &rset));

    /* get server side socket */
    struct sockaddr sa;
    socklen_t sl = sizeof(sa);
    int server = accept(listener, &sa, &sl);
    ck_assert(server >= 0);
    sock_nonblock(server);

    /* disable Nagle algorithm */
    int one = 1;
    ck_assert(setsockopt(server, IPPROTO_TCP, TCP_NODELAY
                         , (char *)&one, sizeof(one)) == 0);

    one = 1;
    ck_assert(setsockopt(client, IPPROTO_TCP, TCP_NODELAY
                         , (char *)&one, sizeof(one)) == 0);

    fds[0] = server;
    fds[1] = client;

    sock_close(listener);
}

static void oob_on_write(void *arg)
{
    oob_test_t *const t = (oob_test_t *)arg;
    ck_assert(sock_erropt(t->fd) == 0);

    char data = OOB_DATA;
    const ssize_t ret = send(t->fd, &data, sizeof(data), MSG_OOB);
    ck_assert(ret == 1);
    t->tx++;
}

static void oob_on_error(void *arg)
{
    oob_test_t *const t = (oob_test_t *)arg;
    ck_assert(sock_erropt(t->fd) == 0);

    while (true)
    {
        char buf[32];
        ssize_t ret = recv(t->fd, buf, sizeof(buf), MSG_OOB);
        if (ret == -1)
        {
            if (sock_blocked(sock_err()))
                break; /* EAGAIN */

#ifdef _WIN32
            if (sock_err() == WSAEINVAL)
#else
            if (sock_err() == EINVAL)
#endif
                break; /* no more OOB bytes */
        }

        ck_assert(ret > 0);
        while (ret > 0)
        {
            ck_assert(buf[--ret] == OOB_DATA);
            t->rx++;
        }
    }

    if (t->rx >= OOB_MAX_BYTES)
    {
        asc_event_set_on_error(t->ev, NULL);
        asc_main_loop_shutdown();
    }
}

START_TEST(tcp_oob)
{
    asc_event_t *oob_ev_a = NULL;
    asc_event_t *oob_ev_b = NULL;

    int fds[2] = { -1, -1 };
    oob_pipe(fds);

    oob_test_t *tests = ASC_ALLOC(2, oob_test_t);
    ck_assert(tests != NULL);

    oob_ev_a = asc_event_init(fds[0], &tests[0]);
    oob_ev_b = asc_event_init(fds[1], &tests[1]);
    ck_assert(oob_ev_a != NULL && oob_ev_b != NULL);

    tests[0].fd = fds[0];
    tests[0].ev = oob_ev_a;

    tests[1].fd = fds[1];
    tests[1].ev = oob_ev_b;

    /* expect OOB to be delivered to on_error */
    asc_event_set_on_read(oob_ev_a, on_fail_event);
    asc_event_set_on_write(oob_ev_a, oob_on_write);
    asc_event_set_on_error(oob_ev_a, oob_on_error);

    asc_event_set_on_read(oob_ev_b, on_fail_event);
    asc_event_set_on_write(oob_ev_b, oob_on_write);
    asc_event_set_on_error(oob_ev_b, oob_on_error);

    ck_assert(asc_main_loop_run() == false);

    asc_log_info("OOB test: RX:%u/TX:%u, RX:%u/TX:%u"
                 , tests[0].rx, tests[0].tx, tests[1].rx, tests[1].tx);

    ck_assert(tests[0].rx > 0 && tests[0].tx > 0);
    ck_assert(tests[1].rx > 0 && tests[1].tx > 0);

    ASC_FREE(oob_ev_a, asc_event_close);
    ASC_FREE(oob_ev_b, asc_event_close);

    sock_close(fds[0]);
    sock_close(fds[1]);

    ASC_FREE(tests, free);
}
END_TEST
#endif /* !HAVE_EVENT_KQUEUE */

/* send UDP packets to and from localhost */
START_TEST(udp_sockets)
{
    // create udp sock 1
    // get port
    // create udp sock 2
    // "connect" to port
    // ??? set buffers?
    // set events
    //   on_write - send 1k
    //   on_read - read all data
    //             if (rx >= 65536)
    //                 left--;
    //             if (left == 0) asc_shutdown
}
END_TEST

/* lots of short lived TCP connections */
START_TEST(series_of_tubes)
{
}
END_TEST

/* on_error handler that doesn't close the event */
static void nce_on_error(void *arg)
{
    __uarg(arg);
}

START_TEST(no_close_on_error)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    ck_assert(sock != -1);

    /* should abort on library cleanup */
    asc_event_t *ev = asc_event_init(sock, NULL);
    asc_event_set_on_error(ev, nce_on_error);
}
END_TEST

Suite *core_event(void)
{
    Suite *const s = suite_create("core/event");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);

    tcase_add_test(tc, push_pull);
    tcase_add_test(tc, tcp_connect);
    tcase_add_test(tc, tcp_refused);
#ifndef HAVE_EVENT_KQUEUE
    tcase_add_test(tc, tcp_oob);
#endif
    tcase_add_test(tc, udp_sockets);
    tcase_add_test(tc, series_of_tubes);

    if (can_fork != CK_NOFORK)
    {
        tcase_add_exit_test(tc, no_close_on_error, EXIT_ABORT);
    }

    suite_add_tcase(s, tc);

    return s;
}
