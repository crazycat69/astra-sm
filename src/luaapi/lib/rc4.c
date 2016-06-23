/*
 * Astra Lua Library (RC4)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
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
 * Lua binding for RC4
 *
 * Methods:
 *      (string):rc4(key)
 *                  - encrypt/decrypt
 */

#include <astra.h>
#include <utils/rc4.h>
#include <luaapi/module.h>

static
int method_rc4(lua_State *L)
{
    const uint8_t *const data = (uint8_t *)luaL_checkstring(L, 1);
    const int data_size = luaL_len(L, 1);

    const uint8_t *const key = (uint8_t *)luaL_checkstring(L, 2);
    const int key_size = luaL_len(L, 2);

    rc4_ctx_t ctx;
    au_rc4_init(&ctx, key, key_size);

    luaL_Buffer b;
    char *const p = luaL_buffinitsize(L, &b, data_size);
    au_rc4_crypt(&ctx, (uint8_t *)p, data, data_size);
    luaL_addsize(&b, data_size);
    luaL_pushresult(&b);

    memset(&ctx, 0, sizeof(ctx));

    return 1;
}

static
void module_load(lua_State *L)
{
    /* <string>:rc4() */
    lua_getglobal(L, "string");
    lua_pushcfunction(L, method_rc4);
    lua_setfield(L, -2, "rc4");
    lua_pop(L, 1); /* string */
}

BINDING_REGISTER(rc4)
{
    .load = module_load,
};
