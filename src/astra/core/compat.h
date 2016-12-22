/*
 * Astra Core (Compatibility library)
 * http://cesbo.com/astra
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

#ifndef _ASC_COMPAT_H_
#define _ASC_COMPAT_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

/*
 * replacement defines
 */

/* [u]int64_t format specs as per C99 */
#ifndef __WORDSIZE
#   if defined __x86_64__ && !defined __ILP32__
#       define __WORDSIZE 64
#   else
#       define __WORDSIZE 32
#   endif
#endif /* !__WORDSIZE */

#ifndef __PRI64_PREFIX
#   if __WORDSIZE == 64 && !defined(__llvm__)
#       define __PRI64_PREFIX "l"
#   else
#       define __PRI64_PREFIX "ll"
#   endif
#endif /* !__PRI64_PREFIX */

#ifndef PRId64
#   define PRId64 __PRI64_PREFIX "d"
#endif /* !PRId64 */

#ifndef PRIu64
#   define PRIu64 __PRI64_PREFIX "u"
#endif /* !PRIu64 */

/* win32-specific defines */
#ifdef _WIN32
    /* open() no-inherit flag */
#   ifndef O_CLOEXEC
#       define O_CLOEXEC _O_NOINHERIT
#   endif /* !O_CLOEXEC */

    /* WSASocket() no-inherit flag; appeared in Win7 SP1 */
#   ifndef WSA_FLAG_NO_HANDLE_INHERIT
#       define WSA_FLAG_NO_HANDLE_INHERIT 0x80
#   endif /* !WSA_FLAG_NO_HANDLE_INHERIT */

    /* file create modes */
#   ifndef S_IRUSR
#       define S_IRUSR _S_IREAD
#   endif /* !S_IRUSR */
#   ifndef S_IWUSR
#       define S_IWUSR _S_IWRITE
#   endif /* !S_IWUSR */
#   ifndef S_IRGRP
#       define S_IRGRP 0
#   endif /* !S_IRGRP */
#   ifndef S_IWGRP
#       define S_IWGRP 0
#   endif /* !S_IWGRP */
#   ifndef S_IROTH
#       define S_IROTH 0
#   endif /* !S_IROTH */
#   ifndef S_IWOTH
#       define S_IWOTH 0
#   endif /* !S_IWOTH */

    /* maximum path length */
#   ifndef PATH_MAX
#       define PATH_MAX MAX_PATH
#   endif /* ! */

    /* cast int to HANDLE */
#   define ASC_TO_HANDLE(__x) ((HANDLE)((intptr_t)(__x)))
    /*
     * NOTE: casting SOCKET to int to HANDLE is far from best practice,
     *       however it seems to work on existing WinAPI implementations;
     *       some future Windows version might break this.
     */

    /* COM release shorthand */
#   define ASC_RELEASE(_obj) ASC_FREE(_obj, IUnknown_Release)

    /* fix error code if pointer equals NULL */
#   define ASC_WANT_PTR(_hr, _ptr) \
        do { \
            if (SUCCEEDED(_hr) && _ptr == NULL) \
                _hr = E_POINTER; \
        } while (0)

    /* same, except S_FALSE code is preserved */
#   define ASC_WANT_ENUM(_hr, _ptr) \
        do { \
            if (_hr == S_OK && _ptr == NULL) \
                _hr = E_POINTER; \
        } while (0)
#endif /* _WIN32 */

/* not defined on some systems */
#ifndef EWOULDBLOCK
#   define EWOULDBLOCK EAGAIN
#endif /* !EWOULDBLOCK */

/*
 * replacement functions
 */

#ifndef HAVE_PREAD
ssize_t pread(int fd, void *buffer, size_t size, off_t off);
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *str, size_t max);
#endif

#ifndef HAVE_STRNLEN
size_t strnlen(const char *str, size_t max) __func_pure;
#endif

#ifdef _WIN32
/* IsProcessInJob() wrapper for legacy builds */
#if _WIN32_WINNT <= _WIN32_WINNT_WIN2K
BOOL cx_IsProcessInJob(HANDLE process, HANDLE job, BOOL *result);
#define IsProcessInJob(...) cx_IsProcessInJob(__VA_ARGS__)
#endif /* _WIN32_WINNT <= _WIN32_WINNT_WIN2K */

/* convert from char (UTF-8) to wchar_t (UTF-16) */
wchar_t *cx_widen(const char *str);

/* convert from wchar_t (UTF-16) to char (UTF-8) */
char *cx_narrow(const wchar_t *str);
#endif /* _WIN32 */

/*
 * standard function wrappers
 */

int cx_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int cx_mkstemp(char *tpl);
int cx_open(const char *path, int flags, ...);
int cx_socket(int family, int type, int protocol);

#ifdef HAVE_EPOLL_CREATE
int cx_epoll_create(int size);
#endif

#ifdef HAVE_KQUEUE
int cx_kqueue(void);
#endif

#ifndef ASC_COMPAT_NOWRAP
    /* accept() */
#   ifdef accept
#       undef accept
#   endif
#   define accept(...) cx_accept(__VA_ARGS__)

    /* mkstemp() */
#   ifdef mkstemp
#       undef mkstemp
#   endif
#   define mkstemp(...) cx_mkstemp(__VA_ARGS__)

    /* open() */
#   ifdef open
#       undef open
#   endif
#   define open(...) cx_open(__VA_ARGS__)

    /* socket() */
#   ifdef socket
#       undef socket
#   endif
#   define socket(...) cx_socket(__VA_ARGS__)

    /* epoll_create() */
#   ifdef epoll_create
#       undef epoll_create
#   endif
#   define epoll_create(...) cx_epoll_create(__VA_ARGS__)

    /* kqueue() */
#   ifdef kqueue
#       undef kqueue
#   endif
#   define kqueue(...) cx_kqueue(__VA_ARGS__)
#endif /* !ASC_COMPAT_NOWRAP */

#endif /* _ASC_COMPAT_H_ */
