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

#include <astra/astra.h>
#include <astra/mpegts/sync.h>
#include <astra/mpegts/pcr.h>

#define MSG(_msg) "[%s] " _msg, sx->name

/* default fill level thresholds (see sync.h) */
#define ENOUGH_BUFFER_BLOCKS 10
#define LOW_BUFFER_BLOCKS 5
#define MIN_BUFFER_BLOCKS 2
#define MAX_BUFFER_BLOCKS 1000

/* default buffer sizes, TS packets */
#define MIN_BUFFER_SIZE ((256 * 1024) / TS_PACKET_SIZE) /* 256 KiB */
#define MAX_BUFFER_SIZE ((8 * 1024 * 1024) / TS_PACKET_SIZE) /* 8 MiB */

/* maximum allowed PCR spacing */
#define MAX_PCR_DELTA ((TS_PCR_FREQ * 150) / 1000) /* 150ms */

/* maximum time difference between dequeue calls */
#define MAX_TIME_DIFF (1 * 1000 * 1000) /* 1s */

/* don't report inter-packet jitter smaller than this value */
#define MIN_IDLE_TIME (5 * 1000) /* 5ms */

/* timeout for new block arrival */
#define MAX_IDLE_TIME (200 * 1000) /* 200ms */

/* interval for reducing buffer allocation size */
#define COMPACT_INTERVAL (10 * 1000 * 1000) /* 10s */

/* marker for unknown PCR PID */
#define PCR_PID_NONE ((unsigned int)-1)

enum sync_reset
{
    SYNC_RESET_ALL = 0,
    SYNC_RESET_BLOCKS,
    SYNC_RESET_PCR,
};

struct ts_sync_t
{
    char name[128];
    ts_packet_t *buf;

    unsigned int low_blocks;
    unsigned int enough_blocks;
    size_t max_size;
    size_t size;
    /* NOTE: packets, not bytes */

    struct
    {
        size_t rcv;  /* RX head */
        size_t pcr;  /* PCR lookahead */
        size_t send; /* TX tail */
    } pos;

    uint64_t last_run;
    uint64_t last_error;
    unsigned int pcr_pid;
    unsigned int num_blocks;

    uint64_t pcr_last;
    uint64_t pcr_cur;

    double quantum;
    double pending;

    void *arg;
    sync_callback_t on_ready;
    ts_callback_t on_ts;
    uint64_t last_compact;

    bool buffered;
};

/*
 * worker functions
 */

/* return number of packets in the buffer */
static inline
size_t buffer_filled(const ts_sync_t *sx)
{
    return ((sx->pos.rcv >= sx->pos.send)
            ? (sx->pos.rcv - sx->pos.send)
            : (sx->size + (sx->pos.rcv - sx->pos.send)));
}

/* return number of packets before PCR lookahead */
static inline
size_t buffer_lookahead(const ts_sync_t *sx)
{
    return ((sx->pos.pcr >= sx->pos.send)
            ? (sx->pos.pcr - sx->pos.send)
            : (sx->size + (sx->pos.pcr - sx->pos.send)));
}

/* return available buffer space in packets */
static inline
size_t buffer_space(const ts_sync_t *sx)
{
    return ((sx->pos.send > sx->pos.rcv)
            ? (sx->pos.send - sx->pos.rcv - 1)
            : (sx->size + (sx->pos.send - sx->pos.rcv - 1)));
}

/* calculate bitrate based on PCR quantum */
static inline
double calc_bitrate(double quantum)
{
    if (quantum > 0.0)
        return (TS_PCR_FREQ / quantum) * TS_PACKET_SIZE * 8;
    else
        return 0.0;
}

/* count blocks after PCR lookahead */
static
unsigned int block_count(const ts_sync_t *sx)
{
    unsigned int count = 1;
    size_t pos = sx->pos.pcr;

    while (pos != sx->pos.rcv)
    {
        const uint8_t *const ts = sx->buf[pos];
        const unsigned int pid = TS_GET_PID(ts);

        if (TS_IS_PCR(ts) && pid == sx->pcr_pid)
        {
            if (++count >= sx->enough_blocks)
                break;
        }

        pos = (pos + 1) % sx->size;
    }

    return count;
}

/* move PCR lookahead to next PCR packet and recalculate bitrate */
static
bool seek_pcr(ts_sync_t *sx)
{
    size_t offset = 0;

    while (sx->pos.pcr != sx->pos.rcv)
    {
        const size_t packets = ++offset;
        const size_t pos = sx->pos.pcr;
        sx->pos.pcr = (sx->pos.pcr + 1) % sx->size;

        /* filter out packets without a PCR value */
        const uint8_t *const ts = sx->buf[pos];
        const unsigned int pid = TS_GET_PID(ts);

        if (!TS_IS_PCR(ts))
            continue;

        if (sx->pcr_pid == PCR_PID_NONE && pid != TS_NULL_PID)
        {
            /* latch onto first PCR PID we see */
            sx->pcr_pid = pid;
            asc_log_debug(MSG("selected PCR PID %u"), pid);
        }

        if (pid != sx->pcr_pid)
            continue;

        /* check PCR validity */
        sx->pcr_last = sx->pcr_cur;
        sx->pcr_cur = TS_GET_PCR(ts);
        offset = 0;

        const uint64_t delta = TS_PCR_DELTA(sx->pcr_last, sx->pcr_cur);
        if (!(delta > 0 && delta < MAX_PCR_DELTA))
        {
            /*
             * PCR discontinuity. Any bitrate estimation at this point
             * will be incorrect, so let's just drop the whole block.
             */
            sx->pos.send = pos;

#ifdef SYNC_DEBUG
            if (sx->pcr_last == TS_TIME_NONE)
            {
                asc_log_debug(MSG("first PCR packet at offset %zu")
                              , (packets - 1) * TS_PACKET_SIZE);
            }
            else if (delta >= MAX_PCR_DELTA)
            {
                asc_log_debug(MSG("PCR discontinuity (%llums), dropping block")
                              , delta / (TS_PCR_FREQ / 1000ULL));
            }
            else if (delta == 0)
            {
                asc_log_debug(MSG("PCR did not increase, dropping block"));
            }
#endif /* SYNC_DEBUG */

            continue;
        }

        /* calculate PCR impact of a single packet at current bitrate */
        sx->quantum = (double)delta / packets;
        return true;
    }

    return false;
}

/* resize buffer while preserving its contents */
static
bool buffer_resize(ts_sync_t *sx, size_t new_size)
{
    if (new_size == 0)
        new_size = sx->size * 2;

    if (new_size < MIN_BUFFER_SIZE)
    {
        asc_log_debug(MSG("cannot shrink buffer to less than minimum size"));
        new_size = MIN_BUFFER_SIZE;
    }

    /* don't let it grow bigger than max_size */
    if (new_size > sx->max_size)
    {
        if (sx->size >= sx->max_size)
        {
            asc_log_debug(MSG("buffer already at max size, cannot expand"));
            return false;
        }
        else
        {
            new_size = sx->max_size;
        }
    }
    else if (new_size == sx->size)
    {
        asc_log_debug(MSG("buffer size unchanged"));
        return true;
    }

    /* adjust pointers */
    const size_t filled = buffer_filled(sx);
    const size_t lookahead = buffer_lookahead(sx);

    if (filled > new_size)
    {
        asc_log_debug(MSG("new size (%zu) is too small for "
                          "current fill level (%zu)"), new_size, filled);

        return false;
    }

    /* move contents to new buffer */
    ts_packet_t *const buf = ASC_ALLOC(new_size, ts_packet_t);

    size_t pos = sx->pos.send;
    size_t left = filled;
    ts_packet_t *ts = buf;

    while (left > 0)
    {
        size_t chunk = sx->size - pos;
        if (left < chunk)
            chunk = left; /* last piece */

        memcpy(ts, sx->buf[pos], sizeof(*ts) * chunk);
        pos = (pos + chunk) % sx->size;

        ts += chunk;
        left -= chunk;
    }

#ifdef SYNC_DEBUG
    asc_log_debug(MSG("buffer %s to %zu slots (%zu bytes)")
                  , (new_size > sx->size ? "expanded" : "shrunk")
                  , new_size, new_size * TS_PACKET_SIZE);
#endif

    /* clean up */
    free(sx->buf);

    sx->pos.rcv = filled;
    sx->pos.pcr = lookahead;
    sx->pos.send = 0;
    sx->size = new_size;
    sx->buf = buf;

    return true;
}

/* reset various buffer facilities */
static
void buffer_reset(ts_sync_t *sx, enum sync_reset type)
{
    switch (type) {
        case SYNC_RESET_ALL:
            /* restore buffer to its initial state */
            sx->pos.rcv = sx->pos.pcr = sx->pos.send = 0;
            sx->last_run = 0;

            buffer_resize(sx, MIN_BUFFER_SIZE);

        case SYNC_RESET_BLOCKS:
            /* restart initial buffering */
            sx->last_error = sx->num_blocks = sx->buffered = 0;

        case SYNC_RESET_PCR:
            /* reset PCR lookahead routine */
            sx->pcr_last = sx->pcr_cur = TS_TIME_NONE;
            sx->pcr_pid = PCR_PID_NONE;
            sx->quantum = sx->pending = 0.0;

            /* start searching from first packet in queue */
            sx->pos.pcr = sx->pos.send;
    }
}

/* return number of milliseconds elapsed since this function's last call */
static
unsigned int update_last_run(ts_sync_t *sx, uint64_t time_now)
{
    uint64_t elapsed = 0;

    if (sx->last_run > 0)
    {
        elapsed = time_now - sx->last_run;

        if (elapsed >= MAX_TIME_DIFF)
        {
            asc_log_error(MSG("time travel detected, resetting buffer"));
            buffer_reset(sx, SYNC_RESET_ALL);

            elapsed = 0;
        }
    }

    sx->last_run = time_now;

    return elapsed;
}

void ts_sync_loop(void *arg)
{
    ts_sync_t *const sx = (ts_sync_t *)arg;

    /* timekeeping */
    const uint64_t time_now = asc_utime();
    const unsigned int elapsed = update_last_run(sx, time_now);

    /* request more packets if needed (pull mode) */
    if (sx->on_ready != NULL && sx->num_blocks < sx->enough_blocks)
        sx->on_ready(sx->arg);

    if (elapsed == 0 || !sx->buffered)
        return; /* let's not divide by zero */

    /* suspend output on underflow */
    unsigned int downtime = 0;

    if (sx->last_error > 0)
    {
        /* check if we can resume output */
        downtime = time_now - sx->last_error;
    }

    if (sx->num_blocks < sx->low_blocks)
    {
        if (sx->last_error == 0)
        {
            /* set error state */
            sx->last_error = time_now;
        }
        else if (downtime >= MAX_IDLE_TIME)
        {
            asc_log_error(MSG("no input in %.2fms, resetting buffer")
                          , downtime / 1000.0);

            buffer_reset(sx, SYNC_RESET_ALL);
        }

        return;
    }
    else if (sx->last_error > 0)
    {
        if (downtime >= MIN_IDLE_TIME)
        {
            asc_log_debug(MSG("buffer underflow; output suspended for %.2fms")
                          , downtime / 1000.0);
        }

        sx->last_error = 0;
    }

    /* dequeue packets */
    sx->pending += (double)elapsed * (TS_PCR_FREQ / 1000000);

    while (sx->pending >= sx->quantum)
    {
        sx->pending -= sx->quantum;

        if (sx->pos.send == sx->pos.pcr)
        {
            /* look up next PCR value */
            const bool found = seek_pcr(sx);

            if (!found)
            {
                asc_log_error(MSG("next PCR not found, resetting buffer"));
                buffer_reset(sx, SYNC_RESET_BLOCKS);

                break;
            }

            sx->num_blocks = block_count(sx);

            if (time_now - sx->last_compact >= COMPACT_INTERVAL)
            {
                /* shrink buffer on < 25% fill */
                const size_t filled = buffer_filled(sx);
                const size_t thresh = sx->size / 4;

                if (filled < thresh && sx->size > MIN_BUFFER_SIZE)
                    buffer_resize(sx, sx->size / 2);

#ifdef SYNC_DEBUG
                /* report buffer status */
                const unsigned int percent = (filled * 100) / sx->size;
                const double bitrate = calc_bitrate(sx->quantum);

                asc_log_debug(MSG("BR: %.2f, fill: %5zu/%5zu (%2u%%), "
                                  "R: %5zu, P: %5zu, S: %5zu, B: %u")
                              , bitrate, filled, sx->size, percent
                              , sx->pos.rcv, sx->pos.pcr, sx->pos.send
                              , sx->num_blocks);

#endif /* SYNC_DEBUG */

                sx->last_compact = time_now;
            }
        }

        sx->on_ts(sx->arg, sx->buf[sx->pos.send]);
        sx->pos.send = (sx->pos.send + 1) % sx->size;
    }
}

bool ts_sync_push(ts_sync_t *sx, const void *buf, size_t count)
{
    const ts_packet_t *const ts = (const ts_packet_t *)buf;

    while (buffer_space(sx) < count)
    {
        const bool ok = buffer_resize(sx, 0);

        if (!ok)
        {
            if (sx->num_blocks == 0)
            {
                /* buffer is at maximum size, yet we couldn't find PCR */
                asc_log_error(MSG("PCR absent or invalid; "
                                  "dropping %zu packets"), count);
            }

            return false;
        }
    }

    for (size_t i = 0; i < count; i++)
    {
        if (TS_IS_PCR(ts[i]))
        {
            const unsigned int pid = TS_GET_PID(ts[i]);

            if (!sx->buffered
                && sx->pcr_pid == PCR_PID_NONE && pid != TS_NULL_PID)
            {
                sx->pcr_pid = pid;
                asc_log_debug(MSG("selected PCR PID %u (init)"), pid);
            }

            if (pid == sx->pcr_pid)
                sx->num_blocks++;
        }

        memcpy(sx->buf[sx->pos.rcv], ts[i], TS_PACKET_SIZE);
        sx->pos.rcv = (sx->pos.rcv + 1) % sx->size;
    }

    if (!sx->buffered
        && sx->num_blocks >= sx->enough_blocks)
    {
        buffer_reset(sx, SYNC_RESET_PCR);
        sx->buffered = true;
    }

    return true;
}

/*
 * create and destroy
 */

ts_sync_t *ts_sync_init(ts_callback_t on_ts, void *arg)
{
    ts_sync_t *const sx = ASC_ALLOC(1, ts_sync_t);

    sx->low_blocks = LOW_BUFFER_BLOCKS;
    sx->enough_blocks = ENOUGH_BUFFER_BLOCKS;
    sx->size = MIN_BUFFER_SIZE;
    sx->max_size = MAX_BUFFER_SIZE;

    static const char def_name[] = "sync";
    memcpy(sx->name, def_name, sizeof(def_name));

    sx->pcr_last = TS_TIME_NONE;
    sx->pcr_cur = TS_TIME_NONE;
    sx->pcr_pid = PCR_PID_NONE;

    sx->on_ts = on_ts;
    sx->arg = arg;

    sx->buf = ASC_ALLOC(sx->size, ts_packet_t);

    return sx;
}

void ts_sync_destroy(ts_sync_t *sx)
{
    free(sx->buf);
    free(sx);
}

void ts_sync_set_on_ready(ts_sync_t *sx, sync_callback_t on_ready)
{
    sx->on_ready = on_ready;
}

void ts_sync_set_fname(ts_sync_t *sx, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsnprintf(sx->name, sizeof(sx->name), format, ap);
    va_end(ap);
}

bool ts_sync_set_opts(ts_sync_t *sx, const char *opts)
{
    unsigned int numopts[3] = { 0, 0, 0 };

    /* break up option string */
    const char *ch, *str;
    unsigned int idx = 0;

    for (ch = str = opts; *ch != '\0'; ch++)
    {
        if (*ch == ',')
        {
            numopts[idx] = atoi(str);
            str = ch + 1;

            if (++idx >= ASC_ARRAY_SIZE(numopts))
            {
                return false;
            }
        }
        else if (!(*ch >= '0' && *ch <= '9'))
        {
            return false;
        }
    }

    numopts[idx] = atoi(str);

    /* set fill thresholds */
    const unsigned int enough = numopts[0];
    const unsigned int low = numopts[1];

    if ((enough > 0 || low > 0)
        && !ts_sync_set_blocks(sx, enough, low))
    {
        return false;
    }

    /* set maximum buffer size */
    const unsigned int mbytes = numopts[2];

    if (mbytes > 0
        && !ts_sync_set_max_size(sx, mbytes))
    {
        return false;
    }

    return true;
}

bool ts_sync_set_max_size(ts_sync_t *sx, unsigned int mbytes)
{
    const size_t max_size = (mbytes * 1024ULL * 1024ULL) / TS_PACKET_SIZE;

    if (max_size < MIN_BUFFER_SIZE || max_size < sx->size)
    {
        asc_log_error(MSG("new buffer size limit is too small"));
        return false;
    }

    asc_log_debug(MSG("setting buffer size limit to %u MiB"), mbytes);
    sx->max_size = max_size;

    return true;
}

bool ts_sync_set_blocks(ts_sync_t *sx, unsigned int enough, unsigned int low)
{
    if (enough == 0)
        enough = sx->enough_blocks;

    if (low == 0)
        low = sx->low_blocks;

    if (!(enough >= MIN_BUFFER_BLOCKS && enough <= MAX_BUFFER_BLOCKS)
        || !(low >= MIN_BUFFER_BLOCKS && low <= MAX_BUFFER_BLOCKS))
    {
        asc_log_error(MSG("requested buffer fill thresholds out of range"));
        return false;
    }

    if (low > enough)
        low = enough;

    asc_log_debug(MSG("setting buffer fill thresholds: normal = %u, low = %u")
                  , enough, low);

    sx->enough_blocks = enough;
    sx->low_blocks = low;

    return true;
}

void ts_sync_query(const ts_sync_t *sx, ts_sync_stat_t *out)
{
    memset(out, 0, sizeof(*out));

    out->size = sx->size;
    out->filled = buffer_filled(sx);
    out->num_blocks = sx->num_blocks;
    out->bitrate = calc_bitrate(sx->quantum);

    out->enough_blocks = sx->enough_blocks;
    out->low_blocks = sx->low_blocks;
    out->max_size = sx->max_size;

    /* suggested packet count to push */
    if (out->filled == 0 || sx->num_blocks < sx->low_blocks)
    {
        out->want = sx->size / 2;
    }
    else if (sx->num_blocks < sx->enough_blocks)
    {
        const size_t more = sx->enough_blocks - sx->num_blocks;
        out->want = (out->filled / sx->num_blocks) * more * 2;
    }
    else
    {
        out->want = 0;
    }
}

void ts_sync_reset(ts_sync_t *sx)
{
    buffer_reset(sx, SYNC_RESET_ALL);
}
