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

#include <astra/astra.h>
#include <astra/utils/iso8859.h>
#include <astra/utils/strhex.h>
#include <astra/mpegts/descriptors.h>
#include <astra/mpegts/psi.h>

#define HEX_BUFSIZE 128

#define DESC_FUNCTION(__type_name) \
    static void desc_##__type_name(lua_State *L, const uint8_t *desc)

#define DESC_LIST(__type_id, __type_name) \
    { __type_id, #__type_name, desc_##__type_name }

typedef struct
{
    unsigned type;
    const char *name;

    void (*parser)(lua_State *, const uint8_t *);
} dvb_descriptor_t;

static const char __data[] = "data";
static const char __type_name[] = "type_name";
static const char __strip[] = "... (strip)";

static void push_description_text(lua_State *L, const uint8_t *data)
{
    luaL_Buffer b;
    luaL_buffinit(L, &b);

    char *text = au_iso8859_dec(&data[1], data[0]);
    luaL_addstring(&b, text);
    free(text);

    luaL_pushresult(&b);
}

static char *strncpy_print(char *dest, const uint8_t *src, size_t len)
{
    char *p = dest;

    for (size_t i = 0; i < len; i++)
    {
        const char c = src[i];
        *(p++) = (c > 0x1f && c < 0x7f) ? c : '.';
    }
    *p = '\0';

    return dest;
}

static char *fancy_hex_str(const uint8_t *ptr, size_t len)
{
    char *const buf = ASC_ALLOC(HEX_BUFSIZE, char);

    size_t pos = 0;
    buf[pos++] = '0';
    buf[pos++] = 'x';

    for (size_t i = 0; i < len; i++)
    {
        const size_t space = HEX_BUFSIZE - pos;

        if (space <= sizeof(__strip))
        {
            /* no space left */
            snprintf(&buf[pos], space, "%s", __strip);
            break;
        }

        au_hex2str(&buf[pos], ptr++, 1);
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

    lua_pushinteger(L, caid);
    lua_setfield(L, -2, "caid");

    lua_pushinteger(L, ca_pid);
    lua_setfield(L, -2, "pid");

    const uint8_t ca_info_size = desc[1] - 4; // 4 = caid + ca_pid
    if(ca_info_size > 0)
    {
        char *text = fancy_hex_str(&desc[6], ca_info_size);
        lua_pushstring(L, text);
        free(text);

        lua_setfield(L, -2, __data);
    }
}

DESC_FUNCTION(lang)
{
    char lang[4];
    strncpy_print(lang, &desc[2], 3);

    lua_pushstring(L, lang);
    lua_setfield(L, -2, "lang");
}

DESC_FUNCTION(maximum_bitrate)
{
    const uint32_t bitrate =
        ((desc[2] & 0x3f) << 16) | (desc[3] << 8) | desc[4];

    lua_pushinteger(L, bitrate);
    lua_setfield(L, -2, "maximum_bitrate");
}

DESC_FUNCTION(service)
{
    lua_pushinteger(L, desc[2]);
    lua_setfield(L, -2, "service_type_id");

    desc += 3;
    // service provider
    if(desc[0] > 0)
        push_description_text(L, desc);
    else
        lua_pushstring(L, "");
    lua_setfield(L, -2, "service_provider");

    desc += desc[0] + 1;
    // service name
    if(desc[0] > 0)
        push_description_text(L, desc);
    else
        lua_pushstring(L, "");
    lua_setfield(L, -2, "service_name");
}

DESC_FUNCTION(short_event)
{
    char lang[4];
    strncpy_print(lang, &desc[2], 3);
    lua_pushstring(L, lang);
    lua_setfield(L, -2, "lang");

    desc += 5; // skip 1:tag + 1:length + 3:lang
    push_description_text(L, desc);
    lua_setfield(L, -2, "event_name");

    desc += desc[0] + 1;
    push_description_text(L, desc);
    lua_setfield(L, -2, "text_char");
}

DESC_FUNCTION(extended_event)
{
    lua_pushinteger(L, desc[2] >> 4);
    lua_setfield(L, -2, "desc_num");

    lua_pushinteger(L, desc[2] & 0x0F);
    lua_setfield(L, -2, "last_desc_num");

    char lang[4];
    strncpy_print(lang, &desc[3], 3);
    lua_pushstring(L, lang);
    lua_setfield(L, -2, "lang");

    desc += 6; // skip 1:tag + 1:length + 1:desc_num:last_desc_num + 3:lang

    if(desc[0] > 0)
    {
        lua_newtable(L); // items[]

        const uint8_t *_item_ptr = &desc[1];
        const uint8_t *const _item_ptr_end = _item_ptr + desc[0];

        while(_item_ptr < _item_ptr_end)
        {
            const int item_count = luaL_len(L, -1) + 1;
            lua_pushinteger(L, item_count);

            lua_newtable(L);

            push_description_text(L, _item_ptr);
            lua_setfield(L, -2, "item_desc");
            _item_ptr += _item_ptr[0] + 1;

            push_description_text(L, _item_ptr);
            lua_setfield(L, -2, "item");
            _item_ptr += _item_ptr[0] + 1;

            lua_settable(L, -3);
       }

       lua_setfield(L, -2, "items");
    }

    desc += desc[0] + 1;
    // text
    if(desc[0] > 0)
         push_description_text(L, desc);
    else
         lua_pushstring(L, "");
    lua_setfield(L, -2, "text");
}

DESC_FUNCTION(stream_id)
{
    lua_pushinteger(L, desc[2]);
    lua_setfield(L, -2, "stream_id");
}

DESC_FUNCTION(caid)
{
    lua_pushinteger(L, ((desc[2] << 8) | desc[3]));
    lua_setfield(L, -2, "caid");
}

DESC_FUNCTION(content)
{
    lua_newtable(L); // items[]

    const uint8_t *_item_ptr = &desc[2];
    const uint8_t *const _item_ptr_end = _item_ptr + desc[1];

    while(_item_ptr < _item_ptr_end)
    {
        const int item_count = luaL_len(L, -1) + 1;
        lua_pushinteger(L, item_count);

        lua_newtable(L);

        lua_pushinteger(L, _item_ptr[0] >> 4);
        lua_setfield(L, -2, "cn_l1");
        lua_pushinteger(L, _item_ptr[0] & 0xF);
        lua_setfield(L, -2, "cn_l2");
        lua_pushinteger(L, _item_ptr[1] >> 4);
        lua_setfield(L, -2, "un_l1");
        lua_pushinteger(L, _item_ptr[1] & 0xF);
        lua_setfield(L, -2, "un_l2");

        lua_settable(L, -3);
        _item_ptr += 2;
    }
    lua_setfield(L, -2, "items");
}

DESC_FUNCTION(parental_rating)
{
    lua_newtable(L); // items[]

    const uint8_t *_item_ptr = &desc[2];
    const uint8_t *const _item_ptr_end = _item_ptr + desc[1];

    while(_item_ptr < _item_ptr_end)
    {
        const int item_count = luaL_len(L, -1) + 1;
        lua_pushinteger(L, item_count);

        char country[4];
        strncpy_print(country, &_item_ptr[0], 3);
        lua_pushstring(L, country);
        lua_setfield(L, -2, "country");

        lua_pushinteger(L, _item_ptr[3]);
        lua_setfield(L, -2, "rating");

        lua_settable(L, -3);
        _item_ptr += 4;
    }
    lua_setfield(L, -2, "items");
}

DESC_FUNCTION(teletext)
{
    lua_newtable(L);

    const size_t desc_size = desc[1];
    desc += 2;

    const uint8_t *const end = desc + desc_size;
    while (desc < end)
    {
        const int item_count = luaL_len(L, -1) + 1;
        lua_pushinteger(L, item_count);

        lua_newtable(L);

        char lang[4];
        strncpy_print(lang, desc, 3);

        lua_pushstring(L, lang);
        lua_setfield(L, -2, "lang");

        const unsigned type = (desc[3] & 0xF8) >> 3;
        lua_pushstring(L, teletext_type_string(type));
        lua_setfield(L, -2, "page_type");

        const unsigned page_number = ((desc[3] & 0x7) << 8) | desc[4];
        lua_pushinteger(L, page_number);
        lua_setfield(L, -2, "page_number");

        lua_settable(L, -3);
        desc += 5;
    }

    lua_setfield(L, -2, "items");
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
        lua_pushinteger(L, *desc);
        lua_setfield(L, -2, "component_type");
        desc++;
    }

    if (bsid_flag)
    {
        lua_pushinteger(L, *desc);
        lua_setfield(L, -2, "bsid");
        desc++;
    }

    if (mainid_flag)
    {
        lua_pushinteger(L, *desc);
        lua_setfield(L, -2, "mainid");
        desc++;
    }

    if (asvc_flag)
    {
        lua_pushinteger(L, *desc);
        lua_setfield(L, -2, "asvc");
        desc++;
    }
}

DESC_FUNCTION(unknown)
{
    const int desc_size = 2 + desc[1];

    char *text = fancy_hex_str(desc, desc_size);
    lua_pushstring(L, text);
    free(text);

    lua_setfield(L, -2, __data);
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

    /* last item is the default type */
    DESC_LIST(0x100, unknown),
};

void mpegts_desc_to_lua(lua_State *L, const uint8_t *desc)
{
    const unsigned type_id = desc[0];

    lua_newtable(L);

    lua_pushinteger(L, type_id);
    lua_setfield(L, -2, "type_id");

    const dvb_descriptor_t *item = NULL;
    for (size_t i = 0; i < ASC_ARRAY_SIZE(known_descriptors); i++)
    {
        item = &known_descriptors[i];
        if (item->type == type_id)
            break;
    }

    lua_pushstring(L, item->name);
    lua_setfield(L, -2, __type_name);

    item->parser(L, desc);
}
