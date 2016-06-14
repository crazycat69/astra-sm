/*
 * Astra Lua API (Module)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
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

#include <astra.h>
#include <luaapi/module.h>

struct module_data_t
{
    MODULE_LUA_DATA();
};

void module_register(lua_State *L, const char *name, const luaL_Reg *methods)
{
    lua_newtable(L);

    lua_newtable(L);
    luaL_setfuncs(L, methods, 0);
    lua_setmetatable(L, -2);

    lua_setglobal(L, name);
}

void module_new(lua_State *L, module_data_t *mod, const luaL_Reg *meta_methods
                , const module_method_t *mod_methods)
{
    lua_newtable(L);
    for (const luaL_Reg *m = meta_methods; m->name != NULL; m++)
    {
        lua_pushlightuserdata(L, mod);
        lua_pushcclosure(L, m->func, 1);
        lua_setfield(L, -2, m->name);
    }
    lua_setmetatable(L, -2);

    for (const module_method_t *m = mod_methods; m->name != NULL; m++)
    {
        lua_pushlightuserdata(L, mod);
        lua_pushlightuserdata(L, (void *)m->method);
        lua_pushcclosure(L, module_thunk, 2);
        lua_setfield(L, -2, m->name);
    }

    if (lua_gettop(L) == 3)
    {
        lua_pushvalue(L, MODULE_OPTIONS_IDX);
        lua_setfield(L, 3, "__options");
    }

    mod->__lua = L;
}

int module_thunk(lua_State *L)
{
    void *const mod = lua_touserdata(L, lua_upvalueindex(1));
    void *const func = lua_touserdata(L, lua_upvalueindex(2));

    return ((module_callback_t)func)(L, (module_data_t *)mod);
}

bool module_option_integer(lua_State *L, const char *name, int *integer)
{
    if (lua_type(L, MODULE_OPTIONS_IDX) != LUA_TTABLE)
        return false;

    lua_getfield(L, MODULE_OPTIONS_IDX, name);
    const int type = lua_type(L, -1);
    bool result = false;

    if (type == LUA_TNUMBER)
    {
        *integer = lua_tointeger(L, -1);
        result = true;
    }
    else if (type == LUA_TSTRING)
    {
        const char *str = lua_tostring(L, -1);
        *integer = atoi(str);
        result = true;
    }
    else if (type == LUA_TBOOLEAN)
    {
        *integer = lua_toboolean(L, -1);
        result = true;
    }

    lua_pop(L, 1);
    return result;
}

bool module_option_string(lua_State *L, const char *name, const char **string
                          , size_t *length)
{
    if (lua_type(L, MODULE_OPTIONS_IDX) != LUA_TTABLE)
        return false;

    lua_getfield(L, MODULE_OPTIONS_IDX, name);
    const int type = lua_type(L, -1);
    bool result = false;

    if (type == LUA_TSTRING)
    {
        if (length)
            *length = luaL_len(L, -1);
        *string = lua_tostring(L, -1);
        result = true;
    }

    lua_pop(L, 1);
    return result;
}

bool module_option_boolean(lua_State *L, const char *name, bool *boolean)
{
    if (lua_type(L, MODULE_OPTIONS_IDX) != LUA_TTABLE)
        return false;

    lua_getfield(L, MODULE_OPTIONS_IDX, name);
    const int type = lua_type(L, -1);
    bool result = false;

    if (type == LUA_TNUMBER)
    {
        *boolean = (lua_tointeger(L, -1) != 0) ? true : false;
        result = true;
    }
    else if (type == LUA_TSTRING)
    {
        const char *str = lua_tostring(L, -1);
        *boolean = (!strcmp(str, "true")
                    || !strcmp(str, "on")
                    || !strcmp(str, "1"));
        result = true;
    }
    else if (type == LUA_TBOOLEAN)
    {
        *boolean = lua_toboolean(L, -1);
        result = true;
    }

    lua_pop(L, 1);
    return result;
}
