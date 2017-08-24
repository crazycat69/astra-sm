/*
 * Astra Lua Library (JSON)
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
#include <astra/luaapi/module.h>

#define MSG(_msg) "[json] " _msg

/* maximum number of elements in Lua stack */
#define JSON_MAX_STACK 1000

/*
 * encoding
 */

static
void walk_table(lua_State *L, luaL_Buffer *buf);

static
void set_string(luaL_Buffer *buf, const char *str)
{
    luaL_addchar(buf, '"');

    for (size_t i = 0; str[i] != '\0'; ++i)
    {
        switch (str[i])
        {
            case '\\': luaL_addlstring(buf, "\\\\", 2); break;
            case '"':  luaL_addlstring(buf, "\\\"", 2); break;
            case '\t': luaL_addlstring(buf, "\\t", 2);  break;
            case '\r': luaL_addlstring(buf, "\\r", 2);  break;
            case '\n': luaL_addlstring(buf, "\\n", 2);  break;
            case '\f': luaL_addlstring(buf, "\\f", 2);  break;
            case '\b': luaL_addlstring(buf, "\\b", 2);  break;

            default:
                luaL_addchar(buf, str[i]);
                break;
        }
    }

    luaL_addchar(buf, '"');
}

static
void set_value(lua_State *L, luaL_Buffer *buf)
{
    const char *num;

    switch (lua_type(L, -1))
    {
        case LUA_TTABLE:
            walk_table(L, buf);
            break;

        case LUA_TBOOLEAN:
            luaL_addstring(buf, lua_toboolean(L, -1) ? "true" : "false");
            break;

        case LUA_TNUMBER:
            num = lua_tostring(L, -1);
            if (strpbrk(num, "nN") != NULL) /* catch NaN/Inf */
                luaL_error(L, MSG("cannot encode: invalid number: %s"), num);

            luaL_addstring(buf, num);
            break;

        case LUA_TSTRING:
            set_string(buf, lua_tostring(L, -1));
            break;

        case LUA_TNIL:
            luaL_addlstring(buf, "null", 4);
            break;

        default:
            luaL_error(L, MSG("cannot encode: type '%s' is not supported")
                       , lua_typename(L, lua_type(L, -1)));
            break; /* unreachable */
    }
}

static
void walk_table(lua_State *L, luaL_Buffer *buf)
{
    luaL_checkstack(L, 1, "cannot encode: not enough stack slots");
    if (lua_gettop(L) > JSON_MAX_STACK)
        luaL_error(L, MSG("cannot encode: nested table depth exceeds limit"));

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

            set_string(buf, lua_tostring(L, -2));
            luaL_addchar(buf, ':');
            set_value(L, buf);
        }

        luaL_addchar(buf, '}');
    }
}

static
const char *json_encode(lua_State *L, size_t *len)
{
    lua_State *const Lb = lua_newthread(L);
    lua_insert(L, -2); /* push back slave Lua state */

    luaL_Buffer b;
    luaL_buffinit(Lb, &b);

    set_value(L, &b);
    luaL_pushresult(&b);
    lua_insert(L, -2);

    return lua_tolstring(Lb, -1, len);
}

static
int method_encode(lua_State *L)
{
    luaL_checkany(L, 1);
    lua_pushstring(L, json_encode(L, NULL));

    return 1;
}

static
int method_save(lua_State *L)
{
    const char *const filename = luaL_checkstring(L, 1);
    luaL_checkany(L, 2);

    size_t json_len = 0;
    const char *const json = json_encode(L, &json_len);

    const int flags = O_WRONLY | O_CREAT | O_TRUNC;
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    const int fd = open(filename, flags, mode);
    if (fd == -1)
        luaL_error(L, MSG("open(): %s: %s"), filename, strerror(errno));

    size_t skip = 0;
    while (skip < json_len)
    {
        const ssize_t written = write(fd, &json[skip], json_len - skip);
        if (written == -1)
        {
            close(fd);
            luaL_error(L, MSG("write(): %s: %s"), filename, strerror(errno));
        }

        skip += written;
    }

    if (write(fd, "\n", 1) == -1)
        { ; } /* ignore */

    if (close(fd) == -1)
        luaL_error(L, MSG("close(): %s: %s"), filename, strerror(errno));

    return 0;
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

    luaL_error(L, MSG("cannot decode: unterminated comment at position %d")
               , (int)pos);

    return 0; /* unreachable */
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
            switch (str[++pos])
            {
                case '/':  luaL_addchar(&b, '/');  break;
                case '\\': luaL_addchar(&b, '\\'); break;
                case '"':  luaL_addchar(&b, '"');  break;
                case 't':  luaL_addchar(&b, '\t'); break;
                case 'r':  luaL_addchar(&b, '\r'); break;
                case 'n':  luaL_addchar(&b, '\n'); break;
                case 'f':  luaL_addchar(&b, '\f'); break;
                case 'b':  luaL_addchar(&b, '\b'); break;

                default:
                    luaL_error(L, MSG("cannot decode: unknown escape "
                                      "sequence '\\%c' at offset %d")
                               , str[pos], (int)pos);
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
        luaL_error(L, MSG("cannot decode: unterminated string at offset %d")
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
        luaL_error(L, MSG("cannot decode: invalid number at offset %d")
                   , (int)pos);
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
            luaL_error(L, MSG("cannot decode: expected '\"' at offset %d")
                       , (int)pos);
        }

        /* key */
        pos = scan_string(L, str, pos + 1);
        pos = skip_space(str, pos);

        if (str[pos] != ':')
        {
            luaL_error(L, MSG("cannot decode: expected ':' at offset %d")
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
            luaL_error(L, MSG("cannot decode: expected ',' at offset %d")
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
            luaL_error(L, MSG("cannot decode: expected ',' at offset %d")
                       , (int)pos);
        }
    } while (true);
}

static
size_t scan_json(lua_State *L, const char *str, size_t pos)
{
    luaL_checkstack(L, 1, "cannot decode: not enough stack slots");
    if (lua_gettop(L) > JSON_MAX_STACK)
        luaL_error(L, MSG("cannot decode: nested table depth exceeds limit"));

    pos = skip_space(str, pos);

    switch (str[pos])
    {
        case '\0':
            luaL_error(L, MSG("cannot decode: premature end at offset %d")
                       , (int)pos);
            break; /* unreachable */

        case '/':
            if (str[pos + 1] != '*')
            {
                luaL_error(L, MSG("cannot decode: expected '/*' at offset %d")
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
                luaL_error(L, MSG("cannot decode: invalid input at offset %d")
                           , (int)pos);
            }
            break;
    }

    return pos;
}

static
void json_decode(lua_State *L, const char *str, size_t len)
{
    const int top = lua_gettop(L);

    if (len > 0)
    {
        size_t pos = scan_json(L, str, 0);
        pos = skip_space(str, pos);

        if (pos < len)
        {
            luaL_error(L, MSG("cannot decode: trailing garbage at offset %d")
                       , (int)pos);
        }
    }

    if (top == lua_gettop(L))
        lua_pushnil(L);
}

static
int method_decode(lua_State *L)
{
    size_t json_len = 0;
    const char *const json = luaL_checklstring(L, 1, &json_len);

    json_decode(L, json, json_len);

    return 1;
}

static
int method_load(lua_State *L)
{
    const char *const filename = luaL_checkstring(L, 1);

    const int fd = open(filename, O_RDONLY);
    if (fd == -1)
        luaL_error(L, MSG("open(): %s: %s"), filename, strerror(errno));

    struct stat sb;
    if (fstat(fd, &sb) != 0)
    {
        close(fd);
        luaL_error(L, MSG("fstat(): %s: %s"), filename, strerror(errno));
    }

    if (sb.st_size < 0 || sb.st_size > INTPTR_MAX)
    {
        close(fd);
        luaL_error(L, MSG("cannot load %s: file is too large"), filename);
    }

    const size_t json_len = sb.st_size;
    char *const json = (char *)lua_newuserdata(L, json_len + 1);
    json[json_len] = '\0';

    size_t skip = 0;
    while (skip < json_len)
    {
        const ssize_t got = read(fd, &json[skip], json_len - skip);
        if (got == -1)
        {
            close(fd);
            luaL_error(L, MSG("read(): %s: %s"), filename, strerror(errno));
        }

        skip += got;
    }

    if (close(fd) == -1)
        luaL_error(L, MSG("close(): %s: %s"), filename, strerror(errno));

    json_decode(L, json, json_len);

    return 1;
}

static
void module_load(lua_State *L)
{
    static const luaL_Reg api[] =
    {
        { "encode", method_encode },
        { "save", method_save },
        { "decode", method_decode },
        { "load", method_load },
        { NULL, NULL },
    };

    luaL_newlib(L, api);
    lua_setglobal(L, "json");
}

BINDING_REGISTER(json)
{
    .load = module_load,
};
