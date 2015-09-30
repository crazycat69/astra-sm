/*
 * Astra Lua API (State Initialization)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015, Artem Kharitonov <artem@sysert.ru>
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
#include <luaapi/state.h>

#define MSG(_msg) "[luaapi/state] " _msg

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

lua_State *lua_api_init(void)
{
    lua_State *const L = luaL_newstate();
    asc_assert(L != NULL, MSG("luaL_newstate() failed"));
    luaL_openlibs(L);

    /* load modules */
    for (size_t i = 0; astra_mods[i] != NULL; i++)
        astra_mods[i](L);

    /* change package.path */
#ifdef LUA_DEBUG
    asc_log_debug(MSG("setting package.path to '%s'"), PACKAGE_PATH);
#endif

    lua_getglobal(L, "package");
    lua_pushstring(L, PACKAGE_PATH);
    lua_setfield(L, -2, "path");
    lua_pushstring(L, "");
    lua_setfield(L, -2, "cpath");
    lua_pop(L, 1);

    return L;
}

void lua_api_destroy(lua_State *L)
{
    lua_close(L);
}
