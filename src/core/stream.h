/*
 * Astra Core (Stream API)
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

#ifndef _ASC_STREAM_H_
#define _ASC_STREAM_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

#include <core/list.h>
#include <core/luapi.h>

typedef struct module_stream_t module_stream_t;

typedef void (*stream_callback_t)(module_data_t *, const uint8_t *);
typedef void (*demux_callback_t)(void *, uint16_t);

struct module_stream_t
{
    module_data_t *self;
    module_stream_t *parent;

    // stream
    stream_callback_t on_ts;
    asc_list_t *childs;

    // demux
    demux_callback_t join_pid;
    demux_callback_t leave_pid;

    uint8_t *pid_list;
};

#define MODULE_STREAM_DATA() module_stream_t __stream

// stream

void __module_stream_init(module_stream_t *stream);
void __module_stream_destroy(module_stream_t *stream);
void __module_stream_attach(module_stream_t *stream, module_stream_t *child);
void __module_stream_send(void *arg, const uint8_t *ts);

#define module_stream_init(_mod, _on_ts) \
    do { \
        _mod->__stream.self = _mod; \
        _mod->__stream.on_ts = _on_ts; \
        __module_stream_init(&_mod->__stream); \
        lua_getfield(lua, MODULE_OPTIONS_IDX, "upstream"); \
        if(lua_type(lua, -1) == LUA_TLIGHTUSERDATA) \
        { \
            module_stream_t *_stream = (module_stream_t *)lua_touserdata(lua, -1); \
            __module_stream_attach(_stream, &_mod->__stream); \
        } \
        lua_pop(lua, 1); \
    } while (0)

#define module_stream_demux_set(_mod, _join_pid, _leave_pid) \
    do { \
        _mod->__stream.pid_list = (uint8_t *)calloc(MAX_PID, sizeof(uint8_t)); \
        asc_assert(_mod->__stream.pid_list != NULL, "[stream] calloc() failed"); \
        _mod->__stream.join_pid = _join_pid; \
        _mod->__stream.leave_pid = _leave_pid; \
    } while (0)

#define module_stream_destroy(_mod) \
    do { \
        if(_mod->__stream.self) \
        { \
            if(_mod->__stream.pid_list) \
            { \
                for(int __i = 0; __i < MAX_PID; ++__i) \
                { \
                    if(_mod->__stream.pid_list[__i]) \
                    { \
                        module_stream_demux_leave_pid(_mod, __i); \
                    } \
                } \
                free(_mod->__stream.pid_list); \
                _mod->__stream.pid_list = NULL; \
            } \
            __module_stream_destroy(&_mod->__stream); \
            _mod->__stream.self = NULL; \
        } \
    } while (0)

#define module_stream_send(_mod, _ts) \
    __module_stream_send(&_mod->__stream, _ts)

// demux

#define module_stream_demux_check_pid(_mod, _pid) \
    (_mod->__stream.pid_list[_pid] > 0)

#define module_stream_demux_join_pid(_mod, _pid) \
    do { \
        const uint16_t ___pid = _pid; \
        asc_assert(_mod->__stream.pid_list != NULL \
                   , "%s:%d module_stream_demux_set() is required", __FILE__, __LINE__); \
        ++_mod->__stream.pid_list[___pid]; \
        if(_mod->__stream.pid_list[___pid] == 1 \
           && _mod->__stream.parent \
           && _mod->__stream.parent->join_pid) \
        { \
            _mod->__stream.parent->join_pid(_mod->__stream.parent->self, ___pid); \
        } \
    } while (0)

#define module_stream_demux_leave_pid(_mod, _pid) \
    do { \
        const uint16_t ___pid = _pid; \
        asc_assert(_mod->__stream.pid_list != NULL \
                   , "%s:%d module_stream_demux_set() is required", __FILE__, __LINE__); \
        if(_mod->__stream.pid_list[___pid] > 0) \
        { \
            --_mod->__stream.pid_list[___pid]; \
            if(_mod->__stream.pid_list[___pid] == 0 \
               && _mod->__stream.parent \
               && _mod->__stream.parent->leave_pid) \
            { \
                _mod->__stream.parent->leave_pid(_mod->__stream.parent->self, ___pid); \
            } \
        } \
        else \
        { \
            asc_log_error("%s:%d module_stream_demux_leave_pid() double call pid:%d" \
                          , __FILE__, __LINE__, ___pid); \
        } \
    } while (0)

// base

#define MODULE_STREAM_METHODS() \
    static int module_stream_stream(module_data_t *mod) \
    { \
        lua_pushlightuserdata(lua, &mod->__stream); \
        return 1; \
    }

#define MODULE_STREAM_METHODS_REF() \
    { "stream", module_stream_stream }

#endif /* _ASC_STREAM_H_ */
