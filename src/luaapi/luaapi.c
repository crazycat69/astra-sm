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

#include <astra.h>
#include <luaapi/luaapi.h>

#define MSG(_msg) "[luaapi] " _msg

static
int errfunc(lua_State *L)
{
    int level = 0;
    lua_Debug ar;

    asc_log_error(MSG("unhandled Lua error"));
    asc_log_error("%s", lua_tostring(L, -1));

    while (lua_getstack(L, level, &ar))
    {
        lua_getinfo(L, "nSl", &ar);
        asc_log_error("%d: %s:%d -- %s [%s]", ++level, ar.short_src
                      , ar.currentline, (ar.name) ? ar.name : "<unknown>"
                      , ar.what);
    }

    return 1;
}

int lua_tr_call(lua_State *L, int nargs, int nresults)
{
    const int erridx = lua_gettop(L) - nargs;
    lua_pushcfunction(L, errfunc);
    lua_insert(L, erridx);

    const int ret = lua_pcall(L, nargs, nresults, erridx);
    lua_remove(L, erridx);

    return ret;
}
