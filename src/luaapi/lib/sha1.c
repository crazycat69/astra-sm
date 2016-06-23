/*
 * Astra Lua Library (SHA-1)
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
 * Lua binding for SHA1
 *
 * Methods:
 *      (string):sha1()
 *                  - calculate SHA1 hash
 */

#include <astra.h>
#include <utils/sha1.h>
#include <luaapi/module.h>

static
int method_sha1(lua_State *L)
{
    const char *const data = luaL_checkstring(L, 1);
    const int data_size = luaL_len(L, 1);

    sha1_ctx_t ctx;
    memset(&ctx, 0, sizeof(sha1_ctx_t));
    au_sha1_init(&ctx);
    au_sha1_update(&ctx, (uint8_t *)data, data_size);
    uint8_t digest[SHA1_DIGEST_SIZE];
    au_sha1_final(&ctx, digest);

    lua_pushlstring(L, (char *)digest, sizeof(digest));
    return 1;
}

static
void module_load(lua_State *L)
{
    /* <string>:sha1() */
    lua_getglobal(L, "string");
    lua_pushcfunction(L, method_sha1);
    lua_setfield(L, -2, "sha1");
    lua_pop(L, 1); /* string */
}

BINDING_REGISTER(sha1)
{
    .load = module_load,
};
