/*
 * Astra Core
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

#ifndef _ASC_ASSERT_H_
#define _ASC_ASSERT_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

#include <astra/core/init.h>

#define ASC_ASSERT(__cond, ...) \
    do { \
        if (!(__cond)) \
        { \
            fprintf(stderr, "%s:%u: %s: assertion `%s' failed\n" \
                    , __FILE__, __LINE__, __FUNCTION__, #__cond); \
            fprintf(stderr, __VA_ARGS__); \
            putc('\n', stderr); \
            asc_lib_abort(); \
        } \
    } while (0)

#define ASC_STATIC_ASSERT(_cond) \
    extern int __asc_static_assert[((_cond) ? 1 : -1)]

#endif /* _ASC_ASSERT_H_ */
