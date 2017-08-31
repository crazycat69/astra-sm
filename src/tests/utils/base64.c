/*
 * Astra Unit Tests
 * http://cesbo.com/astra
 *
 * Copyright (C) 2016-2017, Artem Kharitonov <artem@3phase.pw>
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
#include <astra/utils/base64.h>

static
void setup(void)
{
    asc_srand();
}

/* RFC 4648 test vectors */
static
const char *vec_list[][2] =
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
        char *outn = au_base64_enc(text, strlen(text), NULL);
        ck_assert(out != NULL && outn != NULL);
        ck_assert(out[out_size] == '\0');
        ck_assert(strlen(out) == strlen(outn));
        ck_assert(out_size == strlen(b64));
        ck_assert(!strcmp(out, outn));
        ck_assert(!strcmp(out, b64));
        free(out);
        free(outn);

        /* decode */
        out_size = 0;
        out = (char *)au_base64_dec(b64, strlen(b64), &out_size);
        outn = (char *)au_base64_dec(b64, strlen(b64), NULL);
        ck_assert(out != NULL && outn != NULL);
        ck_assert(out[out_size] == '\0');
        ck_assert(strlen(out) == strlen(outn));
        ck_assert(out_size == strlen(text));
        ck_assert(!strcmp(out, outn));
        ck_assert(!strncmp(out, text, out_size));
        free(out);
        free(outn);
    }
}
END_TEST

/* encode and decode random data */
#define BUF_SIZE (256 * 1024)
#define ITERATIONS 100

START_TEST(random_data)
{
    char *const buf = ASC_ALLOC(BUF_SIZE, char);

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

/* incomplete input */
START_TEST(incomplete)
{
    static const char str_plain[] = "testtest\nfoobar\n";
    static const char str_b64[] = "dGVzdHRlc3QKZm9vYmFyCg==";

    for (size_t i = 0; i < sizeof(str_b64); i++)
    {
        size_t len = (size_t)-1;
        char *const out = (char *)au_base64_dec(str_b64, i, &len);
        ck_assert(out != NULL);
        ck_assert(len < i || (i <= 1 && len == 0));
        ck_assert(len == strlen(out));
        ck_assert(!memcmp(out, str_plain, len));
        free(out);
    }
}
END_TEST

/* attempt to decode malformed base64 */
static inline
bool valid_b64(char c)
{
    return ((c == '+') || (c == '/') || (c >= '0' && c <= '9')
            || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

START_TEST(malformed)
{
    for (unsigned int i = 0; i < 10000; i++)
    {
        /* generate random base64 string */
        const size_t b64_len = (rand() % 512);
        char *const b64 = (char *)ASC_ALLOC(b64_len + 1, char);

        for (size_t j = 0; j < b64_len;)
        {
            const char c = rand();
            if (valid_b64(c))
            {
                b64[j++] = c;
            }
        }

        if (b64_len > 1 && rand() & 1)
        {
            b64[b64_len - 1] = '=';
            if (b64_len > 2 && rand() & 1)
                b64[b64_len - 2] = '=';
        }

        /* create reference decoder output */
        size_t ref_len = 1234;
        char *const ref = (char *)au_base64_dec(b64, b64_len, &ref_len);
        ck_assert(ref != NULL);
        ck_assert(ref_len <= b64_len);

        /* "corrupt" input in a random place */
        size_t bomb = b64_len;

        if (b64_len > 0)
        {
            bomb = rand() % b64_len;
            while (valid_b64(b64[bomb] = rand()))
                { ; } /* nothing */
        }

        size_t plain_len = 43210;
        char *const plain = (char *)au_base64_dec(b64, b64_len, &plain_len);
        ck_assert(plain != NULL);
        ck_assert(plain_len >= strlen(plain));
        ck_assert(plain_len <= bomb);
        ck_assert(!memcmp(ref, plain, plain_len));

        free(plain);
        free(ref);
        free(b64);
    }
}
END_TEST

Suite *utils_base64(void)
{
    Suite *const s = suite_create("utils/base64");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, setup, NULL);

    tcase_add_test(tc, test_vectors);
    tcase_add_test(tc, random_data);
    tcase_add_test(tc, incomplete);
    tcase_add_test(tc, malformed);

    suite_add_tcase(s, tc);

    return s;
}
