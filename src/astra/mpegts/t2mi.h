/*
 * Astra TS Library (T2-MI de-encapsulator)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015-2017, Artem Kharitonov <artem@3phase.pw>
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

#ifndef _TS_T2MI_
#define _TS_T2MI_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

#include <astra/luaapi/stream.h>

#define T2MI_PLP_AUTO 0x100

typedef struct ts_t2mi_t ts_t2mi_t;

ts_t2mi_t *ts_t2mi_init(void) __asc_result;
void ts_t2mi_destroy(ts_t2mi_t *mi);

void ts_t2mi_set_fname(ts_t2mi_t *mi, const char *format
                       , ...) __asc_printf(2, 3);
void ts_t2mi_set_callback(ts_t2mi_t *mi, ts_callback_t cb, void *arg);
void ts_t2mi_set_plp(ts_t2mi_t *mi, unsigned plp_id);
void ts_t2mi_set_payload(ts_t2mi_t *mi, uint16_t pnr, uint16_t pid);
void ts_t2mi_set_demux(ts_t2mi_t *mi, module_data_t *mod
                       , demux_callback_t join_pid
                       , demux_callback_t leave_pid);

void ts_t2mi_decap(ts_t2mi_t *mi, const uint8_t *ts);

#endif /* _TS_T2MI_ */
