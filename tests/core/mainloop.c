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
#include <core/mainloop.h>
#include <core/timer.h>

/* basic shutdown and reload commands */
START_TEST(controls)
{
    asc_main_loop_shutdown();
    bool again = asc_main_loop_run();
    ck_assert_msg(again == false, "expected shutdown");

    asc_main_loop_reload();
    again = asc_main_loop_run();
    ck_assert_msg(again == true, "expected restart");
}
END_TEST

/* immediate exit procedure */
#define EXIT_TEST 123

static __dead void on_exit_timer(void *arg)
{
    __uarg(arg);
    asc_lib_exit(EXIT_TEST);
}

static void on_process_exit(void)
{
    ck_assert(asc_exit_status == EXIT_TEST);
}

START_TEST(exit_status)
{
    atexit(on_process_exit);

    asc_timer_t *timer = asc_timer_one_shot(100, on_exit_timer, NULL);
    ck_assert(timer != NULL);

    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    ck_abort_msg("didn't expect to reach this point");
}
END_TEST

/* shutdown after 1000 iterations of a 1ms timer */
#define ITERATIONS 1000

static void on_iteration(void *arg)
{
    __uarg(arg);

    unsigned *counter = (unsigned *)arg;
    if (++(*counter) >= ITERATIONS)
        asc_main_loop_shutdown();
}

START_TEST(iterations)
{
    unsigned counter = 0;
    asc_timer_t *timer = asc_timer_init(1, on_iteration, &counter);
    ck_assert(timer != NULL);

    const bool again = asc_main_loop_run();
    ck_assert(again == false);
    ck_assert(counter == ITERATIONS);
}
END_TEST

/* block thread then call asc_main_loop_shutdown() until it aborts */
static void on_block(void *arg)
{
    __uarg(arg);

    while (true)
    {
        asc_main_loop_shutdown();
        asc_usleep(100000);
    }
}

static void on_process_abort(void)
{
    ck_assert(asc_exit_status == EXIT_MAINLOOP);
}

START_TEST(blocked_thread)
{
    atexit(on_process_abort);

    asc_timer_t *timer = asc_timer_one_shot(1, on_block, NULL);
    ck_assert(timer != NULL);

    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    ck_abort_msg("didn't expect to reach this point");
}
END_TEST

/* callback queue and timers */
static unsigned int cb_remaining;

static void on_cb_timer(void *arg);

static void on_callback(void *arg)
{
    if (--cb_remaining > 0)
    {
        asc_timer_t *timer = asc_timer_one_shot(10, on_cb_timer, arg);
        ck_assert(timer != NULL);
    }
    else
    {
        asc_main_loop_shutdown();
    }
}

static void on_cb_timer(void *arg)
{
    asc_job_queue(NULL, on_callback, arg);
}

START_TEST(callback_simple)
{
    cb_remaining = 10;
    on_callback(NULL);

    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    ck_assert(cb_remaining == 0);
}
END_TEST

/* pruning the callback queue */
#define CB_OWNERS 20
#define CB_COUNT 100
#define CB_MARKER ~0U

static void on_last(void *arg)
{
    __uarg(arg);
    asc_main_loop_shutdown();
}

static void on_pruned(void *arg)
{
    unsigned int *cnt = (unsigned *)arg;
    (*cnt)++;
}

START_TEST(callback_prune)
{
    unsigned int cb_counters[CB_OWNERS] = { 0 };

    for (size_t i = 0; i < CB_COUNT; i++)
    {
        const size_t idx = rand() % CB_OWNERS;
        unsigned int *const cnt = &cb_counters[idx];
        asc_job_queue(cnt, on_pruned, cnt);
    }
    asc_job_queue(NULL, on_last, NULL);

    const size_t prune_idx = rand() % CB_OWNERS;
    unsigned int *const prune = &cb_counters[prune_idx];
    cb_counters[prune_idx] = CB_MARKER;
    asc_job_prune(prune);

    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    ck_assert(cb_counters[prune_idx] == CB_MARKER);
}
END_TEST

/* callback procedure cancelling the next ones */
#define BS_OWNER (void *)0xdeadbeef

static void on_backstab(void *arg)
{
    unsigned int *const i = (unsigned *)arg;

    if ((*i)++ == 0)
        asc_job_prune(BS_OWNER);
    else
        asc_main_loop_shutdown();
}

START_TEST(callback_cancel)
{
    unsigned int triggered = 0;

    for (size_t i = 0; i < 10; i++)
        asc_job_queue(BS_OWNER, on_backstab, &triggered);

    asc_job_queue(NULL, on_backstab, &triggered);

    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    ck_assert(triggered == 2);
}
END_TEST

Suite *core_mainloop(void)
{
    Suite *const s = suite_create("core/mainloop");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);

    if (can_fork != CK_NOFORK)
        tcase_set_timeout(tc, 5);

    tcase_add_test(tc, controls);
    tcase_add_test(tc, iterations);
    tcase_add_test(tc, callback_simple);
    tcase_add_test(tc, callback_prune);
    tcase_add_test(tc, callback_cancel);

    if (can_fork != CK_NOFORK)
    {
        tcase_add_exit_test(tc, exit_status, EXIT_TEST);
        tcase_add_exit_test(tc, blocked_thread, EXIT_MAINLOOP);
    }

    suite_add_tcase(s, tc);

    return s;
}
