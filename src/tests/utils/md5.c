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
#include <astra/utils/md5.h>

/* pre-defined test strings */
typedef struct
{
    const char *msg;
    const char *hash;
} md5_test_t;

static const md5_test_t test_strings[] =
{
    {
        "abc",
        "\x90\x01\x50\x98\x3c\xd2\x4f\xb0\xd6\x96\x3f\x7d\x28\xe1\x7f\x72",
    },
    {
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
        "\x82\x15\xef\x07\x96\xa2\x0b\xca\xaa\xe1\x16\xd3\x87\x6c\x66\x4a",
    },
    {
        "The quick brown fox jumps over the lazy dog",
        "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6",
    },
    {
        "foo",
        "\xac\xbd\x18\xdb\x4c\xc2\xf8\x5c\xed\xef\x65\x4f\xcc\xc4\xa4\xd8",
    },
    {
        "foobar",
        "\x38\x58\xf6\x22\x30\xac\x3c\x91\x5f\x30\x0c\x66\x43\x12\xc6\x3f",
    },
};

START_TEST(test_vectors)
{
    for (size_t i = 0; i < ASC_ARRAY_SIZE(test_strings); i++)
    {
        const char *const msg = test_strings[i].msg;
        const char *const expect = test_strings[i].hash;
        uint8_t hash[MD5_DIGEST_SIZE] = { 0 };

        md5_ctx_t test;
        au_md5_init(&test);
        au_md5_update(&test, (uint8_t *)msg, strlen(msg));
        au_md5_final(&test, hash);

        ck_assert(!memcmp(hash, expect, sizeof(hash)));
    }
}
END_TEST

/* character 'a' repeated 1000000 times */
START_TEST(million_a)
{
    md5_ctx_t test;
    au_md5_init(&test);

    for (size_t i = 0; i < 1000000; i++)
    {
        static const uint8_t a = 'a';
        au_md5_update(&test, &a, sizeof(a));
    }

    static const uint8_t expect[MD5_DIGEST_SIZE] =
    {
        0x77, 0x07, 0xd6, 0xae, 0x4e, 0x02, 0x7c, 0x70,
        0xee, 0xa2, 0xa9, 0x35, 0xc2, 0x29, 0x6f, 0x21,
    };

    uint8_t hash[MD5_DIGEST_SIZE] = { 0 };
    au_md5_final(&test, hash);

    ck_assert(!memcmp(hash, expect, sizeof(hash)));
}
END_TEST

/* salted passwords */
typedef struct
{
    const char *str;
    const char *salt;
    const char *out;
} md5_pwd_t;

static const md5_pwd_t test_pwds[] =
{
    {
        "abc", "",
        "$1$$j0yT3c/2mYPQF09fpvPLb0",
    },
    {
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "24k2HGno",
        "$1$24k2HGno$dlqH.myjULrcEFC/LVrlX/",
    },
    {
        "The quick brown fox jumps over the lazy dog", "9Uiu7vSwRoDowN7U",
        "$1$9Uiu7vSw$daEwZ1SA6sXzBZGF.xkOV1",
    },
    {
        "foo", "HDYlw",
        "$1$HDYlw$qyfPl9FlYEXpRB7ouWf7f.",
    },
    {
        "foobar", "2",
        "$1$2$rQyl54/VMYUjo3joS8y2r0",
    },
};

START_TEST(pwd_crypt)
{
    for (size_t i = 0; i < ASC_ARRAY_SIZE(test_pwds); i++)
    {
        const md5_pwd_t *const p = &test_pwds[i];

        char out[36] = { '\0' };
        au_md5_crypt(p->str, p->salt, out);
        ck_assert(!strcmp(out, p->out));
    }
}
END_TEST

Suite *utils_md5(void)
{
    Suite *const s = suite_create("utils/md5");

    TCase *const tc = tcase_create("default");
    tcase_add_test(tc, test_vectors);
    tcase_add_test(tc, million_a);
    tcase_add_test(tc, pwd_crypt);
    suite_add_tcase(s, tc);

    return s;
}
