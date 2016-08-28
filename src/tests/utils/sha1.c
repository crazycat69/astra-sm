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
#include <astra/utils/sha1.h>

/* pre-defined test strings */
typedef struct
{
    const char *msg;
    size_t repeat;
    const char *hash;
} sha1_test_t;

static const sha1_test_t test_strings[] =
{
    {
        "",
        1,

        "\xda\x39\xa3\xee\x5e\x6b\x4b\x0d\x32\x55\xbf\xef\x95\x60"
        "\x18\x90\xaf\xd8\x07\x09",
    },
    {
        "abc",
        1,

        "\xa9\x99\x3e\x36\x47\x06\x81\x6a\xba\x3e\x25\x71\x78\x50"
        "\xc2\x6c\x9c\xd0\xd8\x9d",
    },
    {
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
        1,

        "\x84\x98\x3e\x44\x1c\x3b\xd2\x6e\xba\xae\x4a\xa1\xf9\x51"
        "\x29\xe5\xe5\x46\x70\xf1",
    },
    {
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
        "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
        1,

        "\xa4\x9b\x24\x46\xa0\x2c\x64\x5b\xf4\x19\xf9\x95\xb6\x70"
        "\x91\x25\x3a\x04\xa2\x59",
    },
    {
        "a",
        1000000,

        "\x34\xaa\x97\x3c\xd4\xc4\xda\xa4\xf6\x1e\xeb\x2b\xdb\xad"
        "\x27\x31\x65\x34\x01\x6f",
    },
};

START_TEST(test_vectors)
{
    for (size_t i = 0; i < ASC_ARRAY_SIZE(test_strings); i++)
    {
        const char *const msg = test_strings[i].msg;
        const char *const expect = test_strings[i].hash;
        uint8_t hash[SHA1_DIGEST_SIZE] = { 0 };

        sha1_ctx_t test;
        au_sha1_init(&test);
        for (size_t j = 0; j < test_strings[i].repeat; j++)
            au_sha1_update(&test, (uint8_t *)msg, strlen(msg));

        au_sha1_final(&test, hash);
        ck_assert(!memcmp(hash, expect, sizeof(hash)));
    }
}
END_TEST

Suite *utils_sha1(void)
{
    Suite *const s = suite_create("utils/sha1");

    TCase *const tc = tcase_create("default");
    tcase_add_test(tc, test_vectors);
    suite_add_tcase(s, tc);

    return s;
}
