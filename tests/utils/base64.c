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

#include "../test_libastra.h"
#include <utils/base64.h>

/* RFC 4648 test vectors */
static const char *vec_list[][2] =
{
    { "",       ""         },
    { "f",      "Zg=="     },
    { "fo",     "Zm8="     },
    { "foo",    "Zm9v"     },
    { "foob",   "Zm9vYg==" },
    { "fooba",  "Zm9vYmE=" },
    { "foobar", "Zm9vYmFy" },
};

START_TEST(test_vectors)
{
    for (size_t i = 0; i < ASC_ARRAY_SIZE(vec_list); i++)
    {
        const char *const text = vec_list[i][0];
        const char *const b64 = vec_list[i][1];

        /* encode */
        size_t out_size = 0;
        char *out = au_base64_enc(text, strlen(text), &out_size);
        ck_assert(out != NULL);
        ck_assert(out_size == strlen(b64));
        ck_assert(!strcmp(out, b64));
        free(out);

        /* decode */
        out_size = 0;
        out = (char *)au_base64_dec(b64, strlen(b64), &out_size);
        ck_assert(out != NULL);
        ck_assert(out_size == strlen(text));
        ck_assert(!strncmp(out, text, out_size));
        free(out);
    }
}
END_TEST

/* encode and decode random data */
#define BUF_SIZE (256 * 1024)
#define ITERATIONS 100

START_TEST(random_data)
{
    char *const buf = ASC_ALLOC(BUF_SIZE, char);

    asc_srand();
    for (size_t i = 0; i < ITERATIONS; i++)
    {
        const size_t len = rand() % BUF_SIZE;
        for (size_t j = 0; j < len; j++)
            buf[j] = rand();

        size_t out_size = 0;
        char *const b64 = au_base64_enc(buf, len, &out_size);
        ck_assert(b64 != NULL);

        out_size = 0;
        char *const data = (char *)au_base64_dec(b64, strlen(b64), &out_size);
        ck_assert(data != NULL);
        ck_assert(out_size == len);
        ck_assert(!memcmp(buf, data, len));

        free(b64);
        free(data);
    }

    free(buf);
}
END_TEST

Suite *utils_base64(void)
{
    Suite *const s = suite_create("utils/base64");

    TCase *const tc = tcase_create("default");
    tcase_add_test(tc, test_vectors);
    tcase_add_test(tc, random_data);
    suite_add_tcase(s, tc);

    return s;
}
