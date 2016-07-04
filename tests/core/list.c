/*
 * Astra: Unit tests
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015, Artem Kharitonov <artem@sysert.ru>
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
#include <core/list.h>

static asc_list_t *list = NULL;

static void setup(void)
{
    lib_setup();
    list = asc_list_init();
}

static void teardown(void)
{
    ASC_FREE(list, asc_list_destroy);
    lib_teardown();
}

START_TEST(empty_list)
{
    for (size_t i = 0; i < 3; i++)
    {
        switch (i)
        {
            case 1: asc_list_next(list); break;
            case 2: asc_list_first(list); break;
            default: break;
        }
        ck_assert(asc_list_size(list) == 0);
        ck_assert(asc_list_eol(list));
    }

    size_t count = 0;
    asc_list_for(list)
        count++;

    ck_assert(count == 0);
}
END_TEST

START_TEST(random_values)
{
    void *data[128];
    size_t i = 0;

    /* normal order */
    for (i = 0; i < ASC_ARRAY_SIZE(data); i++)
    {
        data[i] = (void *)((intptr_t)rand());
        asc_list_insert_tail(list, data[i]);
    }

    i = 0;
    asc_list_first(list);
    while (!asc_list_eol(list))
    {
        ck_assert(asc_list_data(list) == data[i++]);
        asc_list_remove_current(list);
    }
    ck_assert(asc_list_size(list) == 0);

    /* reverse order */
    for (i = 0; i < ASC_ARRAY_SIZE(data); i++)
    {
        data[i] = (void *)((intptr_t)rand());
        asc_list_insert_head(list, data[i]);
    }

    asc_list_first(list);
    while (!asc_list_eol(list))
    {
        ck_assert(asc_list_data(list) == data[--i]);
        asc_list_remove_current(list);
    }
    ck_assert(asc_list_size(list) == 0);
}
END_TEST

START_TEST(selective_delete)
{
    void *data[128];
    size_t i;

    intptr_t last = 0;
    for (i = 0; i < ASC_ARRAY_SIZE(data); i++)
    {
        last += 1 + (rand() % 100);
        data[i] = (void *)last;
        asc_list_insert_tail(list, data[i]);
    }

    for (i = 0; i < ASC_ARRAY_SIZE(data); i++)
    {
        if ((intptr_t)data[i] % 2)
            continue;

        asc_list_remove_item(list, data[i]);
        data[i] = NULL;
    }

    asc_list_first(list);
    for (i = 0; i < ASC_ARRAY_SIZE(data); i++)
    {
        if (data[i] == NULL)
            continue;

        ck_assert(!asc_list_eol(list));
        ck_assert(asc_list_data(list) == data[i]);
        asc_list_remove_current(list);
    }
    ck_assert(asc_list_eol(list));
}
END_TEST

START_TEST(no_data_empty)
{
    ck_assert(asc_list_data(list) == NULL);
}
END_TEST

START_TEST(no_data_full)
{
    void *data[16];
    memset(data, 0x1f, sizeof(data));
    for (size_t i = 0; i < ASC_ARRAY_SIZE(data); i++)
        asc_list_insert_tail(list, data[i]);
}
END_TEST

START_TEST(clear_list)
{
    const unsigned items[] = { 0, 1, 2, 3, 4, 5 };
    for (size_t i = 0; i < ASC_ARRAY_SIZE(items); i++)
    {
        void *const ptr = (void *)((intptr_t)items[i]);
        asc_list_insert_tail(list, ptr);
    }

    size_t idx = 0;
    asc_list_clear(list)
    {
        void *const ptr = asc_list_data(list);
        ck_assert((uintptr_t)ptr == items[idx++]);
    }
    ck_assert(asc_list_size(list) == 0);
    ck_assert(asc_list_eol(list));
}
END_TEST

START_TEST(till_empty)
{
    const unsigned items[] = { 0xface, 0xbeef, 0xcafe, 0xf00d };

    /* valid use: current item is removed on every iteration */
    for (size_t i = 0; i < ASC_ARRAY_SIZE(items); i++)
        asc_list_insert_tail(list, (void *)((intptr_t)items[i]));

    ck_assert(asc_list_size(list) == ASC_ARRAY_SIZE(items));

    asc_list_till_empty(list)
        asc_list_remove_current(list);

    ck_assert(asc_list_size(list) == 0);
    ck_assert(asc_list_eol(list));

    /* invalid use (not removing items) */
    for (size_t i = 0; i < ASC_ARRAY_SIZE(items); i++)
        asc_list_insert_tail(list, (void *)((intptr_t)items[i]));

    ck_assert(asc_list_size(list) == ASC_ARRAY_SIZE(items));

    void *prev = NULL;
    asc_list_till_empty(list)
    {
        /* this will loop until stopped */
        void *ptr = asc_list_data(list);

        if (prev != NULL)
        {
            ck_assert(prev == ptr);
            break;
        }

        prev = ptr;
    }

    asc_list_clear(list);
}
END_TEST

Suite *core_list(void)
{
    Suite *const s = suite_create("core/list");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, empty_list);
    tcase_add_test(tc, random_values);
    tcase_add_test(tc, selective_delete);
    tcase_add_test(tc, clear_list);
    tcase_add_test(tc, till_empty);

    if (can_fork != CK_NOFORK)
    {
        tcase_add_exit_test(tc, no_data_empty, EXIT_ABORT);
        tcase_add_exit_test(tc, no_data_full, EXIT_ABORT);
    }

    suite_add_tcase(s, tc);

    return s;
}
