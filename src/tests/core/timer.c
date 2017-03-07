/*
 * Astra Unit Tests
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015-2016, Artem Kharitonov <artem@3phase.pw>
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
#include <astra/core/mainloop.h>
#include <astra/core/timer.h>

typedef struct
{
    asc_timer_t *timer;

    uint64_t last_run;
    unsigned triggered;
    unsigned interval;
} timer_test_t;

static bool timed_out;
static uint64_t time_stop;

static void on_stop(void *arg)
{
    ASC_UNUSED(arg);

    timed_out = true;
    time_stop = asc_utime();

    asc_main_loop_shutdown();
}

static uint64_t run_loop(unsigned ms)
{
    const uint64_t start = asc_utime();

    timed_out = false;
    time_stop = 0;
    asc_timer_t *const stopper = asc_timer_one_shot(ms, on_stop, NULL);
    ck_assert(stopper != NULL);

    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    const uint64_t bench = (time_stop - start) / 1000;
    fail_unless(bench <= (ms * 1.3), "too slow!");

    return bench;
}

/* do nothing for 500ms */
START_TEST(empty_loop)
{
    const unsigned duration = 500;
    const uint64_t bench = run_loop(duration);
    ck_assert(bench >= duration);
    ck_assert(timed_out == true);
}
END_TEST

/* hundred timers with the same interval */
static void on_hundred(void *arg)
{
    timer_test_t *const timer = (timer_test_t *)arg;

    const uint64_t now = asc_utime();
    if (timer->last_run)
    {
        const unsigned diff = (now - timer->last_run) / 1000;
        ck_assert_msg(diff >= timer->interval
                      , "timer interval too short: %ums", diff);
    }

    timer->last_run = now;
    timer->triggered++;
}

START_TEST(hundred_timers)
{
    timer_test_t data[100] = { { NULL, 0, 0, 0 } };

    const unsigned ms = get_timer_res() / 1000;
    const unsigned expect_max = 1000 / ms;
    const unsigned expect_min = expect_max / 3;
    asc_log_info("timer resolution: %ums, expecting %u to %u events per timer"
                 , ms, expect_min, expect_max);

    for (size_t i = 0; i < ASC_ARRAY_SIZE(data); i++)
    {
        data[i].timer = asc_timer_init(ms, on_hundred, &data[i]);
        data[i].interval = ms;
    }

    /* run for 1 second */
    const unsigned duration = 1000;
    run_loop(duration);
    ck_assert(timed_out == true);

    for (size_t i = 0; i < ASC_ARRAY_SIZE(data); i++)
    {
        fail_unless(data[i].triggered > expect_min
                    && data[i].triggered <= expect_max
                    , "missed event count (wanted from %u to %u, got %u)"
                    , expect_min, expect_max, data[i].triggered);
    }
}
END_TEST

/* single normal timer */
static void on_single_timer(void *arg)
{
    unsigned *const count = (unsigned *)arg;

    time_stop = asc_utime();
    if (++(*count) >= 10)
        asc_main_loop_shutdown();
}

START_TEST(single_timer)
{
    unsigned triggered = 0;
    asc_timer_t *const timer =
        asc_timer_init(40, on_single_timer, &triggered);
    ck_assert(timer != NULL);

    const uint64_t bench = run_loop(800);
    ck_assert(bench >= 400 - 50); /* 40ms * 10 events before shutdown */
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
    asc_timer_t *const timer =
        asc_timer_one_shot(50, on_single_one_shot, &triggered);
    ck_assert(timer != NULL);

    run_loop(150);
    ck_assert(timed_out == true);
    ck_assert(triggered == 1);
}
END_TEST

/* cancel one shot timer */
static void on_cancel_failed(void *arg)
{
    ASC_UNUSED(arg);
    fail("timer did not get cancelled");
}

static void on_try_cancel(void *arg)
{
    asc_timer_destroy((asc_timer_t *)arg);
}

START_TEST(cancel_one_shot)
{
    asc_timer_t *const timer1 =
        asc_timer_one_shot(200, on_cancel_failed, NULL);
    ck_assert(timer1 != NULL);

    asc_timer_t *const timer2 =
        asc_timer_one_shot(100, on_try_cancel, timer1);
    ck_assert(timer2 != NULL);

    run_loop(300);
    ck_assert(timed_out == true);
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

    run_loop(500);
    ck_assert(timed_out == true);
}
END_TEST

Suite *core_timer(void)
{
    Suite *const s = suite_create("core/timer");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);

    if (can_fork != CK_NOFORK)
        tcase_set_timeout(tc, 5);

    tcase_add_test(tc, empty_loop);
    tcase_add_test(tc, hundred_timers);
    tcase_add_test(tc, single_timer);
    tcase_add_test(tc, single_one_shot);
    tcase_add_test(tc, cancel_one_shot);
    tcase_add_test(tc, blocked_thread);

    suite_add_tcase(s, tc);

    return s;
}
