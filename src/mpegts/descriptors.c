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

#define DESC_FUNCTION(__type_name) \
    static void desc_##__type_name(const uint8_t *desc)

#define DESC_LIST(__type_id, __type_name) \
    { __type_id, #__type_name, desc_##__type_name }

typedef void (*descriptor_parser_t)(const uint8_t *);

typedef struct
{
    unsigned type;
    const char *name;

    descriptor_parser_t parser;
} dvb_descriptor_t;

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

static inline const char *teletext_type_string(unsigned type_id)
{
    switch (type_id)
    {
        case 0x01: return "initial page";
        case 0x02: return "subtitle page";
        case 0x03: return "additional information";
        case 0x04: return "programming schedule";
        case 0x05: return "hearing impaired subtitle";
        default:   return "reserved";
    }
}

/*
 * parser functions
 */
DESC_FUNCTION(cas)
{
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
}

DESC_FUNCTION(lang)
{
    char lang[4];
    lang[0] = safe_char((char)desc[2]);
    lang[1] = safe_char((char)desc[3]);
    lang[2] = safe_char((char)desc[4]);
    lang[3] = 0x00;

    lua_pushstring(lua, lang);
    lua_setfield(lua, -2, "lang");
}

DESC_FUNCTION(maximum_bitrate)
{
    const uint32_t bitrate =
        ((desc[2] & 0x3f) << 16) | (desc[3] << 8) | desc[4];

    lua_pushnumber(lua, bitrate);
    lua_setfield(lua, -2, "maximum_bitrate");
}

DESC_FUNCTION(service)
{
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
}

DESC_FUNCTION(short_event)
{
    const char lang[] = { desc[2], desc[3], desc[4], 0x00 };
    lua_pushstring(lua, lang);
    lua_setfield(lua, -2, "lang");

    desc += 5; // skip 1:tag + 1:length + 3:lang
    push_description_text(desc);
    lua_setfield(lua, -2, "event_name");

    desc += desc[0] + 1;
    push_description_text(desc);
    lua_setfield(lua, -2, "text_char");
}

DESC_FUNCTION(extended_event)
{
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
}

DESC_FUNCTION(stream_id)
{
    lua_pushnumber(lua, desc[2]);
    lua_setfield(lua, -2, "stream_id");
}

DESC_FUNCTION(caid)
{
    lua_pushnumber(lua, ((desc[2] << 8) | desc[3]));
    lua_setfield(lua, -2, "caid");
}

DESC_FUNCTION(content)
{
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
}

DESC_FUNCTION(parental_rating)
{
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
}

DESC_FUNCTION(teletext)
{
    lua_newtable(lua);

    const size_t desc_size = desc[1];
    desc += 2;

    const uint8_t *const end = desc + desc_size;
    while (desc < end)
    {
        const int item_count = luaL_len(lua, -1) + 1;
        lua_pushnumber(lua, item_count);

        lua_newtable(lua);

        char lang[4];
        lang[0] = safe_char((char)desc[0]);
        lang[1] = safe_char((char)desc[1]);
        lang[2] = safe_char((char)desc[2]);
        lang[3] = 0x00;

        lua_pushstring(lua, lang);
        lua_setfield(lua, -2, "lang");

        const unsigned type = (desc[3] & 0xF8) >> 3;
        lua_pushstring(lua, teletext_type_string(type));
        lua_setfield(lua, -2, "page_type");

        const unsigned page_number = ((desc[3] & 0x7) << 8) | desc[4];
        lua_pushnumber(lua, page_number);
        lua_setfield(lua, -2, "page_number");

        lua_settable(lua, -3);
        desc += 5;
    }

    lua_setfield(lua, -2, "items");
}

DESC_FUNCTION(ac3)
{
    const bool component_type_flag = desc[2] & 0x80;
    const bool bsid_flag = desc[2] & 0x40;
    const bool mainid_flag = desc[2] & 0x20;
    const bool asvc_flag = desc[2] & 0x10;
    desc += 3;

    if (component_type_flag)
    {
        lua_pushnumber(lua, *desc);
        lua_setfield(lua, -2, "component_type");
        desc++;
    }

    if (bsid_flag)
    {
        lua_pushnumber(lua, *desc);
        lua_setfield(lua, -2, "bsid");
        desc++;
    }

    if (mainid_flag)
    {
        lua_pushnumber(lua, *desc);
        lua_setfield(lua, -2, "mainid");
        desc++;
    }

    if (asvc_flag)
    {
        lua_pushnumber(lua, *desc);
        lua_setfield(lua, -2, "asvc");
        desc++;
    }
}

DESC_FUNCTION(unknown)
{
    const int desc_size = 2 + desc[1];

    char *text = fancy_hex_str(desc, desc_size);
    lua_pushstring(lua, text);
    free(text);

    lua_setfield(lua, -2, __data);
}

/*
 * public interface
 */
static const dvb_descriptor_t known_descriptors[] = {
    DESC_LIST(0x09, cas),
    DESC_LIST(0x0a, lang),
    DESC_LIST(0x0e, maximum_bitrate),
    DESC_LIST(0x48, service),
    DESC_LIST(0x4d, short_event),
    DESC_LIST(0x4e, extended_event),
    DESC_LIST(0x52, stream_id),
    DESC_LIST(0x53, caid),
    DESC_LIST(0x54, content),
    DESC_LIST(0x55, parental_rating),
    DESC_LIST(0x56, teletext),
    DESC_LIST(0x6a, ac3),
};

void mpegts_desc_to_lua(const uint8_t *desc)
{
    const unsigned type_id = desc[0];

    lua_newtable(lua);

    lua_pushnumber(lua, type_id);
    lua_setfield(lua, -2, "type_id");

    for (size_t i = 0; i < ASC_ARRAY_SIZE(known_descriptors); i++)
    {
        const dvb_descriptor_t *const item = &known_descriptors[i];
        if (item->type == type_id)
        {
            lua_pushstring(lua, item->name);
            lua_setfield(lua, -2, __type_name);

            item->parser(desc);
            return;
        }
    }

    /* dump raw descriptor data */
    lua_pushstring(lua, "unknown");
    lua_setfield(lua, -2, __type_name);

    desc_unknown(desc);
}
