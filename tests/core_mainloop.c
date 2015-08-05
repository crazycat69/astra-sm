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

/* basic shutdown and reload commands */
START_TEST(controls)
{
    astra_shutdown();
    bool again = asc_main_loop_run();
    ck_assert_msg(again == false, "expected shutdown");

    astra_reload();
    again = asc_main_loop_run();
    ck_assert_msg(again == true, "expected restart");
}
END_TEST

/* immediate exit procedure */
#define EXIT_TEST 123

static __noreturn void on_exit_timer(void *arg)
{
    __uarg(arg);
    astra_exit(EXIT_TEST);
}

static void on_process_exit(void)
{
    ck_assert(astra_exit_status == EXIT_TEST);
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
        astra_shutdown();
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

/* block thread then call astra_shutdown() until it aborts */
#define EXIT_ABORT 2

static void on_block(void *arg)
{
    __uarg(arg);

    while (true)
    {
        astra_shutdown();
        asc_usleep(100000);
    }
}

static void on_process_abort(void)
{
    ck_assert(astra_exit_status == EXIT_ABORT);
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

Suite *core_mainloop(void)
{
    Suite *const s = suite_create("mainloop");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, setup, teardown);
#ifndef _WIN32
    tcase_set_timeout(tc, 5);
#endif

    tcase_add_test(tc, controls);
    tcase_add_exit_test(tc, exit_status, EXIT_TEST);
    tcase_add_test(tc, iterations);
    tcase_add_exit_test(tc, blocked_thread, EXIT_ABORT);

    suite_add_tcase(s, tc);

    return s;
}
