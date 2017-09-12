/*
 * Astra Utils (JSON)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
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

#include <astra/astra.h>
#include <astra/utils/json.h>
#include <astra/utils/strhex.h>

/* maximum number of elements in Lua stack */
#define JSON_MAX_STACK 1000

/*
 * encoding
 */

static
void walk_table(lua_State *L, luaL_Buffer *buf);

static
void set_string(luaL_Buffer *buf, const char *str, size_t len)
{
    luaL_addchar(buf, '"');

    for (size_t i = 0; i < len; i++)
    {
        const char c = str[i];

        switch (c)
        {
            case '/':  luaL_addlstring(buf, "\\/", 2);  break;
            case '\\': luaL_addlstring(buf, "\\\\", 2); break;
            case '"':  luaL_addlstring(buf, "\\\"", 2); break;
            case '\t': luaL_addlstring(buf, "\\t", 2);  break;
            case '\r': luaL_addlstring(buf, "\\r", 2);  break;
            case '\n': luaL_addlstring(buf, "\\n", 2);  break;
            case '\f': luaL_addlstring(buf, "\\f", 2);  break;
            case '\b': luaL_addlstring(buf, "\\b", 2);  break;

            default:
            {
                if (c >= 0 && c <= 0x1f)
                {
                    /* control character */
                    char esc[7] = { '\\', 'u', '0', '0' };
                    au_hex2str(&esc[4], &c, 1);
                    luaL_addlstring(buf, esc, 6);
                }
                else
                {
                    luaL_addchar(buf, c);
                }

                break;
            }
        }
    }

    luaL_addchar(buf, '"');
}

static
void set_value(lua_State *L, luaL_Buffer *buf)
{
    switch (lua_type(L, -1))
    {
        case LUA_TTABLE:
        {
            walk_table(L, buf);
            break;
        }

        case LUA_TBOOLEAN:
        {
            if (lua_toboolean(L, -1))
                luaL_addlstring(buf, "true", 4);
            else
                luaL_addlstring(buf, "false", 5);
            break;
        }

        case LUA_TNUMBER:
        {
            char num[64] = { 0 };
            const int len = snprintf(num, sizeof(num), "%.14g"
                                     , lua_tonumber(L, -1));
            if (len < 0)
                luaL_error(L, "cannot encode: snprintf() failed");

            if (strpbrk(num, "nN") != NULL) /* catch NaN/Inf */
                luaL_error(L, "cannot encode: invalid number: %s", num);

            luaL_addlstring(buf, num, len);
            break;
        }

        case LUA_TSTRING:
        {
            size_t len = 0;
            const char *const str = lua_tolstring(L, -1, &len);
            set_string(buf, str, len);
            break;
        }

        case LUA_TNIL:
        {
            luaL_addlstring(buf, "null", 4);
            break;
        }

        default:
        {
            luaL_error(L, "cannot encode: type '%s' is not supported"
                       , lua_typename(L, lua_type(L, -1)));
            break; /* unreachable */
        }
    }
}

static
void walk_table(lua_State *L, luaL_Buffer *buf)
{
    luaL_checkstack(L, 1, "cannot encode: not enough stack slots");
    if (lua_gettop(L) > JSON_MAX_STACK)
        luaL_error(L, "cannot encode: nested table depth exceeds limit");

    int pairs_count = 0;
    lua_foreach(L, -2)
        ++pairs_count;

    const bool is_array = (luaL_len(L, -1) == pairs_count);
    bool is_first = true;

    if (is_array)
    {
        luaL_addchar(buf, '[');

        lua_foreach(L, -2)
        {
            if (!is_first)
                luaL_addchar(buf, ',');
            else
                is_first = false;

            set_value(L, buf);
        }

        luaL_addchar(buf, ']');
    }
    else
    {
        luaL_addchar(buf, '{');

        lua_foreach(L, -2)
        {
            if (!is_first)
                luaL_addchar(buf, ',');
            else
                is_first = false;

            size_t len = 0;
            const char *const str = lua_tolstring(L, -2, &len);
            set_string(buf, str, len);

            luaL_addchar(buf, ':');
            set_value(L, buf);
        }

        luaL_addchar(buf, '}');
    }
}

static
int json_encode(lua_State *L)
{
    lua_State *const sec = lua_newthread(L);
    lua_insert(L, -2); /* push back slave Lua state */

    luaL_Buffer b;
    luaL_buffinit(sec, &b);
    set_value(L, &b);
    luaL_pushresult(&b);
    lua_xmove(sec, L, 1);

    return 1;
}

/* replace the value at the top of the stack with its JSON representation */
int au_json_enc(lua_State *L)
{
    lua_pushcfunction(L, json_encode);

    if (lua_gettop(L) > 1)
        lua_insert(L, -2);
    else
        lua_pushnil(L);

    return lua_pcall(L, 1, 1, 0);
}

/*
 * decoding
 */

static
size_t scan_json(lua_State *L, const char *str, size_t pos);

static __asc_noinline
size_t skip_space(const char *str, size_t pos)
{
    do
    {
        switch (str[pos])
        {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                ++pos;
                continue;

            default:
                return pos;
        }
    } while (true);
}

static
size_t skip_comment(lua_State *L, const char *str, size_t pos)
{
    for (; str[pos] != '\0'; ++pos)
    {
        if (str[pos] == '*' && str[pos + 1] == '/')
            return pos + 2;
    }

    luaL_error(L, "cannot decode: unterminated comment at position %d"
               , (int)pos);

    return 0; /* unreachable */
}

static inline
unsigned int decode_surrogate(unsigned int hi, unsigned int lo)
{
    return (((hi & 0x3ff) << 10) | (lo & 0x3ff)) + 0x10000;
}

static inline
bool hi_surrogate(unsigned int cp)
{
    return ((cp & 0xfc00) == 0xd800);
}

static inline
bool lo_surrogate(unsigned int cp)
{
    return ((cp & 0xfc00) == 0xdc00);
}

static
void utf8_encode(luaL_Buffer *buf, unsigned int cp)
{
    if (cp <= 0x7f)
    {
        luaL_addchar(buf, cp);
    }
    else if (cp <= 0x7ff)
    {
        luaL_addchar(buf, (cp >> 6) | 0xc0);
        luaL_addchar(buf, (cp & 0x3f) | 0x80);
    }
    else if (cp <= 0xffff)
    {
        luaL_addchar(buf, (cp >> 12) | 0xe0);
        luaL_addchar(buf, ((cp >> 6) & 0x3f) | 0x80);
        luaL_addchar(buf, (cp & 0x3f) | 0x80);
    }
    else
    {
        luaL_addchar(buf, (cp >> 18) | 0xf0);
        luaL_addchar(buf, ((cp >> 12) & 0x3f) | 0x80);
        luaL_addchar(buf, ((cp >> 6) & 0x3f) | 0x80);
        luaL_addchar(buf, (cp & 0x3f) | 0x80);
    }
}

static
unsigned int scan_codepoint(const char *str, size_t pos)
{
    unsigned int cp = 0;

    for (size_t i = 0; i < 4; i++)
    {
        const char c = str[pos + i];

        cp <<= 4;
        if (c >= '0' && c <= '9')
            cp |= (c - '0');
        else if (c >= 'A' && c <= 'F')
            cp |= (c - 'A' + 10);
        else if (c >= 'a' && c <= 'f')
            cp |= (c - 'a' + 10);
        else
            return UINT_MAX;
    }

    return cp;
}

static
size_t scan_unicode(lua_State *L, luaL_Buffer *buf
                    , const char *str, size_t pos)
{
    unsigned int cp = 0;
    const unsigned int hi = scan_codepoint(str, pos);

    if (hi == UINT_MAX)
    {
        luaL_error(L, "cannot decode: invalid unicode escape "
                      "sequence at offset %d", (int)pos);
    }

    if (hi_surrogate(hi))
    {
        pos += 4;
        if (!(str[pos] == '\\' && str[pos + 1] == 'u'))
        {
            luaL_error(L, "cannot decode: expected unicode low surrogate "
                          "at offset %d", (int)pos);
        }

        pos += 2;
        const unsigned int lo = scan_codepoint(str, pos);

        if (lo == UINT_MAX || !lo_surrogate(lo))
        {
            luaL_error(L, "cannot decode: invalid unicode low surrogate "
                          "at offset %d", (int)pos);
        }

        pos += 3;
        cp = decode_surrogate(hi, lo);
    }
    else
    {
        pos += 3;
        cp = hi;
    }

    utf8_encode(buf, cp);

    return pos;
}

static
size_t scan_string(lua_State *L, const char *str, size_t pos)
{
    luaL_Buffer b;
    luaL_buffinit(L, &b);

    for (; str[pos] != '\0'; ++pos)
    {
        if (str[pos] == '"')
        {
            break;
        }
        else if (str[pos] == '\\')
        {
            ++pos;
            switch (str[pos])
            {
                case '/':  luaL_addchar(&b, '/');  break;
                case '\\': luaL_addchar(&b, '\\'); break;
                case '"':  luaL_addchar(&b, '"');  break;
                case 't':  luaL_addchar(&b, '\t'); break;
                case 'r':  luaL_addchar(&b, '\r'); break;
                case 'n':  luaL_addchar(&b, '\n'); break;
                case 'f':  luaL_addchar(&b, '\f'); break;
                case 'b':  luaL_addchar(&b, '\b'); break;

                case 'u':
                    pos = scan_unicode(L, &b, str, pos + 1);
                    break;

                case '\0':
                    luaL_error(L, "cannot decode: incomplete escape sequence "
                                  "at offset %d", (int)pos);
                    break; /* unreachable */

                default:
                    luaL_error(L, "cannot decode: unknown escape sequence "
                                  "'\\%c' at offset %d", str[pos], (int)pos);
                    break; /* unreachable */
            }
        }
        else
        {
            luaL_addchar(&b, str[pos]);
        }
    }

    if (str[pos] == '\0')
    {
        luaL_error(L, "cannot decode: unterminated string at offset %d"
                   , (int)pos);
    }

    luaL_pushresult(&b);

    return pos + 1;
}

static
size_t scan_number(lua_State *L, const char *str, size_t pos)
{
    luaL_Buffer b;
    luaL_buffinit(L, &b);

    for (; str[pos] != '\0'; ++pos)
    {
        const char c = str[pos];
        if (!(c >= '0' && c <= '9') && c != '-' && c != '+'
            && c != '.' && c != 'e' && c != 'E')
        {
            break;
        }

        luaL_addchar(&b, c);
    }

    luaL_pushresult(&b);

    int isnum = 0;
    lua_pushnumber(L, lua_tonumberx(L, -1, &isnum));
    lua_remove(L, -2);

    if (!isnum)
    {
        luaL_error(L, "cannot decode: invalid number at offset %d", (int)pos);
    }

    return pos;
}

static
size_t scan_object(lua_State *L, const char *str, size_t pos)
{
    lua_newtable(L);

    do
    {
        pos = skip_space(str, pos);

        if (str[pos] == ',')
        {
            /* next item */
            ++pos;
            continue;
        }
        else if (str[pos] == '}')
        {
            /* object end */
            return ++pos;
        }
        else if (str[pos] == '/' && str[pos + 1] == '*')
        {
            /* comment block */
            pos = skip_comment(L, str, pos + 2);
            continue;
        }
        else if (str[pos] != '"')
        {
            /* invalid key */
            luaL_error(L, "cannot decode: expected '\"' at offset %d"
                       , (int)pos);
        }

        /* key */
        pos = scan_string(L, str, pos + 1);
        pos = skip_space(str, pos);

        if (str[pos] != ':')
        {
            luaL_error(L, "cannot decode: expected ':' at offset %d"
                       , (int)pos);
        }

        /* value */
        pos = skip_space(str, pos + 1);
        pos = scan_json(L, str, pos);
        lua_settable(L, -3);

        /* require item separators */
        pos = skip_space(str, pos);
        if (str[pos] != ',' && str[pos] != '}')
        {
            luaL_error(L, "cannot decode: expected ',' at offset %d"
                       , (int)pos);
        }
    } while (true);
}

static
size_t scan_array(lua_State *L, const char *str, size_t pos)
{
    lua_newtable(L);

    do
    {
        pos = skip_space(str, pos);

        if (str[pos] == ',')
        {
            /* next item */
            ++pos;
            continue;
        }
        else if (str[pos] == ']')
        {
            /* array end */
            return ++pos;
        }
        else if (str[pos] == '/' && str[pos + 1] == '*')
        {
            /* comment block */
            pos = skip_comment(L, str, pos + 2);
            continue;
        }

        /* array item */
        const int idx = luaL_len(L, -1) + 1;
        pos = scan_json(L, str, pos);
        lua_rawseti(L, -2, idx);

        /* require item separators */
        pos = skip_space(str, pos);
        if (str[pos] != ',' && str[pos] != ']')
        {
            luaL_error(L, "cannot decode: expected ',' at offset %d"
                       , (int)pos);
        }
    } while (true);
}

static
size_t scan_json(lua_State *L, const char *str, size_t pos)
{
    luaL_checkstack(L, 1, "cannot decode: not enough stack slots");
    if (lua_gettop(L) > JSON_MAX_STACK)
        luaL_error(L, "cannot decode: nested table depth exceeds limit");

    pos = skip_space(str, pos);

    switch (str[pos])
    {
        case '\0':
            luaL_error(L, "cannot decode: premature end at offset %d"
                       , (int)pos);
            break; /* unreachable */

        case '/':
            if (str[pos + 1] != '*')
            {
                luaL_error(L, "cannot decode: expected '/*' at offset %d"
                           , (int)pos);
            }

            pos = skip_comment(L, str, pos + 2);
            pos = scan_json(L, str, pos);
            break;

        case '{':
            pos = scan_object(L, str, pos + 1);
            break;

        case '[':
            pos = scan_array(L, str, pos + 1);
            break;

        case '"':
            pos = scan_string(L, str, pos + 1);
            break;

        default:
            if ((str[pos] >= '0' && str[pos] <= '9')
                || str[pos] == '-' || str[pos] == '.')
            {
                pos = scan_number(L, str, pos);
            }
            else if (!strncmp(&str[pos], "true", 4))
            {
                lua_pushboolean(L, true);
                pos += 4;
            }
            else if (!strncmp(&str[pos], "false", 5))
            {
                lua_pushboolean(L, false);
                pos += 5;
            }
            else if (!strncmp(&str[pos], "null", 4))
            {
                lua_pushnil(L);
                pos += 4;
            }
            else
            {
                luaL_error(L, "cannot decode: invalid input at offset %d"
                           , (int)pos);
            }
            break;
    }

    return pos;
}

static
int json_decode(lua_State *L)
{
    const char *const str = (char *)lua_touserdata(L, lua_upvalueindex(1));
    const size_t len = *(size_t *)lua_touserdata(L, lua_upvalueindex(2));

    if (len > 0)
    {
        size_t pos = scan_json(L, str, 0);
        pos = skip_space(str, pos);

        if (pos < len)
        {
            luaL_error(L, "cannot decode: trailing garbage at offset %d"
                       , (int)pos);
        }
    }

    if (lua_gettop(L) == 0)
        lua_pushnil(L);

    return 1;
}

/* decode JSON and push decoded value onto the stack */
int au_json_dec(lua_State *L, const char *str, size_t len)
{
    lua_pushlightuserdata(L, (void *)str);
    lua_pushlightuserdata(L, &len);
    lua_pushcclosure(L, json_decode, 2);

    return lua_pcall(L, 0, 1, 0);
}
