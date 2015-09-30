/*
 * Astra Lua API
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
 *                    2015, Artem Kharitonov <artem@sysert.ru>
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

#ifndef _LUA_API_H_
#define _LUA_API_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

#ifndef __cplusplus
#   include <lua.h>
#   include <lualib.h>
#   include <lauxlib.h>
#else
#   include <lua.hpp>
#endif /* !__cplusplus */

typedef struct module_data_t module_data_t;
typedef int (*module_callback_t)(lua_State *L, module_data_t *);

typedef struct
{
    const char *name;
    module_callback_t method;
} module_method_t;

bool module_option_integer(lua_State *L, const char *name, int *integer);
bool module_option_string(lua_State *L, const char *name, const char **string
                          , size_t *length);
bool module_option_boolean(lua_State *L, const char *name, bool *boolean);

#define lua_foreach(_lua, _idx) \
    for(lua_pushnil(_lua); lua_next(_lua, _idx); lua_pop(_lua, 1))

#define MODULE_OPTIONS_IDX 2

#define MODULE_LUA_DATA() \
    lua_State *__lua

#define MODULE_L(_mod) \
    ((_mod)->__lua)

#define MODULE_LUA_BINDING(_name) \
    LUA_API int luaopen_##_name(lua_State *L)

#define MODULE_LUA_METHODS() \
    static const module_method_t __module_methods[] =

#define MODULE_LUA_REGISTER(_name) \
    static const char __module_name[] = #_name; \
    static int __module_tostring(lua_State *L) \
    { \
        lua_pushstring(L, __module_name); \
        return 1; \
    } \
    static int __module_thunk(lua_State *L) \
    { \
        module_data_t *const mod = \
            (module_data_t *)lua_touserdata(L, lua_upvalueindex(1)); \
        module_method_t *const m = \
            (module_method_t *)lua_touserdata(L, lua_upvalueindex(2)); \
        return m->method(L, mod); \
    } \
    static int __module_delete(lua_State *L) \
    { \
        module_data_t *const mod = \
            (module_data_t *)lua_touserdata(L, lua_upvalueindex(1)); \
        module_destroy(mod); \
        free(mod); \
        return 0; \
    } \
    static int __module_new(lua_State *L) \
    { \
        size_t i; \
        static const luaL_Reg __meta_methods[] = \
        { \
            { "__gc", __module_delete }, \
            { "__tostring", __module_tostring }, \
        }; \
        module_data_t *const mod = (module_data_t *)calloc(1, sizeof(*mod)); \
        asc_assert(mod != NULL, "[luaapi] calloc() failed"); \
        lua_newtable(L); \
        lua_newtable(L); \
        for(i = 0; i < ASC_ARRAY_SIZE(__meta_methods); ++i) \
        { \
            const luaL_Reg *const m = &__meta_methods[i]; \
            lua_pushlightuserdata(L, (void *)mod); \
            lua_pushcclosure(L, m->func, 1); \
            lua_setfield(L, -2, m->name); \
        } \
        lua_setmetatable(L, -2); \
        for(i = 0; i < ASC_ARRAY_SIZE(__module_methods); ++i) \
        { \
            const module_method_t *const m = &__module_methods[i]; \
            if(!m->name) break; \
            lua_pushlightuserdata(L, (void *)mod); \
            lua_pushlightuserdata(L, (void *)m); \
            lua_pushcclosure(L, __module_thunk, 2); \
            lua_setfield(L, -2, m->name); \
        } \
        if(lua_gettop(L) == 3) \
        { \
            lua_pushvalue(L, MODULE_OPTIONS_IDX); \
            lua_setfield(L, 3, "__options"); \
        } \
        mod->__lua = L; \
        module_init(L, mod); \
        return 1; \
    } \
    MODULE_LUA_BINDING(_name) \
    { \
        static const luaL_Reg meta_methods[] = \
        { \
            { "__tostring", __module_tostring }, \
            { "__call", __module_new }, \
            { NULL, NULL } \
        }; \
        lua_newtable(L); \
        lua_newtable(L); \
        luaL_setfuncs(L, meta_methods, 0); \
        lua_setmetatable(L, -2); \
        lua_setglobal(L, __module_name); \
        return 1; \
    }

#include <bindings.h>

#endif /* _LUA_API_H_ */
