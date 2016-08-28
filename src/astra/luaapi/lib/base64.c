/*
 * Astra Lua Library (Base64)
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
 * Encode/decode base64
 *
 * Methods:
 *      base64.encode(string)
 *                  - convert data to base64
 *      base64.decode(base64)
 *                  - convert base64 to data
 *
 * Alternate usage:
 *      (string):b64e()
 *                  - encode string
 *      (string):b64d()
 *                  - decode string
 */

#include <astra/astra.h>
#include <astra/utils/base64.h>
#include <astra/luaapi/module.h>

static
int method_encode(lua_State *L)
{
    const char *const data = luaL_checkstring(L, 1);
    const int data_size = luaL_len(L, 1);

    size_t data_enc_size = 0;
    char *data_enc = au_base64_enc(data, data_size, &data_enc_size);
    lua_pushlstring(L, data_enc, data_enc_size);

    free(data_enc);
    return 1;
}

static
int method_decode(lua_State *L)
{
    const char *const data = luaL_checkstring(L, 1);
    const int data_size = luaL_len(L, 1);

    size_t data_dec_size = 0;
    char *data_dec = (char *)au_base64_dec(data, data_size, &data_dec_size);
    lua_pushlstring(L, data_dec, data_dec_size);

    free(data_dec);
    return 1;
}

static
void module_load(lua_State *L)
{
    /* <string>:b64e(), b64d() */
    lua_getglobal(L, "string");
    lua_pushcfunction(L, method_encode);
    lua_setfield(L, -2, "b64e");
    lua_pushcfunction(L, method_decode);
    lua_setfield(L, -2, "b64d");
    lua_pop(L, 1); /* string */

    /* base64.encode(s), decode(s) */
    static const luaL_Reg api[] =
    {
        { "encode", method_encode },
        { "decode", method_decode },
        { NULL, NULL },
    };

    luaL_newlib(L, api);
    lua_setglobal(L, "base64");
}

BINDING_REGISTER(base64)
{
    .load = module_load,
};
