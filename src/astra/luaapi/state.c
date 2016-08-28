/*
 * Astra Lua API (State Initialization)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015-2016, Artem Kharitonov <artem@3phase.pw>
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

#include <astra/astra.h>
#include <astra/luaapi/state.h>
#include <astra/luaapi/module.h>

#include "lib/list.h"

#define MSG(_msg) "[lua] " _msg

/* global Lua state */
lua_State *lua = NULL;

static
int panic_handler(lua_State *L)
{
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

    /* add os.dirsep for convenience */
    lua_getglobal(L, "os");
    lua_pushstring(L, LUA_DIRSEP);
    lua_setfield(L, -2, "dirsep");
    lua_pop(L, 1);

    /* set package search path */
    luaL_Buffer b;
    luaL_buffinit(L, &b);

    const char *const envvar = getenv("ASC_SCRIPTDIR");
    if (envvar != NULL)
    {
        char *const tmp = strdup(envvar);
        asc_assert(tmp != NULL, MSG("strdup() failed"));

        for (char *ptr = NULL, *tok = strtok_r(tmp, ";", &ptr)
             ; tok != NULL; tok = strtok_r(NULL, ";", &ptr))
        {
            luaL_addstring(&b, tok);
            luaL_addstring(&b, LUA_DIRSEP "?.lua;");
        }

        free(tmp);
    }

#ifdef _WIN32
    char buf[MAX_PATH] = { 0 };
    char *sl = NULL;
    const DWORD ret = GetModuleFileName(NULL, buf, sizeof(buf));

    if (ret > 0 && ret < sizeof(buf) && (sl = strrchr(buf, '\\')) != NULL)
    {
        *sl = '\0';

        luaL_addstring(&b, buf); /* <exe path>\scripts\?.lua */
        luaL_addstring(&b, LUA_DIRSEP "scripts" LUA_DIRSEP "?.lua;");

        luaL_addstring(&b, buf); /* <exe>\data\?.lua */
        luaL_addstring(&b, LUA_DIRSEP "data" LUA_DIRSEP "?.lua");
    }
#else /* _WIN32 */
    luaL_addstring(&b, ASC_SCRIPTDIR LUA_DIRSEP "?.lua;");
    luaL_addstring(&b, ASC_DATADIR LUA_DIRSEP "?.lua");
#endif /* !_WIN32 */

    lua_getglobal(L, "package");
    luaL_pushresult(&b);
    lua_setfield(L, -2, "path");
    lua_pushstring(L, "");
    lua_setfield(L, -2, "cpath");
    lua_pop(L, 1);

    /* load libraries */
    for (size_t i = 0; lua_lib_list[i] != NULL; i++)
        module_register(L, lua_lib_list[i]);

    return L;
}

void lua_api_destroy(lua_State *L)
{
    lua_close(L);
}
