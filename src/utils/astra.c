/*
 * Astra Utils
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
 * Set of the astra methods and variables for lua
 *
 * Variables:
 *      astra.package
 *                  - string, autoconf package name
 *      astra.version
 *                  - string, astra version string
 *      astra.fullname
 *                  - string, package plus version
 *      astra.debug - boolean, whether this is a debug build
 *
 * Methods:
 *      astra.abort()
 *                  - abort execution
 *      astra.exit([status])
 *                  - immediate exit from astra
 *      astra.reload()
 *                  - restart without terminating the process
 *      astra.shutdown()
 *                  - schedule graceful shutdown
 */

#include <astra.h>
#include <core/mainloop.h>

__noreturn
static int method_exit(lua_State *L)
{
    int status = luaL_optinteger(L, 1, EXIT_SUCCESS);
    astra_exit(status);
}

__noreturn
static int method_abort(lua_State *L)
{
    __uarg(L);
    astra_abort();
}

static int method_reload(lua_State *L)
{
    __uarg(L);
    astra_reload();
    return 0;
}

static int method_shutdown(lua_State *L)
{
    __uarg(L);
    astra_shutdown();
    return 0;
}

MODULE_LUA_BINDING(astra)
{
    static const luaL_Reg astra_api[] =
    {
        { "exit", method_exit },
        { "abort", method_abort },
        { "reload", method_reload },
        { "shutdown", method_shutdown },
        { NULL, NULL },
    };

    luaL_newlib(L, astra_api);

    lua_pushboolean(lua,
#ifdef DEBUG
                    1
#else
                    0
#endif
                    );

    lua_setfield(lua, -2, "debug");

    lua_pushstring(lua, PACKAGE_STRING);
    lua_setfield(lua, -2, "fullname");

    lua_pushstring(lua, PACKAGE_NAME);
    lua_setfield(lua, -2, "package");

    lua_pushstring(lua, PACKAGE_VERSION);
    lua_setfield(lua, -2, "version");

    lua_setglobal(L, "astra");

    return 1;
}
