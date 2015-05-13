/*
 * Astra Module: MPEG-TS (DVB descriptors)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
 *                    2015, Artem Kharitonov <artem@sysert.ru>
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

#define HEX_BUFSIZE 128

static const char __data[] = "data";
static const char __type_name[] = "type_name";
static const char __strip[] = "... (strip)";

static void push_description_text(const uint8_t *data)
{
    luaL_Buffer b;
    luaL_buffinit(lua, &b);

    char *text = iso8859_decode(&data[1], data[0]);
    luaL_addstring(&b, text);
    free(text);

    luaL_pushresult(&b);
}

static inline char safe_char(char c)
{
    return (c > 0x1f && c < 0x7f) ? c : '.';
}

static char *fancy_hex_str(const uint8_t *ptr, const uint8_t len)
{
    char *buf = (char *)calloc(1, HEX_BUFSIZE);
    asc_assert(buf != NULL, "calloc() failed");

    unsigned int pos = 0;
    buf[pos++] = '0';
    buf[pos++] = 'x';

    for(unsigned int i = 0; i < len; i++)
    {
        const unsigned int space = HEX_BUFSIZE - pos;

        if (space <= sizeof(__strip))
        {
            /* no space left */
            snprintf(&buf[pos], space, "%s", __strip);
            break;
        }

        hex_to_str(&buf[pos], ptr++, 1);
        pos += 2;
    }

    return buf;
}

// descriptor list goes here

void mpegts_desc_to_lua(const uint8_t *desc)
{
    lua_newtable(lua);

    lua_pushnumber(lua, desc[0]);
    lua_setfield(lua, -2, "type_id");

    switch(desc[0])
    {
        case 0x09:
        { /* CA */
            lua_pushstring(lua, "cas");
            lua_setfield(lua, -2, __type_name);

            const uint16_t ca_pid = DESC_CA_PID(desc);
            const uint16_t caid = desc[2] << 8 | desc[3];

            lua_pushnumber(lua, caid);
            lua_setfield(lua, -2, "caid");

            lua_pushnumber(lua, ca_pid);
            lua_setfield(lua, -2, "pid");

            const uint8_t ca_info_size = desc[1] - 4; // 4 = caid + ca_pid
            if(ca_info_size > 0)
            {
                char *text = fancy_hex_str(&desc[6], ca_info_size);
                lua_pushstring(lua, text);
                free(text);

                lua_setfield(lua, -2, __data);
            }
            break;
        }
        case 0x0A:
        { /* ISO-639 language */
            static const char __lang[] = "lang";
            lua_pushstring(lua, __lang);
            lua_setfield(lua, -2, __type_name);

            char lang[4];
            lang[0] = safe_char((char)desc[2]);
            lang[1] = safe_char((char)desc[3]);
            lang[2] = safe_char((char)desc[4]);
            lang[3] = 0x00;

            lua_pushstring(lua, lang);
            lua_setfield(lua, -2, __lang);
            break;
        }
        case 0x48:
        { /* Service Descriptor */
            lua_pushstring(lua, "service");
            lua_setfield(lua, -2, __type_name);

            lua_pushnumber(lua, desc[2]);
            lua_setfield(lua, -2, "service_type_id");

            desc += 3;
            // service provider
            if(desc[0] > 0)
                push_description_text(desc);
            else
                lua_pushstring(lua, "");
            lua_setfield(lua, -2, "service_provider");

            desc += desc[0] + 1;
            // service name
            if(desc[0] > 0)
                push_description_text(desc);
            else
                lua_pushstring(lua, "");
            lua_setfield(lua, -2, "service_name");

            break;
        }
        case 0x4D:
        { /* Short Even */
            lua_pushstring(lua, "short_event_descriptor");
            lua_setfield(lua, -2, __type_name);

            const char lang[] = { desc[2], desc[3], desc[4], 0x00 };
            lua_pushstring(lua, lang);
            lua_setfield(lua, -2, "lang");

            desc += 5; // skip 1:tag + 1:length + 3:lang
            push_description_text(desc);
            lua_setfield(lua, -2, "event_name");

            desc += desc[0] + 1;
            push_description_text(desc);
            lua_setfield(lua, -2, "text_char");

            break;
        }
        case 0x4E:
        { /* Extended Event */
            lua_pushstring(lua, "extended_event_descriptor");
            lua_setfield(lua, -2, __type_name);

            lua_pushnumber(lua, desc[2] >> 4);
            lua_setfield(lua, -2, "desc_num");

            lua_pushnumber(lua, desc[2] & 0x0F);
            lua_setfield(lua, -2, "last_desc_num");

            const char lang[] = { desc[3], desc[4], desc[5], 0x00 };
            lua_pushstring(lua, lang);
            lua_setfield(lua, -2, "lang");

            desc += 6; // skip 1:tag + 1:length + 1:desc_num:last_desc_num + 3:lang

            if(desc[0] > 0)
            {
                lua_newtable(lua); // items[]

                const uint8_t *_item_ptr = &desc[1];
                const uint8_t *const _item_ptr_end = _item_ptr + desc[0];

                while(_item_ptr < _item_ptr_end)
                {
                    const int item_count = luaL_len(lua, -1) + 1;
                    lua_pushnumber(lua, item_count);

                    lua_newtable(lua);

                    push_description_text(_item_ptr);
                    lua_setfield(lua, -2, "item_desc");
                    _item_ptr += _item_ptr[0] + 1;

                    push_description_text(_item_ptr);
                    lua_setfield(lua, -2, "item");
                    _item_ptr += _item_ptr[0] + 1;

                    lua_settable(lua, -3);
               }

               lua_setfield(lua, -2, "items");
            }

            desc += desc[0] + 1;
            // text
            if(desc[0] > 0)
                 push_description_text(desc);
            else
                 lua_pushstring(lua, "");
            lua_setfield(lua, -2, "text");

            break;
        }
        case 0x52:
        { /* Stream Identifier */
            static const char __stream_id[] = "stream_id";
            lua_pushstring(lua, __stream_id);
            lua_setfield(lua, -2, __type_name);

            lua_pushnumber(lua, desc[2]);
            lua_setfield(lua, -2, __stream_id);
            break;
        }
        case 0x54:
        { /* Content [category] */
            lua_pushstring(lua, "content_descriptor");
            lua_setfield(lua, -2, __type_name);

            lua_newtable(lua); // items[]

            const uint8_t *_item_ptr = &desc[2];
            const uint8_t *const _item_ptr_end = _item_ptr + desc[1];

            while(_item_ptr < _item_ptr_end)
            {
                const int item_count = luaL_len(lua, -1) + 1;
                lua_pushnumber(lua, item_count);

                lua_newtable(lua);

                lua_pushnumber(lua, _item_ptr[0] >> 4);
                lua_setfield(lua, -2, "cn_l1");
                lua_pushnumber(lua, _item_ptr[0] & 0xF);
                lua_setfield(lua, -2, "cn_l2");
                lua_pushnumber(lua, _item_ptr[1] >> 4);
                lua_setfield(lua, -2, "un_l1");
                lua_pushnumber(lua, _item_ptr[1] & 0xF);
                lua_setfield(lua, -2, "un_l2");

                lua_settable(lua, -3);
                _item_ptr += 2;
            }
            lua_setfield(lua, -2, "items");

            break;
        }
        case 0x55:
        { /* Parental Rating Descriptor [rating] */
            lua_pushstring(lua, "parental_rating_descriptor");
            lua_setfield(lua, -2, __type_name);

            lua_newtable(lua); // items[]

            const uint8_t *_item_ptr = &desc[2];
            const uint8_t *const _item_ptr_end = _item_ptr + desc[1];

            while(_item_ptr < _item_ptr_end)
            {
                const int item_count = luaL_len(lua, -1) + 1;
                lua_pushnumber(lua, item_count);

                const char country[] = { _item_ptr[0], _item_ptr[1], _item_ptr[2], 0x00 };
                lua_pushstring(lua, country);
                lua_setfield(lua, -2, "country");

                lua_pushnumber(lua, _item_ptr[3]);
                lua_setfield(lua, -2, "rating");

                lua_settable(lua, -3);
                _item_ptr += 4;
            }
            lua_setfield(lua, -2, "items");

            break;
        }
        default:
        {
            lua_pushstring(lua, "unknown");
            lua_setfield(lua, -2, __type_name);

            const int desc_size = 2 + desc[1];

            char *text = fancy_hex_str(desc, desc_size);
            lua_pushstring(lua, text);
            free(text);

            lua_setfield(lua, -2, __data);
            break;
        }
    }
}
