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

#include "test_libastra.h"

enum fork_status can_fork;

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
