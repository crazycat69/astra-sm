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
#include <astra/utils/rc4.h>

/* RFC 6229 test vectors */
#define OFFSET_COUNT 9
#define TEST_BUFFER_SIZE 4112
#define TEST_SAMPLE_SIZE 32

typedef struct
{
    const char *key;
    uint8_t data[OFFSET_COUNT][TEST_SAMPLE_SIZE];
} rc4_test_t;

static const size_t offsets[OFFSET_COUNT] =
{
    0, 240, 496, 752, 1008, 1520, 2032, 3056, 4080,
};

#include "rc4_vectors.h"

START_TEST(test_vectors)
{
    for (size_t i = 0; i < ASC_ARRAY_SIZE(test_data); i++)
    {
        const char *const key = test_data[i].key;
        uint8_t buf[TEST_BUFFER_SIZE] = { 0 };
        uint8_t dst[TEST_BUFFER_SIZE] = { 0 };
        rc4_ctx_t test;

        au_rc4_init(&test, (uint8_t *)key, strlen(key));
        au_rc4_crypt(&test, dst, buf, sizeof(buf));

        for (size_t j = 0; j < ASC_ARRAY_SIZE(offsets); j++)
        {
            const size_t off = offsets[j];
            ck_assert(!memcmp(&dst[off], &test_data[i].data[j]
                              , TEST_SAMPLE_SIZE));
        }
    }
}
END_TEST

/* encrypt and decrypt random data */
#define BUF_SIZE (256 * 1024)
#define ITERATIONS 100
#define MAX_KEY_LENGTH 32

START_TEST(random_data)
{
    asc_srand();
    for (size_t i = 0; i < ITERATIONS; i++)
    {
        /* generate random key */
        const size_t keylen = 1 + (rand() % MAX_KEY_LENGTH);
        uint8_t key[MAX_KEY_LENGTH];

        for (size_t j = 0; j < keylen; j++)
            key[j] = rand();

        /* fill buffer */
        const size_t buflen = 1 + (rand() % BUF_SIZE);
        uint8_t *const expect = ASC_ALLOC(buflen, uint8_t);

        for (size_t j = 0; j < buflen; j++)
            expect[j] = rand();

        /* pass 1: encrypt */
        uint8_t *const dst = ASC_ALLOC(buflen, uint8_t);

        rc4_ctx_t test;
        au_rc4_init(&test, key, keylen);
        au_rc4_crypt(&test, dst, expect, buflen);

        /* pass 2: decrypt */
        uint8_t *const buf = ASC_ALLOC(buflen, uint8_t);

        au_rc4_init(&test, key, keylen);
        au_rc4_crypt(&test, buf, dst, buflen);
        ck_assert(!memcmp(buf, expect, buflen));

        free(buf);
        free(dst);
        free(expect);
    }
}
END_TEST

Suite *utils_rc4(void)
{
    Suite *const s = suite_create("utils/rc4");

    TCase *const tc = tcase_create("default");
    tcase_add_test(tc, test_vectors);
    tcase_add_test(tc, random_data);
    suite_add_tcase(s, tc);

    return s;
}
