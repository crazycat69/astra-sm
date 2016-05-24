/*
 * Astra Core (Memory allocation)
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

#ifndef _ASC_ALLOC_H_
#define _ASC_ALLOC_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

#define ASC_ALLOC(_nmemb, _type) \
    (_type *)asc_calloc(_nmemb, sizeof(_type))

#define ASC_FREE(_ptr, _destructor) \
    do { \
        if (_ptr != NULL) { \
            _destructor(_ptr); \
            _ptr = NULL; \
        } \
    } while (0)

static inline __wur
void *asc_calloc(size_t nmemb, size_t size)
{
    void *const p = calloc(nmemb, size);
    asc_assert(p != NULL, "[core/alloc] calloc() failed");

    return p;
}

#endif /* _ASC_ALLOC_H_ */
