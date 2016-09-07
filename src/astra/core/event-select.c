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

#ifdef HAVE_SYS_SELECT_H
#   include <sys/select.h>
#endif

#define MSG(_msg) "[core/event-select] " _msg

typedef struct
{
    asc_list_t *event_list;
    bool is_changed;

    int max_fd;
    fd_set rmaster;
    fd_set wmaster;
    fd_set emaster;
} event_observer_t;

static event_observer_t event_observer;

void asc_event_core_init(void)
{
    memset(&event_observer, 0, sizeof(event_observer));
    event_observer.event_list = asc_list_init();
}

void asc_event_core_destroy(void)
{
    if (!event_observer.event_list)
        return;

    asc_event_t *prev_event = NULL;
    asc_list_till_empty(event_observer.event_list)
    {
        asc_event_t *const event =
            (asc_event_t *)asc_list_data(event_observer.event_list);

        asc_assert(event != prev_event
                   , MSG("loop on asc_event_core_destroy() event:%p")
                   , (void *)event);

        if (event->on_error)
            event->on_error(event->arg);

        prev_event = event;
    }

    ASC_FREE(event_observer.event_list, asc_list_destroy);
}

void asc_event_core_loop(unsigned int timeout)
{
    if (asc_list_size(event_observer.event_list) == 0)
    {
        asc_usleep(timeout * 1000ULL); /* dry run */
        return;
    }

    fd_set rset;
    fd_set wset;
    fd_set eset;
    memcpy(&rset, &event_observer.rmaster, sizeof(rset));
    memcpy(&wset, &event_observer.wmaster, sizeof(wset));
    memcpy(&eset, &event_observer.emaster, sizeof(eset));

    struct timeval tv = {
        .tv_sec = (timeout / 1000),
        .tv_usec = (timeout % 1000) * 1000UL,
    };
    const int ret = select(event_observer.max_fd + 1
                           , &rset, &wset, &eset, &tv);

    if (ret == -1)
    {
#ifndef _WIN32
        if (errno == EINTR)
            return;
#endif /* !_WIN32 */

        asc_log_error(MSG("select() failed: %s"), asc_error_msg());
        asc_lib_abort();
    }
    else if (ret > 0)
    {
        event_observer.is_changed = false;
        asc_list_for(event_observer.event_list)
        {
            asc_event_t *const event =
                (asc_event_t *)asc_list_data(event_observer.event_list);

            if (event->on_read && FD_ISSET(event->fd, &rset))
            {
                event->on_read(event->arg);
                if (event_observer.is_changed)
                    break;
            }
            if (event->on_error && FD_ISSET(event->fd, &eset))
            {
                event->on_error(event->arg);
                if (event_observer.is_changed)
                    break;
            }
            if (event->on_write && FD_ISSET(event->fd, &wset))
            {
                event->on_write(event->arg);
                if (event_observer.is_changed)
                    break;
            }
        }
    }
}

void asc_event_subscribe(asc_event_t *event)
{
    if (event->on_read)
        FD_SET((unsigned)event->fd, &event_observer.rmaster);
    else
        FD_CLR((unsigned)event->fd, &event_observer.rmaster);

    if (event->on_write)
        FD_SET((unsigned)event->fd, &event_observer.wmaster);
    else
        FD_CLR((unsigned)event->fd, &event_observer.wmaster);

    if (event->on_error)
        FD_SET((unsigned)event->fd, &event_observer.emaster);
    else
        FD_CLR((unsigned)event->fd, &event_observer.emaster);
}

asc_event_t *asc_event_init(int fd, void *arg)
{
    asc_event_t *const event = ASC_ALLOC(1, asc_event_t);

    event->fd = fd;
    event->arg = arg;

    if (fd > event_observer.max_fd)
        event_observer.max_fd = fd;

    asc_list_insert_tail(event_observer.event_list, event);
    event_observer.is_changed = true;

    return event;
}

void asc_event_close(asc_event_t *event)
{
    if (!event)
        return;

    event_observer.is_changed = true;

    event->on_read = NULL;
    event->on_write = NULL;
    event->on_error = NULL;
    asc_event_subscribe(event);

    if (event->fd < event_observer.max_fd)
    {
        asc_list_remove_item(event_observer.event_list, event);
        free(event);
        return;
    }

    event_observer.max_fd = 0;
    asc_list_first(event_observer.event_list);
    while (!asc_list_eol(event_observer.event_list))
    {
        asc_event_t *const item =
            (asc_event_t *)asc_list_data(event_observer.event_list);

        if (item == event)
        {
            asc_list_remove_current(event_observer.event_list);
            free(event);
        }
        else
        {
            if (item->fd > event_observer.max_fd)
                event_observer.max_fd = item->fd;

            asc_list_next(event_observer.event_list);
        }
    }
}
