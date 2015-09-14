/*
 * Astra Module: Pipe
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

#ifndef _PIPE_H_
#define _PIPE_H_ 1

#include <astra.h>
#include <core/stream.h>
#include <core/child.h>
#include <core/timer.h>
#include <mpegts/sync.h>

#define MSG(_msg) "[%s] " _msg, mod->name

struct module_data_t
{
    MODULE_STREAM_DATA();

    const char *prefix;
    char name[128];
    unsigned delay;
    bool enable_sync;

    mpegts_sync_t *sync;
    asc_timer_t *sync_loop;

    bool can_send;
    size_t dropped;

    asc_child_cfg_t config;
    asc_child_t *child;

    asc_timer_t *restart;
};

void pipe_init(module_data_t *mod);
void pipe_destroy(module_data_t *mod);

void pipe_upstream_ts(module_data_t *mod, const uint8_t *ts);
void pipe_child_ts(void *arg, const uint8_t *ts, size_t len);
void pipe_child_text(void *arg, const uint8_t *text, size_t len);

void pipe_on_close(void *arg, int exit_code);
void pipe_on_retry(void *arg);
void pipe_on_ready(void *arg);

#endif /* _PIPE_H_ */
