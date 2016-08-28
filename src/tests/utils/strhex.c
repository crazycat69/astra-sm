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
#include <astra/utils/strhex.h>

/* pre-defined test strings */
typedef struct
{
    const char *str;
    uint8_t data[512];
    size_t datalen;
} strhex_test_t;

static const strhex_test_t test_strings[] =
{
    {
        .str = "",
        .data = { 0 },
        .datalen = 0,
    },
    {
        .str = "000102030405060708090a0b0c0d0e0f",
        .data = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        },
        .datalen = 16,
    },
    {
        .str = "00102030405060708090a0b0c0d0e0f0",
        .data = {
            0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
            0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0,
        },
        .datalen = 16,
    },
    {
        .str = "dEAdBeEfcAFe",
        .data = { 0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe },
        .datalen = 6,
    },
    {
        .str = "B3bbc62Ee0588b632f9aa50a0ac21dA26E360a95"
               "d02aCDb7e9674b350D3e459E08e2b9ee799a187b"
               "1efeb2057112c7e01a0127B59Aa38164d232f902"
               "269f1Fc4b60a080Ff86c7228f9547Fdf7407b854",
        .data = {
            0xb3, 0xbb, 0xc6, 0x2e, 0xe0, 0x58, 0x8b, 0x63, 0x2f, 0x9a,
            0xa5, 0x0a, 0x0a, 0xc2, 0x1d, 0xa2, 0x6e, 0x36, 0x0a, 0x95,
            0xd0, 0x2a, 0xcd, 0xb7, 0xe9, 0x67, 0x4b, 0x35, 0x0d, 0x3e,
            0x45, 0x9e, 0x08, 0xe2, 0xb9, 0xee, 0x79, 0x9a, 0x18, 0x7b,
            0x1e, 0xfe, 0xb2, 0x05, 0x71, 0x12, 0xc7, 0xe0, 0x1a, 0x01,
            0x27, 0xb5, 0x9a, 0xa3, 0x81, 0x64, 0xd2, 0x32, 0xf9, 0x02,
            0x26, 0x9f, 0x1f, 0xc4, 0xb6, 0x0a, 0x08, 0x0f, 0xf8, 0x6c,
            0x72, 0x28, 0xf9, 0x54, 0x7f, 0xdf, 0x74, 0x07, 0xb8, 0x54,
        },
        .datalen = 80,
    },
};

START_TEST(test_vectors)
{
    for (size_t i = 0; i < ASC_ARRAY_SIZE(test_strings); i++)
    {
        const strhex_test_t *const test = &test_strings[i];

        /* hex string to binary */
        uint8_t hbuf[512] = { 0 };
        /* NOTE: buffer size must be at least (strlen(str) / 2) */
        uint8_t *const hp = au_str2hex(test->str, hbuf, sizeof(hbuf));
        ck_assert(hp == hbuf);
        ck_assert(!memcmp(hbuf, test->data, test->datalen));

        /* binary to hex string */
        char sbuf[512] = { 0 };
        /* NOTE: buffer size must be at least ((datalen * 2) + 1) */
        char *const sp = au_hex2str(sbuf, test->data, test->datalen);
        ck_assert(sp == sbuf);
        ck_assert(strlen(sbuf) == (test->datalen * 2));
        ck_assert(!strcasecmp(sbuf, test->str));
    }
}
END_TEST

/* invalid hex strings */
static const strhex_test_t invalid[] =
{
    /* NOTE: out-of-range characters are treated as zeroes */
    {
        .str = "foobar",
        .data = { 0xf0, 0x0b, 0xa0 },
        .datalen = 3,
    },
    {
        .str = "The quick brown fox jumps over the lazy dog",
        .data = {
            0x00, 0xe0, 0x00, 0x0c, 0x00, 0xb0, 0x00, 0x00, 0xf0, 0x00,
            0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x0e, 0x00, 0xa0, 0x00,
            0xd0,
        },
        .datalen = 21,
    },
    {
        .str = "AaBbCcDdEeFfGgHhIi55",
        .data = {
            0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x00, 0x00, 0x55,
        },
        .datalen = 10,
    },
};

START_TEST(invalid_strings)
{
    for (size_t i = 0; i < ASC_ARRAY_SIZE(invalid); i++)
    {
        const strhex_test_t *const test = &invalid[i];

        uint8_t hbuf[512] = { 0 };
        uint8_t *const hp = au_str2hex(test->str, hbuf, sizeof(hbuf));
        ck_assert(hp == hbuf);
        ck_assert(!memcmp(hbuf, test->data, test->datalen));
    }
}
END_TEST

/* omitted destination buffer length */
START_TEST(omit_dstlen)
{
    for (size_t i = 0; i < ASC_ARRAY_SIZE(test_strings); i++)
    {
        const strhex_test_t *const test = &test_strings[i];

        uint8_t hbuf[512] = { 0 };
        uint8_t *const hp = au_str2hex(test->str, hbuf, 0);
        ck_assert(hp == hbuf);
        ck_assert(!memcmp(hbuf, test->data, test->datalen));
    }
}
END_TEST

/* random binary to hex */
#define ITERATIONS 1000
#define MAX_BUF_SIZE (32 * 1024)

START_TEST(random_h2s)
{
    asc_srand();
    for (size_t i = 0; i < ITERATIONS; i++)
    {
        /* binary buffer */
        const size_t hbsize = 1 + (rand() % MAX_BUF_SIZE);
        uint8_t *const hbuf = ASC_ALLOC(hbsize, uint8_t);

        for (size_t j = 0; j < hbsize; j++)
            hbuf[j] = rand();

        /* hex string buffer */
        const size_t sbsize = (hbsize * 2) + 1;
        char *const sbuf = ASC_ALLOC(sbsize, char);

        char *const sp = au_hex2str(sbuf, hbuf, hbsize);
        ck_assert(sp == sbuf);
        ck_assert(strlen(sbuf) == sbsize - 1);

        /* control buffer */
        uint8_t *const ctlbuf = ASC_ALLOC(hbsize, uint8_t);

        uint8_t *const cp = au_str2hex(sbuf, ctlbuf, hbsize);
        ck_assert(cp == ctlbuf);
        ck_assert(!memcmp(ctlbuf, hbuf, hbsize));

        free(ctlbuf);
        free(sbuf);
        free(hbuf);
    }
}
END_TEST

/* random hex to binary */
START_TEST(random_s2h)
{
    for (size_t i = 0; i < ITERATIONS; i++)
    {
        const size_t hbsize = 1 + (rand() % MAX_BUF_SIZE);

        /* make a random hex string */
        const size_t sbsize = (hbsize * 2) + 1;
        char *const sbuf = ASC_ALLOC(sbsize, char);

        static const char hexstr[] = "0123456789abcdef";
        for (size_t j = 0; j < sbsize - 1; j++)
            sbuf[j] = hexstr[rand() % (sizeof(hexstr) - 1)];

        ck_assert(strlen(sbuf) == sbsize - 1);

        /* fill binary buffer */
        uint8_t *const hbuf = ASC_ALLOC(hbsize, uint8_t);
        uint8_t *const hp = au_str2hex(sbuf, hbuf, hbsize);
        ck_assert(hp == hbuf);

        /* convert to string and compare */
        char *const ctlbuf = ASC_ALLOC(sbsize, char);
        char *const cp = au_hex2str(ctlbuf, hbuf, hbsize);
        ck_assert(cp == ctlbuf);
        ck_assert(strlen(ctlbuf) == sbsize - 1);
        ck_assert(!strcasecmp(ctlbuf, sbuf));

        free(ctlbuf);
        free(hbuf);
        free(sbuf);
    }
}
END_TEST

Suite *utils_strhex(void)
{
    Suite *const s = suite_create("utils/strhex");

    TCase *const tc = tcase_create("default");
    tcase_add_test(tc, test_vectors);
    tcase_add_test(tc, invalid_strings);
    tcase_add_test(tc, omit_dstlen);
    tcase_add_test(tc, random_h2s);
    tcase_add_test(tc, random_s2h);
    suite_add_tcase(s, tc);

    return s;
}
