/*
 * Astra Core
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
 *                    2015, Artem Kharitonov <artem@sysert.ru>
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
#include <core/socket.h>

#ifdef _WIN32
#   include <ws2tcpip.h>
#   define SHUT_RD SD_RECEIVE
#   define SHUT_WR SD_SEND
#   define SHUT_RDWR SD_BOTH
#else
#   include <sys/socket.h>
#   include <arpa/inet.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#   ifdef HAVE_NETINET_SCTP_H
#       include <netinet/sctp.h>
#   endif
#   include <netdb.h>
#endif

#ifdef IGMP_EMULATION
#   define IP_HEADER_SIZE 24
#   define IGMP_HEADER_SIZE 8
#endif

#define MSG(_msg) "[core/socket %d] " _msg, sock->fd

struct asc_socket_t
{
    int fd;
    int family;
    int type;
    int protocol;

    asc_event_t *event;

    struct sockaddr_in addr;
    struct sockaddr_in sockaddr; /* recvfrom, sendto, set_sockaddr */

    struct ip_mreq mreq;

    /* Callbacks */
    void *arg;
    event_callback_t on_read;      /* data read */
    event_callback_t on_close;     /* error occured (connection closed) */
    event_callback_t on_ready;     /* data send is possible now */
};

/*
 * sending multicast: socket(LOOPBACK) -> set_if() -> sendto() -> close()
 * receiving multicast: socket(REUSEADDR | BIND) -> join() -> read() -> close()
 */

#ifdef _WIN32
void asc_socket_core_init(void)
{
    WSADATA wsaData;

    const int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0)
    {
        char buf[1024];
        asc_log_error("[core/socket] WSAStartup() failed: %s"
                      , asc_strerror(err, buf, sizeof(buf)));

        asc_lib_abort();
    }
}

void asc_socket_core_destroy(void)
{
    WSACleanup();
}
#endif /* _WIN32 */

/*
 *   ooooooo  oooooooooo ooooooooooo oooo   oooo
 * o888   888o 888    888 888    88   8888o  88
 * 888     888 888oooo88  888ooo8     88 888o88
 * 888o   o888 888        888    oo   88   8888
 *   88ooo88  o888o      o888ooo8888 o88o    88
 *
 */

static asc_socket_t *sock_init(int family, int type, int protocol, void *arg)
{
    const int fd = socket(family, type, protocol);
    asc_assert(fd != -1, "[core/socket] failed to open socket: %s"
               , asc_error_msg());

    asc_socket_t *const sock = ASC_ALLOC(1, asc_socket_t);

    sock->fd = fd;
    sock->mreq.imr_multiaddr.s_addr = INADDR_NONE;
    sock->family = family;
    sock->type = type;
    sock->protocol = protocol;
    sock->arg = arg;
    asc_socket_set_nonblock(sock, true);

    return sock;
}

asc_socket_t *asc_socket_open_tcp4(void *arg)
{
    return sock_init(PF_INET, SOCK_STREAM, IPPROTO_TCP, arg);
}

asc_socket_t *asc_socket_open_udp4(void *arg)
{
    return sock_init(PF_INET, SOCK_DGRAM, IPPROTO_UDP, arg);
}

asc_socket_t *asc_socket_open_sctp4(void *arg)
{
#ifdef IPPROTO_SCTP
    return sock_init(PF_INET, SOCK_STREAM, IPPROTO_SCTP, arg);
#else
    asc_log_error("[core/socket] SCTP support unavailable; falling back to TCP");
    return asc_socket_open_tcp4(arg);
#endif /* IPPROTO_SCTP */
}

/*
 *   oooooooo8 ooooo         ooooooo    oooooooo8 ooooooooooo
 * o888     88  888        o888   888o 888         888    88
 * 888          888        888     888  888oooooo  888ooo8
 * 888o     oo  888      o 888o   o888         888 888    oo
 *  888oooo88  o888ooooo88   88ooo88   o88oooo888 o888ooo8888
 *
 */

static inline int sock_close(asc_socket_t *sock)
{
#ifdef _WIN32
    const int ret = closesocket(sock->fd);
#else
    const int ret = close(sock->fd);
#endif /* _WIN32 */

    sock->fd = -1;
    return ret;
}

void asc_socket_shutdown_recv(asc_socket_t *sock)
{
    shutdown(sock->fd, SHUT_RD);
}

void asc_socket_shutdown_send(asc_socket_t *sock)
{
    shutdown(sock->fd, SHUT_WR);
}

void asc_socket_shutdown_both(asc_socket_t *sock)
{
    shutdown(sock->fd, SHUT_RDWR);
}

void asc_socket_close(asc_socket_t *sock)
{
    if(sock == NULL)
        return;

    if(sock->event != NULL)
        asc_event_close(sock->event);

    if(sock->fd != -1)
    {
        asc_socket_shutdown_both(sock);
        if (sock_close(sock) != 0)
        {
            asc_log_error("[core/socket] failed to close socket: %s"
                          , asc_error_msg());
        }
    }

    free(sock);
}

/*
 * ooooooooooo ooooo  oooo ooooooooooo oooo   oooo ooooooooooo
 *  888    88   888    88   888    88   8888o  88  88  888  88
 *  888ooo8      888  88    888ooo8     88 888o88      888
 *  888    oo     88888     888    oo   88   8888      888
 * o888ooo8888     888     o888ooo8888 o88o    88     o888o
 *
 */

static void __asc_socket_on_close(void *arg)
{
    asc_socket_t *sock = (asc_socket_t *)arg;
    if(sock->on_close)
        sock->on_close(sock->arg);
}

static void __asc_socket_on_connect(void *arg)
{
    asc_socket_t *sock = (asc_socket_t *)arg;
    asc_event_set_on_write(sock->event, NULL);
    event_callback_t __on_ready = sock->on_ready;
    sock->on_ready(sock->arg);
    if(__on_ready == sock->on_ready)
        sock->on_ready = NULL;
}

static void __asc_socket_on_accept(void *arg)
{
    asc_socket_t *sock = (asc_socket_t *)arg;
    sock->on_read(sock->arg);
}

static void __asc_socket_on_read(void *arg)
{
    asc_socket_t *sock = (asc_socket_t *)arg;
    if(sock->on_read)
        sock->on_read(sock->arg);
}

static void __asc_socket_on_ready(void *arg)
{
    asc_socket_t *sock = (asc_socket_t *)arg;
    if(sock->on_ready)
        sock->on_ready(sock->arg);
}

static bool __asc_socket_check_event(asc_socket_t *sock)
{
    const bool is_callback = (   sock->on_read != NULL
                              || sock->on_ready != NULL
                              || sock->on_close != NULL);

    if(sock->event == NULL)
    {
        if(is_callback == true)
            sock->event = asc_event_init(sock->fd, sock);
    }
    else
    {
        if(is_callback == false)
        {
            asc_event_close(sock->event);
            sock->event = NULL;
        }
    }

    return (sock->event != NULL);
}

void asc_socket_set_on_read(asc_socket_t *sock, event_callback_t on_read)
{
    if(sock->on_read == on_read)
        return;

    sock->on_read = on_read;

    if(__asc_socket_check_event(sock))
    {
        if(on_read != NULL)
            on_read = __asc_socket_on_read;
        asc_event_set_on_read(sock->event, on_read);
    }
}

void asc_socket_set_on_ready(asc_socket_t *sock, event_callback_t on_ready)
{
    if(sock->on_ready == on_ready)
        return;

    sock->on_ready = on_ready;

    if(__asc_socket_check_event(sock))
    {
        if(on_ready != NULL)
            on_ready = __asc_socket_on_ready;
        asc_event_set_on_write(sock->event, on_ready);
    }
}

void asc_socket_set_on_close(asc_socket_t *sock, event_callback_t on_close)
{
    if(sock->on_close == on_close)
        return;

    sock->on_close = on_close;

    if(__asc_socket_check_event(sock))
    {
        if(on_close != NULL)
            on_close = __asc_socket_on_close;
        asc_event_set_on_error(sock->event, on_close);
    }
}

/*
 * oooooooooo ooooo oooo   oooo ooooooooo
 *  888    888 888   8888o  88   888    88o
 *  888oooo88  888   88 888o88   888    888
 *  888    888 888   88   8888   888    888
 * o888ooo888 o888o o88o    88  o888ooo88
 *
 */

bool asc_socket_bind(asc_socket_t *sock, const char *addr, int port)
{
    memset(&sock->addr, 0, sizeof(sock->addr));
    sock->addr.sin_family = sock->family;
    sock->addr.sin_port = htons(port);
    if(addr) // INADDR_ANY by default
        sock->addr.sin_addr.s_addr = inet_addr(addr);

#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
    sock->addr.sin_len = sizeof(struct sockaddr_in);
#endif

#ifdef SO_REUSEPORT
    if(sock->type == SOCK_DGRAM)
    {
        const int optval = 1;
        socklen_t optlen = sizeof(optval);
        setsockopt(sock->fd, SOL_SOCKET, SO_REUSEPORT, &optval, optlen);
    }
#endif /* SO_REUSEPORT */

    if(bind(sock->fd, (struct sockaddr *)&sock->addr, sizeof(sock->addr)) == -1)
    {
        asc_log_error(MSG("bind() to %s:%d failed: %s"), addr, port, asc_error_msg());
        return false;
    }
    return true;
}

/*
 * ooooo       ooooo  oooooooo8 ooooooooooo ooooooooooo oooo   oooo
 *  888         888  888        88  888  88  888    88   8888o  88
 *  888         888   888oooooo     888      888ooo8     88 888o88
 *  888      o  888          888    888      888    oo   88   8888
 * o888ooooo88 o888o o88oooo888    o888o    o888ooo8888 o88o    88
 *
 */

void asc_socket_listen(  asc_socket_t *sock
                       , event_callback_t on_accept, event_callback_t on_error)
{
    asc_assert(on_accept && on_error, MSG("listen() - on_ok/on_err not specified"));
    if(listen(sock->fd, SOMAXCONN) == -1)
    {
        asc_log_error(MSG("listen() on socket failed: %s"), asc_error_msg());
        sock_close(sock);

        return;
    }

    sock->on_read = on_accept;
    sock->on_ready = NULL;
    sock->on_close = on_error;
    if(sock->event == NULL)
        sock->event = asc_event_init(sock->fd, sock);

    asc_event_set_on_read(sock->event, __asc_socket_on_accept);
    asc_event_set_on_write(sock->event, NULL);
    asc_event_set_on_error(sock->event, __asc_socket_on_close);
}

/*
 *      o       oooooooo8   oooooooo8 ooooooooooo oooooooooo  ooooooooooo
 *     888    o888     88 o888     88  888    88   888    888 88  888  88
 *    8  88   888         888          888ooo8     888oooo88      888
 *   8oooo88  888o     oo 888o     oo  888    oo   888            888
 * o88o  o888o 888oooo88   888oooo88  o888ooo8888 o888o          o888o
 *
 */

#ifndef _WIN32
#ifndef HAVE_ACCEPT4
static inline
int __accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int fd = accept(sockfd, addr, addrlen);
    if (fd == -1)
        return fd;

    /*
     * NOTE: if some other thread does fork-exec while we're
     *       between accept() and fcntl(), the child still inherits
     *       the socket.
     */
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        close(fd);
        fd = -1;
    }

    return fd;
}
#else /* !HAVE_ACCEPT4 */
    /*
     * NOTE: accept4() is Linux-specific, but also seems to be
     *       present on FreeBSD 10.
     */
#   define __accept(__fd, __addr, __addrlen) \
        accept4(__fd, __addr, __addrlen, SOCK_CLOEXEC)
#endif /* !HAVE_ACCEPT4 */
#else /* !_WIN32 */
    /*
     * NOTE: Windows client sockets get their no-inherit setting
     *       from the server socket. Nothing needs to be done, we just
     *       accept() as usual.
     */
#   define __accept(...) accept(__VA_ARGS__)
#endif /* !_WIN32 */

bool asc_socket_accept(asc_socket_t *sock, asc_socket_t **client_ptr
                       , void *arg)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    const int fd = __accept(sock->fd, (struct sockaddr *)&addr, &addrlen);
    if(fd == -1)
    {
        asc_log_error(MSG("accept() failed: %s"), asc_error_msg());
        *client_ptr = NULL;

        return false;
    }

    asc_socket_t *const client = ASC_ALLOC(1, asc_socket_t);

    client->fd = fd;
    client->addr = addr;
    client->arg = arg;
    asc_socket_set_nonblock(client, true);
    *client_ptr = client;

    return true;
}

/*
 *   oooooooo8   ooooooo  oooo   oooo oooo   oooo ooooooooooo  oooooooo8 ooooooooooo
 * o888     88 o888   888o 8888o  88   8888o  88   888    88 o888     88 88  888  88
 * 888         888     888 88 888o88   88 888o88   888ooo8   888             888
 * 888o     oo 888o   o888 88   8888   88   8888   888    oo 888o     oo     888
 *  888oooo88    88ooo88  o88o    88  o88o    88  o888ooo8888 888oooo88     o888o
 *
 */

void asc_socket_connect(  asc_socket_t *sock, const char *addr, int port
                        , event_callback_t on_connect, event_callback_t on_error)
{
    asc_assert(on_connect && on_error, MSG("connect() - on_ok/on_err not specified"));
    memset(&sock->addr, 0, sizeof(sock->addr));
    sock->addr.sin_family = sock->family;
    // sock->addr.sin_addr.s_addr = inet_addr(addr);
    sock->addr.sin_port = htons(port);

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = sock->type;
    hints.ai_family = sock->family;
    const int gai_err = getaddrinfo(addr, NULL, &hints, &res);
    if(gai_err == 0)
    {
        memcpy(&sock->addr.sin_addr
               , &((struct sockaddr_in *)res->ai_addr)->sin_addr
               , sizeof(sock->addr.sin_addr));
        freeaddrinfo(res);
    }
    else
    {
        asc_log_error(MSG("getaddrinfo() failed '%s' [%s])"), addr, gai_strerror(gai_err));
        sock_close(sock);

        return;
    }

    if(connect(sock->fd, (struct sockaddr *)&sock->addr, sizeof(sock->addr)) == -1)
    {
#ifdef _WIN32
        const int sock_err = WSAGetLastError();
#else
        const int sock_err = errno;
#endif /* _WIN32 */

        switch(sock_err)
        {
#ifdef _WIN32
            case WSAEWOULDBLOCK:
            case WSAEINPROGRESS:
                break;
#else
            case EISCONN:
            case EINPROGRESS:
            case EAGAIN:
#if EWOULDBLOCK != EAGAIN
            case EWOULDBLOCK:
#endif
                break;
#endif /* _WIN32 */

            default:
                asc_log_error(MSG("connect(): %s:%d: %s"), addr, port
                              , asc_error_msg());
                sock_close(sock);

                return;
        }
    }

    sock->on_read = NULL;
    sock->on_ready = on_connect;
    sock->on_close = on_error;
    if(sock->event == NULL)
        sock->event = asc_event_init(sock->fd, sock);

    asc_event_set_on_read(sock->event, NULL);
    asc_event_set_on_write(sock->event, __asc_socket_on_connect);
    asc_event_set_on_error(sock->event, __asc_socket_on_close);
}

/*
 * oooooooooo  ooooooooooo  oooooooo8 ooooo  oooo
 *  888    888  888    88 o888     88  888    88
 *  888oooo88   888ooo8   888           888  88
 *  888  88o    888    oo 888o     oo    88888
 * o888o  88o8 o888ooo8888 888oooo88      888
 *
 */

ssize_t asc_socket_recv(asc_socket_t *sock, void *buffer, size_t size)
{
    return recv(sock->fd, (char *)buffer, size, 0);
}

ssize_t asc_socket_recvfrom(asc_socket_t *sock, void *buffer, size_t size)
{
    socklen_t slen = sizeof(struct sockaddr_in);
    return recvfrom(sock->fd, (char *)buffer, size, 0
                    , (struct sockaddr *)&sock->sockaddr, &slen);
}

/*
 *  oooooooo8 ooooooooooo oooo   oooo ooooooooo
 * 888         888    88   8888o  88   888    88o
 *  888oooooo  888ooo8     88 888o88   888    888
 *         888 888    oo   88   8888   888    888
 * o88oooo888 o888ooo8888 o88o    88  o888ooo88
 *
 */

ssize_t asc_socket_send(asc_socket_t *sock, const void *buffer, size_t size)
{
    const ssize_t ret = send(sock->fd, (const char *)buffer, size, 0);
    if(ret == -1)
    {
        if(asc_socket_would_block())
            return 0;
    }
    return ret;
}

ssize_t asc_socket_sendto(asc_socket_t *sock, const void *buffer, size_t size)
{
    const socklen_t slen = sizeof(struct sockaddr_in);
    return sendto(sock->fd, (const char *)buffer, size, 0
                  , (struct sockaddr *)&sock->sockaddr, slen);
}

/*
 * ooooo oooo   oooo ooooooooooo  ooooooo
 *  888   8888o  88   888    88 o888   888o
 *  888   88 888o88   888ooo8   888     888
 *  888   88   8888   888       888o   o888
 * o888o o88o    88  o888o        88ooo88
 *
 */

int asc_socket_fd(asc_socket_t *sock)
{
    return sock->fd;
}

const char *asc_socket_addr(asc_socket_t *sock)
{
    return inet_ntoa(sock->addr.sin_addr);
}

int asc_socket_port(asc_socket_t *sock)
{
    struct sockaddr_in s;
    socklen_t slen = sizeof(s);
    memset(&s, 0, slen);

    if(getsockname(sock->fd, (struct sockaddr *)&s, &slen) != -1)
        return htons(s.sin_port);

    return -1;
}

/*
 *  oooooooo8 ooooooooooo ooooooooooo          oo    oo
 * 888         888    88  88  888  88           88oo88
 *  888oooooo  888ooo8        888     ooooooo o88888888o
 *         888 888    oo      888               oo88oo
 * o88oooo888 o888ooo8888    o888o             o88  88o
 *
 */

void asc_socket_set_nonblock(asc_socket_t *sock, bool is_nonblock)
{
    if(is_nonblock == false && sock->event)
    {
        sock->on_read = NULL;
        sock->on_ready = NULL;
        sock->on_close = NULL;

        asc_event_close(sock->event);
        sock->event = NULL;
    }

#ifdef _WIN32
    unsigned long nonblock = (is_nonblock == true) ? 1 : 0;
    const int ret = ioctlsocket(sock->fd, FIONBIO, &nonblock);
#else
    const int flags = (is_nonblock == true)
                    ? (fcntl(sock->fd, F_GETFL) | O_NONBLOCK)
                    : (fcntl(sock->fd, F_GETFL) & ~O_NONBLOCK);
    const int ret = fcntl(sock->fd, F_SETFL, flags);
#endif

    asc_assert(ret == 0, MSG("failed to set non-blocking mode: %s")
               , asc_error_msg());
}

void asc_socket_set_sockaddr(asc_socket_t *sock, const char *addr, int port)
{
    memset(&sock->sockaddr, 0, sizeof(sock->sockaddr));
    sock->sockaddr.sin_family = sock->family;
    sock->sockaddr.sin_addr.s_addr = (addr) ? inet_addr(addr) : INADDR_ANY;
    sock->sockaddr.sin_port = htons(port);
}

void asc_socket_set_reuseaddr(asc_socket_t *sock, int is_on)
{
    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR
               , (const char *)&is_on, sizeof(is_on));
}

void asc_socket_set_non_delay(asc_socket_t *sock, int is_on)
{
    switch(sock->protocol)
    {
#ifdef IPPROTO_SCTP
        case IPPROTO_SCTP:
#ifdef SCTP_NODELAY
            setsockopt(sock->fd, sock->protocol, SCTP_NODELAY
                       , (const char *)&is_on, sizeof(is_on));
#else
            asc_log_error(MSG("SCTP_NODELAY is not available"));
#endif /* SCTP_NODELAY */
            break;
#endif /* IPPROTO_SCTP */
        case IPPROTO_TCP:
            setsockopt(sock->fd, sock->protocol, TCP_NODELAY
                       , (const char *)&is_on, sizeof(is_on));
            break;
        default:
            break;
    }
}

void asc_socket_set_keep_alive(asc_socket_t *sock, int is_on)
{
    setsockopt(sock->fd, SOL_SOCKET, SO_KEEPALIVE
               , (const char *)&is_on, sizeof(is_on));
}

void asc_socket_set_broadcast(asc_socket_t *sock, int is_on)
{
    setsockopt(sock->fd, SOL_SOCKET, SO_BROADCAST
               , (const char *)&is_on, sizeof(is_on));
}

void asc_socket_set_timeout(asc_socket_t *sock, int rcvmsec, int sndmsec)
{
#ifdef _WIN32
    if(rcvmsec > 0)
    {
        setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO
                   , (const char *)&rcvmsec, sizeof(rcvmsec));
    }

    if(sndmsec > 0)
    {
        setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO
                   , (const char *)&sndmsec, sizeof(sndmsec));
    }
#else /* _WIN32 */
    struct timeval tv;

    if(rcvmsec > 0)
    {
        tv.tv_sec = rcvmsec / 1000;
        tv.tv_usec = rcvmsec % 1000;
        setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    if(sndmsec > 0)
    {
        if(rcvmsec != sndmsec)
        {
            tv.tv_sec = sndmsec / 1000;
            tv.tv_usec = sndmsec % 1000;
        }
        setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
#endif /* _WIN32 */
}

static int sock_set_buffer(int fd, int type, int size)
{
    int val = size;
    socklen_t slen = sizeof(val);
#ifdef __linux__
    val /= 2;
#endif /* __linux__ */

    if (setsockopt(fd, SOL_SOCKET, type, (const char *)&val, slen) != 0)
        return -1;

    val = 0;
    if (getsockopt(fd, SOL_SOCKET, type, (char *)&val, &slen) != 0)
        return -1;

    return val;
}

void asc_socket_set_buffer(asc_socket_t *sock, int rcvbuf, int sndbuf)
{
    int got;

    /* receive buffer */
    if(rcvbuf > 0)
    {
        got = sock_set_buffer(sock->fd, SO_RCVBUF, rcvbuf);
        if(got == -1)
        {
            asc_log_error(MSG("failed to set rcvbuf = `%d': %s")
                          , rcvbuf, asc_error_msg());
        }
        else if(got != rcvbuf)
        {
            asc_log_warning(MSG("requested rcvbuf = `%d', got `%d' instead")
                            , rcvbuf, got);
        }
    }

    /* send buffer */
    if(sndbuf > 0)
    {
        got = sock_set_buffer(sock->fd, SO_SNDBUF, sndbuf);
        if(got == -1)
        {
            asc_log_error(MSG("failed to set sndbuf = `%d': %s")
                          , sndbuf, asc_error_msg());
        }
        else if(got != sndbuf)
        {
            asc_log_warning(MSG("requested sndbuf = `%d', got `%d' instead")
                            , sndbuf, got);
        }
    }
}

/*
 * oooo     oooo       oooooooo8     o       oooooooo8 ooooooooooo
 *  8888o   888      o888     88    888     888        88  888  88
 *  88 888o8 88      888           8  88     888oooooo     888
 *  88  888  88  ooo 888o     oo  8oooo88           888    888
 * o88o  8  o88o 888  888oooo88 o88o  o888o o88oooo888    o888o
 *
 */

#ifdef IGMP_EMULATION
static uint16_t in_chksum(uint8_t *buffer, int size)
{
    uint16_t *_buffer = (uint16_t *)buffer;
    register int nleft = size;
    register int sum = 0;
    uint16_t answer = 0;

    while(nleft > 1)
    {
        sum += *_buffer;
        ++_buffer;
        nleft -= 2;
    }

    if(nleft == 1)
    {
        *(uint8_t *)(&answer) = *(uint8_t *)_buffer;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return htons(answer);
}

static void create_igmp_packet(uint8_t *buffer, uint8_t igmp_type, uint32_t dst_addr)
{
    // IP Header
    buffer[0] = (4 << 4) | (6); // Version | IHL
    buffer[1] = 0xC0; // TOS
    // Total length
    const uint16_t total_length = IP_HEADER_SIZE + IGMP_HEADER_SIZE;
    buffer[2] = (total_length >> 8) & 0xFF;
    buffer[3] = (total_length) & 0xFF;
    // ID: buffer[4], buffer[5]
    // Fragmen offset: buffer[6], buffer[7]
    buffer[6] = 0x40; // DF flag
    buffer[8] = 1; // TTL
    buffer[9] = IPPROTO_IGMP; // Protocol
    // Checksum
    const uint16_t ip_checksum = in_chksum(buffer, IP_HEADER_SIZE);
    buffer[10] = (ip_checksum >> 8) & 0xFF;
    buffer[11] = (ip_checksum) & 0xFF;
    // Source address
    const uint32_t _src_addr = htonl(INADDR_ANY);
    buffer[12] = (_src_addr >> 24) & 0xFF;
    buffer[13] = (_src_addr >> 16) & 0xFF;
    buffer[14] = (_src_addr >> 8) & 0xFF;
    buffer[15] = (_src_addr) & 0xFF;
    // Destination address
    const uint32_t _dst_addr = htonl(dst_addr);
    buffer[16] = (_dst_addr >> 24) & 0xFF;
    buffer[17] = (_dst_addr >> 16) & 0xFF;
    buffer[18] = (_dst_addr >> 8) & 0xFF;
    buffer[19] = (_dst_addr) & 0xFF;
    // Options
    buffer[20] = 0x94;
    buffer[21] = 0x04;
    buffer[22] = 0x00;
    buffer[23] = 0x00;

    // IGMP
    buffer[24] = igmp_type; // Type
    buffer[25] = 0; // Max resp time

    buffer[28] = (_dst_addr >> 24) & 0xFF;
    buffer[29] = (_dst_addr >> 16) & 0xFF;
    buffer[30] = (_dst_addr >> 8) & 0xFF;
    buffer[31] = (_dst_addr) & 0xFF;

    const uint16_t igmp_checksum =
        in_chksum(&buffer[IP_HEADER_SIZE], IGMP_HEADER_SIZE);
    buffer[26] = (igmp_checksum >> 8) & 0xFF;
    buffer[27] = (igmp_checksum) & 0xFF;
}
#endif

void asc_socket_set_multicast_if(asc_socket_t *sock, const char *addr)
{
    if(!addr)
        return;

    struct in_addr a;
    a.s_addr = inet_addr(addr);

    if(setsockopt(sock->fd, IPPROTO_IP, IP_MULTICAST_IF
                  , (const char *)&a, sizeof(a)) == -1)
    {
        asc_log_error(MSG("failed to set if = `%s': %s"), addr
                      , asc_error_msg());
    }
}

void asc_socket_set_multicast_ttl(asc_socket_t *sock, int ttl)
{
    if(ttl <= 0)
        return;

    if(setsockopt(sock->fd, IPPROTO_IP, IP_MULTICAST_TTL
                  , (const char *)&ttl, sizeof(ttl)) == -1)
    {
        asc_log_error(MSG("failed to set ttl = `%d': %s"), ttl
                      , asc_error_msg());
    }
}

void asc_socket_set_multicast_loop(asc_socket_t *sock, int is_on)
{
    setsockopt(sock->fd, IPPROTO_IP, IP_MULTICAST_LOOP
               , (const char *)&is_on, sizeof(is_on));
}

/* multicast_* */

static int __asc_socket_multicast_cmd(asc_socket_t *sock, int cmd)
{
    int ret = setsockopt(sock->fd, IPPROTO_IP, cmd
                         , (const char *)&sock->mreq, sizeof(sock->mreq));
    if(ret == -1)
        return -1;

#ifdef IGMP_EMULATION
    uint8_t buffer[IP_HEADER_SIZE + IGMP_HEADER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_addr.s_addr = sock->mreq.imr_multiaddr.s_addr;
    dst.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
    dst.sin_len = sizeof(dst);
#endif

    create_igmp_packet(buffer,
        (cmd == IP_ADD_MEMBERSHIP) ? 0x16 : 0x17,
        sock->mreq.imr_multiaddr.s_addr);

    const int raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if(raw_sock == -1)
        return -1;

    ret = sendto(raw_sock, (const char *)&buffer, sizeof(buffer), 0
                 , (struct sockaddr *)&dst, sizeof(dst));
    close(raw_sock);
    if(ret == -1)
        return -1;
#endif /* IGMP_EMULATION */

    return 0;
}

void asc_socket_multicast_join(asc_socket_t *sock, const char *addr
                               , const char *localaddr)
{
    memset(&sock->mreq, 0, sizeof(sock->mreq));
    sock->mreq.imr_multiaddr.s_addr = inet_addr(addr);
    if(sock->mreq.imr_multiaddr.s_addr == INADDR_NONE)
    {
        asc_log_error(MSG("failed to join multicast group `%s': %s"), addr
                      , asc_error_msg());
        return;
    }
    if(!IN_MULTICAST(ntohl(sock->mreq.imr_multiaddr.s_addr)))
    {
        sock->mreq.imr_multiaddr.s_addr = INADDR_NONE;
        return;
    }
    if(localaddr != NULL)
    {
        sock->mreq.imr_interface.s_addr = inet_addr(localaddr);
        if(sock->mreq.imr_interface.s_addr == INADDR_NONE)
        {
            asc_log_error(MSG("failed to set local address `%s'"), localaddr);
            sock->mreq.imr_interface.s_addr = INADDR_ANY;
        }
    }

    if(__asc_socket_multicast_cmd(sock, IP_ADD_MEMBERSHIP) == -1)
    {
        const char *err = asc_error_msg();
        asc_log_error(MSG("failed to join multicast group `%s': %s")
                      , inet_ntoa(sock->mreq.imr_multiaddr), err);

        sock->mreq.imr_multiaddr.s_addr = INADDR_NONE;
    }
}

void asc_socket_multicast_leave(asc_socket_t *sock)
{
    if(sock->mreq.imr_multiaddr.s_addr == INADDR_NONE)
        return;

    if(__asc_socket_multicast_cmd(sock, IP_DROP_MEMBERSHIP) == -1)
    {
        const char *err = asc_error_msg();
        asc_log_error(MSG("failed to leave multicast group `%s': %s")
                      , inet_ntoa(sock->mreq.imr_multiaddr), err);
    }
}

void asc_socket_multicast_renew(asc_socket_t *sock)
{
    if(sock->mreq.imr_multiaddr.s_addr == INADDR_NONE)
        return;

    while(1)
    {
        if(__asc_socket_multicast_cmd(sock, IP_DROP_MEMBERSHIP) == -1)
            break;

        if(__asc_socket_multicast_cmd(sock, IP_ADD_MEMBERSHIP) == -1)
            break;

        return;
    }

    const char *err = asc_error_msg();
    asc_log_error(MSG("failed to renew multicast group `%s': %s")
                  , inet_ntoa(sock->mreq.imr_multiaddr), err);
}
