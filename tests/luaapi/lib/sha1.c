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

/* encrypt and decrypt test strings */
START_TEST(test_vectors)
{
    static const char *const script =
        "local test = {" "\n"
        "    {" "\n"
        "        ''," "\n"
        "        'da39a3ee5e6b4b0d3255bfef95601890afd80709'," "\n"
        "    }," "\n"
        "    {" "\n"
        "        'abc'," "\n"
        "        'a9993e364706816aba3e25717850c26c9cd0d89d'," "\n"
        "    }," "\n"
        "    {" "\n"
        "        'abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq'," "\n"
        "        '84983e441c3bd26ebaae4aa1f95129e5e54670f1'," "\n"
        "    }," "\n"
        "    {" "\n"
        "        'abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu'," "\n"
        "        'a49b2446a02c645bf419f995b67091253a04a259'," "\n"
        "    }," "\n"
        "}" "\n"
        "for _, v in pairs(test) do" "\n"
        "    assert(((v[1]:sha1()):hex()):lower() == v[2])" "\n"
        "end" "\n";

    ck_assert_msg(luaL_dostring(L, script) == 0, lua_tostring(L, -1));
}
END_TEST

Suite *luaapi_lib_sha1(void)
{
    Suite *const s = suite_create("luaapi/lib/sha1");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);
    tcase_add_test(tc, test_vectors);
    suite_add_tcase(s, tc);

    return s;
}
