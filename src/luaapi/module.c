/*
 * Astra Lua API (Module)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *               2015-2016, Artem Kharitonov <artem@3phase.pw>
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
#include <luaapi/stream.h>

struct module_data_t
{
    /*
     * NOTE: data structs in all modules MUST begin with the following
     *       members. Use MODULE_DATA() macro when defining module
     *       structs to add appropriate size padding.
     */
    const module_manifest_t *manifest;
    lua_State *lua;
};

/*
 * module instance init and cleanup
 */

#define GET_MANIFEST(_L) \
    ((module_manifest_t *)lua_touserdata(_L, lua_upvalueindex(1)))

#define GET_MODULE_DATA(_L) \
    ((module_data_t *)lua_touserdata(L, lua_upvalueindex(2)))

static
int callback_thunk(lua_State *L)
{
    void *const mod = lua_touserdata(L, lua_upvalueindex(1));
    void *const method = lua_touserdata(L, lua_upvalueindex(2));

    return ((module_method_t *)method)->func(L, (module_data_t *)mod);
}

static
void add_methods(lua_State *L, const module_data_t *mod
                 , const module_method_t *list)
{
    while (list->name != NULL)
    {
        lua_pushlightuserdata(L, (void *)mod);
        lua_pushlightuserdata(L, (void *)list);
        lua_pushcclosure(L, callback_thunk, 2);
        lua_setfield(L, -2, list->name);

        list++;
    }
}

static
int method_tostring(lua_State *L)
{
    lua_pushstring(L, GET_MANIFEST(L)->name);
    return 1;
}

static
int method_gc(lua_State *L)
{
    const module_manifest_t *const manifest = GET_MANIFEST(L);
    module_data_t *const mod = GET_MODULE_DATA(L);

    if (manifest->reg->destroy != NULL)
        manifest->reg->destroy(mod);

    free(mod);

    return 0;
}

static
int method_new(lua_State *L)
{
    const module_manifest_t *const manifest = GET_MANIFEST(L);

    /* create table representing module instance */
    module_data_t *const mod = (module_data_t *)asc_calloc(1, manifest->size);
    lua_newtable(L);

    /* set up metatable */
    static const luaL_Reg meta_methods[] =
    {
        { "__gc", method_gc },
        { "__tostring", method_tostring },
        { NULL, NULL },
    };

    lua_newtable(L);
    lua_pushlightuserdata(L, (void *)manifest);
    lua_pushlightuserdata(L, mod);
    luaL_setfuncs(L, meta_methods, 2);
    lua_setmetatable(L, -2);

    /* set up user methods */
    if (manifest->reg->methods != NULL)
        add_methods(L, mod, manifest->reg->methods);

    if (manifest->type == MODULE_TYPE_STREAM)
        add_methods(L, mod, module_stream_methods);

    /* set up options table */
    if (lua_gettop(L) == 3)
    {
        lua_pushvalue(L, MODULE_OPTIONS_IDX);
        lua_setfield(L, 3, "__options");
    }

    /* run module-specific initialization */
    mod->manifest = manifest;
    mod->lua = L;

    if (manifest->reg->init != NULL)
        manifest->reg->init(L, mod);

    /* pass new table to Lua */
    return 1;
}

void module_register(lua_State *L, const module_manifest_t *manifest)
{
    if (manifest->type == MODULE_TYPE_BASIC
        || manifest->type == MODULE_TYPE_STREAM)
    {
        static const luaL_Reg meta_methods[] =
        {
            { "__call", method_new },
            { "__tostring", method_tostring },
            { NULL, NULL },
        };

        lua_newtable(L);

        lua_newtable(L);
        lua_pushlightuserdata(L, (void *)manifest);
        luaL_setfuncs(L, meta_methods, 1);
        lua_setmetatable(L, -2);

        lua_setglobal(L, manifest->name);
    }

    if (manifest->reg->load != NULL)
        manifest->reg->load(L);
}

lua_State *module_lua(const module_data_t *mod)
{
    return mod->lua;
}

/*
 * module option getters
 */

bool module_option_integer(lua_State *L, const char *name, int *integer)
{
    if (lua_type(L, MODULE_OPTIONS_IDX) != LUA_TTABLE)
        return false;

    lua_getfield(L, MODULE_OPTIONS_IDX, name);
    const int type = lua_type(L, -1);
    bool result = false;

    if (type == LUA_TNUMBER)
    {
        *integer = lua_tointeger(L, -1);
        result = true;
    }
    else if (type == LUA_TSTRING)
    {
        const char *str = lua_tostring(L, -1);
        *integer = atoi(str);
        result = true;
    }
    else if (type == LUA_TBOOLEAN)
    {
        *integer = lua_toboolean(L, -1);
        result = true;
    }

    lua_pop(L, 1);
    return result;
}

bool module_option_string(lua_State *L, const char *name, const char **string
                          , size_t *length)
{
    if (lua_type(L, MODULE_OPTIONS_IDX) != LUA_TTABLE)
        return false;

    lua_getfield(L, MODULE_OPTIONS_IDX, name);
    const int type = lua_type(L, -1);
    bool result = false;

    if (type == LUA_TSTRING || type == LUA_TNUMBER)
    {
        *string = lua_tolstring(L, -1, length);
        result = true;
    }
    else if (type == LUA_TBOOLEAN)
    {
        /* convert to string */
        lua_pushstring(L, (lua_toboolean(L, -1) ? "true" : "false"));
        *string = lua_tolstring(L, -1, length);
        lua_setfield(L, MODULE_OPTIONS_IDX, name);
        result = true;
    }

    lua_pop(L, 1);
    return result;
}

bool module_option_boolean(lua_State *L, const char *name, bool *boolean)
{
    if (lua_type(L, MODULE_OPTIONS_IDX) != LUA_TTABLE)
        return false;

    lua_getfield(L, MODULE_OPTIONS_IDX, name);
    const int type = lua_type(L, -1);
    bool result = false;

    if (type == LUA_TNUMBER)
    {
        *boolean = (lua_tointeger(L, -1) != 0) ? true : false;
        result = true;
    }
    else if (type == LUA_TSTRING)
    {
        const char *str = lua_tostring(L, -1);
        *boolean = (!strcmp(str, "true")
                    || !strcmp(str, "on")
                    || !strcmp(str, "1"));
        result = true;
    }
    else if (type == LUA_TBOOLEAN)
    {
        *boolean = lua_toboolean(L, -1);
        result = true;
    }

    lua_pop(L, 1);
    return result;
}
