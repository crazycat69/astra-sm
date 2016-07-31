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

#include "../../test_libastra.h"
#include <luaapi/state.h>

#define L lua

static void check_pid(const char *filename)
{
    ck_assert(access(filename, R_OK) == 0);

    char pidstr[512] = { 0 };
    const int ret = snprintf(pidstr, sizeof(pidstr)
                             , "%lld\n", (long long)getpid());
    ck_assert(ret > 0);
    ck_assert(strlen(pidstr) > 0);

    char buf[512] = { 0 };
    FILE *const f = fopen(filename, "r");
    ck_assert(f != NULL);
    ck_assert(fgets(buf, sizeof(buf), f) == buf);
    ck_assert(strlen(buf) > 0);
    ck_assert(!strcmp(pidstr, buf));
    ck_assert(fclose(f) == 0);
}

static void check_no_pid(const char *filename)
{
    ck_assert(access(filename, F_OK) != 0);
    ck_assert(errno == ENOENT);
}

/* empty argument (should fail) */
START_TEST(no_arg_c)
{
    lua_getglobal(L, "pidfile");
    ck_assert(lua_istable(L, -1));

    ck_assert(lua_pcall(L, 0, 0, 0) != 0);
}
END_TEST

START_TEST(no_arg_lua)
{
    static const char *const script =
        "local ret = pcall(pidfile)" "\n"
        "if ret ~= false then error('expected failure') end" "\n";

    ck_assert_msg(luaL_dostring(L, script) == 0, lua_tostring(L, -1));
}
END_TEST

/* create pidfile twice */
START_TEST(twice_c)
{
    lua_getglobal(L, "pidfile");
    lua_pushstring(L, "test.pid");
    ck_assert_msg(lua_pcall(L, 1, 0, 0) == 0, lua_tostring(L, -1));
    check_pid("test.pid");

    lua_getglobal(L, "pidfile");
    lua_pushstring(L, "test2.pid");
    ck_assert(lua_pcall(L, 1, 0, 0) != 0);
    check_no_pid("test2.pid");
}
END_TEST

START_TEST(twice_lua)
{
    static const char *const script =
        "local ret = pcall(pidfile, 'test.pid')" "\n"
        "assert(ret == true)" "\n"
        "ret = pcall(pidfile, 'test2.pid')" "\n"
        "assert(ret == false)" "\n";

    ck_assert_msg(luaL_dostring(L, script) == 0, lua_tostring(L, -1));

    check_pid("test.pid");
    check_no_pid("test2.pid");
}
END_TEST

/* pidfile removal on garbage collection */
START_TEST(gc_c)
{
    lua_getglobal(L, "pidfile");
    lua_pushstring(L, "test.pid");
    ck_assert(lua_pcall(L, 1, 0, 0) == 0);
    check_pid("test.pid");

    /* remove pidfile function and run gc */
    lua_pushnil(L);
    lua_setglobal(L, "pidfile");
    lua_gc(lua, LUA_GCCOLLECT, 0);
    check_no_pid("test.pid");
}
END_TEST

START_TEST(gc_lua)
{
    static const char *const script =
        "local ret = pcall(pidfile, 'test.pid')" "\n"
        "assert(ret == true)" "\n"
        "local f = assert(io.open('test.pid'))" "\n"
        "f:close()" "\n"
        "pidfile = nil" "\n"
        "collectgarbage()" "\n";

    ck_assert_msg(luaL_dostring(L, script) == 0, lua_tostring(L, -1));
    check_no_pid("test.pid");
}
END_TEST

/* close pidfile */
START_TEST(close_c)
{
    /* open */
    lua_getglobal(L, "pidfile");
    lua_pushstring(L, "test.pid");
    ck_assert(lua_pcall(L, 1, 0, 0) == 0);
    check_pid("test.pid");

    /* close */
    lua_getglobal(L, "pidfile");
    lua_getfield(L, -1, "close");
    ck_assert(lua_isfunction(L, -1));
    ck_assert(lua_pcall(L, 0, 0, 0) == 0);
    check_no_pid("test.pid");

    /* reopen */
    lua_getglobal(L, "pidfile");
    lua_pushstring(L, "test2.pid");
    ck_assert(lua_pcall(L, 1, 0, 0) == 0);
    check_pid("test2.pid");
}
END_TEST

START_TEST(close_lua)
{
    static const char *const script =
        "assert(pcall(pidfile, 'test.pid') == true)" "\n"
        "assert(io.open('test.pid')):close()" "\n"
        "pidfile.close()" "\n"
        "assert(io.open('test.pid') == nil)" "\n"
        "assert(pcall(pidfile, 'test2.pid') == true)" "\n"
        "assert(io.open('test2.pid')):close()" "\n";

    ck_assert_msg(luaL_dostring(L, script) == 0, lua_tostring(L, -1));
    check_pid("test2.pid");
}
END_TEST

/* overwrite pidfile */
START_TEST(overwrite)
{
    static const char buf[] = "TEST TEST TEST\n";

    FILE *const f = fopen("test.pid", "w");
    ck_assert(f != NULL);
    ck_assert(fwrite(buf, strlen(buf), 1, f) == 1);
    ck_assert(fclose(f) == 0);

    lua_getglobal(L, "pidfile");
    lua_pushstring(L, "test.pid");
    ck_assert(lua_pcall(L, 1, 0, 0) == 0);
    check_pid("test.pid");
}
END_TEST

Suite *luaapi_lib_pidfile(void)
{
    Suite *const s = suite_create("luaapi/lib/pidfile");

    TCase *const tc_c = tcase_create("from_c");
    tcase_add_checked_fixture(tc_c, lib_setup, lib_teardown);
    tcase_add_test(tc_c, no_arg_c);
    tcase_add_test(tc_c, twice_c);
    tcase_add_test(tc_c, gc_c);
    tcase_add_test(tc_c, close_c);
    tcase_add_test(tc_c, overwrite);
    suite_add_tcase(s, tc_c);

    TCase *const tc_lua = tcase_create("from_lua");
    tcase_add_checked_fixture(tc_lua, lib_setup, lib_teardown);
    tcase_add_test(tc_lua, no_arg_lua);
    tcase_add_test(tc_lua, twice_lua);
    tcase_add_test(tc_lua, gc_lua);
    tcase_add_test(tc_lua, close_lua);
    suite_add_tcase(s, tc_lua);

    return s;
}
