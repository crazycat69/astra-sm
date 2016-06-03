/*
 * Astra Core (Compatibility library)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *               2015-2016, Artem Kharitonov <artem@3phase.pw>
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

#define ASC_COMPAT_NOWRAP

#include <astra.h>
#include <core/compat.h>

#ifndef HAVE_PREAD
ssize_t pread(int fd, void *buffer, size_t size, off_t off)
{
    if (lseek(fd, off, SEEK_SET) != off)
        return -1;

    return read(fd, buffer, size);
}
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *str, size_t max)
{
    size_t len = strnlen(str, max);
    char *res = (char *)malloc(len + 1);
    if (res)
    {
        memcpy(res, str, len);
        res[len] = '\0';
    }
    return res;
}
#endif

#ifndef HAVE_STRNLEN
size_t strnlen(const char *str, size_t max)
{
    const char *end = memchr(str, 0, max);
    return end ? (size_t)(end - str) : max;
}
#endif

int cx_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int fd;

#if defined(HAVE_ACCEPT4) && defined(SOCK_CLOEXEC)
    /*
     * NOTE: accept4() is Linux-specific, but also seems to be
     *       present on FreeBSD starting from version 10.
     */
    fd = accept4(sockfd, addr, addrlen, SOCK_CLOEXEC);
    if (fd != -1)
        return fd;
#endif /* HAVE_ACCEPT4 && SOCK_CLOEXEC */

    fd = accept(sockfd, addr, addrlen);
    if (fd == -1)
        return fd;

#ifdef _WIN32
    /* older Windows versions seem to default to inheritable sockets */
    const HANDLE sock = ASC_TO_HANDLE(fd);
    if (!SetHandleInformation(sock, HANDLE_FLAG_INHERIT, 0))
    {
        closesocket(fd);
        fd = -1;
    }
#else /* _WIN32 */
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        close(fd);
        fd = -1;
    }
#endif /* !_WIN32 */

    return fd;
}

int cx_open(const char *path, int flags, ...)
{
    mode_t mode = 0;

    if (flags & O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }

#ifdef _O_BINARY
    /*
     * NOTE: win32 text mode is not particularly useful except
     *       for causing bugs and slowing down writes.
     */
    flags |= _O_BINARY;
#endif /* _O_BINARY */

#ifdef O_CLOEXEC
    int fd = open(path, flags | O_CLOEXEC, mode);
#else
    /* older system with no atomic way of setting FD_CLOEXEC */
    int fd = open(path, flags, mode);
    if (fd == -1)
        return fd;

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        close(fd);
        fd = -1;
    }
#endif /* O_CLOEXEC */

    return fd;
}

int cx_socket(int family, int type, int protocol)
{
    int fd;

#ifdef _WIN32
    fd = WSASocket(family, type, protocol, NULL, 0
                   , WSA_FLAG_NO_HANDLE_INHERIT);
    if (fd != -1)
        return fd;

    /* probably pre-7/SP1 version of Windows */
    fd = WSASocket(family, type, protocol, NULL, 0, 0);
    if (fd == -1)
        return fd;

    const HANDLE sock = ASC_TO_HANDLE(fd);
    if (!SetHandleInformation(sock, HANDLE_FLAG_INHERIT, 0))
    {
        closesocket(fd);
        fd = -1;
    }
#else /* _WIN32 */
#ifdef SOCK_CLOEXEC
    /* try newer atomic API first */
    fd = socket(family, type | SOCK_CLOEXEC, protocol);
    if (fd != -1)
        return fd;
#endif /* SOCK_CLOEXEC */

    fd = socket(family, type, protocol);
    if (fd == -1)
        return fd;

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        close(fd);
        fd = -1;
    }
#endif /* _WIN32 */

    return fd;
}
