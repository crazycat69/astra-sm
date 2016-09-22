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

void asc_event_set_on_read(asc_event_t *event, event_callback_t on_read)
{
    if (event->on_read == on_read)
        return;

    event->on_read = on_read;
    asc_event_subscribe(event);
}

void asc_event_set_on_write(asc_event_t *event, event_callback_t on_write)
{
    if (event->on_write == on_write)
        return;

    event->on_write = on_write;
    asc_event_subscribe(event);
}

void asc_event_set_on_error(asc_event_t *event, event_callback_t on_error)
{
    if (event->on_error == on_error)
        return;

    event->on_error = on_error;
    asc_event_subscribe(event);
}
