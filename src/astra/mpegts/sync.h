/*
 * Astra TS Library (Sync buffer)
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

#ifndef _TS_SYNC_
#define _TS_SYNC_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

/* default timer interval, milliseconds */
#define SYNC_INTERVAL_MSEC 5 /* 5ms */

typedef struct ts_sync_t ts_sync_t;
typedef void (*sync_callback_t)(void *);

typedef struct
{
    /* buffer configuration */
    unsigned int enough_blocks;
    unsigned int low_blocks;
    size_t max_size;

    /* operational status */
    double bitrate;
    size_t size;
    size_t filled;
    size_t want;
    unsigned int num_blocks;
} ts_sync_stat_t;

ts_sync_t *ts_sync_init(ts_callback_t on_ts, void *arg) __asc_result;
void ts_sync_destroy(ts_sync_t *sx);

void ts_sync_set_on_ready(ts_sync_t *sx, sync_callback_t on_ready);
void ts_sync_set_fname(ts_sync_t *sx, const char *format
                       , ...) __asc_printf(2, 3);

/*
 * Option string format:
 *    [normal = 10],[low = 5],[max size in MiB = 8]
 *
 * For example, the string "40,20,16" would be parsed as follows:
 *  - Queue 40 blocks before starting output ("normal" fill level).
 *  - Suspend output when there's less than 20 blocks in the buffer.
 *  - Buffer size cannot exceed 16 MiB.
 *
 * Any part can be omitted, e.g. "80"/",,16"/etc. are considered valid.
 *
 * Default is "10,5,8".
 */
bool ts_sync_set_opts(ts_sync_t *sx, const char *opts);
bool ts_sync_set_max_size(ts_sync_t *sx, unsigned int mbytes);
bool ts_sync_set_blocks(ts_sync_t *sx, unsigned int enough, unsigned int low);

void ts_sync_query(const ts_sync_t *sx, ts_sync_stat_t *out);
void ts_sync_reset(ts_sync_t *sx);

void ts_sync_loop(void *arg);
bool ts_sync_push(ts_sync_t *sx, const void *buf
                  , size_t count) __asc_result;

#endif /* _TS_SYNC_ */
