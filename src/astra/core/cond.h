/*
 * Astra Core (Condition variable)
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

#ifndef _ASC_COND_H_
#define _ASC_COND_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

#include <astra/core/mutex.h>

#ifndef _WIN32

/*
 * POSIX condition variables
 */

#include <pthread.h>

typedef pthread_cond_t asc_cond_t;

static inline
void asc_cond_init(asc_cond_t *cond)
{
    const int ret = pthread_cond_init(cond, NULL);
    asc_assert(ret == 0, "[core/cond] couldn't init condition: %s"
               , strerror(ret));
}

static inline
void asc_cond_destroy(asc_cond_t *cond)
{
    const int ret = pthread_cond_destroy(cond);
    asc_assert(ret == 0, "[core/cond] couldn't destroy condition: %s"
               , strerror(ret));
}

static inline
void asc_cond_signal(asc_cond_t *cond)
{
    const int ret = pthread_cond_signal(cond);
    asc_assert(ret == 0, "[core/cond] couldn't signal condition: %s"
               , strerror(ret));
}

static inline
void asc_cond_broadcast(asc_cond_t *cond)
{
    const int ret = pthread_cond_broadcast(cond);
    asc_assert(ret == 0, "[core/cond] couldn't broadcast condition: %s"
               , strerror(ret));
}

static inline
void asc_cond_wait(asc_cond_t *cond, asc_mutex_t *mutex)
{
    const int ret = pthread_cond_wait(cond, mutex);
    asc_assert(ret == 0, "[core/cond] couldn't wait on condition: %s"
               , strerror(ret));
}

bool asc_cond_timedwait(asc_cond_t *cond, asc_mutex_t *mutex
                        , unsigned long ms);

#else /* !_WIN32 */

typedef CONDITION_VARIABLE asc_cond_t;

#if _WIN32_WINNT >= _WIN32_WINNT_VISTA

/*
 * Native Windows condition variable support (>= Vista)
 */

static inline
void asc_cond_init(asc_cond_t *cond)
{
    InitializeConditionVariable(cond);
}

static inline
void asc_cond_destroy(asc_cond_t *cond)
{
    /* no cleanup needed */
    __uarg(cond);
}

static inline
void asc_cond_signal(asc_cond_t *cond)
{
    WakeConditionVariable(cond);
}

static inline
void asc_cond_broadcast(asc_cond_t *cond)
{
    WakeAllConditionVariable(cond);
}

static inline
void asc_cond_wait(asc_cond_t *cond, asc_mutex_t *mutex)
{
    const BOOL ret = SleepConditionVariableCS(cond, mutex, INFINITE);
    asc_assert(ret
               , "[core/cond] couldn't wait on condition: %s"
               , asc_error_msg());
}

static inline
bool asc_cond_timedwait(asc_cond_t *cond, asc_mutex_t *mutex
                        , unsigned long ms)
{
    const BOOL ret = SleepConditionVariableCS(cond, mutex, ms);
    asc_assert(ret || GetLastError() == ERROR_TIMEOUT
               , "[core/cond] couldn't wait on condition: %s"
               , asc_error_msg());

    return ret;
}

#else /* _WIN32_WINNT >= _WIN32_WINNT_VISTA */

/*
 * Emulated condition variables for legacy Windows builds
 */

void asc_cond_init(asc_cond_t *cond);
void asc_cond_destroy(asc_cond_t *cond);
void asc_cond_signal(asc_cond_t *cond);
void asc_cond_broadcast(asc_cond_t *cond);
bool asc_cond_timedwait(asc_cond_t *cond, asc_mutex_t *mutex
                        , unsigned long ms);

static inline
void asc_cond_wait(asc_cond_t *cond, asc_mutex_t *mutex)
{
    const bool ret = asc_cond_timedwait(cond, mutex, INFINITE);
    asc_assert(ret, "[core/cond] couldn't wait on condition");
}

#endif /* _WIN32_WINNT < _WIN32_WINNT_VISTA */
#endif /* _WIN32 */

#endif /* _ASC_COND_H_ */
