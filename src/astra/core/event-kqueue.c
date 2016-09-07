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

#include <sys/event.h>

#define MSG(_msg) "[core/event-kqueue] " _msg

static inline
int __event_init(void)
{
    int fd = -1;

#ifdef HAVE_KQUEUE1
    /* kqueue1() is only present on NetBSD as of this writing */
    fd = kqueue1(O_CLOEXEC);
#endif
    if (fd != -1)
        return fd;

    fd = kqueue();
    if (fd == -1)
        return fd;

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        close(fd);
        fd = -1;
    }

    return fd;
}

typedef struct
{
    asc_list_t *event_list;
    bool is_changed;

    int fd;
    struct kevent ed_list[EV_LIST_SIZE];
} event_observer_t;

static event_observer_t event_observer;

void asc_event_core_init(void)
{
    memset(&event_observer, 0, sizeof(event_observer));
    event_observer.event_list = asc_list_init();

    event_observer.fd = __event_init();
    asc_assert(event_observer.fd != -1
               , MSG("failed to init event observer [%s]")
               , strerror(errno));
}

void asc_event_core_destroy(void)
{
    if (!event_observer.event_list || !event_observer.fd)
        return;

    close(event_observer.fd);
    event_observer.fd = 0;

    asc_event_t *prev_event = NULL;
    asc_list_till_empty(event_observer.event_list)
    {
        asc_event_t *event = (asc_event_t *)asc_list_data(event_observer.event_list);
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

    const struct timespec ts = {
        (timeout / 1000), /* tv_sec */
        (timeout % 1000) * 1000000UL, /* tv_nsec */
    };
    const int ret = kevent(event_observer.fd, NULL, 0
                           , event_observer.ed_list, EV_LIST_SIZE, &ts);

    if (ret == -1)
    {
        asc_assert(errno == EINTR, MSG("event observer critical error [%s]"), strerror(errno));
        return;
    }

    event_observer.is_changed = false;
    for (int i = 0; i < ret; ++i)
    {
        struct kevent *ed = &event_observer.ed_list[i];

        asc_event_t *event = (asc_event_t *)ed->udata;
        const bool is_rd = (ed->data > 0) && (ed->filter == EVFILT_READ);
        const bool is_wr = (ed->data > 0) && (ed->filter == EVFILT_WRITE);
        const bool is_er = (ed->flags & ~EV_ADD) && (!is_rd || is_wr);

        if (event->on_read && is_rd)
        {
            event->on_read(event->arg);
            if (event_observer.is_changed)
                break;
        }
        if (event->on_error && is_er)
        {
            event->on_error(event->arg);
            if (event_observer.is_changed)
                break;
        }
        if (event->on_write && is_wr)
        {
            event->on_write(event->arg);
            if (event_observer.is_changed)
                break;
        }
    }
}

void asc_event_subscribe(asc_event_t *event)
{
    int ret = 0;
    struct kevent ed;

    do
    {
        if (event->on_read)
        {
            EV_SET(&ed, event->fd, EVFILT_READ, EV_ADD | EV_EOF | EV_ERROR, 0, 0, event);
            ret = kevent(event_observer.fd, &ed, 1, NULL, 0, NULL);
            if (ret == -1)
                break;
        }
        else
        {
            EV_SET(&ed, event->fd, EVFILT_READ, EV_DELETE, 0, 0, event);
            kevent(event_observer.fd, &ed, 1, NULL, 0, NULL);
        }

        if (event->on_write)
        {
            EV_SET(&ed, event->fd, EVFILT_WRITE, EV_ADD | EV_EOF | EV_ERROR, 0, 0, event);
            ret = kevent(event_observer.fd, &ed, 1, NULL, 0, NULL);
            if (ret == -1)
                break;
        }
        else
        {
            EV_SET(&ed, event->fd, EVFILT_WRITE, EV_DELETE, 0, 0, event);
            kevent(event_observer.fd, &ed, 1, NULL, 0, NULL);
        }

        return;
    } while (0);

    asc_assert(ret != -1, MSG("failed to set fd=%d [%s]")
               , event->fd, strerror(errno));
}

asc_event_t *asc_event_init(int fd, void *arg)
{
    asc_event_t *const event = ASC_ALLOC(1, asc_event_t);

    event->fd = fd;
    event->arg = arg;

    asc_list_insert_tail(event_observer.event_list, event);
    event_observer.is_changed = true;

    return event;
}

void asc_event_close(asc_event_t *event)
{
    if (event == NULL)
        return;

    struct kevent ed;

    if (event->on_read != NULL)
    {
        EV_SET(&ed, event->fd, EVFILT_READ, EV_DELETE, 0, 0, event);
        kevent(event_observer.fd, &ed, 1, NULL, 0, NULL);
    }
    if (event->on_write != NULL)
    {
        EV_SET(&ed, event->fd, EVFILT_WRITE, EV_DELETE, 0, 0, event);
        kevent(event_observer.fd, &ed, 1, NULL, 0, NULL);
    }

    event_observer.is_changed = true;
    asc_list_remove_item(event_observer.event_list, event);

    free(event);
}
