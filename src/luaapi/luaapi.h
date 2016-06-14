/*
 * Astra Lua API
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

void module_register(lua_State *L, const char *name, const luaL_Reg *methods);
void module_new(lua_State *L, module_data_t *mod, const luaL_Reg *meta_methods
                , const module_method_t *mod_methods);
int module_thunk(lua_State *L);

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
    static int __module_tostring(lua_State *L) \
    { \
        lua_pushstring(L, #_name); \
        return 1; \
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
        static const luaL_Reg __meta_methods[] = \
        { \
            { "__gc", __module_delete }, \
            { "__tostring", __module_tostring }, \
            { NULL, NULL }, \
        }; \
        lua_newtable(L); \
        module_data_t *const mod = ASC_ALLOC(1, module_data_t); \
        module_new(L, mod, __meta_methods, __module_methods); \
        module_init(L, mod); \
        return 1; \
    } \
    MODULE_LUA_BINDING(_name) \
    { \
        static const luaL_Reg __meta_methods[] = \
        { \
            { "__call", __module_new }, \
            { "__tostring", __module_tostring }, \
            { NULL, NULL }, \
        }; \
        module_register(L, #_name, __meta_methods); \
        return 1; \
    }

#include <bindings.h>

#endif /* _LUA_API_H_ */
