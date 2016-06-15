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
#include <luaapi/module.h>
#include <bindings.h>

#define MSG(_msg) "[luaapi] " _msg

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

static const module_manifest_t *module_list[] = {
    LUA_CORE_BINDINGS
    LUA_STREAM_BINDINGS
    NULL
};

static int panic_handler(lua_State *L) {
    const char *const err = lua_tostring(L, -1);

    asc_log_error("%s", err);
    asc_log_error(MSG("unprotected Lua error, aborting execution"));
    asc_lib_exit(EXIT_ABORT);

    return 0;
}

lua_State *lua_api_init(void)
{
    lua_State *const L = luaL_newstate();
    asc_assert(L != NULL, MSG("luaL_newstate() failed"));

    luaL_openlibs(L);
    lua_atpanic(L, panic_handler);

    /* load modules */
    for (size_t i = 0; module_list[i] != NULL; i++)
        module_register(L, module_list[i]);

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
