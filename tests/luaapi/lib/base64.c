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

/* encode and decode test strings */
START_TEST(test_vectors)
{
    static const char *const script =
        "local test = {" "\n"
        "    { '', '' }," "\n"
        "    { 'f', 'Zg==' }," "\n"
        "    { 'fo', 'Zm8=' }," "\n"
        "    { 'foo', 'Zm9v' }," "\n"
        "    { 'foob', 'Zm9vYg==' }," "\n"
        "    { 'fooba', 'Zm9vYmE=' }," "\n"
        "    { 'foobar', 'Zm9vYmFy' }," "\n"
        "}" "\n"
        "for _, v in pairs(test) do" "\n"
        "    local text = v[1]" "\n"
        "    local b64 = v[2]" "\n"
        "    assert(text:b64e() == b64)" "\n"
        "    assert(base64.encode(text) == b64)" "\n"
        "    assert(b64:b64d() == text)" "\n"
        "    assert(base64.decode(b64) == text)" "\n"
        "end" "\n";

    ck_assert_msg(luaL_dostring(L, script) == 0, lua_tostring(L, -1));
}
END_TEST

Suite *luaapi_lib_base64(void)
{
    Suite *const s = suite_create("luaapi/lib/base64");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);
    tcase_add_test(tc, test_vectors);
    suite_add_tcase(s, tc);

    return s;
}
