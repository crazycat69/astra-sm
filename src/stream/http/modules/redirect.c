/*
 * Astra Module: HTTP Module: Redirect
 * http://cesbo.com/astra
 *
 * Copyright (C) 2014-2015, Andrey Dyldin <and@cesbo.com>
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
#include "../http.h"

struct module_data_t
{
    MODULE_DATA();

    const char *location;
    int code;
};

/* Stack: 1 - instance, 2 - server, 3 - client, 4 - request */
static int module_call(lua_State *L, module_data_t *mod)
{
    http_client_t *const client = (http_client_t *)lua_touserdata(L, 3);

    if(lua_isnil(L, 4))
        return 0;

    http_client_redirect(client, mod->code, mod->location);

    return 0;
}

static int __module_call(lua_State *L)
{
    module_data_t *const mod =
        (module_data_t *)lua_touserdata(L, lua_upvalueindex(1));

    return module_call(L, mod);
}

static void module_init(lua_State *L, module_data_t *mod)
{
    module_option_string(L, "location", &mod->location, NULL);
    ASC_ASSERT(mod->location != NULL, "[http_redirect] option 'location' is required");

    mod->code = 302;
    module_option_integer(L, "code", &mod->code);

    // Set callback for http route
    lua_getmetatable(L, 3);
    lua_pushlightuserdata(L, (void *)mod);
    lua_pushcclosure(L, __module_call, 1);
    lua_setfield(L, -2, "__call");
    lua_pop(L, 1);
}

static void module_destroy(module_data_t *mod)
{
    __uarg(mod);
}

MODULE_REGISTER(http_redirect)
{
    .init = module_init,
    .destroy = module_destroy,
};
