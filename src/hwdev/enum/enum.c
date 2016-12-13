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
 * Device enumerator
 *
 * Methods:
 *      hw_enum.drivers()
 *                  - return table containing a list of supported device types
 *      hw_enum.devices(driver)
 *                  - list devices currently present in the system
 */

#include "enum.h"

#define MSG(_msg) "[hw_enum] " _msg

#ifdef _WIN32
extern const hwdev_module_t hwdev_bda;
#endif

static const hwdev_module_t *hwdev_modules[] =
{
#ifdef _WIN32
    &hwdev_bda,
#endif
    NULL,
};

static
const hwdev_module_t *hw_find_driver(const char *drvname)
{
    const hwdev_module_t *drv = NULL;
    for (size_t i = 0; hwdev_modules[i] != NULL; i++)
    {
        if (!strcmp(hwdev_modules[i]->name, drvname))
        {
            drv = hwdev_modules[i];
            break;
        }
    }

    return drv;
}

static
int method_drivers(lua_State *L)
{
    lua_newtable(L);
    for (const hwdev_module_t **drv = hwdev_modules; *drv != NULL; drv++)
    {
        lua_pushstring(L, (*drv)->name);
        lua_pushstring(L, (*drv)->description);
        lua_settable(L, -3);
    }

    return 1;
}

static
int method_devices(lua_State *L)
{
    const char *const drvname = luaL_checkstring(L, 1);
    const hwdev_module_t *const drv = hw_find_driver(drvname);

    if (drv == NULL)
    {
        luaL_error(L, MSG("driver '%s' is not available in this build")
                   , drvname);
    }

    /* call driver-specific enumerator function */
    lua_pushcfunction(L, drv->enumerate);
    lua_call(L, 0, 1);
    luaL_checktype(L, -1, LUA_TTABLE);

    return 1;
}

static
void module_load(lua_State *L)
{
    static const luaL_Reg api[] =
    {
        { "drivers", method_drivers },
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
