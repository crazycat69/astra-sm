#include <astra.h>

#include "sync.h"

#define MSG(_msg) "[sync %s] " _msg, sync->name

/* buffer this many blocks before starting output */
#define BUFFER_PCR_COUNT 10

/* initial buffer size, TS packets */
#define BUFFER_MIN_SIZE ((32 * 1024 * 1024) / TS_PACKET_SIZE)
#define BUFFER_MAX_SIZE (16 * BUFFER_MIN_SIZE)

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

    unsigned int bitrate;
    unsigned int pcr_pid;
    unsigned int num_blocks;
    uint64_t last_run;
    bool buffered;

    uint64_t pcr_last;
    uint64_t pcr_cur;
    size_t pending;
    size_t offset;

    void *arg;
    sync_callback_t on_read;
    ts_callback_t on_write;
};

static bool seek_pcr(mpegts_sync_t *sync);
static void reset_pcr(mpegts_sync_t *sync);

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
__asc_inline
void mpegts_sync_set_arg(mpegts_sync_t *sync, void *arg)
{
    sync->arg = arg;
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
void mpegts_sync_loop(void *arg)
{
    mpegts_sync_t *sync = arg;

    const uint64_t time_now = asc_utime();
    uint64_t elapsed = sync->last_run ? (time_now - sync->last_run) : 0;
    sync->last_run = time_now;

    /* data request hook */
    if (sync->on_read && mpegts_sync_space(sync))
        sync->on_read(sync->arg);

    if (!sync->buffered)
    {
        if (seek_pcr(sync))
        {
            if (++sync->num_blocks >= BUFFER_PCR_COUNT)
            {
                reset_pcr(sync);
                sync->buffered = true;
            }

            asc_log_debug(MSG("buffered blocks: %u/%u%s")
                          , sync->num_blocks, BUFFER_PCR_COUNT
                          , (sync->buffered ? ", starting output" : ""));
        }
        else if (!mpegts_sync_space(sync))
        {
/*
            if (!expand_buffer(sync))
            {
                asc_log_error();
                reset_buffer(sync);
            }
*/
        }

        return;
    }

    if (sync->pos.send == sync->pos.pcr && !seek_pcr(sync))
    {
        sync->num_blocks = 0;
        sync->buffered = false;
        reset_pcr(sync);

        asc_log_error(MSG("next PCR not found, resetting"));
        return;
    }

    sync->pending += (sync->bitrate / (1000000 / elapsed)) / 8;
    while (sync->pending > TS_PACKET_SIZE)
    {
        if (sync->pos.send >= sync->size)
        {
            /* buffer wrap around */
            sync->pos.send = 0;
        }
        else if (sync->pos.send == sync->pos.pcr)
        {
            /* block end */
            //pending=0 ?
            break;
        }

        const uint8_t *ts = sync->buf[sync->pos.send++];
        if (sync->on_write)
            sync->on_write(sync->arg, ts);

        sync->pending -= TS_PACKET_SIZE;
    }
}

bool mpegts_sync_push(mpegts_sync_t *sync, void *buf, size_t count)
{
    const size_t space = mpegts_sync_space(sync);
    if (count > space /*&& !expand_buffer(sync)*/)
        // TODO: expand buffer
        return false;

    size_t left = count;
    const ts_packet_t *ts = buf;
    do
    {
        size_t chunk = sync->size - sync->pos.rcv;
        if (left < chunk)
            chunk = left; /* last piece */

        memmove(&sync->buf[sync->pos.rcv], ts, sizeof(*ts) * chunk);
        if (left > chunk)
            /* wrap over */
            sync->pos.rcv = 0;
        else
            sync->pos.rcv += chunk;

        ts += chunk;
        left -= chunk;
    } while (left > 0);

    return true;
}

static bool seek_pcr(mpegts_sync_t *sync)
{
    while (sync->pos.pcr != sync->pos.rcv)
    {
        if (sync->pos.pcr >= sync->size)
            /* buffer wrap around */
            sync->pos.pcr = 0;

        const uint64_t bytes = sync->offset;
        sync->offset += TS_PACKET_SIZE;

        const size_t lookahead = sync->pos.pcr++;
        const uint8_t *ts = sync->buf[lookahead];

        if (TS_IS_PCR(ts))
        {
            if (!sync->pcr_pid)
                sync->pcr_pid = TS_GET_PID(ts);

            if (TS_GET_PID(ts) == sync->pcr_pid)
            {
                sync->pcr_last = sync->pcr_cur;
                sync->pcr_cur = TS_GET_PCR(ts);
                sync->offset = 0;

                if (sync->pcr_last == XTS_NONE)
                {
                    /* beginning of the first block; start output from here */
                    sync->pos.send = lookahead;

                    if (bytes > 0)
                    {
                        asc_log_debug(MSG("first PCR at %zu bytes; "
                                          "dropping everything before it"), bytes);
                    }

                    continue;
                }
                else if (sync->pcr_cur <= sync->pcr_last)
                {
                    /* clock reset or wrap around; wait for next PCR packet */
                    continue;
                }

                const uint64_t delta = sync->pcr_cur - sync->pcr_last;
                const uint64_t usecs = delta / (PCR_TIME_BASE / 1000000);
                sync->bitrate = bytes * (1000000 / usecs) * 8;

                if (sync->bitrate)
                    return true;
            }
        }
    }

    return false;
}

static inline void reset_pcr(mpegts_sync_t *sync)
{
    sync->pos.pcr = sync->pos.send;
    sync->bitrate = sync->pending = sync->pcr_pid = 0;
    sync->pcr_last = sync->pcr_cur = XTS_NONE;
}
