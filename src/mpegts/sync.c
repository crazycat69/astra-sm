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

#include <astra.h>
#include <mpegts/sync.h>
#include <mpegts/pcr.h>

#define MSG(_msg) "[%s] " _msg, sx->name

/* buffer this many blocks before starting output */
#define ENOUGH_BUFFER_BLOCKS 20

/* low fill threshold; stop sending TS until more blocks arrive */
#define LOW_BUFFER_BLOCKS 10

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

    /* packets, not bytes */
    size_t size;
    size_t max_size;

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
    sync_callback_t on_read;
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
    mpegts_sync_t *const sx = (mpegts_sync_t *)calloc(1, sizeof(*sx));
    asc_assert(sx != NULL, "[sync] calloc() failed");

    sx->size = MIN_BUFFER_SIZE;
    sx->max_size = MAX_BUFFER_SIZE;
    strcpy(sx->name, "sync");

    sx->pcr_last = XTS_NONE;
    sx->pcr_cur = XTS_NONE;

    sx->buf = (ts_packet_t *)calloc(sx->size, sizeof(*sx->buf));
    asc_assert(sx->buf != NULL, "[sync] calloc() failed");

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
void mpegts_sync_reset(mpegts_sync_t *sx, enum mpegts_sync_reset type)
{
    switch (type) {
        case SYNC_RESET_ALL:
            /* restore buffer to its initial state */
            sx->pos.rcv = sx->pos.pcr = sx->pos.send = 0;
            sx->last_run = 0;

            mpegts_sync_resize(sx, MIN_BUFFER_SIZE);

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

void mpegts_sync_set_fname(mpegts_sync_t *sx, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    vsnprintf(sx->name, sizeof(sx->name), format, ap);

    va_end(ap);
}

void mpegts_sync_set_arg(mpegts_sync_t *sx, void *arg)
{
    sx->arg = arg;
}

void mpegts_sync_set_max_size(mpegts_sync_t *sx, size_t max_size)
{
    if (sx->size > max_size)
    {
        asc_log_error(MSG("current size is larger than new size limit"));
        return;
    }

    sx->max_size = max_size;
}

void mpegts_sync_set_on_read(mpegts_sync_t *sx, sync_callback_t on_read)
{
    sx->on_read = on_read;
}

void mpegts_sync_set_on_write(mpegts_sync_t *sx, ts_callback_t on_write)
{
    sx->on_write = on_write;
}

size_t mpegts_sync_get_max_size(const mpegts_sync_t *sx)
{
    return sx->max_size;
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
            if (++count >= ENOUGH_BUFFER_BLOCKS)
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

static __func_pure
size_t buffer_space(const mpegts_sync_t *sx)
{
    ssize_t space = sx->pos.send - sx->pos.rcv - 1;

    if (space < 0)
    {
        space += sx->size;
        if (space < 0)
            /* shouldn't happen */
            return 0;
    }

    return space;
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
    if (sx->on_read && sx->num_blocks < ENOUGH_BUFFER_BLOCKS)
        sx->on_read(sx->arg);

    /* initial buffering */
    if (!sx->buffered)
    {
        if (seek_pcr(sx))
        {
            sx->num_blocks += block_count(sx);

            if (sx->num_blocks >= ENOUGH_BUFFER_BLOCKS)
            {
                /* got enough data to start output */
                mpegts_sync_reset(sx, SYNC_RESET_PCR);
                sx->buffered = true;
            }

            if (!(sx->num_blocks % 5))
            {
                asc_log_debug(MSG("buffered blocks: %u (min %u)%s")
                              , sx->num_blocks, ENOUGH_BUFFER_BLOCKS
                              , (sx->buffered ? ", starting output" : ""));
            }
        }
        else if (!buffer_space(sx)
                 && !mpegts_sync_resize(sx, 0))
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
        const size_t filled = sx->size - buffer_space(sx);
        const size_t thresh = sx->size / 4;

        if (filled < thresh && sx->size > MIN_BUFFER_SIZE)
            mpegts_sync_resize(sx, sx->size / 2);

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
        sx->num_blocks = block_count(sx);
        downtime = time_now - sx->last_error;
    }

    if (sx->num_blocks < LOW_BUFFER_BLOCKS)
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
    while (buffer_space(sx) < count)
    {
        const bool expanded = mpegts_sync_resize(sx, 0);

        if (!expanded)
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

    size_t left = count;
    const ts_packet_t *ts = (const ts_packet_t *)buf;

    while (left > 0)
    {
        size_t chunk = sx->size - sx->pos.rcv;
        if (left < chunk)
            chunk = left; /* last piece */

        memcpy(&sx->buf[sx->pos.rcv], ts, sizeof(*ts) * chunk);

        sx->pos.rcv += chunk;
        if (sx->pos.rcv >= sx->size)
            /* buffer wrap around */
            sx->pos.rcv = 0;

        ts += chunk;
        left -= chunk;
    }

    return true;
}

bool mpegts_sync_resize(mpegts_sync_t *sx, size_t new_size)
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
    ts_packet_t *const buf = (ts_packet_t *)calloc(new_size, sizeof(*buf));
    asc_assert(buf != NULL, MSG("calloc() failed"));

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
