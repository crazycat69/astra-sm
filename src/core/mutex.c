/*
 * Astra Core (Mutex)
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

#include <astra.h>
#include <core/mutex.h>

bool asc_mutex_timedlock(asc_mutex_t *mutex, unsigned int ms)
#ifdef HAVE_PTHREAD_MUTEX_TIMEDLOCK
{
    /* use native OS implementation */
    struct timespec ts = { 0, 0 };

#ifdef HAVE_CLOCK_GETTIME
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
#endif
    {
        struct timeval tv = { 0, 0 };

        if (gettimeofday(&tv, NULL) == 0)
        {
            ts.tv_sec = tv.tv_sec;
            ts.tv_nsec = tv.tv_usec * 1000L;
        }
        else
        {
            /* last resort; this won't work well with <1s timeouts */
            ts.tv_sec = time(NULL);
        }
    }

    /* try not to overflow tv_nsec */
    ts.tv_sec += (ms / 1000);
    ts.tv_nsec += (ms % 1000) * 1000000;
    ts.tv_sec += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;

    const int ret = pthread_mutex_timedlock(mutex, &ts);
    if (ret == 0)
        return true;

    asc_assert(ret == ETIMEDOUT, "[core/mutex] couldn't lock mutex: %s"
               , strerror(ret));

    return false;
}
#else /* HAVE_PTHREAD_MUTEX_TIMEDLOCK */
{
    /* sleep and try to lock until we hit the timeout */
    const uint64_t us = ms * 1000ULL;
    const uint64_t timeout = asc_utime() + us;

    while (true)
    {
#ifdef _WIN32
        const bool ret = TryEnterCriticalSection(mutex);
        if (ret)
            return true;
#else /* _WIN32 */
        const int ret = pthread_mutex_trylock(mutex);
        if (ret == 0)
            return true;

        asc_assert(ret == EBUSY, "[core/mutex] couldn't lock mutex: %s"
                   , strerror(ret));
#endif /* !_WIN32 */

        /* check for timeout and time travel */
        const uint64_t now = asc_utime();
        if (now > timeout || timeout > now + us)
            break;

        asc_usleep(1000); /* 1ms */
    }

    return false;
}
#endif /* !HAVE_PTHREAD_MUTEX_TIMEDLOCK */
