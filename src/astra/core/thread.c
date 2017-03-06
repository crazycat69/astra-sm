/*
 * Astra Core (Auxiliary thread)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
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
#include <astra/core/thread.h>
#include <astra/core/mutex.h>
#include <astra/core/list.h>
#include <astra/core/mainloop.h>

#define MSG(_msg) "[core/thread %p] " _msg, (void *)thr

struct asc_thread_t
{
    thread_callback_t proc;
    thread_callback_t on_close;
    void *arg;

#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t *thread;
#endif
};

typedef struct
{
    asc_list_t *list;
} asc_thread_mgr_t;

static
asc_thread_mgr_t *thread_mgr = NULL;

void asc_thread_core_init(void)
{
    thread_mgr = ASC_ALLOC(1, asc_thread_mgr_t);
    thread_mgr->list = asc_list_init();
}

void asc_thread_core_destroy(void)
{
    if (thread_mgr == NULL)
        return;

    asc_thread_t *thr, *prev = NULL;
    asc_list_till_empty(thread_mgr->list)
    {
        thr = (asc_thread_t *)asc_list_data(thread_mgr->list);
        ASC_ASSERT(thr != prev, MSG("on_close didn't join thread"));

        if (thr->on_close != NULL)
            thr->on_close(thr->arg);
        else
            asc_thread_join(thr);

        prev = thr;
    }

    ASC_FREE(thread_mgr->list, asc_list_destroy);
    ASC_FREE(thread_mgr, free);
}

/* this is queued to main loop when thread exits */
static
void on_thread_exit(void *arg)
{
    asc_thread_t *const thr = (asc_thread_t *)arg;

    if (thr->on_close != NULL)
        thr->on_close(thr->arg);
    else
        asc_thread_join(thr);
}

/* wrapper around thread function */
#ifdef _WIN32
static __stdcall
unsigned int thread_proc(void *arg)
#else
static
void *thread_proc(void *arg)
#endif
{
    asc_thread_t *const thr = (asc_thread_t *)arg;

    thr->proc(thr->arg);
    asc_job_queue(thr, on_thread_exit, thr);

    return 0;
}

asc_thread_t *asc_thread_init(void *arg, thread_callback_t proc
                              , thread_callback_t on_close)
{
    asc_thread_t *const thr = ASC_ALLOC(1, asc_thread_t);
    asc_list_insert_tail(thread_mgr->list, thr);

    thr->proc = proc;
    thr->on_close = on_close;
    thr->arg = arg;

#ifdef _WIN32
    const intptr_t ret = _beginthreadex(NULL, 0, thread_proc, thr, 0, NULL);
    ASC_ASSERT(ret > 0, MSG("failed to create thread: %s"), strerror(errno));

    thr->thread = (HANDLE)ret;
#else /* _WIN32 */
    thr->thread = ASC_ALLOC(1, pthread_t);

    const int ret = pthread_create(thr->thread, NULL, thread_proc, thr);
    ASC_ASSERT(ret == 0, MSG("failed to create thread: %s"), strerror(ret));
#endif /* !_WIN32 */

    return thr;
}

void asc_thread_join(asc_thread_t *thr)
{
#ifdef _WIN32
    const DWORD ret = WaitForSingleObject(thr->thread, INFINITE);
    if (ret != WAIT_OBJECT_0)
        asc_log_error(MSG("failed to join thread: %s"), asc_error_msg());

    CloseHandle(thr->thread);
#else /* _WIN32 */
    const int ret = pthread_join(*thr->thread, NULL);
    if (ret != 0)
        asc_log_error(MSG("failed to join thread: %s"), strerror(ret));

    free(thr->thread);
#endif /* !_WIN32 */

    asc_list_remove_item(thread_mgr->list, thr);
    asc_job_prune(thr);

    free(thr);
}

/*
 * thread buffer (deprecated)
 */

struct asc_thread_buffer_t
{
    uint8_t *buffer;
    size_t size;
    size_t read;
    size_t write;
    size_t count;

    asc_mutex_t mutex;
};

asc_thread_buffer_t *asc_thread_buffer_init(size_t size)
{
    asc_thread_buffer_t *const buffer = ASC_ALLOC(1, asc_thread_buffer_t);

    buffer->size = size;
    buffer->buffer = ASC_ALLOC(size, uint8_t);
    asc_mutex_init(&buffer->mutex);

    return buffer;
}

void asc_thread_buffer_destroy(asc_thread_buffer_t *buffer)
{
    free(buffer->buffer);
    asc_mutex_destroy(&buffer->mutex);
    free(buffer);
}

void asc_thread_buffer_flush(asc_thread_buffer_t *buffer)
{
    asc_mutex_lock(&buffer->mutex);
    buffer->count = 0;
    buffer->read = 0;
    buffer->write = 0;
    asc_mutex_unlock(&buffer->mutex);
}

ssize_t asc_thread_buffer_read(asc_thread_buffer_t *buffer, void *data
                               , size_t size)
{
    asc_mutex_lock(&buffer->mutex);
    if (size > buffer->count)
        size = buffer->count;

    if (!size)
    {
        asc_mutex_unlock(&buffer->mutex);
        return 0;
    }

    const size_t next_read = buffer->read + size;
    if (next_read < buffer->size)
    {
        memcpy(data, &buffer->buffer[buffer->read], size);
        buffer->read += size;
    }
    else if (next_read > buffer->size)
    {
        const size_t tail = buffer->size - buffer->read;
        memcpy(data, &buffer->buffer[buffer->read], tail);
        buffer->read = size - tail;
        memcpy(&((uint8_t *)data)[tail], buffer->buffer, buffer->read);
    }
    else
    {
        memcpy(data, &buffer->buffer[buffer->read], size);
        buffer->read = 0;
    }

    buffer->count -= size;
    asc_mutex_unlock(&buffer->mutex);

    return size;
}

ssize_t asc_thread_buffer_write(asc_thread_buffer_t *buffer, const void *data
                                , size_t size)
{
    if (!size)
        return 0;

    asc_mutex_lock(&buffer->mutex);
    if (buffer->count + size > buffer->size)
    {
        asc_mutex_unlock(&buffer->mutex);
        return -1; // buffer overflow
    }

    if (buffer->write + size >= buffer->size)
    {
        const size_t tail = buffer->size - buffer->write;
        memcpy(&buffer->buffer[buffer->write], data, tail);
        buffer->write = size - tail;
        if(buffer->write > 0)
            memcpy(buffer->buffer, &((uint8_t *)data)[tail], buffer->write);
    }
    else
    {
        memcpy(&buffer->buffer[buffer->write], data, size);
        buffer->write += size;
    }
    buffer->count += size;
    asc_mutex_unlock(&buffer->mutex);

    return size;
}
