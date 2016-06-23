/*
 * Astra Lua Library (ISO-8859)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2013-2015, Andrey Dyldin <and@cesbo.com>
 *               2014, Vitaliy Batin <fyrerx@gmail.com>
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

#include <astra.h>
#include <luaapi/module.h>

static
uint8_t *iso8859_1_encode(const uint8_t *data, size_t size)
{
    uint8_t *const text = ASC_ALLOC(size + 1, uint8_t);
    uint8_t c;
    size_t i = 0, j = 0;

    while(i < size)
    {
        c = data[i++];
        if(c < 0x80)
        {
            if(!c) break;
            text[j++] = c;
        }
        else
        {
            text[j++] = ((c & 0x03) << 6) | (data[i++] & 0x3F);
        }
    }

    text[j] = '\0';
    return text;
}

static
uint8_t *iso8859_5_encode(const uint8_t *data, size_t size)
{
    uint8_t *const text = ASC_ALLOC(size + 1, uint8_t);
    uint8_t c;
    size_t i = 0, j = 0;

    while(i < size)
    {
        c = data[i++];

        if(c < 0x80)
        {
            if(!c) break;
            text[j++] = c;
        }
        else if(c == 0xD1)
        {
            text[j++] = 0xE0 | data[i++];
        }
        else if(c == 0xD0)
        {
            c = data[i++];
            if(c & 0x20)
                text[j++] = 0xC0 | (c & 0x1F);
            else
                text[j++] = 0xA0 | (c & 0x1F);
        }
    }

    text[j] = '\0';
    return text;
}

static
int method_iso8859_encode(lua_State *L)
{
    const int part = luaL_checkinteger(L, 1);
    const uint8_t *data = (const uint8_t *)luaL_checkstring(L, 2);
    const size_t data_size = luaL_len(L, 2);

    uint8_t *iso8859;
    luaL_Buffer b;

    switch(part)
    {
        case 1:
            iso8859 = iso8859_1_encode(data, data_size);
            lua_pushstring(L, (char *)iso8859);
            free(iso8859);
            break;

        case 5:
            luaL_buffinit(L, &b);
            luaL_addchar(&b, 0x10);
            luaL_addchar(&b, 0x00);
            luaL_addchar(&b, 0x05);
            iso8859 = iso8859_5_encode(data, data_size);
            luaL_addstring(&b, (const char *)iso8859);
            free(iso8859);
            luaL_pushresult(&b);
            break;

        default:
            lua_pushnil(L);
            lua_pushfstring(L, "charset %d is not supported", part);
            asc_log_error("[iso8859] %s", lua_tostring(L, -1));
            return 2;
    }

    return 1;
}

static
void module_load(lua_State *L)
{
    static const luaL_Reg api[] =
    {
        { "encode", method_iso8859_encode },
        { NULL, NULL },
    };

    luaL_newlib(L, api);
    lua_setglobal(L, "iso8859");
}

BINDING_REGISTER(iso8859)
{
    .load = module_load,
};
