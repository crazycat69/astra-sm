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

typedef struct
{
    asc_timer_t *timer;

    uint64_t last_run;
    unsigned triggered;
    unsigned interval;
} timer_test_t;

static bool timed_out;

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

static void on_stop(void *arg)
{
    __uarg(arg);
    timed_out = true;
    astra_shutdown();
}

static uint64_t run_loop(unsigned ms)
{
    const uint64_t start = asc_utime();

    timed_out = false;
    asc_timer_t *const stopper = asc_timer_one_shot(ms, on_stop, NULL);
    ck_assert(stopper != NULL);

    asc_main_loop_run();

    const uint64_t bench = (asc_utime() - start) / 1000;
    fail_unless(bench <= (ms * 1.3));

    return bench;
}

/*
 * unit tests
 */

/* do nothing for 500ms */
START_TEST(empty_loop)
{
    const unsigned duration = 500;
    const uint64_t bench = run_loop(duration);
    ck_assert(bench >= duration);
    ck_assert(timed_out == true);
}
END_TEST

/* bunch of 1ms timers */
static void on_millisecond(void *arg)
{
    timer_test_t *const timer = (timer_test_t *)arg;

    const uint64_t now = asc_utime();
    if (timer->last_run)
    {
        const unsigned diff = now - timer->last_run;
        ck_assert_msg(diff >= timer->interval
                      , "timer interval too short: %uus", diff);
    }

    timer->last_run = now;
    timer->triggered++;
}

START_TEST(millisecond)
{
    timer_test_t data[100] = { { NULL, 0, 0, 0 } };

    for (size_t i = 0; i < ASC_ARRAY_SIZE(data); i++)
    {
        const unsigned ms = 1;
        data[i].timer = asc_timer_init(ms, on_millisecond, &data[i]);
        data[i].interval = ms * 1000;
    }

    /* run for 1 second */
    const unsigned duration = 1000;
    run_loop(duration);

    for (size_t i = 0; i < ASC_ARRAY_SIZE(data); i++)
        fail_unless(data[i].triggered > (duration / 2));
}
END_TEST

/* single normal timer */
static void on_single_timer(void *arg)
{
    unsigned *const count = (unsigned *)arg;
    if (++(*count) >= 10)
        astra_shutdown();
}

START_TEST(single_timer)
{
    unsigned triggered = 0;
    asc_timer_init(40, on_single_timer, &triggered);
    const uint64_t bench = run_loop(500);

    ck_assert(bench >= 400);
    ck_assert(timed_out == false);
    ck_assert(triggered == 10);
}
END_TEST

/* single one shot timer */
static void on_single_one_shot(void *arg)
{
    unsigned *const triggered = (unsigned *)arg;
    (*triggered)++;
}

START_TEST(single_one_shot)
{
    unsigned triggered = 0;
    asc_timer_one_shot(50, on_single_one_shot, &triggered);
    run_loop(150);

    ck_assert(triggered == 1);
}
END_TEST

/* cancel one shot timer */
static void on_cancel_failed(void *arg)
{
    __uarg(arg);
    fail("timer did not get cancelled");
}

static void on_try_cancel(void *arg)
{
    asc_timer_destroy((asc_timer_t *)arg);
}

START_TEST(cancel_one_shot)
{
    asc_timer_t *const timer = asc_timer_one_shot(200, on_cancel_failed, NULL);
    asc_timer_one_shot(100, on_try_cancel, timer);
    run_loop(300);
}
END_TEST

/* blocked thread */
static void on_block_thread(void *arg)
{
    timer_test_t *const timer = (timer_test_t *)arg;

    if (timer->last_run)
    {
        const unsigned diff = asc_utime() - timer->last_run;
        ck_assert_msg(diff >= timer->interval
                      , "timer interval too short: %uus", diff);
    }

    asc_usleep(timer->interval * 1.5);
    timer->last_run = asc_utime();
}

START_TEST(blocked_thread)
{
    const unsigned ms = 50;

    timer_test_t timer = { NULL, 0, 0, ms * 1000 };
    timer.timer = asc_timer_init(ms, on_block_thread, &timer);

    run_loop(200);
}
END_TEST

Suite *core_timer(void)
{
    Suite *const s = suite_create("timer");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, setup, teardown);
#ifndef _WIN32
    tcase_set_timeout(tc, 5);
#endif

    tcase_add_test(tc, empty_loop);
    tcase_add_test(tc, millisecond);
    tcase_add_test(tc, single_timer);
    tcase_add_test(tc, single_one_shot);
    tcase_add_test(tc, cancel_one_shot);
    tcase_add_test(tc, blocked_thread);

    suite_add_tcase(s, tc);

    return s;
}
