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
#include <core/child.h>
#include <core/mainloop.h>

#ifndef _WIN32
#   include <signal.h>
#endif /* !_WIN32 */

#define TEST_SLAVE "./test_slave"

static void fail_on_read(void *arg, const void *buf, size_t len)
{
    __uarg(arg);
    __uarg(buf);
    __uarg(len);

    ck_abort_msg("unexpected read event");
}

static void fail_on_close(void *arg, int status)
{
    __uarg(arg);
    __uarg(status);

    ck_abort_msg("unexpected close event");
}

/* get child's pid */
static asc_child_t *pid_child = NULL;
static pid_t pid_value = -1;

static void pid_on_read(void *arg, const void *buf, size_t len)
{
    ck_assert(arg == (void *)0x1234);

    char pid_str[32] = { 0 };
    int ret = snprintf(pid_str, sizeof(pid_str), "%lld", (long long)pid_value);
    ck_assert(ret > 0);

    /* line-buffered mode removes newlines at the end of each string */
    ck_assert(strlen((char *)buf) == len);
    ck_assert(strlen(pid_str) == len);
    ck_assert(strcmp(pid_str, (char *)buf) == 0);

    /*
     * NOTE: calling asc_child_close() *OR* asc_child_destroy() directly
     *       from inside an event handler is dangerous: it could free()
     *       the child before the buffering routine in core/child has a
     *       chance to complete its work and return.
     */
    asc_job_queue(NULL, (loop_callback_t)asc_child_close, pid_child);
}

static void pid_on_close(void *arg, int status)
{
#ifdef _WIN32
    ck_assert(status == (int)STATUS_CONTROL_C_EXIT);
#else /* WIN32 */
    ck_assert(status == (128 + SIGTERM));
#endif /* !_WIN32 */

    ck_assert(arg == (void *)0x1234);

    pid_child = NULL;
    asc_main_loop_shutdown();
}

START_TEST(read_pid)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_pid";
    cfg.command = TEST_SLAVE " pid";
    cfg.sout.mode = cfg.serr.mode = CHILD_IO_TEXT;
    cfg.sout.on_flush = pid_on_read;
    cfg.serr.on_flush = fail_on_read; /* not expecting stderr */
    cfg.on_close = pid_on_close;
    cfg.arg = (void *)0x1234;

    pid_child = asc_child_init(&cfg);
    ck_assert(pid_child != NULL);

    pid_value = asc_child_pid(pid_child);
    ck_assert(pid_value > 0);

    ck_assert(asc_main_loop_run() == false);
    ck_assert(pid_child == NULL);
}
END_TEST

/* terminate unresponsive child */
#define BANDIT_TIME (1.5 * 1000000) /* 1.5 sec */

static asc_child_t *bandit_child = NULL;
static uint64_t bandit_time = 0;

static void bandit_on_read(void *arg, const void *buf, size_t len)
{
    ck_assert(len == 4 && strcmp("peep", (char *)buf) == 0);

    if (arg != NULL)
    {
        bandit_time = asc_utime();
        asc_job_queue(NULL, (loop_callback_t)asc_child_close, bandit_child);
    }
    else
    {
        asc_main_loop_shutdown();
    }
}

static void bandit_on_close(void *arg, int status)
{
    __uarg(arg);

#ifdef _WIN32
    ck_assert(status == EXIT_FAILURE);
#else /* _WIN32 */
    ck_assert(status == (128 + SIGKILL));
#endif /* !_WIN32 */

    /* forced shutdown should take around 1.5sec */
    const uint64_t bench = asc_utime() - bandit_time;
    ck_assert((BANDIT_TIME * 0.7) <= bench
              && bench <= (BANDIT_TIME * 1.3));

    bandit_child = NULL;
    asc_main_loop_shutdown();
}

START_TEST(bandit_no_block)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_bandit";
    cfg.command = TEST_SLAVE " bandit";
    cfg.sout.mode = cfg.serr.mode = CHILD_IO_TEXT;
    cfg.sout.on_flush = fail_on_read; /* not expecting stdout */
    cfg.serr.on_flush = bandit_on_read;
    cfg.on_close = bandit_on_close;
    cfg.arg = (void *)0x1234;

    /* normal kill via main loop */
    bandit_child = asc_child_init(&cfg);
    ck_assert(bandit_child != NULL);

    ck_assert(asc_main_loop_run() == false);
    ck_assert(bandit_child == NULL);
}
END_TEST

START_TEST(bandit_block)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_bandit";
    cfg.command = TEST_SLAVE " bandit";
    cfg.sout.mode = cfg.serr.mode = CHILD_IO_TEXT;
    cfg.sout.on_flush = fail_on_read; /* not expecting stdout */
    cfg.serr.on_flush = bandit_on_read;
    cfg.on_close = fail_on_close; /* or close event */
    cfg.arg = NULL;

    /* start child and wait until it disables signals */
    bandit_child = asc_child_init(&cfg);
    ck_assert(bandit_child != NULL);

    ck_assert(asc_main_loop_run() == false);

    /* do blocking kill */
    bandit_time = asc_utime();
    asc_child_destroy(bandit_child);
    bandit_child = NULL;

    const uint64_t bench = asc_utime() - bandit_time;
    ck_assert((BANDIT_TIME * 0.7) <= bench
              && bench <= (BANDIT_TIME * 1.3));
}
END_TEST

/* stdio pipes closed on far side */
static void far_on_close(void *arg, int status)
{
    __uarg(arg);

    /*
     * NOTE: whenever an stdio pipe is closed on the far side,
     *       core/child should terminate the process if it hadn't
     *       already quit.
     */
#ifdef _WIN32
    ck_assert(status == (int)STATUS_CONTROL_C_EXIT);
#else /* _WIN32 */
    ck_assert(status == (128 + SIGTERM));
#endif /* !_WIN32 */

    asc_main_loop_shutdown();
}

START_TEST(far_close)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_close";
    cfg.command = TEST_SLAVE " close";
    cfg.sout.mode = cfg.serr.mode = CHILD_IO_TEXT;
    cfg.sout.on_flush = cfg.serr.on_flush = fail_on_read;
    cfg.on_close = far_on_close;

    asc_child_t *const child = asc_child_init(&cfg);
    ck_assert(child != NULL);

    ck_assert(asc_main_loop_run() == false);
}
END_TEST

Suite *core_child(void)
{
    Suite *const s = suite_create("child");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);

#ifndef _WIN32
    if (can_fork != CK_NOFORK)
        tcase_set_timeout(tc, 10);
#endif /* !_WIN32 */

    tcase_add_test(tc, read_pid);
    tcase_add_test(tc, bandit_no_block);
    tcase_add_test(tc, bandit_block);
    tcase_add_test(tc, far_close);

    suite_add_tcase(s, tc);

    return s;
}
