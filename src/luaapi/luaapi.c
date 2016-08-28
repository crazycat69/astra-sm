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

#define MSG(_msg) "[lua] " _msg

/* custom error handler; returns table containing Lua stack trace */
#define ERR_ADDSTR(...) \
    do { \
        const int __idx = luaL_len(L, -1) + 1; \
        lua_pushinteger(L, __idx); \
        lua_pushfstring(L, __VA_ARGS__); \
        lua_settable(L, -3); \
    } while (0)

static
int err_func(lua_State *L)
{
    const char *const err_str = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_newtable(L);
    ERR_ADDSTR("%s", err_str);

    int level = 0;
    lua_Debug ar;

    while (lua_getstack(L, ++level, &ar))
    {
        lua_getinfo(L, "nSl", &ar);
        ERR_ADDSTR("%d: %s:%d -- %s [%s]", level, ar.short_src
                   , ar.currentline, (ar.name) ? ar.name : "<unknown>"
                   , ar.what);
    }

    if (level > 1)
        ERR_ADDSTR("end stack trace");

    return 1;
}

/* lua_pcall() wrapper that uses the error handler above */
int lua_tr_call(lua_State *L, int nargs, int nresults)
{
    const int erridx = lua_gettop(L) - nargs;
    lua_pushcfunction(L, err_func);
    lua_insert(L, erridx);

    const int ret = lua_pcall(L, nargs, nresults, erridx);
    lua_remove(L, erridx);

    return ret;
}

/* pop table from stack and send contents to error log */
void lua_err_log(lua_State *L)
{
    asc_log_error(MSG("unhandled Lua error"));

    if (lua_istable(L, -1))
    {
        lua_foreach(L, -2)
            asc_log_error(MSG("%s"), lua_tostring(L, -1));
    }
    else if (lua_isstring(L, -1))
    {
        asc_log_error(MSG("%s"), lua_tostring(L, -1));
    }
    else
    {
        asc_log_error(MSG("BUG: lua_err_log(): expected table/string, got %s")
                      , lua_typename(L, lua_type(L, -1)));
    }

    if (lua_gettop(L) > 0)
        lua_pop(L, 1);
}
