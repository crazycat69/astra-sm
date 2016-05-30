/*
 * Astra Module: MPEG-TS (Sync buffer)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015-2016, Artem Kharitonov <artem@3phase.pw>
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

#include <astra.h>
#include <mpegts/sync.h>
#include <mpegts/pcr.h>

#define MSG(_msg) "[%s] " _msg, sx->name

/* default fill level thresholds (see sync.h) */
#define ENOUGH_BUFFER_BLOCKS 20
#define LOW_BUFFER_BLOCKS 10
#define MIN_BUFFER_BLOCKS 5
#define MAX_BUFFER_BLOCKS 1000

/* default buffer sizes, TS packets */
#define MIN_BUFFER_SIZE ((256 * 1024) / TS_PACKET_SIZE) /* 256 KiB */
#define MAX_BUFFER_SIZE ((32 * 1024 * 1024) / TS_PACKET_SIZE) /* 32 MiB */

/* maximum allowed PCR spacing */
#define MAX_PCR_DELTA ((PCR_TIME_BASE * 150) / 1000) /* 150ms */

/* don't report inter-packet jitter smaller than this value */
#define MIN_IDLE_TIME (5 * 1000) /* 5ms */

/* timeout for new block arrival */
#define MAX_IDLE_TIME (200 * 1000) /* 200ms */

typedef uint8_t ts_packet_t[TS_PACKET_SIZE];

struct mpegts_sync_t
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
        size_t rcv;  /* RX tail */
        size_t pcr;  /* PCR lookahead */
        size_t send; /* TX tail */
    } pos;

    uint64_t last_run;
    uint64_t last_error;
    unsigned int pcr_pid;
    unsigned int num_blocks;

    uint64_t pcr_last;
    uint64_t pcr_cur;
    size_t offset;

    double bitrate;
    double pending;

    void *arg;
    sync_callback_t on_ready;
    ts_callback_t on_write;
#ifdef SYNC_DEBUG
    uint64_t last_report;
#endif /* SYNC_DEBUG */

    bool buffered;
};

/*
 * create and destroy
 */

mpegts_sync_t *mpegts_sync_init(void)
{
    mpegts_sync_t *const sx = ASC_ALLOC(1, mpegts_sync_t);

    sx->low_blocks = LOW_BUFFER_BLOCKS;
    sx->enough_blocks = ENOUGH_BUFFER_BLOCKS;
    sx->size = MIN_BUFFER_SIZE;
    sx->max_size = MAX_BUFFER_SIZE;

    static const char def_name[] = "sync";
    strncpy(sx->name, def_name, sizeof(def_name));

    sx->pcr_last = XTS_NONE;
    sx->pcr_cur = XTS_NONE;

    sx->buf = ASC_ALLOC(sx->size, ts_packet_t);

    return sx;
}

void mpegts_sync_destroy(mpegts_sync_t *sx)
{
    free(sx->buf);
    free(sx);
}

/*
 * setters and getters
 */

void mpegts_sync_set_fname(mpegts_sync_t *sx, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    vsnprintf(sx->name, sizeof(sx->name), format, ap);

    va_end(ap);
}

void mpegts_sync_set_on_ready(mpegts_sync_t *sx, sync_callback_t on_ready)
{
    sx->on_ready = on_ready;
}

void mpegts_sync_set_on_write(mpegts_sync_t *sx, ts_callback_t on_write)
{
    sx->on_write = on_write;
}

void mpegts_sync_set_arg(mpegts_sync_t *sx, void *arg)
{
    sx->arg = arg;
}

bool mpegts_sync_parse_opts(mpegts_sync_t *sx, const char *opts)
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
                return false;
        }
        else if (!(*ch >= '0' && *ch <= '9'))
            return false;
    }

    numopts[idx] = atoi(str);

    /* set fill thresholds */
    const unsigned int enough = numopts[0];
    const unsigned int low = numopts[1];

    if ((enough > 0 || low > 0)
        && !mpegts_sync_set_blocks(sx, enough, low))
    {
        return false;
    }

    /* set maximum buffer size */
    const unsigned int mbytes = numopts[2];

    if (mbytes > 0
        && !mpegts_sync_set_max_size(sx, mbytes))
    {
        return false;
    }

    return true;
}

bool mpegts_sync_set_max_size(mpegts_sync_t *sx, unsigned int mbytes)
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

bool mpegts_sync_set_blocks(mpegts_sync_t *sx, unsigned int enough
                            , unsigned int low)
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

static
size_t buffer_slots(const mpegts_sync_t *sx, bool filled);

void mpegts_sync_query(const mpegts_sync_t *sx, mpegts_sync_stat_t *out)
{
    out->size = sx->size;
    out->filled = buffer_slots(sx, true);
    out->num_blocks = sx->num_blocks;
    out->bitrate = sx->bitrate;

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

/*
 * worker functions
 */

static __func_pure
unsigned int block_count(const mpegts_sync_t *sx)
{
    unsigned int count = 1;

    /* count blocks after PCR lookahead */
    size_t pos = sx->pos.pcr;
    while (pos != sx->pos.rcv)
    {
        const uint8_t *const ts = sx->buf[pos];
        if (TS_IS_PCR(ts) && TS_GET_PID(ts) == sx->pcr_pid)
        {
            if (++count >= sx->enough_blocks)
                break;
        }

        if (++pos >= sx->size)
            /* buffer wrap around */
            pos = 0;
    }

    return count;
}

static
bool seek_pcr(mpegts_sync_t *sx)
{
    while (sx->pos.pcr != sx->pos.rcv)
    {
        const size_t lookahead = sx->pos.pcr;
        const uint8_t *const ts = sx->buf[lookahead];
        const unsigned int pid = TS_GET_PID(ts);

        const size_t bytes = sx->offset;
        sx->offset += TS_PACKET_SIZE;

        if (++sx->pos.pcr >= sx->size)
            /* buffer wrap around */
            sx->pos.pcr = 0;

        if (!TS_IS_PCR(ts))
            continue;

        if (!sx->pcr_pid && pid != NULL_TS_PID)
        {
            /* latch onto first PCR pid we see */
            sx->pcr_pid = pid;
            asc_log_debug(MSG("selected PCR pid %u"), sx->pcr_pid);
        }

        if (pid != sx->pcr_pid)
            continue;

        /* check PCR validity */
        sx->pcr_last = sx->pcr_cur;
        sx->pcr_cur = TS_GET_PCR(ts);
        sx->offset = 0;

        if (sx->pcr_last == XTS_NONE)
        {
            /* beginning of the first block; start output from here */
            sx->pos.send = lookahead;
            if (bytes > 0)
            {
                asc_log_debug(MSG("first PCR packet at %zu bytes; "
                                  "dropping everything before it"), bytes);
            }
            continue;
        }

        int64_t delta = sx->pcr_cur - sx->pcr_last;
        if (delta < 0)
        {
            /* clock reset or wrap around */
            delta += PCR_MAX + 1;
#ifdef SYNC_DEBUG
            const int ms = delta / (PCR_TIME_BASE / 1000);
            asc_log_debug(MSG("PCR decreased, assuming wrap around with delta %dms"), ms);
#endif /* SYNC_DEBUG */
        }

        if (delta <= 0)
        {
            /* shouldn't happen */
            asc_log_error(MSG("PCR did not increase!"));
            continue;
        }
        else if (delta >= MAX_PCR_DELTA)
        {
            const unsigned int ms = delta / (PCR_TIME_BASE / 1000);
            asc_log_error(MSG("PCR jumped forward by %ums, skipping block"), ms);

            /* in case this happens during initial buffering */
            sx->pos.send = lookahead;
            sx->num_blocks = 0;

            continue;
        }

        /* calculate momentary bitrate */
        const double inv_usecs = (double)PCR_TIME_BASE / delta;
        sx->bitrate = (bytes + TS_PACKET_SIZE) * inv_usecs;
        /* NOTE: inv_usecs = (1000 / PCR_INTERVAL) */

        if (sx->bitrate > 0)
            return true;
    }

    return false;
}

static
unsigned int usecs_elapsed(mpegts_sync_t *sx, uint64_t time_now)
{
    unsigned int elapsed;

    if (sx->last_run)
    {
        elapsed = time_now - sx->last_run;

        if (time_now < sx->last_run || elapsed > 1000000)
        {
            asc_log_error(MSG("time travel detected; resetting"));
            mpegts_sync_reset(sx, SYNC_RESET_ALL);

            elapsed = 0;
        }
    }
    else
    {
        /* no previous timestamp */
        elapsed = 0;
    }

    sx->last_run = time_now;
    return elapsed;
}

static
size_t buffer_slots(const mpegts_sync_t *sx, bool filled)
{
    ssize_t cnt;

    if (filled)
        cnt = sx->pos.rcv - sx->pos.send;
    else
        cnt = sx->pos.send - sx->pos.rcv - 1;

    if (cnt < 0)
    {
        cnt += sx->size;
        if (cnt < 0)
            /* shouldn't happen */
            return 0;
    }

    return cnt;
}

static
bool buffer_resize(mpegts_sync_t *sx, size_t new_size)
{
    if (!new_size)
        new_size = sx->size * 2;

    if (new_size < MIN_BUFFER_SIZE)
    {
        asc_log_warning(MSG("cannot shrink buffer to less than its minimum size"));
        new_size = MIN_BUFFER_SIZE;
    }

    /* don't let it grow bigger than max_size */
    if (new_size > sx->max_size)
    {
        if (sx->size >= sx->max_size)
        {
            asc_log_debug(MSG("buffer already at maximum size, cannot expand"));
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
    ssize_t filled = sx->pos.rcv - sx->pos.send;
    if (filled < 0)
        filled += sx->size;

    if (filled > (ssize_t)new_size)
    {
        asc_log_error(MSG("new size (%zu) is too small for current fill level (%zu)")
                      , new_size, filled);

        return false;
    }

    ssize_t lookahead = sx->pos.pcr - sx->pos.send;
    if (lookahead < 0)
        lookahead += sx->size;

    sx->pos.rcv = filled;
    sx->pos.pcr = lookahead;

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

        memcpy(ts, &sx->buf[pos], sizeof(*ts) * chunk);

        pos += chunk;
        if (pos >= sx->size)
            /* buffer wrap around */
            pos = 0;

        ts += chunk;
        left -= chunk;
    }

    /* clean up */
#ifdef SYNC_DEBUG
    asc_log_debug(MSG("buffer %s to %zu slots (%zu bytes)")
                  , (new_size > sx->size ? "expanded" : "shrunk")
                  , new_size, new_size * TS_PACKET_SIZE);
#endif /* SYNC_DEBUG */

    free(sx->buf);

    sx->pos.send = 0;
    sx->size = new_size;
    sx->buf = buf;

    return true;
}

void mpegts_sync_loop(void *arg)
{
    mpegts_sync_t *const sx = (mpegts_sync_t *)arg;

    /* timekeeping */
    const uint64_t time_now = asc_utime();
    const unsigned int elapsed = usecs_elapsed(sx, time_now);

    if (!elapsed)
        /* let's not divide by zero */
        return;

    /* data request hook */
    if (sx->on_ready && sx->num_blocks < sx->enough_blocks)
        sx->on_ready(sx->arg);

    /* initial buffering */
    if (!sx->buffered)
    {
        if (seek_pcr(sx))
        {
            if (sx->num_blocks >= sx->enough_blocks)
            {
                /* got enough data to start output */
                mpegts_sync_reset(sx, SYNC_RESET_PCR);
                sx->buffered = true;
            }

            if (!(sx->num_blocks % 5))
            {
                asc_log_debug(MSG("buffered blocks: %u (min %u)%s")
                              , sx->num_blocks, sx->enough_blocks
                              , (sx->buffered ? ", starting output" : ""));
            }
        }
        else if (!buffer_slots(sx, false) && !buffer_resize(sx, 0))
        {
            asc_log_error(MSG("PCR absent or invalid; resetting buffer"));
            mpegts_sync_reset(sx, SYNC_RESET_ALL);
        }

        return;
    }

    /* next block lookup */
    if (sx->pos.send == sx->pos.pcr)
    {
        const bool found = seek_pcr(sx);

        if (!found)
        {
            asc_log_error(MSG("next PCR not found; buffering..."));
            mpegts_sync_reset(sx, SYNC_RESET_BLOCKS);

            return;
        }

        sx->num_blocks = block_count(sx);

        /* shrink buffer on < 25% fill */
        const size_t filled = buffer_slots(sx, true);
        const size_t thresh = sx->size / 4;

        if (filled < thresh && sx->size > MIN_BUFFER_SIZE)
            buffer_resize(sx, sx->size / 2);

#ifdef SYNC_DEBUG
        if (time_now - sx->last_report > 10000000) /* 10 sec */
        {
            const unsigned int percent = (filled * 100) / sx->size;

            asc_log_debug(MSG("BR: %.2f, fill: %5zu/%5zu (%2u%%), R: %5zu, P: %5zu, S: %5zu, B: %u")
                          , sx->bitrate, filled, sx->size, percent
                          , sx->pos.rcv, sx->pos.pcr, sx->pos.send
                          , sx->num_blocks);

            sx->last_report = time_now;
        }
#endif /* SYNC_DEBUG */
    }

    /* underflow correction */
    unsigned int downtime = 0;

    if (sx->last_error)
    {
        /* check if we can resume output */
        downtime = time_now - sx->last_error;
    }

    if (sx->num_blocks < sx->low_blocks)
    {
        if (!sx->last_error)
        {
            /* set error state */
            sx->last_error = time_now;
        }
        else if (downtime >= MAX_IDLE_TIME)
        {
            asc_log_error(MSG("no input in %.2fms; resetting")
                          , downtime / 1000.0);

            mpegts_sync_reset(sx, SYNC_RESET_ALL);
        }

        return;
    }
    else if (sx->last_error)
    {
        if (downtime >= MIN_IDLE_TIME)
        {
            asc_log_debug(MSG("buffer underflow; output suspended for %.2fms")
                          , downtime / 1000.0);
        }
        sx->last_error = 0;
    }

    /* output */
    sx->pending += (sx->bitrate / (1000000.0 / elapsed));
    while (sx->pending > TS_PACKET_SIZE)
    {
        if (sx->pos.send >= sx->size)
            /* buffer wrap around */
            sx->pos.send = 0;

        if (sx->pos.send == sx->pos.pcr)
            /* block end */
            break;

        const uint8_t *ts = sx->buf[sx->pos.send++];
        if (sx->on_write)
            sx->on_write(sx->arg, ts);

        sx->pending -= TS_PACKET_SIZE;
    }
}

bool mpegts_sync_push(mpegts_sync_t *sx, const void *buf, size_t count)
{
    while (buffer_slots(sx, false) < count)
    {
        if (!buffer_resize(sx, 0))
        {
            if (!sx->num_blocks)
            {
                /* buffer is at maximum size, yet we couldn't find PCR */
                asc_log_error(MSG("PCR absent or invalid; dropping %zu packet%s")
                              , count, (count > 1) ? "s" : "");
            }

            return false;
        }
    }

    const ts_packet_t *ts = (const ts_packet_t *)buf;

    /* update block count */
    for (size_t i = 0; i < count; i++)
    {
        if (TS_IS_PCR(ts[i]) && TS_GET_PID(ts[i]) == sx->pcr_pid)
            sx->num_blocks++;
    }

    while (count > 0)
    {
        size_t chunk = sx->size - sx->pos.rcv;
        if (count < chunk)
            chunk = count; /* last piece */

        memcpy(&sx->buf[sx->pos.rcv], ts, sizeof(*ts) * chunk);

        sx->pos.rcv += chunk;
        if (sx->pos.rcv >= sx->size)
            /* buffer wrap around */
            sx->pos.rcv = 0;

        ts += chunk;
        count -= chunk;
    }

    return true;
}

void mpegts_sync_reset(mpegts_sync_t *sx, enum mpegts_sync_reset type)
{
    switch (type) {
        case SYNC_RESET_ALL:
            /* restore buffer to its initial state */
            sx->pos.rcv = sx->pos.pcr = sx->pos.send = 0;
            sx->last_run = 0;

            buffer_resize(sx, MIN_BUFFER_SIZE);

        case SYNC_RESET_BLOCKS:
            /* reset output buffering */
            sx->last_error = sx->num_blocks = sx->buffered = 0;

        case SYNC_RESET_PCR:
            /* reset PCR lookahead routine */
            sx->pcr_pid = sx->offset = 0;
            sx->pcr_last = sx->pcr_cur = XTS_NONE;
            sx->bitrate = sx->pending = 0.0;

            /* start searching from first packet in queue */
            sx->pos.pcr = sx->pos.send;
    }
}
