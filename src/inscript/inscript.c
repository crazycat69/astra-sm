/*
 * Astra Built-in script
 * https://cesbo.com/astra
 *
 * Copyright (C) 2014-2015, Andrey Dyldin <and@cesbo.com>
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
#include <inscript/inscript.h>

static const char __module_name[] = "inscript";

static int load_inscript(lua_State *L, const char *buffer, size_t size
                         , const char *name)
{
    int load;

    load = luaL_loadbuffer(L, buffer, size, name);
    if(load != 0)
        return -1;

    load = lua_pcall(L, 0, LUA_MULTRET, 0);
    if(load != 0)
        return -1;

    return 0;
}

static int fn_inscript_callback(lua_State *L)
{
    int load;

    load = load_inscript(L, (const char *)base, sizeof(base), "=base");
    if(load != 0)
        luaL_error(L, "[main] %s", lua_tostring(L, -1));

    static const char *app = "=app";

    lua_getglobal(L, "argv");
    const int argc = luaL_len(L, -1);

    if(argc == 0)
    {
        lua_pop(L, 1); // argv

        lua_getglobal(L, "astra_usage");
        luaL_checktype(L, -1, LUA_TFUNCTION);
        lua_call(L, 0, 0);
        return 0;
    }

    int argv_idx = 1;

    lua_rawgeti(L, -1, 1);
    const char *script = luaL_checkstring(L, -1);
    lua_pop(L, 2); // script + argv

    load = load_inscript(L, (const char *)stream, sizeof(stream), "=stream");
    if(load != 0)
        luaL_error(L, "[main] %s", lua_tostring(L, -1));

    if(!strcmp(script, "-"))
    {
        load = luaL_dofile(L, NULL);
        argv_idx += 1;
    }
    else if(!strcmp(script, "--stream"))
    {
        load = 0;
        argv_idx += 1;
    }
    else if(!strcmp(script, "--analyze"))
    {
        load = load_inscript(L, (const char *)analyze, sizeof(analyze), app);
        argv_idx += 1;
    }
    else if(!strcmp(script, "--xproxy"))
    {
        load = load_inscript(L, (const char *)relay, sizeof(relay), app);
        argv_idx += 1;
    }
    else if(!strcmp(script, "--relay"))
    {
        load = load_inscript(L, (const char *)relay, sizeof(relay), app);
        argv_idx += 1;
    }
    else if(!strcmp(script, "--dvbls"))
    {
        load = load_inscript(L, (const char *)dvbls, sizeof(dvbls), app);
        argv_idx += 1;
    }
    else if(!strcmp(script, "--dvbwrite"))
    {
        load = load_inscript(L, (const char *)dvbwrite, sizeof(dvbwrite), app);
        argv_idx += 1;
    }
    else if(!strcmp(script, "--femon"))
    {
        load = load_inscript(L, (const char *)femon, sizeof(femon), app);
        argv_idx += 1;
    }
    else if(!access(script, R_OK))
    {
        load = luaL_dofile(L, script);
        argv_idx += 1;
    }
    if(load != 0)
        luaL_error(L, "[main] %s", lua_tostring(L, -1));

    lua_getglobal(L, "astra_parse_options");
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushinteger(L, argv_idx);
    lua_call(L, 1, 0);

    lua_getglobal(L, "main");
    if(lua_isfunction(L, -1))
        lua_call(L, 0, 0);
    else
        lua_pop(L, 1);

    return 0;
}

MODULE_LUA_BINDING(inscript)
{
    lua_pushcclosure(L, fn_inscript_callback, 0);
    lua_setglobal(L, __module_name);

    return 1;
}
