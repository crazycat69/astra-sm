/*
 * Astra Lua Library (Logging)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
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

/*
 * Logging bindings for Lua
 *
 * Methods:
 *      log.set({ options })
 *                  - set logging options:
 *                    color     - boolean, color stdout logs
 *                    debug     - boolean, allow debug messages,
 *                                  false by default
 *                    filename  - string, write log to file
 *                    stdout    - boolean, write log to stdout,
 *                                  true by default
 *                    syslog    - string, send log to syslog;
 *                                  ignored on Windows
 *      log.error(message)
 *                  - error message
 *      log.warning(message)
 *                  - warning message
 *      log.info(message)
 *                  - information message
 *      log.debug(message)
 *                  - debug message
 */

#include <astra/astra.h>
#include <astra/luaapi/module.h>

static
int method_set(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_foreach(L, 1)
    {
        const char *const key = lua_tostring(L, -2);

        if (!strcmp(key, "debug"))
        {
            luaL_checktype(L, -1, LUA_TBOOLEAN);
            asc_log_set_debug(lua_toboolean(L, -1));
        }
        else if (!strcmp(key, "filename"))
        {
            const char *const val = luaL_checkstring(L, -1);
            asc_log_set_file((*val != '\0') ? val : NULL);
        }
        else if (!strcmp(key, "syslog"))
        {
#ifndef _WIN32
            const char *const val = luaL_checkstring(L, -1);
            asc_log_set_syslog((*val != '\0') ? val : NULL);
#endif /* !_WIN32 */
        }
        else if (!strcmp(key, "stdout"))
        {
            luaL_checktype(L, -1, LUA_TBOOLEAN);
            asc_log_set_stdout(lua_toboolean(L, -1));
        }
        else if (!strcmp(key, "color"))
        {
            luaL_checktype(L, -1, LUA_TBOOLEAN);
            asc_log_set_color(lua_toboolean(L, -1));
        }
        else
        {
            luaL_error(L, "[log] unknown option: %s", key);
        }
    }

    return 0;
}

static
int method_error(lua_State *L)
{
    asc_log_error("%s", luaL_checkstring(L, 1));
    return 0;
}

static
int method_warning(lua_State *L)
{
    asc_log_warning("%s", luaL_checkstring(L, 1));
    return 0;
}

static
int method_info(lua_State *L)
{
    asc_log_info("%s", luaL_checkstring(L, 1));
    return 0;
}

static
int method_debug(lua_State *L)
{
    asc_log_debug("%s", luaL_checkstring(L, 1));
    return 0;
}

static
void module_load(lua_State *L)
{
    static const luaL_Reg api[] =
    {
        { "set", method_set },
        { "error", method_error },
        { "warning", method_warning },
        { "info", method_info },
        { "debug", method_debug },
        { NULL, NULL },
    };

    luaL_newlib(L, api);
    lua_setglobal(L, "log");
}

BINDING_REGISTER(log)
{
    .load = module_load,
};
