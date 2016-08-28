/*
 * Astra: Unit tests
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

#include "../test_libastra.h"

START_TEST(func_asc_utime)
{
    uint64_t last = asc_utime();
    ck_assert_msg(last != 0, "asc_utime() returned zero");

    for (size_t i = 0; i < 5; i++)
    {
        usleep(10000);

        const uint64_t now = asc_utime();
        ck_assert_msg(now > last, "Time did not increase");

        last = now;
    }
}
END_TEST

START_TEST(func_asc_usleep)
{
    const unsigned res = get_timer_res();

    for (size_t i = 1; i <= 5; i++)
    {
        const uint64_t usecs = i * res;

        const uint64_t time_a = asc_utime();
        asc_usleep(usecs);

        const uint64_t time_b = asc_utime();
        ck_assert_msg(time_b > time_a, "Time did not increase");

        const uint64_t duration = time_b - time_a;
        ck_assert_msg(duration >= (usecs * 0.9) && duration <= (usecs * 2.1)
                      , "Requested %uus sleep, got %" PRIu64 "us"
                      , usecs, duration);
    }
}
END_TEST

Suite *core_clock(void)
{
    Suite *const s = suite_create("core/clock");

    TCase *const tc = tcase_create("default");
    tcase_add_test(tc, func_asc_utime);
    tcase_add_test(tc, func_asc_usleep);
    suite_add_tcase(s, tc);

    return s;
}
