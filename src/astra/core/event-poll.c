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

#include <astra/astra.h>
#include <astra/core/list.h>

#include "event-priv.h"

#ifndef EV_LIST_SIZE
#   define EV_LIST_SIZE 1024
#endif

#ifdef HAVE_POLL_H
#   include <poll.h>
#endif

#if !defined(HAVE_POLL) && defined(HAVE_WSAPOLL)
#   define poll(_a, _b, _c) WSAPoll(_a, _b, _c)
#endif

#define MSG(_msg) "[core/event-poll] " _msg

typedef struct
{
    asc_event_t *event_list[EV_LIST_SIZE];
    bool is_changed;
    int fd_count;

    struct pollfd fd_list[EV_LIST_SIZE];
} event_observer_t;

static event_observer_t event_observer;

void asc_event_core_init(void)
{
    memset(&event_observer, 0, sizeof(event_observer));
}

void asc_event_core_destroy(void)
{
    while (event_observer.fd_count > 0)
    {
        const int next_fd_count = event_observer.fd_count - 1;

        asc_event_t *event = event_observer.event_list[next_fd_count];
        if (event->on_error)
            event->on_error(event->arg);

        asc_assert(event_observer.fd_count == next_fd_count
                   , MSG("loop on asc_event_core_destroy() event:%p")
                   , (void *)event);
    }
}

void asc_event_core_loop(unsigned int timeout)
{
    if (event_observer.fd_count == 0)
    {
        asc_usleep(timeout * 1000ULL); /* dry run */
        return;
    }

    int ret = poll(event_observer.fd_list, event_observer.fd_count, timeout);
    if (ret == -1)
    {
#ifndef _WIN32
        if (errno == EINTR)
            return;
#endif /* !_WIN32 */

        asc_log_error(MSG("poll() failed: %s"), asc_error_msg());
        asc_lib_abort();
    }

    event_observer.is_changed = false;
    for (int i = 0; i < event_observer.fd_count && ret > 0; ++i)
    {
        const short revents = event_observer.fd_list[i].revents;
        if (revents == 0)
            continue;

        --ret;
        asc_event_t *const event = event_observer.event_list[i];
        if (event->on_read && ((revents & POLLIN)
                               || (revents & (POLLERR | POLLHUP)) == POLLHUP))
        {
            event->on_read(event->arg);
            if (event_observer.is_changed)
                break;
        }
        if (event->on_error && (revents & (POLLERR | POLLNVAL)))
        {
            event->on_error(event->arg);
            if (event_observer.is_changed)
                break;
        }
        if (event->on_write && (revents & POLLOUT))
        {
            event->on_write(event->arg);
            if (event_observer.is_changed)
                break;
        }
    }
}

void asc_event_subscribe(asc_event_t *event)
{
    int i;
    for (i = 0; i < event_observer.fd_count; ++i)
    {
        if (event_observer.event_list[i]->fd == event->fd)
            break;
    }
    asc_assert(i < event_observer.fd_count
               , MSG("failed to set fd=%d"), event->fd);

    event_observer.fd_list[i].events = 0;
    if (event->on_read)
        event_observer.fd_list[i].events |= POLLIN;
    if (event->on_write)
        event_observer.fd_list[i].events |= POLLOUT;
}

asc_event_t *asc_event_init(int fd, void *arg)
{
    const int i = event_observer.fd_count;
    memset(&event_observer.fd_list[i], 0, sizeof(struct pollfd));
    event_observer.fd_list[i].fd = fd;

    asc_event_t *const event = ASC_ALLOC(1, asc_event_t);

    event_observer.event_list[i] = event;
    event->fd = fd;
    event->arg = arg;

    event_observer.fd_count += 1;
    event_observer.is_changed = true;

    return event;
}

void asc_event_close(asc_event_t *event)
{
    if (!event)
        return;

    int i;
    for (i = 0; i < event_observer.fd_count; ++i)
    {
        if (event_observer.event_list[i]->fd == event->fd)
            break;
    }
    asc_assert(i < event_observer.fd_count
               , MSG("failed to detach fd=%d"), event->fd);

    for (; i < event_observer.fd_count; ++i)
    {
        memcpy(&event_observer.fd_list[i], &event_observer.fd_list[i + 1]
               , sizeof(struct pollfd));
        event_observer.event_list[i] = event_observer.event_list[i + 1];
    }
    memset(&event_observer.fd_list[i], 0, sizeof(struct pollfd));
    event_observer.event_list[i] = NULL;

    event_observer.fd_count -= 1;
    event_observer.is_changed = true;

    free(event);
}
