/*
 * Astra
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
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
#   define __USE_MINGW_ANSI_STDIO 1
#   define __printf__ __gnu_printf__

    /* maximum set size for select() */
#   define FD_SETSIZE 1024

    /* target XP by default */
#   ifndef _WIN32_WINNT
#       define _WIN32_WINNT 0x0501
#   endif /* !_WIN32_WINNT */

#   include <winsock2.h>
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

#ifndef __cplusplus
#   include <lua.h>
#   include <lualib.h>
#   include <lauxlib.h>
#else
#   include <lua.hpp>
#endif /* !__cplusplus */

/*
 * common macros
 */
#define ASC_ARRAY_SIZE(_a) \
    (sizeof(_a) / sizeof(_a[0]))

#define ASC_FREE(_o, _m) \
    do { \
        if (_o != NULL) { \
            _m(_o); \
            _o = NULL; \
        } \
    } while (0)

#define __uarg(_x) \
    do { (void)_x; } while (0)

#ifndef _WIN32
#   define ASC_PATH_SEPARATOR "/"
#else
#   define ASC_PATH_SEPARATOR "\\"
#endif /* _WIN32 */

#if defined(__GNUC_GNU_INLINE__) \
    || (defined(__GNUC__) && !defined(__GNUC_STDC_INLINE__))
    /* workaround for older GCC versions */
#   define __asc_inline inline
#else
#   define __asc_inline extern inline
#endif

/* function attributes */
#ifndef __wur
#   define __wur __attribute__((__warn_unused_result__))
#endif /* __wur */

#define __fmt_printf(__index, __first) \
    __attribute__((__format__(__printf__, __index, __first)))

#define __func_pure __attribute__((__pure__))
#define __func_const __attribute__((__const__))
#define __noreturn __attribute__((__noreturn__))

/* additional exit codes */
#define EXIT_ABORT      2   /* astra_abort() */
#define EXIT_SIGHANDLER 101 /* signal handling error */
#define EXIT_MAINLOOP   102 /* main loop blocked */

/*
 * public interface
 */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "core/core.h"
#include "mpegts/mpegts.h"
#include "utils/utils.h"

#include "bindings.h"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ASTRA_H_ */
