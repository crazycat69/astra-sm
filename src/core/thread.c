/*
 * Astra Core (Threads)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
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

#include <astra.h>
#include <core/thread.h>
#include <core/event.h>
#include <core/list.h>
#include <core/socket.h>
#include <core/spawn.h>

#define MSG(_msg) "[core/thread %p] " _msg, (void *)thr
#define MSGN(_msg) "[core/thread] " _msg

struct asc_thread_buffer_t
{
    uint8_t *buffer;
    size_t size;
    size_t read;
    size_t write;
    size_t count;

    asc_mutex_t mutex;
};

struct asc_thread_t
{
    thread_callback_t proc;
    thread_callback_t on_close;
    void *arg;

    asc_thread_buffer_t *buffer;
    thread_callback_t on_read;

#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t *thread;
#endif

    bool started;
    bool exited;
    bool can_wake;
};

typedef struct
{
    asc_list_t *list;
    bool is_changed;

    int wake_fd[2];
    asc_event_t *wake_ev;
    unsigned int wake_cnt;
} asc_thread_mgr_t;

static asc_thread_mgr_t *thread_mgr = NULL;

static void on_wake_read(void *arg);

static bool wake_open(void)
{
    int fds[2] = { -1, -1 };

    if (asc_pipe_open(fds, NULL, PIPE_BOTH) != 0)
        return false;

    thread_mgr->wake_fd[0] = fds[0];
    thread_mgr->wake_fd[1] = fds[1];

    thread_mgr->wake_ev = asc_event_init(fds[PIPE_RD], NULL);
    asc_event_set_on_read(thread_mgr->wake_ev, on_wake_read);

    return true;
}

static void wake_close(void)
{
    ASC_FREE(thread_mgr->wake_ev, asc_event_close);

    const int fds[2] = {
        thread_mgr->wake_fd[0],
        thread_mgr->wake_fd[1],
    };

    if (fds[0] != -1)
    {
        thread_mgr->wake_fd[0] = -1;
        asc_pipe_close(fds[0]);
    }

    if (fds[1] != -1)
    {
        thread_mgr->wake_fd[1] = -1;
        asc_pipe_close(fds[1]);
    }
}

static void on_wake_read(void *arg)
{
    __uarg(arg);

    char buf[32];
    const int ret = recv(thread_mgr->wake_fd[PIPE_RD], buf, sizeof(buf), 0);

    switch (ret)
    {
        case -1:
            /* error that may or may not be EWOULDBLOCK */
            if (asc_socket_would_block())
                return;

            asc_log_error(MSGN("wake up recv(): %s"), asc_error_msg());
            break;

        case 0:
            /* connection closed from the other side */
            asc_log_error(MSGN("wake up pipe closed unexpectedly"));
            break;

        default:
            /* successful read */
            return;
    }

    /* this code is highly unlikely to be reached */
    asc_log_warning(MSGN("reopening wake up pipe"));

    wake_close();
    if (!wake_open())
       asc_log_error(MSGN("couldn't reopen pipe: %s"), asc_error_msg());
}

void asc_thread_wake(void)
{
    const int fd = thread_mgr->wake_fd[PIPE_WR];
    static const char byte = '\0';

    if (fd != -1 && send(fd, &byte, 1, 0) == -1)
        asc_log_error(MSGN("wake up send(): %s"), asc_error_msg());
}

void asc_thread_core_init(void)
{
    thread_mgr = (asc_thread_mgr_t *)calloc(1, sizeof(*thread_mgr));
    asc_assert(thread_mgr != NULL, MSGN("calloc() failed"));

    thread_mgr->list = asc_list_init();
    thread_mgr->wake_fd[0] = thread_mgr->wake_fd[1] = -1;
}

void asc_thread_core_destroy(void)
{
    asc_thread_t *thr, *prev = NULL;

    asc_list_first(thread_mgr->list);
    while (!asc_list_eol(thread_mgr->list))
    {
        thr = (asc_thread_t *)asc_list_data(thread_mgr->list);
        asc_assert(thr != prev, MSG("on_close didn't destroy thread"));

        if (thr->on_close != NULL)
        {
            /* NOTE: on_close has to call asc_thread_destroy() */
            thr->on_close(thr->arg);
        }
        else
        {
            if (thr->started && !thr->exited)
                asc_log_debug(MSG("on_close not set, joining thread anyway"));

            asc_thread_destroy(thr);
        }

        prev = thr;
        asc_list_first(thread_mgr->list);
    }

    ASC_FREE(thread_mgr->list, asc_list_destroy);
    ASC_FREE(thread_mgr, free);
}

void asc_thread_core_loop(void)
{
    thread_mgr->is_changed = false;
    asc_list_for(thread_mgr->list)
    {
        asc_thread_t *const thr =
            (asc_thread_t *)asc_list_data(thread_mgr->list);

        if (!thr->started)
            continue;

        if (thr->on_read != NULL && thr->buffer->count > 0)
        {
            thr->on_read(thr->arg);
            if (thread_mgr->is_changed)
                break;
        }

        if (thr->exited)
        {
            if (thr->on_close != NULL)
                thr->on_close(thr->arg);
            else
                asc_thread_destroy(thr);

            if (thread_mgr->is_changed)
                break;
        }
    }
}

asc_thread_t *asc_thread_init(void)
{
    asc_thread_t *const thr = (asc_thread_t *)calloc(1, sizeof(*thr));
    asc_assert(thr != NULL, MSGN("calloc failed()"));

    asc_list_insert_tail(thread_mgr->list, thr);
    thread_mgr->is_changed = true;

    return thr;
}

#ifdef _WIN32
static DWORD WINAPI thread_proc(void *arg)
#else
static void *thread_proc(void *arg)
#endif
{
    asc_thread_t *const thr = (asc_thread_t *)arg;

    thr->started = true;
    thr->proc(thr->arg);
    thr->exited = true;

    if (thr->can_wake)
        asc_thread_wake();

    return 0;
}

void asc_thread_start(asc_thread_t *thr, void *arg, thread_callback_t proc
                      , thread_callback_t on_read, asc_thread_buffer_t *buffer
                      , thread_callback_t on_close, bool can_wake)
{
    asc_assert(thr->thread == NULL, MSG("can't start thread twice"));

    thr->arg = arg;
    thr->proc = proc;
    thr->on_close = on_close;

    if (on_read != NULL && buffer != NULL)
    {
        thr->on_read = on_read;
        thr->buffer = buffer;
    }

    if (can_wake)
    {
        thr->can_wake = true;

        /* open wake up pipe on demand */
        if (thread_mgr->wake_cnt == 0)
        {
            asc_log_debug(MSG("opening main loop wake up pipe"));
            if (!wake_open())
                asc_log_error(MSG("couldn't open pipe: %s"), asc_error_msg());
        }

        ++thread_mgr->wake_cnt;
    }

#ifdef _WIN32
    thr->thread = CreateThread(NULL, 0, thread_proc, thr, 0, NULL);
    asc_assert(thr->thread != NULL, MSG("failed to create thread: %s")
               , asc_error_msg());
#else /* _WIN32 */
    thr->thread = (pthread_t *)calloc(1, sizeof(*thr->thread));
    asc_assert(thr->thread != NULL, MSG("calloc() failed"));

    const int ret = pthread_create(thr->thread, NULL, thread_proc, thr);
    asc_assert(ret == 0, MSG("failed to create thread: %s")
               , strerror(ret));
#endif /* !_WIN32 */
}

void asc_thread_destroy(asc_thread_t *thr)
{
    if (thr->thread != NULL)
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
    }

    if (thr->can_wake)
    {
        /* close wake up pipe if it's no longer needed */
        --thread_mgr->wake_cnt;

        if (thread_mgr->wake_cnt == 0)
        {
            asc_log_debug(MSG("closing main loop wake up pipe"));
            wake_close();
        }
    }

    asc_list_remove_item(thread_mgr->list, thr);
    thread_mgr->is_changed = true;

    free(thr);
}

asc_thread_buffer_t *asc_thread_buffer_init(size_t size)
{
    asc_thread_buffer_t *const buffer =
        (asc_thread_buffer_t *)calloc(1, sizeof(*buffer));
    asc_assert(buffer != NULL, MSGN("calloc() failed"));

    buffer->size = size;
    buffer->buffer = (uint8_t *)malloc(size);
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

    asc_thread_wake();

    return size;
}
