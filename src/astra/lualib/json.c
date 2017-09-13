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
#include <astra/utils/json.h>
#include <astra/luaapi/module.h>

#define MSG(_msg) "[json] " _msg

static
int method_encode(lua_State *L)
{
    luaL_checkany(L, 1);
    if (au_json_enc(L) != 0)
        luaL_error(L, MSG("%s"), lua_tostring(L, -1));

    return 1;
}

static
int method_save(lua_State *L)
{
    const char *const filename = luaL_checkstring(L, 1);
    luaL_checkany(L, 2);

    if (au_json_enc(L) != 0)
        luaL_error(L, MSG("%s"), lua_tostring(L, -1));

    size_t json_len = 0;
    const char *const json = lua_tolstring(L, -1, &json_len);

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

static
int method_decode(lua_State *L)
{
    size_t json_len = 0;
    const char *const json = luaL_checklstring(L, 1, &json_len);

    if (au_json_dec(L, json, json_len) != 0)
        luaL_error(L, MSG("%s"), lua_tostring(L, -1));

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

    if (au_json_dec(L, json, json_len) != 0)
        luaL_error(L, MSG("%s"), lua_tostring(L, -1));

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
