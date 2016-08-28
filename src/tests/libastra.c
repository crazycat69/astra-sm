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

#include "libastra.h"

enum fork_status can_fork;

#define TIME_SAMPLE_COUNT 50
#define TIME_RES_MINIMUM 25000 /* 25ms (less is higher) */

unsigned int get_timer_res(void)
{
    uint64_t total = 0;

    for (unsigned int i = 0; i < TIME_SAMPLE_COUNT; i++)
    {
        const uint64_t time_a = asc_utime();
        asc_usleep(2000);

        const uint64_t time_b = asc_utime();
        ck_assert_msg(time_b > time_a, "Time did not increase");

        const uint64_t duration = time_b - time_a;
        total += duration;
    }

    const uint64_t mean = (total / TIME_SAMPLE_COUNT);
    ck_assert_msg(mean > 1000 && mean < TIME_RES_MINIMUM
                  , "System timer resolution is too low");

    return mean;
}

void lib_setup(void)
{
    asc_srand();
    asc_lib_init();

    /* don't clutter output with library messages */
    asc_log_set_debug(true);
    asc_log_set_stdout(false);
    asc_log_set_file("./libastra.log");
}

void lib_teardown(void)
{
    asc_lib_destroy();
}

static void redirect_output(void)
{
    const char *const fd_str = getenv("OUTPUT_REDIRECT_FD");
    if (fd_str == NULL)
        return;

    const int fd = atoi(fd_str);
    if (fd <= STDERR_FILENO)
    {
        fprintf(stderr, "invalid fd: %s\n", fd_str);
        exit(EXIT_FAILURE);
    }

    if (dup2(fd, STDOUT_FILENO) != STDOUT_FILENO
        || dup2(fd, STDERR_FILENO) != STDERR_FILENO)
    {
        fprintf(stderr, "couldn't redirect output to fd %d: %s\n"
                , fd, strerror(errno));

        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    redirect_output();

    int failed = 0, total = 0;
    for (suite_func_t *p = suite_list; *p != NULL; p++)
    {
        SRunner *const sr = srunner_create((*p)());
        can_fork = srunner_fork_status(sr);
        srunner_run_all(sr, CK_VERBOSE);
        failed += srunner_ntests_failed(sr);
        total += srunner_ntests_run(sr);
        srunner_free(sr);
    }

    printf("\n%d out of %d tests passed\n", total - failed, total);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
