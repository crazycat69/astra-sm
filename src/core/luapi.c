/*
 * Astra Core (Lua API)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
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

#include <astra.h>

#define MSG(_msg) "[core/lua] " _msg

/* search path for Lua */
#ifdef ASC_SCRIPT_DIR
#   define __SCRIPT_DIR ";" ASC_SCRIPT_DIR ASC_PATH_SEPARATOR "?.lua"
#else
#   define __SCRIPT_DIR
#endif

#define PACKAGE_PATH \
    "." ASC_PATH_SEPARATOR "?.lua" __SCRIPT_DIR

/* global Lua state */
lua_State *lua = NULL;

static int (*const astra_mods[])(lua_State *) = {
    LUA_CORE_BINDINGS
    LUA_STREAM_BINDINGS
    NULL
};

void asc_lua_core_init(void)
{
    asc_assert(lua == NULL, MSG("lua is already initialized"));

    lua = luaL_newstate();
    asc_assert(lua != NULL, MSG("luaL_newstate() failed"));
    luaL_openlibs(lua);

    /* load modules */
    for (size_t i = 0; astra_mods[i] != NULL; i++)
        astra_mods[i](lua);

    /* change package.path */
#ifdef DEBUG
    asc_log_info(MSG("setting package.path to '%s'"), PACKAGE_PATH);
#endif

    lua_getglobal(lua, "package");
    lua_pushstring(lua, PACKAGE_PATH);
    lua_setfield(lua, -2, "path");
    lua_pushstring(lua, "");
    lua_setfield(lua, -2, "cpath");
    lua_pop(lua, 1);
}

__asc_inline
void asc_lua_core_destroy(void)
{
    ASC_FREE(lua, lua_close);
}

bool module_option_number(const char *name, int *number)
{
    if(lua_type(lua, MODULE_OPTIONS_IDX) != LUA_TTABLE)
        return false;

    lua_getfield(lua, MODULE_OPTIONS_IDX, name);
    const int type = lua_type(lua, -1);
    bool result = false;

    if(type == LUA_TNUMBER)
    {
        *number = lua_tointeger(lua, -1);
        result = true;
    }
    else if(type == LUA_TSTRING)
    {
        const char *str = lua_tostring(lua, -1);
        *number = atoi(str);
        result = true;
    }
    else if(type == LUA_TBOOLEAN)
    {
        *number = lua_toboolean(lua, -1);
        result = true;
    }

    lua_pop(lua, 1);
    return result;
}

bool module_option_string(const char *name, const char **string, size_t *length)
{
    if(lua_type(lua, MODULE_OPTIONS_IDX) != LUA_TTABLE)
        return false;

    lua_getfield(lua, MODULE_OPTIONS_IDX, name);
    const int type = lua_type(lua, -1);
    bool result = false;

    if(type == LUA_TSTRING)
    {
        if(length)
            *length = luaL_len(lua, -1);
        *string = lua_tostring(lua, -1);
        result = true;
    }


    lua_pop(lua, 1);
    return result;
}

bool module_option_boolean(const char *name, bool *boolean)
{
    if(lua_type(lua, MODULE_OPTIONS_IDX) != LUA_TTABLE)
        return false;

    lua_getfield(lua, MODULE_OPTIONS_IDX, name);
    const int type = lua_type(lua, -1);
    bool result = false;

    if(type == LUA_TNUMBER)
    {
        *boolean = (lua_tointeger(lua, -1) != 0) ? true : false;
        result = true;
    }
    else if(type == LUA_TSTRING)
    {
        const char *str = lua_tostring(lua, -1);
        *boolean = (!strcmp(str, "true") || !strcmp(str, "on") || !strcmp(str, "1"));
        result = true;
    }
    else if(type == LUA_TBOOLEAN)
    {
        *boolean = lua_toboolean(lua, -1);
        result = true;
    }

    lua_pop(lua, 1);
    return result;
}
