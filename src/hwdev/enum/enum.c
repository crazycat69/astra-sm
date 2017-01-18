/*
 * Astra Module: Hardware Enumerator
 *
 * Copyright (C) 2016-2017, Artem Kharitonov <artem@3phase.pw>
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
 * Interface to hardware-specific enumerators
 *
 * Table structure:
 *      hw_enum[module_name].description
 *                  - short text describing the module
 *      hw_enum[module_name].enumerate()
 *                  - function to list devices currently present in the system
 */

#include "enum.h"

/* this gets put in builddir, not srcdir */
#include "hwdev/enum/list.h"

#define MSG(_msg) "[hw_enum] " _msg

static
void module_load(lua_State *L)
{
    lua_newtable(L);

    for (size_t i = 0; enum_list[i] != NULL; i++)
    {
        lua_newtable(L);

        lua_pushstring(L, enum_list[i]->description);
        lua_setfield(L, -2, "description");
        lua_pushcfunction(L, enum_list[i]->enumerate);
        lua_setfield(L, -2, "enumerate");

        lua_setfield(L, -2, enum_list[i]->name);
    }

    lua_setglobal(L, "hw_enum");
}

BINDING_REGISTER(hw_enum)
{
    .load = module_load,
};
