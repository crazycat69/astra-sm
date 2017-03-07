/*
 * Astra Core (Mutex)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015-2017, Artem Kharitonov <artem@3phase.pw>
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
#include <astra/core/mutex.h>

#if !defined(_WIN32) && defined(HAVE_PTHREAD_MUTEX_TIMEDLOCK)

bool asc_mutex_timedlock(asc_mutex_t *mutex, unsigned long ms)
{
    /* use native OS implementation */
    struct timespec ts = { 0, 0 };
    asc_rtctime(&ts, ms);

    const int ret = pthread_mutex_timedlock(mutex, &ts);
    ASC_ASSERT(ret == 0 || ret == ETIMEDOUT
               , "[core/mutex] couldn't lock mutex: %s"
               , strerror(ret));

    return (ret == 0);
}

#else /* !_WIN32 && HAVE_PTHREAD_MUTEX_TIMEDLOCK */

bool asc_mutex_timedlock(asc_mutex_t *mutex, unsigned long ms)
{
    /* spin until we hit the timeout */
    const uint64_t us = ms * 1000ULL;
    const uint64_t timeout = asc_utime() + us;

    bool ret;
    while (!(ret = asc_mutex_trylock(mutex)))
    {
        /* check for timeout and time travel */
        const uint64_t now = asc_utime();
        if (now > timeout || timeout > now + us)
            break;

        asc_usleep(1000); /* 1ms */
    }

    return ret;
}

#endif /* _WIN32 || !HAVE_PTHREAD_MUTEX_TIMEDLOCK */
