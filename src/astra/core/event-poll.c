/*
 * Astra Core (Event notification)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
 *                    2016, Artem Kharitonov <artem@3phase.pw>
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

#include "event-priv.h"

#ifdef _WIN32
#   include <astra/core/mainloop.h> /* for job queue */
#   if !defined(HAVE_POLL) && defined(HAVE_WSAPOLL)
#       define poll(_a, _b, _c) WSAPoll(_a, _b, _c)
#   endif
#   define POLLBAND (POLLRDBAND) /* Windows hates POLLPRI for some reason */
#   define CONNECT_TIMEOUT (300000) /* release wait object after 5 minutes */
#else
#   ifdef HAVE_POLL_H
#       include <poll.h>
#   endif
#   define POLLBAND (POLLRDBAND | POLLPRI)
#endif

#ifndef POLLRDHUP
#   define POLLRDHUP 0 /* only exists on Linux 2.6.17 and newer */
#endif

#define MSG(_msg) "[core/event-poll] " _msg

typedef struct
{
    asc_event_t **ev;
    struct pollfd *fd;
    size_t ev_cnt;
    size_t ev_maxcnt;
    bool is_changed;
} asc_event_mgr_t;

static asc_event_mgr_t *event_mgr = NULL;

#ifdef _WIN32
/*
 * WSAPoll() has an annoying bug where it fails to report connection
 * failure. This workaround uses a wait thread to wait for the FD_CONNECT
 * event. Error notification is then delivered to the main loop via
 * job queue.
 */

static
void wait_connect_fail(asc_event_t *event)
{
    if (event->on_error)
        event->on_error(event->arg);
}

static
void wait_unregister(asc_event_t *event)
{
    if (!UnregisterWaitEx(event->wait, INVALID_HANDLE_VALUE))
        asc_log_error(MSG("UnregisterWaitEx(): %s"), asc_error_msg());

    event->wait = NULL;

    if (WSAEventSelect(event->fd, event->conn_evt, 0) != 0)
        asc_log_error(MSG("WSAEventSelect(): %s"), asc_error_msg());

    ASC_FREE(event->conn_evt, CloseHandle);
    asc_job_prune(event);
}

static CALLBACK
void wait_thread_proc(void *arg, BOOLEAN timedout)
{
    asc_event_t *const event = (asc_event_t *)arg;

    if (!timedout)
    {
        WSANETWORKEVENTS ne;
        const int ret = WSAEnumNetworkEvents(event->fd, event->conn_evt, &ne);

        if (ret == 0 && (ne.lNetworkEvents & FD_CONNECT)
            && ne.iErrorCode[FD_CONNECT_BIT] != 0)
        {
            asc_job_queue(event, (loop_callback_t)wait_connect_fail, event);
        }
    }
    else
    {
        asc_job_queue(event, (loop_callback_t)wait_unregister, event);
    }
}

static
void wait_register(asc_event_t *event)
{
    HANDLE wait = NULL;

    const HANDLE conn_evt = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (conn_evt == NULL)
    {
        asc_log_error(MSG("CreateEvent(): %s"), asc_error_msg());

        return;
    }

    if (WSAEventSelect(event->fd, conn_evt, FD_CONNECT) != 0)
    {
        asc_log_error(MSG("WSAEventSelect(): %s"), asc_error_msg());

        CloseHandle(conn_evt);
        return;
    }

    if (!RegisterWaitForSingleObject(&wait, conn_evt, wait_thread_proc
                                     , event, CONNECT_TIMEOUT
                                     , (WT_EXECUTEONLYONCE
                                        | WT_EXECUTEINWAITTHREAD)))
    {
        asc_log_error(MSG("RegisterWaitForSingleObject(): %s")
                      , asc_error_msg());

        WSAEventSelect(event->fd, conn_evt, 0);
        CloseHandle(conn_evt);
        return;
    }

    event->conn_evt = conn_evt;
    event->wait = wait;
}
#endif /* _WIN32 */

void asc_event_core_init(void)
{
    event_mgr = ASC_ALLOC(1, asc_event_mgr_t);
}

void asc_event_core_destroy(void)
{
    if (event_mgr == NULL)
        return;

    asc_event_t *event, *prev = NULL;
    while (event_mgr->ev_cnt > 0)
    {
        event = event_mgr->ev[0];
        ASC_ASSERT(event != prev, MSG("on_error didn't close event"));

        if (event->on_error != NULL)
            event->on_error(event->arg);
        else
            asc_event_close(event);

        prev = event;
    }

    ASC_FREE(event_mgr->ev, free);
    ASC_FREE(event_mgr->fd, free);
    ASC_FREE(event_mgr, free);
}

bool asc_event_core_loop(unsigned int timeout)
{
    if (event_mgr->ev_cnt == 0)
    {
        asc_usleep(timeout * 1000ULL); /* dry run */
        return true;
    }

    int ret = poll(event_mgr->fd, event_mgr->ev_cnt, timeout);
#ifndef _WIN32
    if (ret == -1 && errno != EINTR)
#else
    if (ret == -1)
#endif
    {
        asc_log_error(MSG("poll() failed: %s"), asc_error_msg());
        return false;
    }

    event_mgr->is_changed = false;
    for (size_t i = 0; i < event_mgr->ev_cnt && ret > 0; i++)
    {
        const short revents = event_mgr->fd[i].revents;
        if (revents <= 0)
            continue;

        ret--;
        asc_event_t *const event = event_mgr->ev[i];

        const bool is_rd = revents & (POLLRDNORM | POLLRDHUP | POLLHUP);
        const bool is_wr = revents & POLLOUT;
        const bool is_er = revents & (POLLERR | POLLNVAL | POLLHUP | POLLBAND);

#ifdef _WIN32
        /* kill connect() wait on first event */
        if (event->wait != NULL)
            wait_unregister(event);
#endif

        if (event->on_read && is_rd)
        {
            event->on_read(event->arg);
            if (event_mgr->is_changed)
                break;
        }
        if (event->on_error && is_er)
        {
            event->on_error(event->arg);
            if (event_mgr->is_changed)
                break;
        }
        if (event->on_write && is_wr)
        {
            event->on_write(event->arg);
            if (event_mgr->is_changed)
                break;
        }
    }

    return true;
}

static
size_t find_event(const asc_event_t *event)
{
    size_t i;

    for (i = 0; i < event_mgr->ev_cnt; i++)
    {
        if (event_mgr->ev[i] == event)
            break;
    }
    ASC_ASSERT(i < event_mgr->ev_cnt, MSG("event %p not in array")
               , (void *)event);

    return i;
}

static
void resize_event_list(void)
{
    const size_t maxcnt = asc_list_calc_size(event_mgr->ev_cnt
                                             , event_mgr->ev_maxcnt
                                             , EVENT_LIST_MIN_SIZE);

    if (event_mgr->ev_maxcnt != maxcnt)
    {
        const size_t ev_size = maxcnt * sizeof(*event_mgr->ev);
        event_mgr->ev = (asc_event_t **)realloc(event_mgr->ev, ev_size);

        const size_t fd_size = maxcnt * sizeof(*event_mgr->fd);
        event_mgr->fd = (struct pollfd *)realloc(event_mgr->fd, fd_size);

        ASC_ASSERT(event_mgr->ev != NULL && event_mgr->fd != NULL
                   , MSG("realloc() failed"));

        event_mgr->ev_maxcnt = maxcnt;
    }
}

void asc_event_subscribe(asc_event_t *event)
{
    const size_t i = find_event(event);

    event_mgr->fd[i].events = 0;
    if (event->on_read)
        event_mgr->fd[i].events |= (POLLRDNORM | POLLRDHUP);
    if (event->on_write)
        event_mgr->fd[i].events |= POLLOUT;
    if (event->on_error)
        event_mgr->fd[i].events |= POLLBAND;
}

asc_event_t *asc_event_init(int fd, void *arg)
{
    asc_event_t *const event = ASC_ALLOC(1, asc_event_t);

    event->fd = fd;
    event->arg = arg;

    /* append new event to the list */
    event_mgr->ev_cnt++;
    event_mgr->is_changed = true;
    resize_event_list();

    const int i = event_mgr->ev_cnt - 1;
    memset(&event_mgr->fd[i], 0, sizeof(*event_mgr->fd));
    event_mgr->fd[i].fd = fd;
    event_mgr->ev[i] = event;

#ifdef _WIN32
    /* watch for connection failure event */
    wait_register(event);
#endif

    return event;
}

void asc_event_close(asc_event_t *event)
{
    const size_t i = find_event(event);

#ifdef _WIN32
    /* stop background wait for connect() */
    if (event->wait != NULL)
        wait_unregister(event);
#endif

    /* shift down array remainder */
    const size_t more = event_mgr->ev_cnt - (i + 1);
    if (more > 0)
    {
        memmove(&event_mgr->ev[i], &event_mgr->ev[i + 1]
                , more * sizeof(*event_mgr->ev));

        memmove(&event_mgr->fd[i], &event_mgr->fd[i + 1]
                , more * sizeof(*event_mgr->fd));
    }

    /* remove last element from the list */
    event_mgr->ev_cnt--;
    event_mgr->is_changed = true;
    resize_event_list();

    free(event);
}
