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

/* hash test strings */
START_TEST(test_vectors)
{
    static const char *const script =
        "local test = {" "\n"
        "    {" "\n"
        "        'abc'," "\n"
        "        '900150983cd24fb0d6963f7d28e17f72'," "\n"
        "    }," "\n"
        "    {" "\n"
        "        'abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq'," "\n"
        "        '8215ef0796a20bcaaae116d3876c664a'," "\n"
        "    }," "\n"
        "    {" "\n"
        "        'The quick brown fox jumps over the lazy dog'," "\n"
        "        '9e107d9d372bb6826bd81d3542a419d6'," "\n"
        "    }," "\n"
        "    {" "\n"
        "        'foo'," "\n"
        "        'acbd18db4cc2f85cedef654fccc4a4d8'," "\n"
        "    }," "\n"
        "    {" "\n"
        "        'foobar'," "\n"
        "        '3858f62230ac3c915f300c664312c63f'," "\n"
        "    }," "\n"
        "}" "\n"
        "for _, v in pairs(test) do" "\n"
        "    local str = v[1]" "\n"
        "    local hash = v[2]" "\n"
        "    assert(((str:md5()):hex()):lower() == hash)" "\n"
        "end" "\n";

    ck_assert_msg(luaL_dostring(L, script) == 0, lua_tostring(L, -1));
}
END_TEST

Suite *lualib_md5(void)
{
    Suite *const s = suite_create("lualib/md5");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);
    tcase_add_test(tc, test_vectors);
    suite_add_tcase(s, tc);

    return s;
}
