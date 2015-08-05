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

enum fork_status can_fork;

int main(void)
{
    SRunner *sr = NULL;

    for (suite_func_t *p = suite_list; *p != NULL; p++)
    {
        if (sr == NULL)
        {
            sr = srunner_create((*p)());
            can_fork = srunner_fork_status(sr);
        }
        else
            srunner_add_suite(sr, (*p)());
    }

    srunner_run_all(sr, CK_VERBOSE);
    const int failed = srunner_ntests_failed(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
