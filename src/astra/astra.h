/*
 * Astra
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

#ifndef _ASTRA_H_
#define _ASTRA_H_ 1

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

    /* target 2K by default */
#   ifndef _WIN32_WINNT
#       define _WIN32_WINNT _WIN32_WINNT_WIN2K
#   endif /* !_WIN32_WINNT */

    /* enable C COM API */
#   define COBJMACROS
#   define CINTERFACE

    /* suppress strsafe.h warnings */
#   define __CRT_STRSAFE_IMPL
#   define __STRSAFE__NO_INLINE

#   include <winsock2.h>
#   include <ws2tcpip.h>
#   if _WIN32_WINNT <= _WIN32_WINNT_WIN2K
#       include <wspiapi.h>
#   endif

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
 * common definitions
 */

/* convenience macros */
#define ASC_ARRAY_SIZE(_a) \
    (sizeof(_a) / sizeof(_a[0]))

#define ASC_UNUSED(_x) \
    do { (void)_x; } while (0)

/* function attributes */
#ifdef __dead
#   define __asc_noreturn __dead
#elif defined(__GNUC__) /* __dead */
#   define __asc_noreturn __attribute__((__noreturn__))
#else /* __GNUC__ */
#   define __asc_noreturn
#endif /* !__GNUC__ */

#ifdef __GNUC__
#define __asc_noinline \
    __attribute__((__noinline__))
#define __asc_printf(_index, _first) \
    __attribute__((__format__(__printf__, _index, _first)))
#define __asc_result \
    __attribute__((__warn_unused_result__))
#else /* __GNUC__ */
#define __asc_noinline
#define __asc_printf(_index, _first)
#define __asc_result
#endif /* !__GNUC__ */

/*
 * public interface
 */

#include "core/assert.h"
#include "core/alloc.h"
#include "core/compat.h"
#include "core/init.h"
#include "core/log.h"
#include "core/clock.h"
#include "core/error.h"

#include "mpegts/mpegts.h"
#include "mpegts/types.h"

#endif /* _ASTRA_H_ */
