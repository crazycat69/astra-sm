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

#include "unit_tests.h"
#include <core/list.h>
#include <core/mainloop.h>
#include <core/thread.h>

typedef struct
{
    asc_thread_t *thread;
    asc_mutex_t *mutex;
    asc_list_t *list;
    unsigned int id;
    unsigned int value;
} thread_test_t;

/*
 * test fixture
 */
static void setup(void)
{
    astra_core_init();
}

static void teardown(void)
{
    astra_core_destroy();
}

/*
 * unit tests
 */

/* set variable and exit */
static void set_value_proc(void *arg)
{
    thread_test_t *const tt = (thread_test_t *)arg;

    tt->value = 0xdeadbeef;
    asc_usleep(150 * 1000); /* 150ms */
}

static void set_value_close(void *arg)
{
    thread_test_t *const tt = (thread_test_t *)arg;

    astra_shutdown();
    asc_thread_destroy(tt->thread);
}

START_TEST(set_value)
{
    thread_test_t tt;
    memset(&tt, 0, sizeof(tt));

    asc_thread_t *const thr = asc_thread_init(&tt);
    ck_assert(thr != NULL);

    tt.thread = thr;

    asc_thread_start(thr, set_value_proc, NULL, NULL, set_value_close);
    ck_assert(asc_main_loop_run() == false);
    ck_assert(tt.value == 0xdeadbeef);
}
END_TEST

/* multiple threads adding items to list */
#define PRODUCER_THREADS 10
#define PRODUCER_ITEMS 100

static unsigned int producer_running;

static void producer_proc(void *arg)
{
    thread_test_t *const tt = (thread_test_t *)arg;

    for (size_t i = 0; i < PRODUCER_ITEMS; i++)
    {
        const unsigned int item = (tt->id << 16) | (tt->value++ & 0xFFFF);

        asc_mutex_lock(tt->mutex);
        asc_list_insert_tail(tt->list, (void *)((intptr_t)item));
        asc_mutex_unlock(tt->mutex);

        asc_usleep(1000);
    }

    asc_mutex_lock(tt->mutex);
    if (--producer_running == 0)
        astra_shutdown();
    asc_mutex_unlock(tt->mutex);
}

START_TEST(producers)
{
    thread_test_t tt[PRODUCER_THREADS];

    asc_list_t *const list = asc_list_init();
    ck_assert(list != NULL);

    asc_mutex_t mutex;
    asc_mutex_init(&mutex);

    /* lock threads until we complete startup */
    asc_mutex_lock(&mutex);

    producer_running = 0;
    for (size_t i = 0; i < ASC_ARRAY_SIZE(tt); i++)
    {
        tt[i].thread = asc_thread_init(&tt[i]);
        ck_assert(tt[i].thread != NULL);

        tt[i].list = list;
        tt[i].mutex = &mutex;
        tt[i].id = i;
        tt[i].value = 0;

        asc_thread_start(tt[i].thread, producer_proc, NULL, NULL, NULL);
        producer_running++;
    }

    /* start "production" */
    asc_mutex_unlock(&mutex);
    ck_assert(asc_main_loop_run() == false);
    ck_assert(producer_running == 0);

    /* check total item count */
    ck_assert(asc_list_size(list) == PRODUCER_THREADS * PRODUCER_ITEMS);

    /* check item order */
    unsigned int counts[PRODUCER_THREADS] = { 0 };
    asc_list_for(list)
    {
        void *const ptr = asc_list_data(list);
        const unsigned int data = (unsigned)((intptr_t)ptr);

        const unsigned int id = data >> 16;
        const unsigned int value = data & 0xFFFF;

        ck_assert(counts[id]++ == value);
    }
}
END_TEST

/* thread that never gets started */
START_TEST(no_start)
{
    asc_thread_t *const thr = asc_thread_init(NULL);
    __uarg(thr);
}
END_TEST

/* buggy cleanup routine */
static void no_destroy_proc(void *arg)
{
    __uarg(arg);
    asc_usleep(50 * 1000); /* 50ms */
}

static void no_destroy_close(void *arg)
{
    __uarg(arg);
    astra_shutdown();
}

START_TEST(no_destroy)
{
    asc_thread_t *const thr = asc_thread_init(NULL);
    asc_thread_start(thr, no_destroy_proc, NULL, NULL, no_destroy_close);

    ck_assert(asc_main_loop_run() == false);
}
END_TEST

Suite *core_thread(void)
{
    Suite *const s = suite_create("thread");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, set_value);
    tcase_add_test(tc, producers);
    tcase_add_test(tc, no_start);

    if (can_fork != CK_NOFORK)
    {
        tcase_set_timeout(tc, 5);
        tcase_add_exit_test(tc, no_destroy, EXIT_ABORT);
    }

    suite_add_tcase(s, tc);

    return s;
}
