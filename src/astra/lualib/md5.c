/*
 * Astra Lua Library (MD5)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *                    2017, Artem Kharitonov <artem@3phase.pw>
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
 * Lua binding for MD5
 *
 * Methods:
 *      (string):md5()
 *                  - calculate MD5 hash
 */

#include <astra/astra.h>
#include <astra/utils/md5.h>
#include <astra/luaapi/module.h>

static
int method_md5(lua_State *L)
{
    size_t data_size = 0;
    const void *const data = luaL_checklstring(L, 1, &data_size);

    uint8_t digest[MD5_DIGEST_SIZE] = { 0 };
    md5_ctx_t ctx;

    memset(&ctx, 0, sizeof(md5_ctx_t));
    au_md5_init(&ctx);
    au_md5_update(&ctx, data, data_size);
    au_md5_final(&ctx, digest);

    lua_pushlstring(L, (char *)digest, sizeof(digest));

    return 1;
}

static
void module_load(lua_State *L)
{
    /* <string>:md5() */
    lua_getglobal(L, "string");
    lua_pushcfunction(L, method_md5);
    lua_setfield(L, -2, "md5");
    lua_pop(L, 1); /* string */
}

BINDING_REGISTER(md5)
{
    .load = module_load,
};
