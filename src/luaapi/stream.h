/*
 * Astra Lua API (Stream Module)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
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

#include <core/list.h>
#include <luaapi/module.h>

typedef struct module_stream_t module_stream_t;

typedef void (*stream_callback_t)(module_data_t *, const uint8_t *);
typedef void (*demux_callback_t)(module_data_t *, uint16_t);

struct module_stream_t
{
    module_data_t *self;
    module_stream_t *parent;

    stream_callback_t on_ts;
    asc_list_t *children;

    demux_callback_t join_pid;
    demux_callback_t leave_pid;
    uint8_t *pid_list;
};

/*
 * streaming module init and cleanup
 */

void __module_stream_init(module_stream_t *stream);
void __module_stream_destroy(module_stream_t *stream);
void __module_stream_attach(module_stream_t *stream, module_stream_t *child);

#define module_stream_init(_mod, _on_ts) \
    do { \
        _mod->__stream.self = _mod; \
        _mod->__stream.on_ts = _on_ts; \
        __module_stream_init(&_mod->__stream); \
        lua_State *const _lua = _mod->__lua; \
        lua_getfield(_lua, MODULE_OPTIONS_IDX, "upstream"); \
        if(lua_type(_lua, -1) == LUA_TLIGHTUSERDATA) \
        { \
            module_stream_t *const _stream = \
                (module_stream_t *)lua_touserdata(_lua, -1); \
            __module_stream_attach(_stream, &_mod->__stream); \
        } \
        lua_pop(_lua, 1); \
    } while (0)

#define module_stream_destroy(_mod) \
    do { \
        if(_mod->__stream.self != NULL) \
        { \
            if(_mod->__stream.pid_list != NULL) \
            { \
                for(int __i = 0; __i < MAX_PID; ++__i) \
                { \
                    if(_mod->__stream.pid_list[__i] > 0) \
                    { \
                        module_demux_leave(_mod, __i); \
                    } \
                } \
                ASC_FREE(_mod->__stream.pid_list, free); \
            } \
            __module_stream_destroy(&_mod->__stream); \
            _mod->__stream.self = NULL; \
        } \
    } while (0)

/*
 * send packet to downstream modules
 */

void module_stream_send(void *arg, const uint8_t *ts);

/*
 * join/leave PID on upstream module instance
 */

#define module_demux_set(_mod, _join_pid, _leave_pid) \
    do { \
        if(_mod->__stream.pid_list == NULL) \
        { \
            uint8_t *const _lst = ASC_ALLOC(MAX_PID, uint8_t); \
            _mod->__stream.pid_list = _lst; \
        } \
        _mod->__stream.join_pid = _join_pid; \
        _mod->__stream.leave_pid = _leave_pid; \
    } while (0)

bool module_demux_check(const module_data_t *mod, uint16_t pid);

#define module_demux_join(_mod, _pid) \
    do { \
        const uint16_t ___pid = _pid; \
        asc_assert(_mod->__stream.pid_list != NULL \
                   , "%s:%d module_demux_set() is required" \
                   , __FILE__, __LINE__); \
        ++_mod->__stream.pid_list[___pid]; \
        if(_mod->__stream.pid_list[___pid] == 1 \
           && _mod->__stream.parent != NULL \
           && _mod->__stream.parent->join_pid != NULL) \
        { \
            _mod->__stream.parent->join_pid(_mod->__stream.parent->self \
                                            , ___pid); \
        } \
    } while (0)

#define module_demux_leave(_mod, _pid) \
    do { \
        const uint16_t ___pid = _pid; \
        asc_assert(_mod->__stream.pid_list != NULL \
                   , "%s:%d module_demux_set() is required" \
                   , __FILE__, __LINE__); \
        if(_mod->__stream.pid_list[___pid] > 0) \
        { \
            --_mod->__stream.pid_list[___pid]; \
            if(_mod->__stream.pid_list[___pid] == 0 \
               && _mod->__stream.parent != NULL \
               && _mod->__stream.parent->leave_pid != NULL) \
            { \
                _mod->__stream.parent->leave_pid(_mod->__stream.parent->self \
                                                 , ___pid); \
            } \
        } \
        else \
        { \
            asc_log_error("%s:%d module_demux_leave() double call pid:%d" \
                          , __FILE__, __LINE__, ___pid); \
        } \
    } while (0)

/*
 * basic Lua methods required for every streaming module
 */

extern const module_method_t module_stream_methods[];

#define STREAM_MODULE_DATA() \
    MODULE_DATA(); \
    module_stream_t __stream

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
