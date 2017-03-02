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

#ifndef _ASC_EVENT_PRIV_H_
#define _ASC_EVENT_PRIV_H_ 1

#include <astra/astra.h>
#include <astra/core/list.h>
#include <astra/core/event.h>

struct asc_event_t
{
    int fd;
    event_callback_t on_read;
    event_callback_t on_write;
    event_callback_t on_error;
    void *arg;

#if defined(_WIN32) && defined(WITH_EVENT_POLL)
    /* these are for WSAPoll non-blocking connect() workaround */
    HANDLE conn_evt;
    HANDLE wait;
#endif
};

void asc_event_subscribe(asc_event_t *event);

/* minimum size for output arrays */
#define EVENT_LIST_MIN_SIZE 1024

#endif /* _ASC_EVENT_PRIV_H_ */
