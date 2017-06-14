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

/*
 * On some systems it's possible to change the size of an fd_set
 * by manually defining FD_SETSIZE.
 */
#define FD_SETSIZE 8192

#include "event-priv.h"

#ifdef HAVE_SYS_SELECT_H
#   include <sys/select.h>
#endif

#define MSG(_msg) "[event-select] " _msg

/*
 * Enforce sane minimum size for fd_set. Linux system headers
 * redefine this as 1024, so don't insist on anything bigger.
 */
ASC_STATIC_ASSERT(FD_SETSIZE >= 1024);

typedef struct
{
    asc_list_t *list;
    bool is_changed;

    int max_fd;
    fd_set rmaster;
    fd_set wmaster;
    fd_set emaster;
} asc_event_mgr_t;

static asc_event_mgr_t *event_mgr = NULL;

void asc_event_core_init(void)
{
    event_mgr = ASC_ALLOC(1, asc_event_mgr_t);
    event_mgr->list = asc_list_init();

    event_mgr->max_fd = -1;
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

    ASC_FREE(event_mgr->list, asc_list_destroy);
    ASC_FREE(event_mgr, free);
}

static inline
bool is_valid_fd(int fd)
{
#ifndef _WIN32
    /*
     * POSIX requires fds to be greater than or equal to 0 and less than
     * FD_SETSIZE. Given that an fd_set is normally a bitmask on those
     * systems, breaking this rule WILL corrupt whatever's located next
     * to the set.
     */
    return (fd >= 0 && fd < (int)FD_SETSIZE);
#else
    /*
     * On Windows, these checks are done inside FD_XXX macros, while
     * select()'s nfds argument is completely ignored. We still warn
     * the user if the fd list gets too large.
     */
    ASC_UNUSED(fd);
    return true;
#endif
}

bool asc_event_core_loop(unsigned int timeout)
{
    if (event_mgr->max_fd < 0)
    {
        asc_usleep(timeout * 1000ULL); /* dry run */
        return true;
    }

    fd_set rset;
    fd_set wset;
    fd_set eset;
    memcpy(&rset, &event_mgr->rmaster, sizeof(rset));
    memcpy(&wset, &event_mgr->wmaster, sizeof(wset));
    memcpy(&eset, &event_mgr->emaster, sizeof(eset));

    struct timeval tv = {
        .tv_sec = (timeout / 1000),
        .tv_usec = (timeout % 1000) * 1000UL,
    };

    int ret = select(event_mgr->max_fd + 1, &rset, &wset, &eset, &tv);
#ifndef _WIN32
    if (ret == -1 && errno != EINTR)
#else
    if (ret == -1)
#endif
    {
        asc_log_error(MSG("select(): %s"), asc_error_msg());
        return false;
    }

    event_mgr->is_changed = false;
    for (asc_list_first(event_mgr->list)
         ; !asc_list_eol(event_mgr->list) && ret > 0
         ; asc_list_next(event_mgr->list))
    {
        asc_event_t *const event =
            (asc_event_t *)asc_list_data(event_mgr->list);

        if (!is_valid_fd(event->fd))
            continue;

        if (event->on_read && FD_ISSET(event->fd, &rset))
        {
            ret--;
            event->on_read(event->arg);
            if (event_mgr->is_changed)
                break;
        }
        if (event->on_error && FD_ISSET(event->fd, &eset))
        {
            ret--;
            event->on_error(event->arg);
            if (event_mgr->is_changed)
                break;
        }
        if (event->on_write && FD_ISSET(event->fd, &wset))
        {
            ret--;
            event->on_write(event->arg);
            if (event_mgr->is_changed)
                break;
        }
    }

    return true;
}

void asc_event_subscribe(asc_event_t *event)
{
    if (!is_valid_fd(event->fd))
    {
        asc_log_error(MSG("fd %d out of range for select(), ignoring events")
                      , event->fd);
        return;
    }

    if (event->on_read)
        FD_SET((unsigned)event->fd, &event_mgr->rmaster);
    else
        FD_CLR((unsigned)event->fd, &event_mgr->rmaster);

    if (event->on_write)
        FD_SET((unsigned)event->fd, &event_mgr->wmaster);
    else
        FD_CLR((unsigned)event->fd, &event_mgr->wmaster);

    if (event->on_error)
        FD_SET((unsigned)event->fd, &event_mgr->emaster);
    else
        FD_CLR((unsigned)event->fd, &event_mgr->emaster);
}

asc_event_t *asc_event_init(int fd, void *arg)
{
    asc_event_t *const event = ASC_ALLOC(1, asc_event_t);

    event->fd = fd;
    event->arg = arg;

    if (fd > event_mgr->max_fd && is_valid_fd(fd))
        event_mgr->max_fd = fd;

#ifdef _WIN32
    if (asc_list_count(event_mgr->list) >= FD_SETSIZE)
        asc_log_error(MSG("fd list is too large, events could get dropped"));
#endif

    asc_list_insert_tail(event_mgr->list, event);
    event_mgr->is_changed = true;

    return event;
}

void asc_event_close(asc_event_t *event)
{
    event_mgr->is_changed = true;

    event->on_read = NULL;
    event->on_write = NULL;
    event->on_error = NULL;
    asc_event_subscribe(event);

    if (event->fd < event_mgr->max_fd)
    {
        asc_list_remove_item(event_mgr->list, event);
        free(event);

        return;
    }

    event_mgr->max_fd = -1;
    asc_list_first(event_mgr->list);
    while (!asc_list_eol(event_mgr->list))
    {
        const asc_event_t *const item =
            (asc_event_t *)asc_list_data(event_mgr->list);

        if (item == event)
        {
            asc_list_remove_current(event_mgr->list);
            free(event);
        }
        else
        {
            if (item->fd > event_mgr->max_fd && is_valid_fd(item->fd))
                event_mgr->max_fd = item->fd;

            asc_list_next(event_mgr->list);
        }
    }
}
