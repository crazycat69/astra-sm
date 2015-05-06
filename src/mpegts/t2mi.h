/*
 * Astra Module: MPEG-TS (T2-MI de-encapsulator)
 * http://cesbo.com/astra
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

#ifndef _TS_T2MI_
#define _TS_T2MI_ 1

typedef struct mpegts_t2mi_t mpegts_t2mi_t;

mpegts_t2mi_t *mpegts_t2mi_init(void);
void mpegts_t2mi_destroy(mpegts_t2mi_t *mi);

void mpegts_t2mi_set_name(mpegts_t2mi_t *mi, const char *name);
void mpegts_t2mi_set_callback(mpegts_t2mi_t *mi, ts_callback_t cb, void *arg);
void mpegts_t2mi_set_plp(mpegts_t2mi_t *mi, unsigned plp_id);
void mpegts_t2mi_set_payload(mpegts_t2mi_t *mi, uint16_t pnr, uint16_t pid);
void mpegts_t2mi_set_demux(mpegts_t2mi_t *mi, void *arg
                           , demux_callback_t join_pid
                           , demux_callback_t leave_pid);

void mpegts_t2mi_decap(mpegts_t2mi_t *mi, const uint8_t *ts);

/* auto PLP selection marker */
#define PLP_ID_NONE 0x100

#endif /* _TS_T2MI_ */
