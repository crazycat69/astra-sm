/*
 * Astra Module: Hardware Device (Lua frontend)
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
 * Module Name:
 *      hw_device
 *
 * Module Options:
 *      driver      - string, driver name
 *
 * Module role, methods and other options are driver-specific.
 * See driver.c in each driver's subdirectory for more information.
 */

#include "hwdev.h"
#include "drivers.h"

#define MSG(_msg) "[hw_device] %s" _msg

static
void module_init(lua_State *L, module_data_t *mod)
{
    const char *drvname = NULL;
    module_option_string(L, "driver", &drvname, NULL);
    if (drvname == NULL)
        luaL_error(L, MSG("option 'driver' is required"));

    mod->drv = hw_find_driver(drvname);
    if (mod->drv == NULL)
    {
        luaL_error(L, MSG("driver '%s' is not available in this build")
                   , drvname);
    }

    if (mod->drv->methods != NULL)
        module_add_methods(L, mod, mod->drv->methods);

    mod->drv->init(L, mod);
}

static
void module_destroy(module_data_t *mod)
{
    mod->drv->destroy(mod);
}

STREAM_MODULE_REGISTER(hw_device)
{
    .init = module_init,
    .destroy = module_destroy,
};
