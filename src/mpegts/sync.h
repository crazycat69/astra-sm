/*
 * Astra Module: MPEG-TS (Sync buffer)
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

#ifndef _TS_SYNC_
#define _TS_SYNC_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

typedef struct mpegts_sync_t mpegts_sync_t;
typedef void (*sync_callback_t)(void *);

typedef enum {
    SYNC_RESET_ALL = 0,
    SYNC_RESET_BLOCKS,
    SYNC_RESET_PCR,
} sync_reset_t;

mpegts_sync_t *mpegts_sync_init(void) __wur;
void mpegts_sync_destroy(mpegts_sync_t *sx);

void mpegts_sync_reset(mpegts_sync_t *sx, sync_reset_t type);
void mpegts_sync_set_arg(mpegts_sync_t *sx, void *arg);
void mpegts_sync_set_max_size(mpegts_sync_t *sx, size_t max_size);
void mpegts_sync_set_fname(mpegts_sync_t *sx, const char *format, ...) __fmt_printf(2, 3);
void mpegts_sync_set_on_read(mpegts_sync_t *sx, sync_callback_t on_read);
void mpegts_sync_set_on_write(mpegts_sync_t *sx, ts_callback_t on_write);
size_t mpegts_sync_space(mpegts_sync_t *sx) __wur;

void mpegts_sync_loop(void *arg);
bool mpegts_sync_push(mpegts_sync_t *sx, const void *buf, size_t count) __wur;
bool mpegts_sync_resize(mpegts_sync_t *sx, size_t new_size);

#endif /* _TS_SYNC_ */
