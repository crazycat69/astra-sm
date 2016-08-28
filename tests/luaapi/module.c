/*
 * Astra: Unit tests
 * http://cesbo.com/astra
 *
 * Copyright (C) 2016, Artem Kharitonov <artem@3phase.pw>
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

#include "../test_libastra.h"
#include <luaapi/module.h>
#include <luaapi/state.h>

/* basic (returns table with methods from manifest) */
struct module_data_t
{
    MODULE_DATA();

    uint8_t *data;
};

static int method_foo(lua_State *L, module_data_t *mod)
{
    ck_assert(module_lua(mod) == L);
    lua_pushstring(L, "foo");
    return 1;
}

static int method_bar(lua_State *L, module_data_t *mod)
{
    ck_assert(module_lua(mod) == L);
    lua_pushinteger(L, 12345);
    return 1;
}

static void module_load(lua_State *L)
{
    lua_getglobal(L, "my_module");
    ck_assert(lua_istable(L, -1));

    const char *const str = luaL_tolstring(L, -1, NULL);
    ck_assert(str != NULL && !strcmp(str, "my_module"));

    lua_pop(L, 2);
}

static bool basic_inited;

static void module_init(lua_State *L, module_data_t *mod)
{
    ck_assert(module_lua(mod) == L);
    ck_assert(lua_istable(L, -1));

    if (!lua_istable(L, MODULE_OPTIONS_IDX))
        return;

    bool error = false;
    if (module_option_boolean(L, "error", &error) && error)
        luaL_error(L, "init error");

    bool stack = false;
    if (module_option_boolean(L, "stack", &stack) && stack)
    {
        lua_settop(L, 0);
        return;
    }

    int opt_int = 0;
    const char *opt_str = NULL;
    bool opt_bool = false;
    size_t len = 0;

    /* non-existent options */
    ck_assert(!module_option_integer(L, "nothing", &opt_int));
    ck_assert(!module_option_string(L, "nothing", &opt_str, NULL));
    ck_assert(!module_option_boolean(L, "nothing", &opt_bool));

    /* booleans */
    ck_assert(module_option_boolean(L, "bool_1", &opt_bool) && !opt_bool);
    ck_assert(module_option_boolean(L, "bool_2", &opt_bool) && opt_bool);
    ck_assert(module_option_boolean(L, "bool_3", &opt_bool) && !opt_bool);
    ck_assert(module_option_boolean(L, "bool_4", &opt_bool) && opt_bool);
    ck_assert(module_option_boolean(L, "bool_5", &opt_bool) && opt_bool);

    /* integers */
    ck_assert(module_option_integer(L, "int_1", &opt_int) && opt_int == 42);
    ck_assert(module_option_integer(L, "int_2", &opt_int) && opt_int == 1234);
    ck_assert(module_option_integer(L, "int_3", &opt_int) && opt_int == 1);
    ck_assert(module_option_integer(L, "int_4", &opt_int) && opt_int == 5123);

    /* strings */
    ck_assert(module_option_string(L, "str_1", &opt_str, NULL)
              && !strcmp(opt_str, "false"));
    ck_assert(module_option_string(L, "str_2", &opt_str, &len)
              && !strcmp(opt_str, "test") && len == strlen(opt_str));
    ck_assert(module_option_string(L, "str_3", &opt_str, &len)
              && !strcmp(opt_str, "12.34") && len == strlen(opt_str));
    ck_assert(module_option_string(L, "str_4", &opt_str, &len)
              && !strcmp(opt_str, "9001") && len == strlen(opt_str));
    ck_assert(module_option_string(L, "str_5", &opt_str, &len)
              && !strcmp(opt_str, "true") && len == strlen(opt_str));

    /* check __options */
    ck_assert(lua_gettop(L) == 3);
    /*
     * 1 = parent table
     * 2 = module options
     * 3 = new instance
     */
    lua_getfield(L, -1, "__options");
    ck_assert(lua_istable(L, -1));
    lua_pop(L, 1);

    mod->data = ASC_ALLOC(128, uint8_t);
    basic_inited = true;
}

static bool basic_destroyed;

static void module_destroy(module_data_t *mod)
{
    ASC_FREE(mod->data, free);
    basic_destroyed = true;
}

static const module_method_t module_methods[] =
{
    { "foo", method_foo },
    { "bar", method_bar },
    { NULL, NULL },
};

MODULE_REGISTER(my_module)
{
    .load = module_load,
    .init = module_init,
    .destroy = module_destroy,
    .methods = module_methods,
};

START_TEST(basic)
{
    lua_State *const L = lua;

    module_register(L, &MODULE_SYMBOL(my_module));
    ck_assert(lua_gettop(L) == 0);

    /* create module instance */
    basic_inited = false;
    basic_destroyed = false;
    lua_getglobal(L, "my_module");
    ck_assert(lua_istable(L, -1));

    lua_newtable(L);
    /* boolean */
    lua_pushboolean(L, 0);      /* bool_1 = false */
    lua_setfield(L, -2, "bool_1");
    lua_pushinteger(L, 100);    /* bool_2 = 100 */
    lua_setfield(L, -2, "bool_2");
    lua_pushstring(L, "false"); /* bool_3 = "false" */
    lua_setfield(L, -2, "bool_3");
    lua_pushstring(L, "on");    /* bool_4 = "on" */
    lua_setfield(L, -2, "bool_4");
    lua_pushstring(L, "1");     /* bool_5 = "1" */
    lua_setfield(L, -2, "bool_5");
    /* integer */
    lua_pushnumber(L, 42.5);    /* int_1 = 42.5 */
    lua_setfield(L, -2, "int_1");
    lua_pushstring(L, "1234");  /* int_2 = "1234" */
    lua_setfield(L, -2, "int_2");
    lua_pushboolean(L, 1);      /* int_3 = true */
    lua_setfield(L, -2, "int_3");
    lua_pushinteger(L, 5123);   /* int_4 = 5123 */
    lua_setfield(L, -2, "int_4");
    /* string */
    lua_pushboolean(L, 0);      /* str_1 = false */
    lua_setfield(L, -2, "str_1");
    lua_pushstring(L, "test");  /* str_2 = "test" */
    lua_setfield(L, -2, "str_2");
    lua_pushnumber(L, 12.34);   /* str_3 = 12.34 */
    lua_setfield(L, -2, "str_3");
    lua_pushinteger(L, 9001);   /* str_4 = 9001 */
    lua_setfield(L, -2, "str_4");
    lua_pushboolean(L, 1);      /* str_5 = true */
    lua_setfield(L, -2, "str_5");

    ck_assert(lua_pcall(L, 1, 1, 0) == 0);
    ck_assert(lua_istable(L, -1));
    ck_assert(basic_inited == true);

    /* init should have converted bools and numbers to strings */
    lua_getfield(L, -1, "__options");
    ck_assert(lua_istable(L, -1));
    /* str_1 */
    lua_getfield(L, -1, "str_1");
    ck_assert(lua_isstring(L, -1));
    ck_assert(!strcmp(lua_tostring(L, -1), "false"));
    lua_pop(L, 1);
    /* str_3 */
    lua_getfield(L, -1, "str_3");
    ck_assert(lua_isstring(L, -1));
    ck_assert(!strcmp(lua_tostring(L, -1), "12.34"));
    lua_pop(L, 1);
    /* str_4 */
    lua_getfield(L, -1, "str_4");
    ck_assert(lua_isstring(L, -1));
    ck_assert(!strcmp(lua_tostring(L, -1), "9001"));
    lua_pop(L, 1);
    /* str_5 */
    lua_getfield(L, -1, "str_5");
    ck_assert(lua_isstring(L, -1));
    ck_assert(!strcmp(lua_tostring(L, -1), "true"));
    lua_pop(L, 2);

    lua_setglobal(L, "my_var");

    /* call methods */
    lua_getglobal(L, "my_var");
    /* foo() */
    lua_getfield(L, -1, "foo");
    ck_assert(lua_isfunction(L, -1));
    ck_assert(lua_pcall(L, 0, 1, 0) == 0);
    ck_assert(lua_isstring(L, -1));
    ck_assert(!strcmp(lua_tostring(L, -1), "foo"));
    lua_pop(L, 1);
    /* bar() */
    lua_getfield(L, -1, "bar");
    ck_assert(lua_isfunction(L, -1));
    ck_assert(lua_pcall(L, 0, 1, 0) == 0);
    ck_assert(lua_isnumber(L, -1));
    ck_assert(lua_tointeger(L, -1) == 12345);
    lua_pop(L, 2);

    /* destroy instance */
    lua_pushnil(L);
    lua_setglobal(L, "my_var");
    lua_gc(lua, LUA_GCCOLLECT, 0);
    ck_assert(basic_destroyed == true);
}
END_TEST

/* check gc on failed init */
START_TEST(basic_error)
{
    lua_State *const L = lua;

    module_register(L, &MODULE_SYMBOL(my_module));
    ck_assert(lua_gettop(L) == 0);

    /* lua error */
    basic_inited = basic_destroyed = false;
    lua_getglobal(L, "my_module");
    ck_assert(lua_gettop(L) == 1);

    lua_newtable(L);
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "error");
    ck_assert(lua_gettop(L) == 2);

    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    ck_assert(lua_gettop(L) == 1);
    ck_assert(lua_isstring(L, -1));
    lua_gc(L, LUA_GCCOLLECT, 0);
    ck_assert(basic_inited == false);
    ck_assert(basic_destroyed == true);
    lua_pop(L, 1);

    /* kill stack on init */
    basic_inited = basic_destroyed = false;
    lua_getglobal(L, "my_module");
    ck_assert(lua_gettop(L) == 1);

    lua_newtable(L);
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "stack");
    ck_assert(lua_gettop(L) == 2);

    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    ck_assert(lua_gettop(L) == 1);
    ck_assert(lua_isstring(L, -1));
    lua_gc(L, LUA_GCCOLLECT, 0);
    ck_assert(basic_inited == false);
    ck_assert(basic_destroyed == true);
    lua_pop(L, 1);
}
END_TEST

/* pass none or more than one argument to constructor */
START_TEST(basic_extra)
{
    lua_State *const L = lua;

    module_register(L, &MODULE_SYMBOL(my_module));
    ck_assert(lua_gettop(L) == 0);

    /* three args */
    basic_destroyed = false;
    lua_getglobal(L, "my_module");
    ck_assert(lua_gettop(L) == 1);
    lua_pushstring(L, "test option");
    lua_pushnil(L);
    lua_pushboolean(L, 0);
    ck_assert(lua_gettop(L) == 4);
    ck_assert(lua_pcall(L, 3, 1, 0) == 0);
    ck_assert(lua_gettop(L) == 1);
    ck_assert(lua_istable(L, -1));

    lua_getfield(L, -1, "__options");
    ck_assert(lua_isstring(L, -1));
    ck_assert(!strcmp(lua_tostring(L, -1), "test option"));

    lua_pop(L, 2);
    lua_gc(L, LUA_GCCOLLECT, 0);
    ck_assert(basic_destroyed == true);

    /* zero args */
    basic_destroyed = false;
    lua_getglobal(L, "my_module");
    ck_assert(lua_gettop(L) == 1);
    ck_assert(lua_pcall(L, 0, 1, 0) == 0);
    ck_assert(lua_gettop(L) == 1);
    ck_assert(lua_istable(L, -1));

    /* NOTE: no args = no __options */
    lua_getfield(L, -1, "__options");
    ck_assert(lua_isnil(L, -1));

    lua_pop(L, 2);
    lua_gc(L, LUA_GCCOLLECT, 0);
    ck_assert(basic_destroyed == true);
}
END_TEST

/* binding (calls function on init, no instantiation) */
static bool my_lib_loaded;

static void bind_load(lua_State *L)
{
    __uarg(L);
    my_lib_loaded = true;
}

BINDING_REGISTER(my_lib)
{
    .load = bind_load,
};

START_TEST(binding)
{
    lua_State *const L = lua;

    my_lib_loaded = false;
    module_register(L, &MODULE_SYMBOL(my_lib));
    ck_assert(my_lib_loaded == true);

    lua_getglobal(L, "my_lib");
    ck_assert(lua_isnil(L, -1));
    lua_pop(L, 1);
}
END_TEST

Suite *luaapi_module(void)
{
    Suite *const s = suite_create("luaapi/module");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);
    tcase_add_test(tc, basic);
    tcase_add_test(tc, basic_error);
    tcase_add_test(tc, basic_extra);
    tcase_add_test(tc, binding);
    suite_add_tcase(s, tc);

    return s;
}
