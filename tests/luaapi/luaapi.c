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
#include <luaapi/state.h>

/* pcall() with stack trace dump */
#define FUZZ_ITERATIONS 1000
#define FUZZ_MAX_PRE  5
#define FUZZ_MAX_ARGS 8
#define FUZZ_MAX_RETS 8

static int func_fail(lua_State *L)
{
    luaL_error(L, "error");
    return 0;
}

static int func_noarg(lua_State *L)
{
    ck_assert(lua_gettop(L) == 0);
    lua_pushlightuserdata(L, (void *)0x1234);
    return 1;
}

static int func_arg(lua_State *L)
{
    ck_assert(lua_gettop(L) == 1);
    ck_assert(lua_isstring(L, -1));
    lua_pushinteger(L, 1234);
    return 2;
}

static int func_fuzz(lua_State *L)
{
    const int nrets = rand() % FUZZ_MAX_RETS;
    for (int i = 0; i < nrets; i++)
    {
        switch (rand() % 5)
        {
            case 0:
                lua_pushnil(L);
                break;

            case 1:
                lua_pushstring(L, "test");
                break;

            case 2:
                lua_pushinteger(L, rand());
                break;

            case 3:
                lua_pushlightuserdata(L, NULL);
                break;

            default:
                luaL_error(L, "foobar");
                break;
        }
    }

    return nrets;
}

START_TEST(trace_call)
{
    lua_State *const L = lua;
    int ret;

    /* error */
    ck_assert(lua_gettop(L) == 0);
    lua_pushcfunction(L, func_fail);
    ret = lua_tr_call(L, 0, 0);
    ck_assert(ret == LUA_ERRRUN);
    ck_assert(lua_gettop(L) == 1);
    ck_assert(lua_istable(L, -1));
    lua_pop(L, 1);

    /* no arguments, 1 return value */
    ck_assert(lua_gettop(L) == 0);
    lua_pushcfunction(L, func_noarg);
    ret = lua_tr_call(L, 0, 1);
    ck_assert(ret == 0);
    ck_assert(lua_gettop(L) == 1);
    ck_assert(lua_type(L, -1) == LUA_TLIGHTUSERDATA);
    ck_assert(lua_touserdata(L, -1) == (void *)0x1234);
    lua_pop(L, 1);

    /* 1 argument, 2 return values */
    ck_assert(lua_gettop(L) == 0);
    lua_pushcfunction(L, func_arg);
    lua_pushstring(L, "argument");
    ret = lua_tr_call(L, 1, 2);
    ck_assert(ret == 0);
    ck_assert(lua_gettop(L) == 2);
    ck_assert(lua_isnumber(L, -1));
    ck_assert(lua_tointeger(L, -1) == 1234);
    ck_assert(lua_isstring(L, -2));
    ck_assert(!strcmp(lua_tostring(L, -2), "argument"));
    lua_pop(L, 2);

    /* random number of args, retvals and errors */
    asc_srand();
    for (size_t i = 0; i < FUZZ_ITERATIONS; i++)
    {
        /* push pre-existing elements */
        ck_assert(lua_gettop(L) == 0);
        const int pre = rand() % FUZZ_MAX_PRE;
        for (int j = 0; j < pre; j++)
            lua_pushinteger(L, rand());

        ck_assert(lua_gettop(L) == pre);

        /* call function */
        const int top = lua_gettop(L);
        const int nargs = rand() % FUZZ_MAX_ARGS;

        lua_pushcfunction(L, func_fuzz);
        for (int j = 0; j < nargs; j++)
        {
            if (rand() % 2)
                lua_pushinteger(L, rand());
            else
                lua_pushstring(L, "foobar");
        }

        ret = lua_tr_call(L, nargs, LUA_MULTRET);
        if (ret != 0)
        {
            ck_assert(lua_istable(L, -1));
            lua_pop(L, 1);
        }
        else
        {
            const int nrets = lua_gettop(L) - top;
            if (nrets > 0)
                lua_pop(L, nrets);
        }

        /* pop pre-existing elements */
        ck_assert(lua_gettop(L) == pre);
        lua_pop(L, pre);
    }

    ck_assert(lua_gettop(L) == 0);
}
END_TEST

/* send topmost element on the stack to error log */
START_TEST(error_logger)
{
    lua_State *const L = lua;
    ck_assert(lua_gettop(L) == 0);

    /* empty stack */
    lua_err_log(L);
    ck_assert(lua_gettop(L) == 0);

    /* table */
    lua_newtable(L);
    lua_pushinteger(L, 1);
    lua_pushstring(L, "lua string on index 1");
    lua_settable(L, -3);
    lua_pushinteger(L, 2);
    lua_pushstring(L, "lua string on index 2");
    lua_settable(L, -3);
    ck_assert(lua_gettop(L) == 1);
    lua_err_log(L);
    ck_assert(lua_gettop(L) == 0);

    /* string */
    lua_pushinteger(L, 1000); /* pre-existing elements */
    lua_pushinteger(L, 2000);
    lua_pushstring(L, "lua string");
    ck_assert(lua_gettop(L) == 3);
    lua_err_log(L);
    ck_assert(lua_gettop(L) == 2);
    ck_assert(lua_isnumber(L, -1) && lua_isnumber(L, -2));
    ck_assert(lua_tointeger(L, -1) == 2000 && lua_tointeger(L, -2) == 1000);
    lua_pop(L, 2);

    /* nil */
    lua_pushnil(L);
    ck_assert(lua_gettop(L) == 1);
    lua_err_log(L);
    ck_assert(lua_gettop(L) == 0);

    /* lightuserdata */
    lua_pushboolean(L, 1); /* pre-existing element */
    lua_pushlightuserdata(L, NULL);
    ck_assert(lua_gettop(L) == 2);
    lua_err_log(L);
    ck_assert(lua_gettop(L) == 1);
    ck_assert(lua_isboolean(L, -1) && lua_toboolean(L, -1) == 1);
    lua_pop(L, 1);
}
END_TEST

Suite *luaapi_luaapi(void)
{
    Suite *const s = suite_create("luaapi/luaapi");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);
    tcase_add_test(tc, trace_call);
    tcase_add_test(tc, error_logger);
    suite_add_tcase(s, tc);

    return s;
}
