/*
 * Astra Core (Child process)
 *
 * Copyright (C) 2015, Artem Kharitonov <artem@sysert.ru>
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

#ifndef _ASC_CHILD_H_
#define _ASC_CHILD_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

#include <core/event.h>

typedef struct asc_child_t asc_child_t;

typedef void (*child_close_callback_t)(void *, int);
typedef void (*child_io_callback_t)(void *, const uint8_t *, size_t);

typedef enum
{
    CHILD_IO_NONE   = 0, /* discard all data */
    CHILD_IO_MPEGTS = 1, /* TS with 188-byte packets */
    CHILD_IO_TEXT   = 2, /* line-buffered stream */
    CHILD_IO_RAW    = 3, /* no buffering at all */
} child_io_mode_t;

typedef struct
{
    child_io_mode_t mode;
    child_io_callback_t on_flush;
    bool ignore_read;
} child_io_cfg_t;

typedef struct
{
    const char *name;
    const char *command;

    child_io_cfg_t sin;
    child_io_cfg_t sout;
    child_io_cfg_t serr;

    event_callback_t on_ready;
    child_close_callback_t on_close;
    void *arg;
    /* NOTE: same argument is used in I/O callbacks */
} asc_child_cfg_t;

asc_child_t *asc_child_init(const asc_child_cfg_t *cfg);
void asc_child_close(asc_child_t *child);
void asc_child_destroy(asc_child_t *child);

ssize_t asc_child_send(asc_child_t *child, const void *buf, size_t len);

void asc_child_set_on_close(asc_child_t *child
                            , child_close_callback_t on_close);
void asc_child_set_on_ready(asc_child_t *child
                            , event_callback_t on_ready);
void asc_child_toggle_input(asc_child_t *child
                            , int child_fd, bool enable);
pid_t asc_child_pid(const asc_child_t *child) __func_pure;

#endif /* _ASC_CHILD_H_ */
