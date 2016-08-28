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

#ifndef _LUA_API_H_
#define _LUA_API_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

#ifndef __cplusplus
#   include <lua.h>
#   include <lualib.h>
#   include <lauxlib.h>
#else
#   include <lua.hpp>
#endif /* !__cplusplus */

int lua_tr_call(lua_State *L, int nargs, int nresults);
void lua_err_log(lua_State *L);

#define lua_foreach(_lua, _idx) \
    for (lua_pushnil(_lua) \
         ; lua_next(_lua, _idx) \
         ; lua_pop(_lua, 1))

#endif /* _LUA_API_H_ */
