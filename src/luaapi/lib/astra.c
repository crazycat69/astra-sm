/*
 * Astra Lua Library (Instance Control)
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
 * Set of Lua methods and variables for basic instance control
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
#include <luaapi/module.h>

static int method_exit(lua_State *L)
{
    const int status = luaL_optinteger(L, 1, EXIT_SUCCESS);
    asc_lib_exit(status);
    return 0; /* unreachable */
}

static int method_abort(lua_State *L)
{
    __uarg(L);
    asc_lib_abort();
    return 0; /* unreachable */
}

static int method_reload(lua_State *L)
{
    __uarg(L);
    asc_main_loop_reload();
    return 0;
}

static int method_shutdown(lua_State *L)
{
    __uarg(L);
    asc_main_loop_shutdown();
    return 0;
}

static void module_load(lua_State *L)
{
    static const luaL_Reg api[] =
    {
        { "exit", method_exit },
        { "abort", method_abort },
        { "reload", method_reload },
        { "shutdown", method_shutdown },
        { NULL, NULL },
    };

    luaL_newlib(L, api);

#ifdef DEBUG
    static const int is_debug = 1;
#else
    static const int is_debug = 0;
#endif

    lua_pushboolean(L, is_debug);
    lua_setfield(L, -2, "debug");

    lua_pushstring(L, PACKAGE_STRING);
    lua_setfield(L, -2, "fullname");

    lua_pushstring(L, PACKAGE_NAME);
    lua_setfield(L, -2, "package");

    lua_pushstring(L, PACKAGE_VERSION);
    lua_setfield(L, -2, "version");

    lua_setglobal(L, "astra");
}

BINDING_REGISTER(astra)
{
    .load = module_load,
};
