/*
 * Astra Core (Clock)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *               2015-2017, Artem Kharitonov <artem@3phase.pw>
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

/* return number of microseconds since an unspecified point in time */
uint64_t asc_utime(void)
{
#ifdef _WIN32
    FILETIME systime = { 0, 0 };
    GetSystemTimeAsFileTime(&systime);

    ULARGE_INTEGER large;
    large.LowPart = systime.dwLowDateTime;
    large.HighPart = systime.dwHighDateTime;

    return large.QuadPart / 10ULL;
#else /* _WIN32 */
#ifdef HAVE_CLOCK_GETTIME
    struct timespec ts = { 0, 0 };

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        clock_gettime(CLOCK_REALTIME, &ts);

    return (ts.tv_sec * 1000000ULL) + (ts.tv_nsec / 1000ULL);
#else
    struct timeval tv = { 0, 0 };

    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000ULL) + tv.tv_usec;
#endif /* !HAVE_CLOCK_GETTIME */
#endif /* !_WIN32 */
}

/* block calling thread for `usec' or more microseconds */
void asc_usleep(uint64_t usec)
{
#ifndef _WIN32
    struct timespec ts =
    {
        .tv_sec = usec / 1000000L,
        .tv_nsec = (usec % 1000000L) * 1000L,
    };

    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        ; /* nothing */
#else
    const HANDLE timer = CreateWaitableTimerW(NULL, TRUE, NULL);
    if (timer != NULL)
    {
        const LARGE_INTEGER ft =
        {
            .QuadPart = -(usec * 10),
        };

        SetWaitableTimer(timer, &ft, 0, NULL, NULL, FALSE);
        WaitForSingleObject(timer, INFINITE);
        CloseHandle(timer);
    }
#endif
}

#ifndef _WIN32
/* get RTC timestamp `offset_ms' milliseconds into the future */
void asc_rtctime(struct timespec *ts, unsigned long offset_ms)
{
#ifdef HAVE_CLOCK_GETTIME
    if (clock_gettime(CLOCK_REALTIME, ts) != 0)
#endif
    {
        struct timeval tv = { 0, 0 };

        if (gettimeofday(&tv, NULL) == 0)
        {
            ts->tv_sec = tv.tv_sec;
            ts->tv_nsec = tv.tv_usec * 1000L;
        }
        else
        {
            /* last resort; this won't work well with <1s offsets */
            ts->tv_sec = time(NULL);
            if (offset_ms > 0 && offset_ms < 1000)
                offset_ms += 2000;
        }
    }

    if (offset_ms > 0)
    {
        /* try not to overflow tv_nsec */
        ts->tv_sec += (offset_ms / 1000L);
        ts->tv_nsec += (offset_ms % 1000L) * 1000000L;
        ts->tv_sec += ts->tv_nsec / 1000000000L;
        ts->tv_nsec %= 1000000000L;
    }
}
#endif /* !_WIN32 */
