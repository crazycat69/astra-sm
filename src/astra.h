/*
 * Astra
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

#ifndef _ASTRA_H_
#define _ASTRA_H_ 1

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

/*
 * system headers
 */
#ifdef _WIN32
    /* reduce header size */
#   define WIN32_LEAN_AND_MEAN

    /* enable C99-compliant printf (e.g. %zu) */
#   ifdef __GNUC__
#       if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ > 3)
#           define __printf__ __gnu_printf__
#       endif /* __GNUC__ >= 4.3 */
#   endif /* __GNUC__ */
#   define __USE_MINGW_ANSI_STDIO 1

    /* increase maximum set size for select() */
#   define FD_SETSIZE 1024

    /* target XP by default */
#   ifndef _WIN32_WINNT
#       define _WIN32_WINNT 0x0501
#   endif /* !_WIN32_WINNT */

#   include <winsock2.h>
#   include <ws2tcpip.h>
#   include <windows.h>
#endif /* _WIN32 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifndef _WIN32
#   include <sys/socket.h>
#endif /* !_WIN32 */

/*
 * common macros
 */
#define ASC_ARRAY_SIZE(_a) \
    (sizeof(_a) / sizeof(_a[0]))

#define __uarg(_x) \
    do { (void)_x; } while (0)

/* function attributes */
#ifndef __wur
#   define __wur __attribute__((__warn_unused_result__))
#endif /* !__wur */

#ifndef __dead
#   define __dead __attribute__((__noreturn__))
#endif /* !__dead */

#define __fmt_printf(__index, __first) \
    __attribute__((__format__(__printf__, __index, __first)))

#define __func_pure __attribute__((__pure__))
#define __func_const __attribute__((__const__))

/* additional exit codes */
#define EXIT_ABORT      2   /* abnormal termination */
#define EXIT_SIGHANDLER 101 /* signal handling error */
#define EXIT_MAINLOOP   102 /* main loop blocked */

/*
 * public interface
 */
#include "core/core.h"
#include "mpegts/mpegts.h"

#endif /* _ASTRA_H_ */
