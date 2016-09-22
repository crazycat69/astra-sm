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
#include <astra/core/list.h>

/* socket mini-API to avoid touching core/socket */
#ifndef _WIN32
#   include <arpa/inet.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#   include <netdb.h>
#   include <sys/ioctl.h>
#   include <sys/resource.h>
#endif

#define SOCK_MIN_BUF 16384

typedef union
{
    struct sockaddr addr;
    struct sockaddr_storage stor;
    struct sockaddr_in in;
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

static void sock_nodelay(int fd)
{
    /* disable Nagle algorithm */
    int one = 1;
    ck_assert(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY
                         , (char *)&one, sizeof(one)) == 0);
}

static void sock_buf(int fd)
{
    int opts[] = { SO_RCVBUF, SO_SNDBUF };

    for (unsigned i = 0; i < ASC_ARRAY_SIZE(opts); i++)
    {
        const int opt = opts[i];

        int val;
        socklen_t len = sizeof(val);
        ck_assert_msg(getsockopt(fd, SOL_SOCKET, opt, (char *)&val, &len) == 0
                      , "getsockopt(): %s", asc_error_msg());

        if (val < SOCK_MIN_BUF)
        {
            val = SOCK_MIN_BUF;
            len = sizeof(val);

            ck_assert_msg(setsockopt(fd, SOL_SOCKET, opt
                                     , (char *)&val, len) == 0
                          , "setsockopt(): %s", asc_error_msg());
            ck_assert_msg(getsockopt(fd, SOL_SOCKET, opt
                                     , (char *)&val, &len) == 0
                          , "getsockopt(): %s", asc_error_msg());
            ck_assert(val >= SOCK_MIN_BUF);
        }
    }
}

static void sock_nonblock(int fd)
{
#ifdef _WIN32
    unsigned long nonblock = 1;
    ck_assert(ioctlsocket(fd, FIONBIO, &nonblock) == 0);
#else /* _WIN32 */
    const int flags = fcntl(fd, F_GETFL);
    ck_assert(flags >= 0);

    ck_assert(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
#endif /* !_WIN32 */
}

static int sock_open(int type, unsigned short *port)
{
    const int fd = socket(AF_INET, type, 0);
    ck_assert(fd >= 0);
    sock_nonblock(fd);
    sock_buf(fd);

    /* bind to loopback */
    test_sa_t sa;

    memset(&sa, 0, sizeof(sa));
    sa.in.sin_family = AF_INET;
    sa.in.sin_port = 0;
    sa.in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    socklen_t addrlen = sizeof(sa.in);
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

    const int ret = connect(fd, &sa.addr, sizeof(sa.in));
    return (ret == 0 ? 0 : sock_err());
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
} pp_pipe_t;

static asc_event_t *pp_ev_a = NULL;
static asc_event_t *pp_ev_b = NULL;
static asc_timer_t *pp_timer = NULL;

static char pp_buf[1024] = { 0 };

static void pp_on_write(void *arg);

static void pp_on_read(void *arg)
{
    pp_pipe_t *const t = (pp_pipe_t *)arg;

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
    pp_pipe_t *const t = (pp_pipe_t *)arg;

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
    pp_timer = asc_timer_one_shot(1000, pp_on_timer, NULL); /* 1s */
    ck_assert(pp_timer != NULL);

    int fds[2] = { -1, -1 };
    const int ret = asc_pipe_open(fds, NULL, PIPE_BOTH);
    ck_assert(ret == 0 && fds[0] != -1 && fds[1] != -1);

    pp_pipe_t *pipes = ASC_ALLOC(2, pp_pipe_t);
    ck_assert(pipes != NULL);

    pp_ev_a = asc_event_init(fds[0], &pipes[0]);
    pp_ev_b = asc_event_init(fds[1], &pipes[1]);
    ck_assert(pp_ev_a != NULL && pp_ev_b != NULL);

    pipes[0].fd = fds[0];
    pipes[0].ev = pp_ev_a;
    pipes[0].peer_fd = fds[1];
    pipes[0].peer_ev = pp_ev_b;

    pipes[1].fd = fds[1];
    pipes[1].ev = pp_ev_b;
    pipes[1].peer_fd = fds[0];
    pipes[1].peer_ev = pp_ev_a;

    asc_event_set_on_error(pp_ev_a, on_fail_event);
    asc_event_set_on_error(pp_ev_b, on_fail_event);

    asc_event_set_on_write(pp_ev_a, pp_on_write);

    ck_assert(asc_main_loop_run() == false);
    for (unsigned int i = 0; i < 2; i++)
    {
        ck_assert(pipes[i].cnt > 0);
        ck_assert(pipes[i].rx > 0);
        ck_assert(pipes[i].tx > 0);

        asc_log_info("event push-pull test %u: cnt=%u rx=%zu tx=%zu"
                     , i, pipes[i].cnt, pipes[i].rx, pipes[i].tx);
    }

    ASC_FREE(pp_ev_a, asc_event_close);
    ASC_FREE(pp_ev_b, asc_event_close);
    ASC_FREE(pipes, free);

    ck_assert(asc_pipe_close(fds[0]) == 0);
    ck_assert(asc_pipe_close(fds[1]) == 0);

    ck_assert(pp_timer == NULL);
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

static unsigned int tc_got[TC_CASE_COUNT] = { 0 };
static unsigned int tc_want[TC_CASE_COUNT] = { 0 };
static int tc_case = -1;

static int tc_ear_fd = -1; /* listener */
static asc_event_t *tc_ear_ev = NULL;

static int tc_clnt_fd = -1; /* client */
static asc_event_t *tc_clnt_ev = NULL;

static int tc_svr_fd = -1; /* server */
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
#define TC_CLEANUP  false

#define TC_BUFSIZE 32
#define TC_ITERATIONS 512

static void tc_send(int fd)
{
    static const char buf[TC_BUFSIZE] = { 0 };
    ck_assert(send(fd, buf, sizeof(buf), 0) == sizeof(buf));
}

static ssize_t tc_recv(int fd)
{
    char buf[TC_BUFSIZE * 4];

    while (true)
    {
        const ssize_t ret = recv(fd, buf, sizeof(buf), 0);
        if (ret == 0)
            return 0;
        else if (ret == -1)
            return sock_err();
        else if (ret != TC_BUFSIZE)
            ck_abort_msg("got %zd bytes!", ret);
    }
}

static void tc_svr_on_read(void *arg);

static void tc_svr_on_write(void *arg);

static void tc_ear_on_accept(void *arg)
{
    __uarg(arg);

    /* accept client */
    test_sa_t sa;
    socklen_t sl = sizeof(sa.in);
    tc_svr_fd = accept(tc_ear_fd, &sa.addr, &sl);
    ck_assert_msg(tc_svr_fd >= 0, "accept() failed");
    sock_nonblock(tc_svr_fd);
    sock_nodelay(tc_svr_fd);
    sock_buf(tc_svr_fd);

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

    /* clean up listener */
    TC_SOCK_KILL(tc_ear_fd, tc_ear_ev, TC_CLEANUP);
}

static void tc_clnt_on_read(void *arg);

static void tc_clnt_on_write(void *arg);

static void tc_clnt_on_connect(void *arg)
{
    __uarg(arg);

    ck_assert_msg(sock_erropt(tc_clnt_fd) == 0, "SO_ERROR is not 0");

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

            ck_assert_msg(ret == 0, "expected zero read!");

            /* clean up client side socket */
            tc_got[TC_SERVER_CLOSE_GRACEFUL]++;
            TC_SOCK_KILL(tc_clnt_fd, tc_clnt_ev, TC_CLEANUP);
            asc_main_loop_shutdown();
            break;

        case TC_CLIENT_CLOSE_GRACEFUL:
            /* drain socket buffers */
            ck_assert_msg(sock_blocked(tc_recv(tc_clnt_fd))
                          , "expected EWOULDBLOCK");

            /* initiate shutdown as soon as fd is writable */
            asc_event_set_on_write(tc_clnt_ev, tc_clnt_on_write);
            asc_event_set_on_read(tc_clnt_ev, on_fail_event);
            break;

        case TC_SERVER_CLOSE_ABORTIVE:
            /* expect abortive shutdown by server */
            ret = tc_recv(tc_clnt_fd);
            if (sock_blocked(ret))
                return;

            ck_assert_msg(ret == 0, "expected zero read!");

            /* clean up client side socket */
            tc_got[TC_SERVER_CLOSE_ABORTIVE]++;
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

            ck_assert_msg(ret == 0, "expected zero read!");

            /* clean up server side socket */
            tc_got[TC_CLIENT_CLOSE_ABORTIVE]++;
            TC_SOCK_KILL(tc_svr_fd, tc_svr_ev, TC_CLEANUP);
            asc_main_loop_shutdown();
            break;

        case TC_CLIENT_CLOSE_GRACEFUL:
            /* expect graceful shutdown by client */
            ret = tc_recv(tc_svr_fd);
            if (sock_blocked(ret))
                return;

            ck_assert_msg(ret == 0, "expected zero read!");

            /* clean up server side socket */
            tc_got[TC_CLIENT_CLOSE_GRACEFUL]++;
            TC_SOCK_KILL(tc_svr_fd, tc_svr_ev, TC_CLEANUP);
            asc_main_loop_shutdown();
            break;

        case TC_SERVER_CLOSE_ABORTIVE:
            /* drain socket buffers */
            ck_assert_msg(sock_blocked(tc_recv(tc_svr_fd))
                          , "expected EWOULDBLOCK");

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
        tc_want[tc_case]++;

        /* open up listening socket */
        short unsigned listen_port = 0;
        tc_ear_fd = sock_open(SOCK_STREAM, &listen_port);
        ck_assert_msg(listen(tc_ear_fd, SOMAXCONN) == 0
                      , "couldn't set up a listening socket");

        tc_ear_ev = asc_event_init(tc_ear_fd, NULL);
        asc_event_set_on_read(tc_ear_ev, tc_ear_on_accept);
        asc_event_set_on_write(tc_ear_ev, on_fail_event);
        asc_event_set_on_error(tc_ear_ev, on_fail_event);

        /* initiate client connection */
        tc_clnt_fd = sock_open(SOCK_STREAM, NULL);
        sock_nodelay(tc_clnt_fd);

        const int ret = sock_connect(tc_clnt_fd, listen_port);
#ifdef _WIN32
        if (ret != 0 && ret != WSAEINPROGRESS && ret != WSAEWOULDBLOCK)
#else
        if (ret != 0 && ret != EINPROGRESS)
#endif
            ck_abort_msg("couldn't initiate TCP connection");

        tc_clnt_ev = asc_event_init(tc_clnt_fd, NULL);
        asc_event_set_on_write(tc_clnt_ev, tc_clnt_on_connect);
        asc_event_set_on_error(tc_clnt_ev, on_fail_event);

        /* run test case */
        ck_assert(asc_main_loop_run() == false);

        ck_assert(tc_ear_fd == -1 && tc_ear_ev == NULL);
        ck_assert(tc_clnt_fd == -1 && tc_clnt_ev == NULL);
        ck_assert(tc_svr_fd == -1 && tc_svr_ev == NULL);
    }

    for (unsigned int i = 0; i < TC_CASE_COUNT; i++)
        ck_assert(tc_got[i] > 0 && tc_got[i] == tc_want[i]);
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
    for (unsigned int i = 0; i < 2; i++)
    {
        /*
         * Get a port number that is known to be closed. There's a minor
         * race condition in here, but who cares.
         */
        unsigned short closed_port = 0;
        const int spoiler = sock_open(SOCK_STREAM, &closed_port);
        tr_fd = sock_open(SOCK_STREAM, NULL);
        sock_close(spoiler);
        sock_nodelay(tr_fd);

        /* set up event object */
        tr_ev = asc_event_init(tr_fd, NULL);

        if (i == 0)
        {
            /* handle read event on first iteration */
            asc_event_set_on_read(tr_ev, tr_on_read);
        }

        asc_event_set_on_write(tr_ev, tr_on_write);
        asc_event_set_on_error(tr_ev, tr_on_error);

        /* initiate non-blocking connection */
        const int ret = sock_connect(tr_fd, closed_port);
#ifndef _WIN32
        if (ret == ECONNREFUSED)
        {
            /* completed right away, but we still want on_error event */
            asc_log_warning("connect() completed right away");
            tr_err = ret;
        }
        else
        {
            ck_assert(ret == EINPROGRESS);
        }
#else /* !_WIN32 */
        ck_assert(ret == WSAEINPROGRESS || ret == WSAEWOULDBLOCK);
#endif /* _WIN32 */

        /* expect on_error handler to clean up the socket */
        ck_assert(asc_main_loop_run() == false);
        ck_assert(tr_fd == -1 && tr_err == -1 && tr_ev == NULL);
    }
}
END_TEST

/* send out-of-band data */
typedef struct
{
    asc_event_t *ev;
    int fd;
    unsigned int rx;
    unsigned int tx;
} oob_sock_t;

#define OOB_MAX_BYTES 1024
#define OOB_DATA 0x10

static void oob_pipe(int fds[2])
{
    /* open sockets */
    unsigned short port = 0;
    const int listener = sock_open(SOCK_STREAM, &port);
    ck_assert(listen(listener, SOMAXCONN) == 0);

    const int client = sock_open(SOCK_STREAM, NULL);
    sock_nodelay(client);

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
    test_sa_t sa;
    socklen_t sl = sizeof(sa.in);
    int server = accept(listener, &sa.addr, &sl);
    ck_assert(server >= 0);
    sock_nonblock(server);
    sock_nodelay(server);
    sock_buf(server);

    fds[0] = server;
    fds[1] = client;

    sock_close(listener);
}

static void oob_on_write(void *arg)
{
    oob_sock_t *const t = (oob_sock_t *)arg;
    ck_assert(sock_erropt(t->fd) == 0);

    char data = OOB_DATA;
    const ssize_t ret = send(t->fd, &data, sizeof(data), MSG_OOB);
    ck_assert(ret == 1);

    t->tx++;
#ifdef WITH_EVENT_KQUEUE
    if (t->tx >= OOB_MAX_BYTES)
    {
        asc_event_set_on_write(t->ev, NULL);
        asc_main_loop_shutdown();
    }
#endif /* WITH_EVENT_KQUEUE */
}

#ifndef WITH_EVENT_KQUEUE
static void oob_on_error(void *arg)
{
    oob_sock_t *const t = (oob_sock_t *)arg;
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
#else /* !WITH_EVENT_KQUEUE */
static void oob_on_error(void *arg)
{
    __uarg(arg);

    /*
     * NOTE: most kqueue implementations don't support polling for
     *       OOB data, so let's make sure it's ignored.
     */
    ck_abort_msg("expected kqueue to ignore TCP out-of-band data!");
}
#endif /* WITH_EVENT_KQUEUE */

START_TEST(tcp_oob)
{
    int fds[2] = { -1, -1 };
    oob_pipe(fds);

    oob_sock_t *socks = ASC_ALLOC(2, oob_sock_t);
    ck_assert(socks != NULL);

    asc_event_t *oob_ev_a = asc_event_init(fds[0], &socks[0]);
    asc_event_t *oob_ev_b = asc_event_init(fds[1], &socks[1]);
    ck_assert(oob_ev_a != NULL && oob_ev_b != NULL);

    socks[0].fd = fds[0];
    socks[0].ev = oob_ev_a;

    socks[1].fd = fds[1];
    socks[1].ev = oob_ev_b;

    /* expect OOB to be delivered to on_error */
    asc_event_set_on_read(oob_ev_a, on_fail_event);
    asc_event_set_on_write(oob_ev_a, oob_on_write);
    asc_event_set_on_error(oob_ev_a, oob_on_error);

    asc_event_set_on_read(oob_ev_b, on_fail_event);
    asc_event_set_on_write(oob_ev_b, oob_on_write);
    asc_event_set_on_error(oob_ev_b, oob_on_error);

    ck_assert(asc_main_loop_run() == false);
    asc_log_info("OOB test: RX:%u/TX:%u, RX:%u/TX:%u"
                 , socks[0].rx, socks[0].tx
                 , socks[1].rx, socks[1].tx);

    ck_assert(socks[0].tx > 0 && socks[1].tx > 0);
#ifndef WITH_EVENT_KQUEUE
    ck_assert(socks[0].rx > 0 && socks[1].rx > 0);
#else
    ck_assert(socks[0].rx == 0 && socks[1].rx == 0);
#endif

    ASC_FREE(oob_ev_a, asc_event_close);
    ASC_FREE(oob_ev_b, asc_event_close);

    ASC_FREE(socks, free);

    sock_close(fds[0]);
    sock_close(fds[1]);
}
END_TEST

/* send UDP packets to and from localhost */
typedef struct
{
    int fd;
    asc_event_t *ev;
    size_t rx;
    size_t tx;
} udp_sock_t;

static char udp_buf[1024];

#define UDP_MAX_PKTS 1024
#define UDP_MAX_BYTES (UDP_MAX_PKTS * sizeof(udp_buf))

static void udp_on_write(void *arg)
{
    udp_sock_t *const s = (udp_sock_t *)arg;

    const ssize_t ret = send(s->fd, udp_buf, sizeof(udp_buf), 0);
    ck_assert(ret == sizeof(udp_buf));
    s->tx += ret;

    asc_event_set_on_write(s->ev, NULL);
}

static void udp_on_read(void *arg)
{
    udp_sock_t *const s = (udp_sock_t *)arg;

    while (true)
    {
        const ssize_t ret = recv(s->fd, udp_buf, sizeof(udp_buf), 0);
        if (ret == -1 && sock_blocked(sock_err()))
            break;

        ck_assert(ret > 0);
        s->rx += ret;
    }

    if (s->rx >= UDP_MAX_BYTES && s->tx >= UDP_MAX_BYTES)
    {
        asc_event_set_on_read(s->ev, NULL);
        asc_main_loop_shutdown();
    }
    else if (s->tx < UDP_MAX_BYTES)
    {
        asc_event_set_on_write(s->ev, udp_on_write);
    }
}

START_TEST(udp_sockets)
{
    /* create UDP sockets */
    unsigned short port_a = 0;
    const int sock_a = sock_open(SOCK_DGRAM, &port_a);

    unsigned short port_b = 0;
    const int sock_b = sock_open(SOCK_DGRAM, &port_b);

    /* set default destination for send() */
    sock_connect(sock_a, port_b);
    sock_connect(sock_b, port_a);

    /* set up event objects */
    udp_sock_t *socks = ASC_ALLOC(2, udp_sock_t);
    asc_event_t *ev_a = asc_event_init(sock_a, &socks[0]);
    asc_event_t *ev_b = asc_event_init(sock_b, &socks[1]);

    socks[0].fd = sock_a;
    socks[0].ev = ev_a;
    socks[1].fd = sock_b;
    socks[1].ev = ev_b;

    asc_event_set_on_write(ev_a, udp_on_write);
    asc_event_set_on_read(ev_a, udp_on_read);
    asc_event_set_on_error(ev_a, on_fail_event);

    asc_event_set_on_write(ev_b, udp_on_write);
    asc_event_set_on_read(ev_b, udp_on_read);
    asc_event_set_on_error(ev_b, on_fail_event);

    /* run test */
    ck_assert(asc_main_loop_run() == false);
    asc_log_info("UDP test stats: RX:%zu/TX:%zu, RX:%zu/TX:%zu"
                 , socks[0].rx, socks[0].tx, socks[1].rx, socks[1].tx);

    ck_assert(socks[0].rx >= UDP_MAX_BYTES && socks[0].tx >= UDP_MAX_BYTES);
    ck_assert(socks[1].rx >= UDP_MAX_BYTES && socks[1].tx >= UDP_MAX_BYTES);
    ck_assert(socks[0].rx == socks[1].tx && socks[1].rx == socks[0].tx);

    ASC_FREE(ev_a, asc_event_close);
    ASC_FREE(ev_b, asc_event_close);
    ASC_FREE(socks, free);

    sock_close(sock_a);
    sock_close(sock_b);
}
END_TEST

/* lots of short lived TCP connections */
typedef struct sot_server_t sot_server_t;

typedef struct
{
    int fd;
    test_sa_t sa;
    asc_event_t *ev;
    sot_server_t *svr;
    size_t rx;
    size_t tx;
} sot_sock_t;

struct sot_server_t
{
    /* listener */
    sot_sock_t sk;

    /* total number of requests created and served */
    size_t nr_reqs;
    size_t nr_completed;

    /* server side sockets */
    asc_list_t *clients;

    /* client side sockets */
    asc_list_t *requests;
};

#define SOT_SERVERS 8 /* how many listening sockets to create */
#define SOT_DEFAULT_CLIENTS 32 /* max. clients per server socket */
#define SOT_RUN_TIME 3000 /* shut down main loop after 3 seconds */
#define SOT_BYTES 16384 /* how much data to send in each direction */

static char sot_buf[1024];

static sot_server_t sot_servers[SOT_SERVERS];
static unsigned int sot_max_clients = SOT_DEFAULT_CLIENTS;

static asc_timer_t *sot_kill = NULL;
static bool sot_shutdown = false;

static bool sot_check(void);

static void sot_on_error(void *arg)
{
    sot_sock_t *const sk = (sot_sock_t *)arg;

    char buf[1024] = { 0 };
    const int err = sock_erropt(sk->fd);
    asc_strerror(err, buf, sizeof(buf));

    ck_abort_msg("error on fd %d: %d, %s", sk->fd, err, buf);
}

static void sot_svr_on_read(void *arg);

static void sot_svr_on_write(void *arg);

static void sot_svr_on_accept(void *arg)
{
    sot_server_t *const svr = (sot_server_t *)arg; /* listener */

    while (true)
    {
        /* accept connection */
        test_sa_t sa;
        socklen_t sl = sizeof(sa.in);

        const int fd = accept(svr->sk.fd, &sa.addr, &sl);
        if (fd == -1 && sock_blocked(sock_err()))
            break;

        ck_assert_msg(fd >= 0, "accept() failed: %s", asc_error_msg());
        sock_nonblock(fd);
        sock_nodelay(fd);
        sock_buf(fd);

        /* add to client list */
        sot_sock_t *const client = ASC_ALLOC(1, sot_sock_t);
        client->svr = svr;
        client->fd = fd;
        memcpy(&client->sa, &sa, sizeof(sa));

        client->ev = asc_event_init(client->fd, client);
        asc_event_set_on_read(client->ev, sot_svr_on_read);
        asc_event_set_on_error(client->ev, sot_on_error);

        asc_list_insert_tail(svr->clients, client);
    }
}

static void sot_svr_on_read(void *arg)
{
    sot_sock_t *const sk = (sot_sock_t *)arg; /* server side */

    while (true)
    {
        const ssize_t ret = recv(sk->fd, sot_buf, sizeof(sot_buf), 0);

        if (ret == 0)
        {
            ck_assert_msg(sk->rx >= SOT_BYTES, "premature hangup");
            ck_assert(sk->tx >= SOT_BYTES);
            sk->svr->nr_completed++;

            asc_event_close(sk->ev);
            asc_list_remove_item(sk->svr->clients, sk);
            sock_close(sk->fd);
            free(sk);

            /* repopulate connection pool */
            if (!sot_check() && sot_shutdown)
                asc_main_loop_shutdown();

            return;
        }
        else
        {
            if (ret == -1 && sock_blocked(sock_err()))
                break;

            ck_assert(ret > 0);
            ck_assert(sk->rx < SOT_BYTES);

            sk->rx += ret;
            if (sk->rx >= SOT_BYTES)
            {
                asc_event_set_on_write(sk->ev, sot_svr_on_write);
                break;
            }
        }
    }
}

static void sot_svr_on_write(void *arg)
{
    sot_sock_t *const sk = (sot_sock_t *)arg; /* server side */

    while (true)
    {
        const ssize_t ret = send(sk->fd, sot_buf, sizeof(sot_buf), 0);
        if (ret == -1 && sock_blocked(sock_err()))
            break;

        ck_assert(ret > 0);
        ck_assert(sk->tx < SOT_BYTES);

        sk->tx += ret;
        if (sk->tx >= SOT_BYTES)
        {
            asc_event_set_on_write(sk->ev, NULL);
            break;
        }
    }
}

static void sot_clnt_on_read(void *arg);

static void sot_clnt_on_write(void *arg);

static void sot_clnt_on_connect(void *arg)
{
    sot_sock_t *const req = (sot_sock_t *)arg; /* client side */

    /* make sure connection was successful */
    int err = sock_erropt(req->fd);
    if (err != 0)
    {
        char buf[1024] = { 0 };
        asc_strerror(err, buf, sizeof(buf));
        ck_abort_msg("%s: fd=%d, %s", __FUNCTION__, req->fd, buf);
    }

    asc_event_set_on_read(req->ev, sot_clnt_on_read);
    asc_event_set_on_write(req->ev, sot_clnt_on_write);
    sot_clnt_on_write(req);
}

static void sot_clnt_on_read(void *arg)
{
    sot_sock_t *const req = (sot_sock_t *)arg; /* client side */

    while (true)
    {
        const ssize_t ret = recv(req->fd, sot_buf, sizeof(sot_buf), 0);
        if (ret == -1 && sock_blocked(sock_err()))
            break;

        ck_assert_msg(ret > 0, "didn't expect the server to hang up");
        ck_assert(req->rx < SOT_BYTES);

        req->rx += ret;
        if (req->rx >= SOT_BYTES)
        {
            ck_assert(req->tx >= SOT_BYTES);

            asc_event_close(req->ev);
            sock_shutdown(req->fd);
            sock_close(req->fd);
            asc_list_remove_item(req->svr->requests, req);
            free(req);

            /* repopulate connection pool */
            if (!sot_check() && sot_shutdown)
                asc_main_loop_shutdown();

            return;
        }
    }
}

static void sot_clnt_on_write(void *arg)
{
    sot_sock_t *const req = (sot_sock_t *)arg; /* client side */

    while (true)
    {
        const ssize_t ret = send(req->fd, sot_buf, sizeof(sot_buf), 0);
        if (ret == -1 && sock_blocked(sock_err()))
            break;

        ck_assert_msg(ret > 0, "%s", asc_error_msg());
        ck_assert(req->tx < SOT_BYTES);

        req->tx += ret;
        if (req->tx >= SOT_BYTES)
        {
            asc_event_set_on_write(req->ev, NULL);
            break;
        }
    }
}

static bool sot_check(void)
{
    bool busy = false;
    for (unsigned int i = 0; i < SOT_SERVERS; i++)
    {
        sot_server_t *const svr = &sot_servers[i];

        size_t reqs = asc_list_size(svr->requests);
        size_t clients = asc_list_size(svr->clients);
        if (sot_shutdown)
        {
            if (reqs > 0 || clients > 0)
                busy = true;

            continue;
        }

        while (reqs++ < sot_max_clients)
        {
            sot_sock_t *const req = ASC_ALLOC(1, sot_sock_t);
            req->svr = svr;

            req->fd = sock_open(SOCK_STREAM, NULL);
            sock_nodelay(req->fd);

            req->ev = asc_event_init(req->fd, req);
            asc_event_set_on_write(req->ev, sot_clnt_on_connect);
            asc_event_set_on_error(req->ev, sot_on_error);

            const int ret = sock_connect(req->fd, svr->sk.sa.in.sin_port);
#ifdef _WIN32
            if (ret != 0 && ret != WSAEINPROGRESS && ret != WSAEWOULDBLOCK)
#else
            if (ret != 0 && ret != EINPROGRESS)
#endif
                ck_abort_msg("couldn't initiate TCP connection");

            asc_list_insert_tail(svr->requests, req);
            svr->nr_reqs++;
        }
    }

    return busy;
}

static void sot_on_kill(void *arg)
{
    __uarg(arg);

    asc_log_info("kill timer!");
    sot_kill = NULL;
    sot_shutdown = true;
}

START_TEST(series_of_tubes)
{
#ifndef _WIN32
    /* adjust open files limit */
    struct rlimit rl;
    ck_assert(getrlimit(RLIMIT_NOFILE, &rl) == 0);
    if (rl.rlim_cur < rl.rlim_max)
    {
        rl.rlim_cur = rl.rlim_max;
        ck_assert(setrlimit(RLIMIT_NOFILE, &rl) == 0);
    }

    asc_log_info("RLIMIT_NOFILE=%lu", (unsigned long)rl.rlim_cur);
#ifdef WITH_EVENT_SELECT
    if (rl.rlim_cur > 1024)
        rl.rlim_cur = 1024;
#endif

    static const int pad = 64;
    const int max = (rl.rlim_cur - SOT_SERVERS - pad) / SOT_SERVERS / 3;
    ck_assert(max > 0);

    sot_max_clients = max;
#endif /* !_WIN32 */
    asc_log_info("%s: servers: %u, max clients per server: %u"
                 , __FUNCTION__, SOT_SERVERS, sot_max_clients);
    asc_log_info("using no more than %u file descriptors"
                 , (sot_max_clients * 2 * SOT_SERVERS) + SOT_SERVERS);

    /* create servers */
    for (unsigned int i = 0; i < SOT_SERVERS; i++)
    {
        sot_server_t *const svr = &sot_servers[i];

        const int fd = sock_open(SOCK_STREAM, NULL);
        ck_assert(listen(fd, SOMAXCONN) == 0);

        svr->sk.fd = fd;
        svr->sk.ev = asc_event_init(fd, svr);
        asc_event_set_on_read(svr->sk.ev, sot_svr_on_accept);
        asc_event_set_on_write(svr->sk.ev, sot_on_error);
        asc_event_set_on_error(svr->sk.ev, sot_on_error);

        socklen_t sl = sizeof(svr->sk.sa.in);
        ck_assert(getsockname(fd, &svr->sk.sa.addr, &sl) == 0);

        svr->clients = asc_list_init();
        svr->requests = asc_list_init();
    }

    /* set up timers and run the test */
    ck_assert(sot_check() == false);
    sot_kill = asc_timer_one_shot(SOT_RUN_TIME, sot_on_kill, NULL);
    sot_shutdown = false;

    ck_assert(asc_main_loop_run() == false);
    ck_assert(sot_kill == NULL);

    /* take servers down */
    size_t total_reqs = 0;
    size_t total_completed = 0;
    for (unsigned int i = 0; i < SOT_SERVERS; i++)
    {
        sot_server_t *const svr = &sot_servers[i];

        total_reqs += svr->nr_reqs;
        total_completed += svr->nr_completed;

        asc_event_close(svr->sk.ev);
        sock_close(svr->sk.fd);
        asc_list_destroy(svr->clients);
        asc_list_destroy(svr->requests);
    }

    asc_log_info("%s: %zu/%zu requests completed"
                 , __FUNCTION__, total_completed, total_reqs);

    ck_assert(total_completed > 0 && total_reqs > 0
              && total_completed == total_reqs);
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
    tcase_add_test(tc, tcp_oob);
    tcase_add_test(tc, udp_sockets);
    tcase_add_test(tc, series_of_tubes);

    if (can_fork != CK_NOFORK)
    {
        tcase_set_timeout(tc, 10);
        tcase_add_exit_test(tc, no_close_on_error, EXIT_ABORT);
    }

    suite_add_tcase(s, tc);

    return s;
}
