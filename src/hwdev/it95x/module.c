/*
 * Astra Module: IT95x (Lua interface)
 *
 * Copyright (C) 2017, Artem Kharitonov <artem@3phase.pw>
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
 *      it95x_output
 *
 * Module Role:
 *      Sink, no demux
 *
 * Module Options:
 *      upstream    - object, stream module instance
 *      name        - string, instance identifier for logging
 *      adapter     - number, device index
 *      devpath     - string, unique OS device path
 *
 * Module Methods:
 *      bitrate     - return recommended bitrate based on user settings
 */

#include "it95x.h"

static
void module_init(lua_State *L, module_data_t *mod)
{
}

static
void module_destroy(module_data_t *mod)
{
}

static
int method_bitrate(lua_State *L, module_data_t *mod)
{
    // TODO
    ASC_UNUSED(mod);

    lua_pushnumber(L, 0);
    return 1;
}

static
const module_method_t module_methods[] =
{
    { "bitrate", method_bitrate },
    { NULL, NULL },
};

STREAM_MODULE_REGISTER(it95x_output)
{
    .init = module_init,
    .destroy = module_destroy,
    .methods = module_methods,
};
