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

#include <astra/astra.h>
#include <astra/core/cond.h>

#ifndef _WIN32

/*
 * POSIX condition variables
 */

bool asc_cond_timedwait(asc_cond_t *cond, asc_mutex_t *mutex
                        , unsigned long ms)
{
    struct timespec ts = { 0, 0 };
    asc_rtctime(&ts, ms);

    const int ret = pthread_cond_timedwait(cond, mutex, &ts);
    ASC_ASSERT(ret == 0 || ret == ETIMEDOUT
               , "[core/cond] couldn't wait on condition: %s"
               , strerror(ret));

    return (ret == 0);
}

#else /* !_WIN32 */
#if _WIN32_WINNT < _WIN32_WINNT_VISTA

/*
 * Emulated condition variables for legacy Windows builds
 *
 * This is the same algorithm used in FFmpeg and x264.
 * See also: http://www.cs.wustl.edu/~schmidt/win32-cv-1.html
 */

/* in case we're a legacy build running on a modern OS */
static bool tried_nt6_cond = false;

typedef void (WINAPI *nt6_init_t)(asc_cond_t *);
static nt6_init_t nt6_init = NULL;

typedef void (WINAPI *nt6_signal_t)(asc_cond_t *);
static nt6_signal_t nt6_signal = NULL;

typedef void (WINAPI *nt6_broadcast_t)(asc_cond_t *);
static nt6_broadcast_t nt6_broadcast = NULL;

typedef BOOL (WINAPI *nt6_timedwait_t)(asc_cond_t *, asc_mutex_t *, DWORD);
static nt6_timedwait_t nt6_timedwait = NULL;

static
void load_nt6_cond(void)
{
    tried_nt6_cond = true;

    const HMODULE kern32 = GetModuleHandleW(L"kernel32.dll");
    if (kern32 == NULL) return;

    const nt6_init_t init =
        (nt6_init_t)GetProcAddress(kern32, "InitializeConditionVariable");
    if (init == NULL) return;

    const nt6_signal_t signal =
        (nt6_signal_t)GetProcAddress(kern32, "WakeConditionVariable");
    if (signal == NULL) return;

    const nt6_broadcast_t broadcast =
        (nt6_broadcast_t)GetProcAddress(kern32, "WakeAllConditionVariable");
    if (broadcast == NULL) return;

    const nt6_timedwait_t timedwait =
        (nt6_timedwait_t)GetProcAddress(kern32, "SleepConditionVariableCS");
    if (timedwait == NULL) return;

    nt6_init = init;
    nt6_signal = signal;
    nt6_broadcast = broadcast;
    nt6_timedwait = timedwait;
}

typedef struct
{
    asc_mutex_t broadcast_lock;
    asc_mutex_t waiter_count_lock;
    HANDLE semaphore;
    HANDLE waiters_done;
    volatile int waiter_count;
    volatile bool is_broadcast;
} emu_cond_t;

void asc_cond_init(asc_cond_t *cond)
{
    /*
     * Don't bother with the "run once" stuff FFmpeg seems to be doing.
     * No one's gonna be calling this from anywhere except the main thread.
     */
    if (!tried_nt6_cond)
        load_nt6_cond();

    if (nt6_init != NULL)
    {
        nt6_init(cond);
        return;
    }

    /* emulated condition variables */
    emu_cond_t *const emu = ASC_ALLOC(1, emu_cond_t);
    cond->Ptr = emu;

    emu->semaphore = CreateSemaphoreW(NULL, 0, 0x7fffffff, NULL);
    ASC_ASSERT(emu->semaphore != NULL
               , "[core/cond] CreateSemaphore() failed: %s", asc_error_msg());

    emu->waiters_done = CreateEventW(NULL, FALSE, FALSE, NULL);
    ASC_ASSERT(emu->waiters_done != NULL
               , "[core/cond] CreateEvent() failed: %s", asc_error_msg());

    asc_mutex_init(&emu->waiter_count_lock);
    asc_mutex_init(&emu->broadcast_lock);
}

void asc_cond_destroy(asc_cond_t *cond)
{
    if (nt6_init != NULL)
    {
        /* no cleanup needed */
        return;
    }

    /* emulated condition variables */
    emu_cond_t *const emu = (emu_cond_t *)cond->Ptr;

    CloseHandle(emu->semaphore);
    CloseHandle(emu->waiters_done);
    asc_mutex_destroy(&emu->waiter_count_lock);
    asc_mutex_destroy(&emu->broadcast_lock);

    free(emu);
}

void asc_cond_signal(asc_cond_t *cond)
{
    if (nt6_signal != NULL)
    {
        nt6_signal(cond);
        return;
    }

    /* emulated condition variables */
    emu_cond_t *const emu = (emu_cond_t *)cond->Ptr;

    asc_mutex_lock(&emu->broadcast_lock);

    asc_mutex_lock(&emu->waiter_count_lock);
    const bool have_waiter = (emu->waiter_count > 0);
    asc_mutex_unlock(&emu->waiter_count_lock);

    if (have_waiter)
    {
        ReleaseSemaphore(emu->semaphore, 1, NULL);
        WaitForSingleObject(emu->waiters_done, INFINITE);
    }

    asc_mutex_unlock(&emu->broadcast_lock);
}

void asc_cond_broadcast(asc_cond_t *cond)
{
    if (nt6_broadcast != NULL)
    {
        nt6_broadcast(cond);
        return;
    }

    /* emulated condition variables */
    emu_cond_t *const emu = (emu_cond_t *)cond->Ptr;

    asc_mutex_lock(&emu->broadcast_lock);

    bool have_waiter = false;
    asc_mutex_lock(&emu->waiter_count_lock);
    if (emu->waiter_count > 0)
    {
        emu->is_broadcast = true;
        have_waiter = true;
    }

    if (have_waiter)
    {
        ReleaseSemaphore(emu->semaphore, emu->waiter_count, NULL);
        asc_mutex_unlock(&emu->waiter_count_lock);
        WaitForSingleObject(emu->waiters_done, INFINITE);
        emu->is_broadcast = false;
    }
    else
    {
        asc_mutex_unlock(&emu->waiter_count_lock);
    }

    asc_mutex_unlock(&emu->broadcast_lock);
}

bool asc_cond_timedwait(asc_cond_t *cond, asc_mutex_t *mutex
                        , unsigned long ms)
{
    if (nt6_timedwait != NULL)
    {
        const BOOL ret = nt6_timedwait(cond, mutex, ms);
        ASC_ASSERT(ret || GetLastError() == ERROR_TIMEOUT
                   , "[core/cond] couldn't wait on condition: %s"
                   , asc_error_msg());

        return ret;
    }

    /* emulated condition variables */
    emu_cond_t *const emu = (emu_cond_t *)cond->Ptr;

    asc_mutex_lock(&emu->broadcast_lock);
    asc_mutex_lock(&emu->waiter_count_lock);
    emu->waiter_count++;
    asc_mutex_unlock(&emu->waiter_count_lock);
    asc_mutex_unlock(&emu->broadcast_lock);

    /* wait for signal with external mutex unlocked */
    asc_mutex_unlock(mutex);
    const DWORD ret = WaitForSingleObject(emu->semaphore, ms);
    const bool signaled = (ret == WAIT_OBJECT_0);

    ASC_ASSERT(signaled || ret == WAIT_TIMEOUT
               , "[core/cond] WaitForSingleObject() failed: %s"
               , asc_error_msg());

    bool last_waiter = false;
    asc_mutex_lock(&emu->waiter_count_lock);
    emu->waiter_count--;
    if (signaled && (emu->waiter_count == 0 || !emu->is_broadcast))
    {
        last_waiter = true;
    }
    asc_mutex_unlock(&emu->waiter_count_lock);

    if (last_waiter)
        SetEvent(emu->waiters_done);

    asc_mutex_lock(mutex);
    return signaled;
}

#endif /* _WIN32_WINNT < _WIN32_WINNT_VISTA */
#endif /* _WIN32 */
