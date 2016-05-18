/*
 * Astra Core (Main loop)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *               2015-2016, Artem Kharitonov <artem@3phase.pw>
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

#ifndef _ASC_MAINLOOP_H_
#define _ASC_MAINLOOP_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

typedef void (*loop_callback_t)(void *);

enum
{
    MAIN_LOOP_NO_SLEEP = 0x00000001,
    MAIN_LOOP_SIGHUP   = 0x00000002,
    MAIN_LOOP_RELOAD   = 0x00000004,
    MAIN_LOOP_SHUTDOWN = 0x00000008,
};

void asc_job_queue(void *owner, loop_callback_t proc, void *arg);
void asc_job_prune(void *owner);

void asc_main_loop_init(void);
void asc_main_loop_destroy(void);
bool asc_main_loop_run(void) __wur;

void asc_main_loop_set(uint32_t flag);
void astra_shutdown(void);

#define asc_main_loop_busy() \
    asc_main_loop_set(MAIN_LOOP_NO_SLEEP)

#define astra_reload() \
    asc_main_loop_set(MAIN_LOOP_RELOAD)

#define astra_sighup() \
    asc_main_loop_set(MAIN_LOOP_SIGHUP)

#endif /* _ASC_MAINLOOP_H_ */
