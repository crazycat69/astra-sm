/*
 * Astra Core (String buffer)
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

#ifndef _HTTP_STRBUF_H_
#define _HTTP_STRBUF_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

#include <astra/luaapi/luaapi.h>

typedef struct string_buffer_t string_buffer_t;

string_buffer_t *string_buffer_alloc(void) __asc_result;
void string_buffer_free(string_buffer_t *buffer);

void string_buffer_addchar(string_buffer_t *buffer, char c);
void string_buffer_addlstring(string_buffer_t *buffer, const char *str
                              , size_t size);
void string_buffer_addvastring(string_buffer_t *buffer, const char *str
                               , va_list ap) __asc_printf(2, 0);
void string_buffer_addfstring(string_buffer_t *buffer, const char *str
                              , ...) __asc_printf(2, 3);

char *string_buffer_release(string_buffer_t *buffer
                            , size_t *size) __asc_result;
void string_buffer_push(lua_State *L, string_buffer_t *buffer);

#endif /* _HTTP_STRBUF_H_ */
