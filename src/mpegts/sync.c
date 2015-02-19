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

#define MSG(_msg) "[sync %s] " _msg, sync->name

/* buffer this many blocks before starting output */
#define BUFFER_PCR_COUNT 10

/* initial buffer size, TS packets */
#define BUFFER_MIN_SIZE ((1 * 1024 * 1024) / TS_PACKET_SIZE) /* 1 MiB */
#define BUFFER_MAX_SIZE (16 * BUFFER_MIN_SIZE)               /* 16 MiB */

/* FIXME */
#define PCR_JUMP_THRESH ((PCR_TIME_BASE * 150) / 1000) /* 150ms */

/* FIXME */
#define UNDERFLOW_THRESH (200 * 1000) /* 200ms */

typedef uint8_t ts_packet_t[TS_PACKET_SIZE];

struct mpegts_sync_t
{
    const char *name;
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
    uint16_t pcr_pid;
    unsigned int num_blocks;
    bool buffered;

    uint64_t pcr_last;
    uint64_t pcr_cur;
    size_t offset;

    double bitrate;
    double pending;

    void *arg;
    sync_callback_t on_read;
    ts_callback_t on_write;
};

/*
 * create and destroy
 */
__asc_inline
mpegts_sync_t *mpegts_sync_init(void)
{
    mpegts_sync_t *sync = calloc(1, sizeof(*sync));
    asc_assert(sync != NULL, "[sync] calloc() failed");

    sync->size = BUFFER_MIN_SIZE;
    sync->max_size = BUFFER_MAX_SIZE;

    sync->buf = calloc(sync->size, sizeof(*sync->buf));
    asc_assert(sync->buf != NULL, "[sync] calloc() failed");

    sync->pcr_last = sync->pcr_cur = XTS_NONE;

    return sync;
}

__asc_inline
void mpegts_sync_destroy(mpegts_sync_t *sync)
{
    free(sync->buf);
    free(sync);
}

/*
 * setters and getters
 */
void mpegts_sync_reset(mpegts_sync_t *sync, sync_reset_t type)
{
    switch (type) {
        case SYNC_RESET_ALL:
            /* restore buffer to its initial state */
            sync->pos.rcv = sync->pos.pcr = sync->pos.send = 0;
            sync->last_run = 0;

            mpegts_sync_resize(sync, BUFFER_MIN_SIZE);

        case SYNC_RESET_BLOCKS:
            /* reset output buffering */
            sync->last_error = sync->num_blocks = sync->buffered = 0;

        case SYNC_RESET_PCR:
            /* reset PCR lookahead routine */
            sync->pcr_pid = sync->offset = 0;
            sync->pcr_last = sync->pcr_cur = XTS_NONE;
            sync->bitrate = sync->pending = 0.0;

            /* start searching from first packet in queue */
            sync->pos.pcr = sync->pos.send;
    }
}

__asc_inline
void mpegts_sync_set_arg(mpegts_sync_t *sync, void *arg)
{
    sync->arg = arg;
}

__asc_inline
void mpegts_sync_set_max_size(mpegts_sync_t *sync, size_t max_size)
{
    sync->max_size = max_size;
}

__asc_inline
void mpegts_sync_set_on_read(mpegts_sync_t *sync, sync_callback_t on_read)
{
    sync->on_read = on_read;
}

__asc_inline
void mpegts_sync_set_on_write(mpegts_sync_t *sync, ts_callback_t on_write)
{
    sync->on_write = on_write;
}

__asc_inline
size_t mpegts_sync_space(mpegts_sync_t *sync)
{
    ssize_t space = sync->pos.send - sync->pos.rcv - 1;

    if (space < 0)
    {
        space += sync->size;
        if (space < 0)
            /* shouldn't happen */
            return 0;
    }

    return space;
}

/*
 * worker functions
 */
static inline
__func_pure size_t block_count(mpegts_sync_t *sync)
{
    size_t count = 0;

    /* count blocks after PCR lookahead */
    for (size_t i = sync->pos.pcr; i != sync->pos.rcv; i++)
    {
        if (i >= sync->size)
            /* buffer wrap around */
            i = 0;

        const uint8_t *ts = sync->buf[i];
        if (TS_IS_PCR(ts) && TS_GET_PID(ts) == sync->pcr_pid)
            count++;
    }

    return count;
}

static
bool seek_pcr(mpegts_sync_t *sync)
{
    while (sync->pos.pcr != sync->pos.rcv)
    {
        const size_t lookahead = sync->pos.pcr;
        const uint8_t *ts = sync->buf[lookahead];
        const bool is_pcr = TS_IS_PCR(ts);

        const size_t bytes = sync->offset;
        sync->offset += TS_PACKET_SIZE;

        if (++sync->pos.pcr >= sync->size)
            /* buffer wrap around */
            sync->pos.pcr = 0;

        if (!sync->pcr_pid && is_pcr)
        {
            /* latch onto first PCR pid we see */
            sync->pcr_pid = TS_GET_PID(ts);
            asc_log_debug(MSG("selected PCR pid %hu"), sync->pcr_pid);
        }

        if (is_pcr && TS_GET_PID(ts) == sync->pcr_pid)
        {
            /* check PCR validity */
            sync->pcr_last = sync->pcr_cur;
            sync->pcr_cur = TS_GET_PCR(ts);
            sync->offset = 0;

            if (sync->pcr_last == XTS_NONE)
            {
                /* beginning of the first block; start output from here */
                sync->pos.send = lookahead;
                if (bytes > 0)
                {
                    asc_log_debug(MSG("first PCR packet at %zu bytes; "
                                      "dropping everything before it"), bytes);
                }
                continue;
            }

            const int64_t delta = sync->pcr_cur - sync->pcr_last;
            if (delta <= 0)
            {
                /* clock reset or wrap around; wait for next PCR packet */
                asc_log_debug(MSG("PCR reset or wrap around"));
                continue;
            }
            else if (delta >= PCR_JUMP_THRESH)
            {
                const unsigned int ms = delta / (PCR_TIME_BASE / 1000);
                asc_log_error(MSG("PCR jumped forward by %ums"), ms);

                /* in case this happens during initial buffering */
                sync->pos.send = lookahead;
                sync->num_blocks = 0;
                // FIXME: test on !sync->buffered

                continue;
            }

            /* calculate momentary bitrate */
            const double inv_usecs = (double)PCR_TIME_BASE / delta;
            sync->bitrate = (bytes + TS_PACKET_SIZE) * inv_usecs;
            /* NOTE: inv_usecs = (1000 / PCR_INTERVAL) */

            if (sync->bitrate > 0)
                return true;
        }
    }

    return false;
}

static inline
unsigned int usecs_elapsed(mpegts_sync_t *sync, uint64_t time_now)
{
    unsigned int elapsed;

    if (sync->last_run)
    {
        elapsed = time_now - sync->last_run;

        if (time_now <= sync->last_run || elapsed > 1000000)
        {
            asc_log_error(MSG("time travel detected; resetting"));
            mpegts_sync_reset(sync, SYNC_RESET_ALL);

            elapsed = 0;
        }
    }
    else
    {
        /* no previous timestamp */
        elapsed = 0;
    }

    sync->last_run = time_now;
    return elapsed;
}

void mpegts_sync_loop(void *arg)
{
    mpegts_sync_t *const sync = arg;

    /* timekeeping */
    const uint64_t time_now = asc_utime();
    const unsigned int elapsed = usecs_elapsed(sync, time_now);

    if (!elapsed)
        /* let's not divide by zero */
        return;

    /* data request hook */
    if (sync->on_read && mpegts_sync_space(sync))
        sync->on_read(sync->arg);

    /* initial buffering */
    if (!sync->buffered)
    {
        if (seek_pcr(sync))
        {
            sync->num_blocks += block_count(sync) + 1;

            if (sync->num_blocks >= BUFFER_PCR_COUNT)
            {
                /* got enough data to start output */
                mpegts_sync_reset(sync, SYNC_RESET_PCR);
                sync->buffered = true;
            }

            asc_log_debug(MSG("buffered blocks: %u (min %u)%s")
                          , sync->num_blocks, BUFFER_PCR_COUNT
                          , (sync->buffered ? ", starting output" : ""));
        }
        else if (!mpegts_sync_space(sync)
                 && !mpegts_sync_resize(sync, 0))
        {
            asc_log_error(MSG("stream does not seem to contain PCR; resetting"));
            mpegts_sync_reset(sync, SYNC_RESET_ALL);
        }

        return;
    }

    /* next block lookup */
    if (sync->pos.send == sync->pos.pcr)
    {
        const bool found = seek_pcr(sync);

        if (!found)
        {
            asc_log_error(MSG("next PCR not found; buffering..."));
            mpegts_sync_reset(sync, SYNC_RESET_BLOCKS);

            return;
        }

        sync->num_blocks = block_count(sync) + 1;
#if 1
        const time_t t = time(NULL);
        printf("[%zu] br=%f, f=%zu/%zu, r=%zu p=%zu s=%zu b=%u\n"
               , t, sync->bitrate
               , sync->size - mpegts_sync_space(sync), sync->size
               , sync->pos.rcv, sync->pos.pcr, sync->pos.send
               , sync->num_blocks);
#endif
    }

    /* underflow correction */
    if (sync->num_blocks < BUFFER_PCR_COUNT)
    {
        sync->num_blocks = block_count(sync) + 1;

        if (!sync->last_error)
        {
            asc_log_debug(MSG("%u blocks short! output suspended")
                          , BUFFER_PCR_COUNT - sync->num_blocks);

            sync->last_error = time_now;
        }
        else
        {
            const unsigned int dt = time_now - sync->last_error;
            if (dt >= UNDERFLOW_THRESH)
            {
                asc_log_error(MSG("no input in %.2fms; resetting"), dt / 1000.0);
                mpegts_sync_reset(sync, SYNC_RESET_ALL);
            }
        }

        return;
    }
    else if (sync->last_error)
    {
        const unsigned int dt = time_now - sync->last_error;
        asc_log_debug(MSG("ok! downtime=%.2fms"), dt / 1000.0);
        sync->last_error = 0;
    }

    /* output */
    sync->pending += (sync->bitrate / (1000000.0 / elapsed));
    while (sync->pending > TS_PACKET_SIZE)
    {
        if (sync->pos.send >= sync->size)
            /* buffer wrap around */
            sync->pos.send = 0;

        if (sync->pos.send == sync->pos.pcr)
            /* block end */
            break;

        const uint8_t *ts = sync->buf[sync->pos.send++];
        if (sync->on_write)
            sync->on_write(sync->arg, ts);

        sync->pending -= TS_PACKET_SIZE;
    }
}

bool mpegts_sync_push(mpegts_sync_t *sync, const void *buf, size_t count)
{
    const size_t space = mpegts_sync_space(sync);
    if (count > space && !mpegts_sync_resize(sync, 0))
        return false;

    size_t left = count;
    const ts_packet_t *ts = (void *)((ptrdiff_t)buf);
    /* FIXME: compiler whining about constness */
    do
    {
        size_t chunk = sync->size - sync->pos.rcv;
        if (left < chunk)
            chunk = left; /* last piece */

        memcpy(&sync->buf[sync->pos.rcv], ts, sizeof(*ts) * chunk);
        if (left > chunk)
            /* wrap over */
            sync->pos.rcv = 0;
        else
            sync->pos.rcv += chunk;

        ts += chunk;
        left -= chunk;
    }
    while (left > 0);

    return true;
}

bool mpegts_sync_resize(mpegts_sync_t *sync, size_t new_size)
{
    if (!new_size)
        new_size = sync->size + BUFFER_MIN_SIZE;

    /* don't let it grow bigger than max_size */
    if (new_size >= sync->max_size)
    {
        if (sync->size == sync->max_size)
        {
            asc_log_debug(MSG("buffer already at maximum size, cannot expand"));
            return false;
        }
        else
        {
            new_size = sync->max_size;
        }
    }
    else if (new_size == sync->size)
    {
        asc_log_debug(MSG("buffer size unchanged"));
        return true;
    }

    /* adjust pointers */
    ssize_t filled = sync->pos.rcv - sync->pos.send;
    if (filled < 0)
        filled += sync->size;

    if (filled > (ssize_t)new_size)
    {
        asc_log_error(MSG("new size (%zu) is too small for current fill level (%zu)")
                      , new_size, filled);

        return false;
    }

    ssize_t lookahead = sync->pos.pcr - sync->pos.send;
    if (lookahead < 0)
        lookahead += sync->size;

    sync->pos.rcv = filled;
    sync->pos.pcr = lookahead;

    /* move contents to new buffer */
    ts_packet_t *const buf = calloc(new_size, sizeof(*buf));
    asc_assert(buf != NULL, "[sync] calloc() failed");

    size_t pos = sync->pos.send;
    size_t left = filled;
    ts_packet_t *ts = buf;
    do
    {
        size_t chunk = sync->size - pos;
        if (left < chunk)
            chunk = left;

        memcpy(ts, &sync->buf[pos], sizeof(*ts) * chunk);
        if (left > chunk)
            /* wrap over */
            pos = 0;
        else
            pos += chunk;

        ts += chunk;
        left -= chunk;
    }
    while (left > 0);

    /* clean up */
    asc_log_debug(MSG("buffer %s to %zu slots (%zu bytes)")
                  , (new_size > sync->size ? "expanded" : "shrunk")
                  , new_size, new_size * TS_PACKET_SIZE);

    free(sync->buf);

    sync->pos.send = 0;
    sync->size = new_size;
    sync->buf = buf;

    return true;
}
