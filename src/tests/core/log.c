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

#include <astra/core/thread.h>

#define TEST_LOG "logtest.txt"

static
int extra_fd = -1;

static
void extra_open(void)
{
    ck_assert(extra_fd == -1);
    extra_fd = open(TEST_LOG, O_RDONLY);
    ck_assert(extra_fd != -1);
}

static
void extra_close(void)
{
    ck_assert(extra_fd != -1);
    close(extra_fd);
    extra_fd = -1;
}

static
void setup(void)
{
    if (access(TEST_LOG, F_OK) == 0)
    {
        ck_assert(unlink(TEST_LOG) == 0);
    }

    asc_lib_init();
    asc_log_set_stdout(false);
    asc_log_set_file(TEST_LOG);
    extra_open();
}

static
void teardown(void)
{
    extra_close();
    asc_lib_destroy();
    ck_assert(unlink(TEST_LOG) == 0);
}

/* test whether debug setting works */
START_TEST(debug_flag)
{
    ck_assert(asc_log_is_debug() == false); /* defaults to false */
    asc_log_debug("expect this to be discarded");
    ck_assert(lseek(extra_fd, 0, SEEK_END) == 0);

    asc_log_set_debug(true);
    ck_assert(asc_log_is_debug() == true);
    asc_log_debug("should show up in the log");
    ck_assert(lseek(extra_fd, 0, SEEK_END) > 0);

    asc_log_set_debug(false);
    ck_assert(asc_log_is_debug() == false);
}
END_TEST

/* verify log file output */
static
void file_check(FILE *f, const char *msg)
{
    char buf[512] = { 0 };
    ck_assert(fgets(buf, sizeof(buf), f) != NULL);
    char *const p = strstr(buf, msg);
    ck_assert(p != NULL);
    ck_assert((p - buf) + strlen(p) == strlen(buf));
}

START_TEST(log_file)
{
    asc_log_set_debug(true);
    ck_assert(lseek(extra_fd, 0, SEEK_END) == 0);

    FILE *const f = fdopen(extra_fd, "rb");
    ck_assert(f != NULL);

    asc_log_error("test error message");
    file_check(f, "ERROR: test error message\n");

    asc_log_warning("test warning message");
    file_check(f, "WARNING: test warning message\n");

    asc_log_info("test info message");
    file_check(f, "INFO: test info message\n");

    asc_log_debug("test debug message");
    file_check(f, "DEBUG: test debug message\n");

    ck_assert(fclose(f) == 0);
}
END_TEST

/* multiple threads logging at the same time */
#define THREAD_COUNT 32
#define MESSAGES_PER_THREAD 32

static
void log_proc(void *arg)
{
    const unsigned int thread_id = (unsigned)((intptr_t)arg);

    for (unsigned int i = 0; i < MESSAGES_PER_THREAD; i++)
    {
        asc_log_info("%u: message %u", thread_id, i);
    }
}

START_TEST(threaded)
{
    asc_log_set_debug(true);
    ck_assert(lseek(extra_fd, 0, SEEK_END) == 0);

    /* run logging threads */
    asc_thread_t *thr[THREAD_COUNT] = { NULL };
    for (size_t i = 0; i < ASC_ARRAY_SIZE(thr); i++)
    {
        thr[i] = asc_thread_init((void *)i, log_proc, NULL);
        ck_assert(thr[i] != NULL);
    }

    for (size_t i = 0; i < ASC_ARRAY_SIZE(thr); i++)
    {
        asc_thread_join(thr[i]);
    }

    /* verify log contents */
    FILE *const f = fdopen(extra_fd, "rb");
    ck_assert(f != NULL);

    unsigned int msg[THREAD_COUNT] = { 0 };
    char buf[512];

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        static const char mark[] = ": INFO: ";
        char *p = strstr(buf, mark);
        ck_assert(p != NULL);
        p += strlen(mark);

        unsigned int thread_id = 0;
        unsigned int msg_id = 0;

        const int ret = sscanf(p, "%u: message %u", &thread_id, &msg_id);
        ck_assert(ret == 2);
        ck_assert(thread_id < THREAD_COUNT);
        ck_assert(msg_id < MESSAGES_PER_THREAD);
        ck_assert(msg[thread_id] == msg_id);

        msg[thread_id]++;
    }

    for (size_t i = 0; i < ASC_ARRAY_SIZE(msg); i++)
        ck_assert(msg[i] == MESSAGES_PER_THREAD);

    ck_assert(fclose(f) == 0);
}
END_TEST

Suite *core_log(void)
{
    Suite *const s = suite_create("core/log");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, debug_flag);
    tcase_add_test(tc, log_file);
    tcase_add_test(tc, threaded);

    suite_add_tcase(s, tc);

    return s;
}
