/*
 * Astra Module: Hardware Enumerator
 *
 * Copyright (C) 2016, Artem Kharitonov <artem@3phase.pw>
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
 * Interface to device-specific enumerators
 *
 * Methods:
 *      hw_enum.modules()
 *                  - return table containing a list of supported device types
 *      hw_enum.devices(module)
 *                  - list devices currently present in the system
 */

#include "enum.h"

/* this gets put in builddir, not srcdir */
#include "hwdev/enum/list.h"

#define MSG(_msg) "[hw_enum] " _msg

static
const hw_enum_t *hw_find_enum(const char *name)
{
    const hw_enum_t *hw_enum = NULL;
    for (size_t i = 0; enum_list[i] != NULL; i++)
    {
        if (!strcmp(enum_list[i]->name, name))
        {
            hw_enum = enum_list[i];
            break;
        }
    }

    return hw_enum;
}

static
int method_modules(lua_State *L)
{
    lua_newtable(L);
    for (const hw_enum_t **hw_enum = enum_list; *hw_enum != NULL; hw_enum++)
    {
        lua_pushstring(L, (*hw_enum)->name);
        lua_pushstring(L, (*hw_enum)->description);
        lua_settable(L, -3);
    }

    return 1;
}

static
int method_devices(lua_State *L)
{
    const char *const mod_name = luaL_checkstring(L, 1);
    const hw_enum_t *const hw_enum = hw_find_enum(mod_name);

    if (hw_enum == NULL)
    {
        luaL_error(L, MSG("module '%s' is not available in this build")
                   , mod_name);
    }

    /* call module-specific enumerator function */
    lua_pushcfunction(L, hw_enum->enumerate);
    lua_call(L, 0, 1);
    luaL_checktype(L, -1, LUA_TTABLE);

    return 1;
}

static
void module_load(lua_State *L)
{
    static const luaL_Reg api[] =
    {
        { "modules", method_modules },
        { "devices", method_devices },
        { NULL, NULL },
    };

    luaL_newlib(L, api);
    lua_setglobal(L, "hw_enum");
}

BINDING_REGISTER(hw_enum)
{
    .load = module_load,
};
