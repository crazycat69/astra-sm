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

#include "../../libastra.h"
#include <astra/luaapi/state.h>

#define L lua

/* get hostname */
START_TEST(get_hostname)
{
    lua_getglobal(L, "utils");
    ck_assert(lua_istable(L, -1));
    lua_getfield(L, -1, "hostname");
    ck_assert(lua_isfunction(L, -1));
    ck_assert(lua_pcall(L, 0, 1, 0) == 0);
    ck_assert(lua_gettop(L) == 2);
    ck_assert(lua_isstring(L, -1));
    ck_assert(luaL_len(L, -1) > 0);
    lua_pop(L, 2);
}
END_TEST

Suite *luaapi_lib_utils(void)
{
    Suite *const s = suite_create("luaapi/lib/utils");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);
    tcase_add_test(tc, get_hostname);
    suite_add_tcase(s, tc);

    return s;
}
