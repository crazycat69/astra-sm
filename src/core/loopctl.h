/*
 * Astra Core (Main loop control)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *                    2015, Artem Kharitonov <artem@sysert.ru>
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

#ifndef _ASC_LOOPCTL_H_
#define _ASC_LOOPCTL_H_ 1

enum
{
    MAIN_LOOP_NO_SLEEP = 0x00000001,
    MAIN_LOOP_SIGHUP   = 0x00000002,
    MAIN_LOOP_RELOAD   = 0x00000004,
    MAIN_LOOP_SHUTDOWN = 0x00000008,
};

typedef struct
{
    jmp_buf jmp;
    uint32_t flags;
} asc_main_loop_t;

extern asc_main_loop_t *main_loop;

void asc_main_loop_init(void);
void asc_main_loop_destroy(void);

void asc_main_loop_set(uint32_t flag);
void asc_main_loop_busy(void);

void astra_exit(void) __noreturn;
void astra_abort(void) __noreturn;
void astra_reload(void);
void astra_shutdown(void);

#endif /* _ASC_LOOPCTL_H_ */
