/*
 * Astra Unit Tests
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015-2017, Artem Kharitonov <artem@3phase.pw>
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
#include <astra/core/list.h>
#include <astra/core/mainloop.h>
#include <astra/core/thread.h>
#include <astra/core/mutex.h>
#include <astra/core/cond.h>

typedef struct
{
    asc_thread_t *thread;
    asc_mutex_t *mutex;
    asc_list_t *list;
    asc_cond_t *cond;
    unsigned int id;
    unsigned int value;
} thread_test_t;

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

    asc_main_loop_shutdown();
    asc_thread_join(tt->thread);
}

START_TEST(set_value)
{
    thread_test_t tt;
    memset(&tt, 0, sizeof(tt));

    tt.thread = asc_thread_init(&tt, set_value_proc, set_value_close);
    ck_assert(tt.thread != NULL);

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
        asc_main_loop_shutdown();
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
        tt[i].list = list;
        tt[i].mutex = &mutex;
        tt[i].id = i;
        tt[i].value = 0;

        tt[i].thread = asc_thread_init(&tt[i], producer_proc, NULL);
        ck_assert(tt[i].thread != NULL);

        producer_running++;
    }

    /* start "production" */
    asc_mutex_unlock(&mutex);
    ck_assert(asc_main_loop_run() == false);
    ck_assert(producer_running == 0);

    /* check total item count */
    ck_assert(asc_list_count(list) == PRODUCER_THREADS * PRODUCER_ITEMS);

    /* check item order */
    unsigned int counts[PRODUCER_THREADS] = { 0 };
    asc_list_clear(list)
    {
        void *const ptr = asc_list_data(list);
        const unsigned int data = (unsigned)((intptr_t)ptr);

        const unsigned int id = data >> 16;
        const unsigned int value = data & 0xFFFF;

        ck_assert(counts[id]++ == value);
    }

    asc_list_destroy(list);
}
END_TEST

/* buggy cleanup routine */
static void no_destroy_proc(void *arg)
{
    ASC_UNUSED(arg);
    asc_usleep(50 * 1000); /* 50ms */
}

static void no_destroy_close(void *arg)
{
    ASC_UNUSED(arg);
    asc_main_loop_shutdown();

    /* BUG: does not call thread_join()! */
}

START_TEST(no_destroy)
{
    asc_thread_t *const thr = asc_thread_init(NULL, no_destroy_proc
                                              , no_destroy_close);
    ck_assert(thr != NULL);

    ck_assert(asc_main_loop_run() == false);
}
END_TEST

/* main thread wake up */
#define WAKE_TASKS 1000

static uint64_t wake_time = 0;
static unsigned int wake_tasks_done = 0;
static bool wake_quit = false;

static asc_thread_t *wake_thr = NULL;
static asc_mutex_t wake_mutex;
static asc_cond_t wake_cond;

static void wake_up_cb(void *arg)
{
    ASC_UNUSED(arg);

    const uint64_t now = asc_utime();
    ck_assert_msg(now >= wake_time && now - wake_time < (5 * 1000)
                  , "didn't wake up within 5ms");

    asc_mutex_lock(&wake_mutex);
    if (++wake_tasks_done >= WAKE_TASKS)
    {
        wake_quit = true;
    }
    asc_cond_signal(&wake_cond);
    asc_mutex_unlock(&wake_mutex);
}

static void wake_up_proc(void *arg)
{
    ASC_UNUSED(arg);

    asc_mutex_lock(&wake_mutex);
    while (true)
    {
        if (!wake_quit)
        {
            wake_time = asc_utime();
            asc_job_queue(NULL, wake_up_cb, NULL);
            asc_wake();
        }
        else
        {
            asc_mutex_unlock(&wake_mutex);
            break;
        }

        asc_cond_wait(&wake_cond, &wake_mutex);
    }
}

static void wake_up_close(void *arg)
{
    ASC_UNUSED(arg);

    asc_thread_join(wake_thr);
    asc_main_loop_shutdown();
}

START_TEST(wake_up)
{
    asc_mutex_init(&wake_mutex);
    asc_cond_init(&wake_cond);
    asc_wake_open();

    wake_tasks_done = 0;
    wake_quit = false;

    wake_thr = asc_thread_init(NULL, wake_up_proc, wake_up_close);
    ck_assert(wake_thr != NULL);

    ck_assert(asc_main_loop_run() == false);
    ck_assert(wake_tasks_done == WAKE_TASKS);

    asc_wake_close();
    asc_cond_destroy(&wake_cond);
    asc_mutex_destroy(&wake_mutex);
}
END_TEST

/* mutex timed lock */
#define TL_P1_WAIT (100 * 1000) /* 100ms */
#define TL_P2_WAIT (200 * 1000) /* 200ms */
#define TL_MS 500 /* timeout in msecs */

#define TL_CHECK_TIME(__start, __timeout) \
    do { \
        const uint64_t __bench = asc_utime() - (__start); \
        ck_assert(__bench <= (__timeout) * 1.3 \
                  && __bench >= (__timeout) * 0.7); \
    } while (0)

static asc_mutex_t tl_mutex1;
static asc_mutex_t tl_mutex2;
static asc_mutex_t tl_mutex3;

static void timedlock_proc(void *arg)
{
    ASC_UNUSED(arg);

    asc_mutex_lock(&tl_mutex2); /* locked M2 */

    /* part 1 */
    ck_assert(asc_mutex_trylock(&tl_mutex1) == false);
    const uint64_t start = asc_utime();
    const bool ret = asc_mutex_timedlock(&tl_mutex1, TL_MS); /* locked M1 */
    ck_assert(ret == true);
    TL_CHECK_TIME(start, TL_P1_WAIT);

    /* part 2 */
    asc_usleep(TL_P2_WAIT);
    asc_mutex_unlock(&tl_mutex2); /* released M2 */

    /* part 3 */
    asc_mutex_lock(&tl_mutex3); /* locked M3 */
    asc_mutex_unlock(&tl_mutex1); /* released M1 */
    asc_mutex_unlock(&tl_mutex3); /* released M3 */
    ck_assert(asc_mutex_trylock(&tl_mutex1) == true); /* locked M1 */
    asc_mutex_unlock(&tl_mutex1); /* released M1 */
}

START_TEST(mutex_timedlock)
{
    asc_mutex_init(&tl_mutex1);
    asc_mutex_init(&tl_mutex2);
    asc_mutex_init(&tl_mutex3);

    asc_mutex_lock(&tl_mutex3); /* locked M3 */

    /* part 1: aux thread waits for main */
    asc_mutex_lock(&tl_mutex1); /* locked M1 */
    asc_thread_t *const thr = asc_thread_init(NULL, timedlock_proc, NULL);
    asc_usleep(TL_P1_WAIT);
    asc_mutex_unlock(&tl_mutex1); /* released M1 */

    /* part 2: main thread waits for aux */
    ck_assert(asc_mutex_trylock(&tl_mutex2) == false);
    uint64_t start = asc_utime();
    bool ret = asc_mutex_timedlock(&tl_mutex2, TL_MS); /* locked M2 */
    ck_assert(ret == true);
    TL_CHECK_TIME(start, TL_P2_WAIT);

    /* part 3: timedlock failure */
    start = asc_utime();
    ret = asc_mutex_timedlock(&tl_mutex1, TL_MS); /* timeout for M1 */
    ck_assert(ret == false);
    TL_CHECK_TIME(start, TL_MS * 1000);

    /* join thread and clean up */
    asc_mutex_unlock(&tl_mutex2); /* released M2 */
    asc_mutex_unlock(&tl_mutex3); /* released M3 */
    asc_thread_join(thr);

    ck_assert(asc_mutex_trylock(&tl_mutex3) == true); /* locked M3 */
    asc_mutex_unlock(&tl_mutex3); /* released M3 */

    asc_mutex_destroy(&tl_mutex1);
    asc_mutex_destroy(&tl_mutex2);
    asc_mutex_destroy(&tl_mutex3);
}
END_TEST

/* condition variable, single thread */
static asc_mutex_t one_mutex;
static asc_cond_t one_cond;
static uint32_t one_val;

static void one_proc(void *arg)
{
    ASC_UNUSED(arg);

    ck_assert(asc_mutex_trylock(&one_mutex) == false);

    /* aux signals main */
    asc_mutex_lock(&one_mutex);
    one_val = 0xdeadbeef;
    asc_cond_signal(&one_cond);
    asc_mutex_unlock(&one_mutex);

    /* main signals aux */
    asc_mutex_lock(&one_mutex);
    asc_cond_wait(&one_cond, &one_mutex);
    ck_assert(one_val == 0xbaadf00d);
    asc_mutex_unlock(&one_mutex);

    one_val = 0xbeefcafe;
}

START_TEST(cond_single)
{
    asc_mutex_init(&one_mutex);
    asc_cond_init(&one_cond);

    /* missed signal */
    ck_assert(asc_mutex_trylock(&one_mutex) == true);
    /* expect these to be ignored as no one's waiting on the variable */
    asc_cond_signal(&one_cond);
    asc_cond_broadcast(&one_cond);
    ck_assert(asc_cond_timedwait(&one_cond, &one_mutex, 100) == false);
    asc_mutex_unlock(&one_mutex);

    /* aux signals main */
    asc_mutex_lock(&one_mutex);
    asc_thread_t *const thr = asc_thread_init(NULL, one_proc, NULL);
    ck_assert(thr != NULL);
    asc_usleep(100000); /* 100ms */
    ck_assert(asc_cond_timedwait(&one_cond, &one_mutex, 200) == true);
    ck_assert(one_val == 0xdeadbeef);
    asc_mutex_unlock(&one_mutex);

    /* main signals aux */
    asc_usleep(100000); /* 100ms */
    asc_mutex_lock(&one_mutex);
    one_val = 0xbaadf00d;
    asc_cond_broadcast(&one_cond);
    asc_mutex_unlock(&one_mutex);

    asc_thread_join(thr);
    ck_assert(one_val == 0xbeefcafe);

    asc_cond_destroy(&one_cond);
    asc_mutex_destroy(&one_mutex);
}
END_TEST

/* condition variable, multiple threads */
#define MULTI_THREADS 128
#define MULTI_TASKS 262144

static bool multi_quit = false;

static void multi_proc(void *arg)
{
    thread_test_t *const tt = (thread_test_t *)arg;

    asc_mutex_lock(tt->mutex);
    while (true)
    {
        asc_list_till_empty(tt->list)
        {
            asc_list_remove_current(tt->list);
            asc_mutex_unlock(tt->mutex);
            asc_usleep(1000);
            tt->value++;
            asc_mutex_lock(tt->mutex);
        }

        if (multi_quit)
        {
            asc_mutex_unlock(tt->mutex);
            break;
        }

        asc_cond_wait(tt->cond, tt->mutex);
    }
}

START_TEST(cond_multi)
{
    thread_test_t tt[MULTI_THREADS];

    asc_list_t *const list = asc_list_init();

    asc_cond_t cond;
    asc_cond_init(&cond);

    asc_mutex_t mutex;
    asc_mutex_init(&mutex);

    /* start worker threads */
    asc_mutex_lock(&mutex);
    for (size_t i = 0; i < ASC_ARRAY_SIZE(tt); i++)
    {
        tt[i].list = list;
        tt[i].mutex = &mutex;
        tt[i].cond = &cond;
        tt[i].id = i;
        tt[i].value = 0;

        tt[i].thread = asc_thread_init(&tt[i], multi_proc, NULL);
        ck_assert(tt[i].thread != NULL);
    }
    asc_mutex_unlock(&mutex);

    /* insert "tasks" */
    for (size_t i = 0; i < MULTI_TASKS; i++)
    {
        asc_mutex_lock(&mutex);

        asc_list_insert_tail(list, (void *)0x1234);
        asc_cond_signal(&cond);

        asc_mutex_unlock(&mutex);
    }

    /* signal threads to quit */
    asc_mutex_lock(&mutex);
    multi_quit = true;
    asc_cond_broadcast(&cond);
    asc_mutex_unlock(&mutex);

    size_t tasks_done = 0;
    for (size_t i = 0; i < ASC_ARRAY_SIZE(tt); i++)
    {
        asc_thread_join(tt[i].thread);
        tasks_done += tt[i].value;
        asc_log_info("thread %zu: %u tasks done", i, tt[i].value);
    }
    asc_log_info("total: %zu tasks", tasks_done);
    ck_assert(tasks_done == MULTI_TASKS);

    asc_mutex_destroy(&mutex);
    asc_cond_destroy(&cond);
    asc_list_destroy(list);
}
END_TEST

Suite *core_thread(void)
{
    Suite *const s = suite_create("core/thread");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);

    tcase_add_test(tc, set_value);
    tcase_add_test(tc, producers);
    tcase_add_test(tc, wake_up);
    tcase_add_test(tc, mutex_timedlock);
    tcase_add_test(tc, cond_single);
    tcase_add_test(tc, cond_multi);

    if (can_fork != CK_NOFORK)
    {
        tcase_set_timeout(tc, 120);
        tcase_add_exit_test(tc, no_destroy, ASC_EXIT_ABORT);
    }

    suite_add_tcase(s, tc);

    return s;
}
