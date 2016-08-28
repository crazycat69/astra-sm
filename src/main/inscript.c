/*
 * Astra (Built-in script)
 * https://cesbo.com/astra
 *
 * Copyright (C) 2014-2015, Andrey Dyldin <and@cesbo.com>
 *                    2016, Artem Kharitonov <artem@3phase.pw>
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
#include <astra/luaapi/luaapi.h>

#include "inscript.h"
#include "../scripts/prepared.h"

static
int searcher(lua_State *L)
{
    const char *const name = luaL_checkstring(L, 1);

    const script_pkg_t *pkg = script_list;
    while (pkg->name != NULL)
    {
        if (!strcmp(pkg->name, name))
        {
            if (luaL_loadbuffer(L, pkg->data, pkg->size, pkg->chunk) != 0)
                luaL_error(L, "%s", lua_tostring(L, -1));

            return 1;
        }

        pkg++;
    }

    lua_pushfstring(L, "\n\tno built-in package '%s'", name);
    return 1;
}

void inscript_init(lua_State *L)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "searchers");
    luaL_checktype(L, -1, LUA_TTABLE);

    const int pos = luaL_len(L, -1) + 1;
    lua_pushcfunction(L, searcher);
    lua_rawseti(L, -2, pos);

    lua_pop(L, 2);
}
