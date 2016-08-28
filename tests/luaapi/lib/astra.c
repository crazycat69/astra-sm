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
#include <core/mainloop.h>
#include <luaapi/state.h>

#define L lua

/* check version data */
START_TEST(version_data)
{
    static const char *const script =
#ifdef DEBUG
        "local dbg = true\n"
#else
        "local dbg = false\n"
#endif
        "assert(type(astra.debug) == 'boolean' and astra.debug == dbg)\n"
        "assert(type(astra.fullname) == 'string' and #astra.fullname > 0)\n"
        "assert(type(astra.package) == 'string' and #astra.package > 0)\n"
        "assert(type(astra.version) == 'string' and #astra.version > 0)\n";

    ck_assert_msg(luaL_dostring(L, script) == 0, lua_tostring(L, -1));
}
END_TEST

/* test main loop controls */
#define ARG_SHUTDOWN (void *)0x100
#define ARG_RELOAD (void *)0x200

static void loop_proc(void *arg)
{
    lua_getglobal(L, "astra");
    ck_assert(lua_istable(L, -1));
    if (arg == ARG_SHUTDOWN)
        lua_getfield(L, -1, "shutdown");
    else
        lua_getfield(L, -1, "reload");

    ck_assert(lua_isfunction(L, -1));
    ck_assert(lua_pcall(L, 0, 0, 0) == 0);
}

START_TEST(astra_loopctl)
{
    asc_job_queue(NULL, loop_proc, ARG_SHUTDOWN);
    ck_assert(asc_main_loop_run() == false);

    asc_job_queue(NULL, loop_proc, ARG_RELOAD);
    ck_assert(asc_main_loop_run() == true);
}
END_TEST

/* test abort */
START_TEST(astra_abort)
{
    lua_getglobal(L, "astra");
    lua_getfield(L, -1, "abort");
    lua_pcall(L, 0, 0, 0);
    ck_abort_msg("didn't expect to reach this code");
}
END_TEST

/* test immediate exit */
#define TEST_EXIT_CODE 42

START_TEST(astra_exit)
{
    lua_getglobal(L, "astra");
    lua_getfield(L, -1, "exit");
    lua_pushinteger(L, TEST_EXIT_CODE);
    lua_pcall(L, 1, 0, 0);
    ck_abort_msg("didn't expect to reach this code");
}
END_TEST

Suite *luaapi_lib_astra(void)
{
    Suite *const s = suite_create("luaapi/lib/astra");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);
    tcase_add_test(tc, version_data);
    tcase_add_test(tc, astra_loopctl);

    if (can_fork != CK_NOFORK)
    {
        tcase_add_exit_test(tc, astra_abort, EXIT_ABORT);
        tcase_add_exit_test(tc, astra_exit, TEST_EXIT_CODE);
    }

    suite_add_tcase(s, tc);

    return s;
}
