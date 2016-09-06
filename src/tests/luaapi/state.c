/*
 * Astra Unit Tests
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

#include "../libastra.h"
#include <astra/luaapi/state.h>

#define L lua

/* directory separator */
START_TEST(dirsep)
{
    lua = lua_api_init();

    lua_getglobal(L, "os");
    ck_assert(lua_istable(L, -1));
    lua_getfield(L, -1, "dirsep");
    ck_assert(lua_isstring(L, -1));
    ck_assert(luaL_len(L, -1) == 1);
#ifdef _WIN32
    ck_assert(!strcmp(lua_tostring(L, -1), "\\"));
#else
    ck_assert(!strcmp(lua_tostring(L, -1), "/"));
#endif
    lua_pop(L, 2);

    ASC_FREE(lua, lua_api_destroy);
}
END_TEST

/* package paths */
START_TEST(pkg_paths)
{
    static const char my_path[] = "ASC_SCRIPTDIR=kitty;puppy;horsey";
    static const char expect[] = "kitty" LUA_DIRSEP "?.lua;"
                                 "puppy" LUA_DIRSEP "?.lua;"
                                 "horsey" LUA_DIRSEP "?.lua;";

    char *const tmp = strdup(my_path);
    ck_assert(tmp != NULL);
    ck_assert(putenv(tmp) == 0);

    lua = lua_api_init();

    lua_getglobal(L, "package");
    ck_assert(lua_istable(L, -1));
    lua_getfield(L, -1, "path");
    ck_assert(lua_isstring(L, -1));
    ck_assert(!strncmp(lua_tostring(L, -1), expect, strlen(expect)));
    lua_pop(L, 1);

    lua_getfield(L, -1, "cpath");
    ck_assert(lua_isstring(L, -1));
    ck_assert(luaL_len(L, -1) == 0);
    ck_assert(!strcmp(lua_tostring(L, -1), ""));
    lua_pop(L, 2);

    ASC_FREE(lua, lua_api_destroy);
}
END_TEST

/* panic handler */
START_TEST(panic)
{
    lib_setup();

    lua_getglobal(L, "does_not_exist");
    lua_call(L, 0, 0);

    ck_abort();
}
END_TEST

Suite *luaapi_state(void)
{
    Suite *const s = suite_create("luaapi/state");

    TCase *const tc = tcase_create("default");

    tcase_add_test(tc, dirsep);
    tcase_add_test(tc, pkg_paths);

    if (can_fork != CK_NOFORK)
        tcase_add_exit_test(tc, panic, EXIT_ABORT);

    suite_add_tcase(s, tc);

    return s;
}
