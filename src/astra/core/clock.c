/*
 * Astra Core (Clock)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *                    2015, Artem Kharitonov <artem@sysert.ru>
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

#include <astra/astra.h>
#include <astra/core/clock.h>

uint64_t asc_utime(void)
{
#ifdef _WIN32
    FILETIME systime = { 0, 0 };
    GetSystemTimeAsFileTime(&systime);

    ULARGE_INTEGER large;
    large.LowPart = systime.dwLowDateTime;
    large.HighPart = systime.dwHighDateTime;

    return large.QuadPart / 10;
#elif defined(HAVE_CLOCK_GETTIME)
    struct timespec ts = { 0, 0 };

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
        clock_gettime(CLOCK_REALTIME, &ts);

    return (ts.tv_sec * 1000000ULL) + (ts.tv_nsec / 1000ULL);
#else
    struct timeval tv = { 0, 0 };

    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000ULL) + tv.tv_usec;
#endif
}

void asc_usleep(uint64_t usec)
{
#ifndef _WIN32
    struct timespec ts = {
        .tv_sec = usec / 1000000,
        .tv_nsec = (usec % 1000000) * 1000,
    };

    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        ; /* nothing */
#else
    LARGE_INTEGER ft;
    ft.QuadPart = -(usec * 10);

    const HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
#endif
}
