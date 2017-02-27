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

#ifndef _ASC_MUTEX_H_
#define _ASC_MUTEX_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

#ifndef _WIN32

#include <pthread.h>

typedef pthread_mutex_t asc_mutex_t;

static inline
void asc_mutex_init(asc_mutex_t *mutex)
{
    const int ret = pthread_mutex_init(mutex, NULL);
    asc_assert(ret == 0, "[core/mutex] couldn't init mutex: %s"
               , strerror(ret));
}

static inline
void asc_mutex_destroy(asc_mutex_t *mutex)
{
    const int ret = pthread_mutex_destroy(mutex);
    asc_assert(ret == 0, "[core/mutex] couldn't destroy mutex: %s"
               , strerror(ret));
}

static inline
void asc_mutex_lock(asc_mutex_t *mutex)
{
    const int ret = pthread_mutex_lock(mutex);
    asc_assert(ret == 0, "[core/mutex] couldn't lock mutex: %s"
               , strerror(ret));
}

static inline __wur
bool asc_mutex_trylock(asc_mutex_t *mutex)
{
    const int ret = pthread_mutex_trylock(mutex);
    asc_assert(ret == 0 || ret == EBUSY
               , "[core/mutex] couldn't lock mutex: %s"
               , strerror(ret));

    return (ret == 0);
}

static inline
void asc_mutex_unlock(asc_mutex_t *mutex)
{
    const int ret = pthread_mutex_unlock(mutex);
    asc_assert(ret == 0, "[core/mutex] couldn't unlock mutex: %s"
               , strerror(ret));
}

#else /* !_WIN32 */

typedef CRITICAL_SECTION asc_mutex_t;

static inline
void asc_mutex_init(asc_mutex_t *mutex)
{
    InitializeCriticalSection(mutex);
}

static inline
void asc_mutex_destroy(asc_mutex_t *mutex)
{
    DeleteCriticalSection(mutex);
}

static inline
void asc_mutex_lock(asc_mutex_t *mutex)
{
    EnterCriticalSection(mutex);
}

static inline __wur
bool asc_mutex_trylock(asc_mutex_t *mutex)
{
    return TryEnterCriticalSection(mutex);
}

static inline
void asc_mutex_unlock(asc_mutex_t *mutex)
{
    LeaveCriticalSection(mutex);
}

#endif /* _WIN32 */

bool asc_mutex_timedlock(asc_mutex_t *mutex, unsigned long ms) __wur;

#endif /* _ASC_MUTEX_H_ */
