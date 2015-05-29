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
#include <setjmp.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/*
 * common macros
 */
#define ASC_ARRAY_SIZE(_a) (sizeof(_a) / sizeof(_a[0]))

#define ASC_FREE(_o, _m) if(_o != NULL) { _m(_o); _o = NULL; }

#ifndef _WIN32
#   define ASC_PATH_SEPARATOR "/"
#else
#   define ASC_PATH_SEPARATOR "\\"
#endif /* _WIN32 */

#define __uarg(_x) { (void)_x; }

#if defined(__GNUC_GNU_INLINE__) \
    || (defined(__GNUC__) && !defined(__GNUC_STDC_INLINE__))
    /* workaround for older GCC versions */
#   define __asc_inline inline
#else
#   define __asc_inline extern inline
#endif

#ifndef __wur
#   define __wur __attribute__((__warn_unused_result__))
#endif /* __wur */

#define __fmt_printf(__index, __first) \
    __attribute__((__format__(__printf__, __index, __first)))

#define __func_pure __attribute__((__pure__))
#define __func_const __attribute__((__const__))
#define __noreturn __attribute__((__noreturn__))

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
