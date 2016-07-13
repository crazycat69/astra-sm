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

/* pre-defined test strings */
START_TEST(test_vectors)
{
    static const char *const script =
        "local test = {" "\n"
        "    {" "\n"
        "        ''," "\n"
        "        ''," "\n"
        "    }," "\n"
        "    {" "\n"
        "        'foo'," "\n"
        "        '666f6f'," "\n"
        "    }," "\n"
        "    {" "\n"
        "        'bar'," "\n"
        "        '626172'," "\n"
        "    }," "\n"
        "    {" "\n"
        "        'foobar'," "\n"
        "        '666f6f626172'," "\n"
        "    }," "\n"
        "    {" "\n"
        "        \"foo\\0bar\"," "\n"
        "        '666f6f00626172'," "\n"
        "    }," "\n"
        "}" "\n"
        "for _, v in pairs(test) do" "\n"
        "    assert((v[1]:hex()):lower() == v[2])" "\n"
        "    assert(string.hex(v[1]):lower() == v[2])" "\n"
        "    assert(v[2]:bin() == v[1])" "\n"
        "    assert(string.bin(v[2]) == v[1])" "\n"
        "end" "\n";

    ck_assert_msg(luaL_dostring(L, script) == 0, lua_tostring(L, -1));
}
END_TEST

Suite *luaapi_lib_strhex(void)
{
    Suite *const s = suite_create("luaapi/lib/strhex");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);
    tcase_add_test(tc, test_vectors);
    suite_add_tcase(s, tc);

    return s;
}
