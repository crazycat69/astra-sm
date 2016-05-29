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

/* default timer interval */
#define SYNC_INTERVAL_MSEC 1 /* 1ms */

typedef struct mpegts_sync_t mpegts_sync_t;
typedef void (*sync_callback_t)(void *);

enum mpegts_sync_reset
{
    SYNC_RESET_ALL = 0,
    SYNC_RESET_BLOCKS,
    SYNC_RESET_PCR,
};

typedef struct
{
    /* buffer configuration */
    unsigned int enough_blocks;
    unsigned int low_blocks;
    size_t max_size;

    /* operational status */
    size_t size;
    size_t filled;
    size_t want;
    double bitrate;
    unsigned int num_blocks;
} mpegts_sync_stat_t;

mpegts_sync_t *mpegts_sync_init(void) __wur;
void mpegts_sync_destroy(mpegts_sync_t *sx);

void mpegts_sync_set_fname(mpegts_sync_t *sx
                           , const char *format, ...) __fmt_printf(2, 3);

void mpegts_sync_set_on_read(mpegts_sync_t *sx, sync_callback_t on_read);
void mpegts_sync_set_on_write(mpegts_sync_t *sx, ts_callback_t on_write);
void mpegts_sync_set_arg(mpegts_sync_t *sx, void *arg);

/*
 * Option string format:
 *    [normal = 20],[low = 10],[max size in MiB = 32]
 *
 * For example, the string "40,20,16" would be parsed as follows:
 *  - Queue 40 blocks before starting output ("normal" fill level).
 *  - Suspend output when there's less than 20 blocks in the buffer.
 *  - Buffer size cannot exceed 16 MiB.
 *
 * Any part can be omitted, e.g. "80"/",,16"/etc. are considered valid.
 *
 * Default is "20,10,32".
 */
bool mpegts_sync_parse_opts(mpegts_sync_t *sx, const char *opts);
bool mpegts_sync_set_max_size(mpegts_sync_t *sx, unsigned int mbytes);
bool mpegts_sync_set_blocks(mpegts_sync_t *sx, unsigned int enough
                            , unsigned int low);

void mpegts_sync_query(const mpegts_sync_t *sx, mpegts_sync_stat_t *out);

void mpegts_sync_loop(void *arg);
bool mpegts_sync_push(mpegts_sync_t *sx, const void *buf, size_t count) __wur;
void mpegts_sync_reset(mpegts_sync_t *sx, enum mpegts_sync_reset type);

#endif /* _TS_SYNC_ */
