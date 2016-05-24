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

#include "unit_tests.h"

#define BUF_SIZE 128

typedef struct
{
    uint8_t buf[BUF_SIZE];
} my_test_t;

static void *cleaned_up;

static void cleanup_routine(void *ptr)
{
    cleaned_up = ptr;
}

static void check_zero(const uint8_t *ptr, size_t size)
{
    for (size_t i = 0; i < size; i++)
        ck_assert(ptr[i] == 0);
}

START_TEST(func_asc_calloc)
{
    /* byte array */
    uint8_t *const buf = (uint8_t *)asc_calloc(BUF_SIZE, sizeof(*buf));
    ck_assert(buf != NULL);
    check_zero(buf, BUF_SIZE);
    free(buf);
}
END_TEST

START_TEST(macros)
{
    void *ptr;

    /* byte array */
    uint8_t *buf = ASC_ALLOC(BUF_SIZE, uint8_t);
    ck_assert(buf != NULL);
    check_zero(buf, BUF_SIZE);

    ASC_FREE(buf, free);
    ck_assert(buf == NULL);

    /* struct */
    cleaned_up = ptr = NULL;

    struct timeval *ts = ASC_ALLOC(1, struct timeval);
    ck_assert(ts != NULL);
    ck_assert(ts->tv_sec == 0);
    ck_assert(ts->tv_usec == 0);

    ptr = ts;
    ASC_FREE(ts, cleanup_routine);
    ck_assert(cleaned_up == ptr);
    ck_assert(ts == NULL);

    /* typedef'd struct */
    cleaned_up = ptr = NULL;

    my_test_t *test = ASC_ALLOC(1, my_test_t);
    ck_assert(test != NULL);
    check_zero(test->buf, sizeof(test->buf));

    ptr = test;
    ASC_FREE(test, cleanup_routine);
    ck_assert(cleaned_up == ptr);
    ck_assert(test == NULL);
}
END_TEST

Suite *core_alloc(void)
{
    Suite *const s = suite_create("alloc");

    TCase *const tc = tcase_create("default");
    tcase_add_test(tc, func_asc_calloc);
    tcase_add_test(tc, macros);
    suite_add_tcase(s, tc);

    return s;
}
