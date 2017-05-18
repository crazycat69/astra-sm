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

#include <sys/event.h>

#define EV_ADD_ALL (EV_ADD | EV_EOF | EV_ERROR)

#define MSG(_msg) "[event-kqueue] " _msg

typedef struct
{
    asc_list_t *list;
    bool is_changed;

    int fd;
    struct kevent *out;
    size_t out_size;
} asc_event_mgr_t;

static asc_event_mgr_t *event_mgr = NULL;

void asc_event_core_init(void)
{
    event_mgr = ASC_ALLOC(1, asc_event_mgr_t);
    event_mgr->list = asc_list_init();

    event_mgr->fd = kqueue();
    ASC_ASSERT(event_mgr->fd != -1
               , MSG("kqueue(): %s"), strerror(errno));
}

void asc_event_core_destroy(void)
{
    if (event_mgr == NULL)
        return;

    asc_event_t *event, *prev = NULL;
    asc_list_till_empty(event_mgr->list)
    {
        event = (asc_event_t *)asc_list_data(event_mgr->list);
        ASC_ASSERT(event != prev, MSG("on_error didn't close event"));

        if (event->on_error != NULL)
            event->on_error(event->arg);
        else
            asc_event_close(event);

        prev = event;
    }

    close(event_mgr->fd);

    ASC_FREE(event_mgr->list, asc_list_destroy);
    ASC_FREE(event_mgr->out, free);
    ASC_FREE(event_mgr, free);
}

bool asc_event_core_loop(unsigned int timeout)
{
    if (asc_list_count(event_mgr->list) == 0)
    {
        asc_usleep(timeout * 1000ULL); /* dry run */
        return true;
    }

    const struct timespec ts = {
        (timeout / 1000), /* tv_sec */
        (timeout % 1000) * 1000000UL, /* tv_nsec */
    };

    const int ret = kevent(event_mgr->fd, NULL, 0, event_mgr->out
                           , event_mgr->out_size, &ts);

    if (ret == -1 && errno != EINTR)
    {
        asc_log_error(MSG("kevent(): %s"), strerror(errno));
        return false;
    }

    event_mgr->is_changed = false;
    for (int i = 0; i < ret; i++)
    {
        const struct kevent *ed = &event_mgr->out[i];
        const asc_event_t *event = (asc_event_t *)ed->udata;

        const bool is_rd = (ed->filter == EVFILT_READ)
                           && (ed->data > 0 || ed->flags & EV_EOF);
        const bool is_wr = (ed->filter == EVFILT_WRITE)
                           && (ed->data > 0 || ed->flags & EV_EOF);
        const bool is_er = (ed->flags & EV_ERROR);

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

void asc_event_subscribe(asc_event_t *event)
{
    struct kevent ed[2];

    if (event->on_read)
        EV_SET(&ed[0], event->fd, EVFILT_READ, EV_ADD_ALL, 0, 0, event);
    else
        EV_SET(&ed[0], event->fd, EVFILT_READ, EV_DELETE, 0, 0, event);

    if (event->on_write)
        EV_SET(&ed[1], event->fd, EVFILT_WRITE, EV_ADD_ALL, 0, 0, event);
    else
        EV_SET(&ed[1], event->fd, EVFILT_WRITE, EV_DELETE, 0, 0, event);

    for (unsigned int i = 0; i < 2; i++)
    {
        const int ret = kevent(event_mgr->fd, &ed[i], 1, NULL, 0, NULL);
        if (ret != 0 && errno != ENOENT && errno != EINTR)
        {
            asc_log_error(MSG("kevent(): couldn't register fd %d: %s")
                          , event->fd, strerror(errno));
        }
    }
}

static
void resize_event_list(void)
{
    const size_t ev_cnt = asc_list_count(event_mgr->list);
    const size_t new_size = asc_list_calc_size(ev_cnt, event_mgr->out_size
                                               , EVENT_LIST_MIN_SIZE);

    if (event_mgr->out_size != new_size)
    {
        const size_t bytes = new_size * sizeof(*event_mgr->out);
        event_mgr->out = (struct kevent *)realloc(event_mgr->out, bytes);
        ASC_ASSERT(event_mgr->out != NULL, MSG("realloc() failed"));

        event_mgr->out_size = new_size;
    }
}

asc_event_t *asc_event_init(int fd, void *arg)
{
    asc_event_t *const event = ASC_ALLOC(1, asc_event_t);

    event->fd = fd;
    event->arg = arg;

    asc_list_insert_tail(event_mgr->list, event);
    event_mgr->is_changed = true;
    resize_event_list();

    return event;
}

void asc_event_close(asc_event_t *event)
{
    event->on_read = NULL;
    event->on_write = NULL;
    asc_event_subscribe(event);

    asc_list_remove_item(event_mgr->list, event);
    event_mgr->is_changed = true;
    resize_event_list();

    free(event);
}
