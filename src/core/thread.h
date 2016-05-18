/*
 * Astra Core (Auxiliary threads)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *               2015-2016, Artem Kharitonov <artem@3phase.pw>
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

#ifndef _ASC_THREAD_H_
#define _ASC_THREAD_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

#ifndef _WIN32
#   include <pthread.h>
#endif

typedef struct asc_thread_t asc_thread_t;
typedef struct asc_thread_buffer_t asc_thread_buffer_t;
typedef void (*thread_callback_t)(void *);

void asc_wake_open(void);
void asc_wake_close(void);
void asc_wake(void);

void asc_thread_core_init(void);
void asc_thread_core_destroy(void);

asc_thread_t *asc_thread_init(void) __wur;
void asc_thread_start(asc_thread_t *thr, void *arg, thread_callback_t proc
                      , thread_callback_t on_read, asc_thread_buffer_t *buffer
                      , thread_callback_t on_close);
void asc_thread_join(asc_thread_t *thr);

asc_thread_buffer_t *asc_thread_buffer_init(size_t buffer_size) __wur;
void asc_thread_buffer_destroy(asc_thread_buffer_t *buffer);

void asc_thread_buffer_flush(asc_thread_buffer_t *buffer);
ssize_t asc_thread_buffer_read(asc_thread_buffer_t *buffer
                               , void *data, size_t size) __wur;
ssize_t asc_thread_buffer_write(asc_thread_buffer_t *buffer
                                , const void *data, size_t size) __wur;

#ifdef _WIN32

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

static inline
void asc_mutex_unlock(asc_mutex_t *mutex)
{
    LeaveCriticalSection(mutex);
}

#else /* _WIN32 */

typedef pthread_mutex_t asc_mutex_t;

static inline
void asc_mutex_init(asc_mutex_t *mutex)
{
    const int ret = pthread_mutex_init(mutex, NULL);
    asc_assert(ret == 0, "[core/thread] couldn't init mutex: %s"
               , strerror(ret));
}

static inline
void asc_mutex_destroy(asc_mutex_t *mutex)
{
    const int ret = pthread_mutex_destroy(mutex);
    asc_assert(ret == 0, "[core/thread] couldn't destroy mutex: %s"
               , strerror(ret));
}

static inline
void asc_mutex_lock(asc_mutex_t *mutex)
{
    const int ret = pthread_mutex_lock(mutex);
    asc_assert(ret == 0, "[core/thread] couldn't lock mutex: %s"
               , strerror(ret));
}

static inline
void asc_mutex_unlock(asc_mutex_t *mutex)
{
    const int ret = pthread_mutex_unlock(mutex);
    asc_assert(ret == 0, "[core/thread] couldn't unlock mutex: %s"
               , strerror(ret));
}

#endif /* !_WIN32 */

#endif /* _ASC_THREAD_H_ */
