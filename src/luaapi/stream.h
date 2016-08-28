/*
 * Astra Lua API (Stream Module)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
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

#ifndef _LUA_STREAM_H_
#define _LUA_STREAM_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

#include <luaapi/module.h>

typedef void (*stream_callback_t)(module_data_t *, const uint8_t *);
typedef void (*demux_callback_t)(module_data_t *, uint16_t);

void module_stream_init(lua_State *L, module_data_t *mod
                        , stream_callback_t on_ts);
void module_stream_destroy(module_data_t *mod);

void module_stream_attach(module_data_t *mod, module_data_t *child);
void module_stream_send(void *arg, const uint8_t *ts);

void module_demux_set(module_data_t *mod, demux_callback_t join_pid
                      , demux_callback_t leave_pid);
void module_demux_join(module_data_t *mod, uint16_t pid);
void module_demux_leave(module_data_t *mod, uint16_t pid);
bool module_demux_check(const module_data_t *mod, uint16_t pid) __func_pure;

extern const module_method_t module_stream_methods[];

#define STREAM_MODULE_DATA_SIZE \
    MODULE_DATA_SIZE

#define STREAM_MODULE_DATA() \
    MODULE_DATA()

#define STREAM_MODULE_REGISTER(_name) \
    extern module_registry_t __registry_##_name; \
    MODULE_MANIFEST_DEF(_name) = \
    { \
        .name = #_name, \
        .size = sizeof(module_data_t), \
        .type = MODULE_TYPE_STREAM, \
        .reg = &__registry_##_name, \
    }; \
    module_registry_t __registry_##_name =

#endif /* _LUA_STREAM_H_ */
