/*
 * Astra Core (Compatibility library)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *               2015-2017, Artem Kharitonov <artem@3phase.pw>
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

#include <astra/astra.h>
#include <astra/core/compat.h>

#ifdef HAVE_EPOLL_CREATE
#   include <sys/epoll.h>
#endif

#ifdef HAVE_KQUEUE
#   include <sys/event.h>
#endif

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

#ifdef _WIN32
#if _WIN32_WINNT <= _WIN32_WINNT_WIN2K
/* IsProcessInJob() wrapper for legacy builds */
BOOL cx_IsProcessInJob(HANDLE process, HANDLE job, BOOL *result)
{
    typedef BOOL (WINAPI *cx_IsProcessInJob_t)(HANDLE, HANDLE, BOOL *);

    static HMODULE kern32;
    if (kern32 == NULL)
    {
        kern32 = LoadLibraryW(L"kernel32.dll");
        if (kern32 == NULL)
            return FALSE;
    }

    static cx_IsProcessInJob_t func;
    if (func == NULL)
    {
        func = (cx_IsProcessInJob_t)GetProcAddress(kern32, "IsProcessInJob");
        if (func == NULL)
            return FALSE;
    }

    return func(process, job, result);
}
#endif /* _WIN32_WINNT <= _WIN32_WINNT_WIN2K */

/* convert from char (UTF-8) to wchar_t (UTF-16) */
wchar_t *cx_widen(const char *str)
{
    int ret = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (ret <= 0)
        return NULL;

    wchar_t *buf = (wchar_t *)calloc((size_t)ret, sizeof(*buf));
    if (buf != NULL)
    {
        ret = MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, ret);
        if (ret <= 0)
        {
            free(buf);
            buf = NULL;
        }
    }
    else
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    }

    return buf;
}

/* convert from wchar_t (UTF-16) to char (UTF-8) */
char *cx_narrow(const wchar_t *str)
{
    int ret = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
    if (ret <= 0)
        return NULL;

    char *buf = (char *)calloc((size_t)ret, sizeof(*buf));
    if (buf != NULL)
    {
        ret = WideCharToMultiByte(CP_UTF8, 0, str, -1, buf, ret, NULL, NULL);
        if (ret <= 0)
        {
            free(buf);
            buf = NULL;
        }
    }
    else
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    }

    return buf;
}

/* return full executable path as a UTF-8 string */
char *cx_exepath(void)
{
    wchar_t *wbuf = NULL;
    DWORD bufsiz = MAX_PATH;
    DWORD ret = 0;
    char *buf = NULL;

    do
    {
        wchar_t *const ptr = wbuf;
        wbuf = (wchar_t *)realloc(wbuf, sizeof(*wbuf) * bufsiz);

        if (wbuf == NULL)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            free(ptr);
            return NULL;
        }

        ret = GetModuleFileNameW(NULL, wbuf, bufsiz);
        if ((ret == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            || ret >= bufsiz)
        {
            /* double the buffer size and try again */
            bufsiz *= 2;
            ret = 0;
        }
        else if (ret == 0)
        {
            /* error is not buffer-related */
            free(wbuf);
            return NULL;
        }
    } while (ret == 0);

    if (wbuf != NULL)
    {
        buf = cx_narrow(wbuf);
        free(wbuf);
    }

    return buf;
}
#endif /* _WIN32 */

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

int cx_mkstemp(char *tpl)
{
    int fd = -1;

#if defined(HAVE_MKOSTEMP) && defined(O_CLOEXEC)
    /* mkostemp(): best case scenario */
    fd = mkostemp(tpl, O_CLOEXEC);
#elif defined(HAVE_MKSTEMP)
    /* mkstemp(): non-atomic close-on-exec */
    fd = mkstemp(tpl);
    if (fd == -1)
        return -1;

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        close(fd);
        fd = -1;
    }
#elif defined(HAVE_MKTEMP)
    /* mktemp(): non-atomic file open */
    const char *const tmp = mktemp(tpl);
    if (tmp == NULL)
        return -1;

    const int flags = O_CREAT | O_WRONLY | O_TRUNC;
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    fd = cx_open(tmp, flags, mode);
    if (fd == -1)
        return -1;
#else
    /* shouldn't happen */
    __uarg(tpl);
    errno = ENOTSUP;
#endif

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
    fd = WSASocketW(family, type, protocol, NULL, 0, 0);
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

#ifdef HAVE_EPOLL_CREATE
int cx_epoll_create(int size)
{
    int fd = -1;

#if defined(HAVE_EPOLL_CREATE1) && defined(EPOLL_CLOEXEC)
    /* epoll_create1() first appeared in Linux kernel 2.6.27 */
    fd = epoll_create1(EPOLL_CLOEXEC);
#endif
    if (fd != -1)
        return fd;

    fd = epoll_create(size);
    if (fd == -1)
        return fd;

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        close(fd);
        fd = -1;
    }

    return fd;
}
#endif /* HAVE_EPOLL_CREATE */

#ifdef HAVE_KQUEUE
int cx_kqueue(void)
{
    int fd = -1;

#if defined(HAVE_KQUEUE1) && defined(O_CLOEXEC)
    /* kqueue1() is only present on NetBSD as of this writing */
    fd = kqueue1(O_CLOEXEC);
#endif
    if (fd != -1)
        return fd;

    fd = kqueue();
    if (fd == -1)
        return fd;

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        close(fd);
        fd = -1;
    }

    return fd;
}
#endif /* HAVE_KQUEUE */
