/*
 * Astra Unit Tests
 * http://cesbo.com/astra
 *
 * Copyright (C) 2017, Artem Kharitonov <artem@3phase.pw>
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

#include <astra/core/timer.h>
#include <astra/core/thread.h>
#include <astra/core/mutex.h>
#include <astra/core/cond.h>
#include <astra/core/mainloop.h>

/* compile-time assertions */
ASC_STATIC_ASSERT(1 != 2);
ASC_STATIC_ASSERT(sizeof(char) > 0);
ASC_STATIC_ASSERT(true == true);

/* true assertions */
START_TEST(good_assert)
{
    const size_t len = 16;

    int *a = ASC_ALLOC(len, int);
    ASC_ASSERT(a != NULL, "expected ASC_ALLOC to succeed");

    for (size_t i = 0; i < len; i++)
        ASC_ASSERT(a[i] == 0, "expected initialized memory");

    ASC_FREE(a, free);
    ASC_ASSERT(a == NULL, "expected pointer to be NULL'd");
}
END_TEST

/* false assertion */
START_TEST(bad_assert)
{
    int *const a = NULL;
    ASC_ASSERT(a != NULL, "this is expected to fail");
    ck_abort();
}
END_TEST

/* false assertion inside auxiliary thread */
static asc_thread_t *asrt_thr = NULL;
static asc_timer_t *asrt_timer = NULL;
static asc_cond_t asrt_cond;
static asc_mutex_t asrt_mutex;

static void asrt_proc(void *arg)
{
    asc_mutex_lock(&asrt_mutex);
    ck_assert(asc_cond_timedwait(&asrt_cond, &asrt_mutex, 1000) == true);
    ASC_ASSERT(arg != NULL, "this is expected to fail");
    ck_abort();
}

static void on_asrt_timer(void *arg)
{
    ASC_UNUSED(arg);

    asc_mutex_lock(&asrt_mutex);
    asc_cond_signal(&asrt_cond);
    asc_mutex_unlock(&asrt_mutex);
}

START_TEST(thread_assert)
{
    lib_setup();

    asc_cond_init(&asrt_cond);
    asc_mutex_init(&asrt_mutex);

    asrt_thr = asc_thread_init(NULL, asrt_proc, NULL);
    ck_assert(asrt_thr != NULL);

    asrt_timer = asc_timer_one_shot(300, on_asrt_timer, NULL);
    ck_assert(asrt_timer != NULL);

    ck_assert(asc_main_loop_run() == false); /* shouldn't return */
    ck_abort();
}
END_TEST

Suite *core_assert(void)
{
    Suite *const s = suite_create("core/assert");
    TCase *const tc = tcase_create("default");

    tcase_add_test(tc, good_assert);

    if (can_fork != CK_NOFORK)
    {
        tcase_add_exit_test(tc, bad_assert, EXIT_ABORT);
        tcase_add_exit_test(tc, thread_assert, EXIT_ABORT);
    }

    suite_add_tcase(s, tc);

    return s;
}
