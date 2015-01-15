/*
 * Astra Module: Remux (Output Buffer)
 *
 * Copyright (C) 2014, Artem Kharitonov <artem@sysert.ru>
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
#include "remux.h"

#define MSG(_msg) "[buffer %s] " _msg, buf->name

typedef enum
{
    STATE_LOW    = 0,
    STATE_NORMAL = 1,
    STATE_HIGH   = 2,
} buffer_state_t;

static const struct timespec buffer_sleep = {
    .tv_sec = 0,
    .tv_nsec = (BUFFER_USLEEP * 1000000),
};

static void thread_loop(void *arg)
{
    remux_buffer_t *const buf = arg;
#ifdef BUFFER_DEBUG
    uint64_t show_fill = 0;
#endif /* BUFFER_DEBUG */

    uint8_t buf_ts[TS_PACKET_SIZE];

    size_t pending = 0;
    buffer_state_t state = STATE_LOW;

    const uint64_t thresh_norm = (buf->size * BUFFER_NORM) / 100;
    const uint64_t thresh_high = (buf->size * BUFFER_HIGH) / 100;

    buf->is_thread_started = true;
    asc_log_debug(MSG("thread started: output rate %llu bytes")
                  , buf->rate);

    /* main loop */
    uint64_t last = asc_utime();

    while(buf->is_thread_started)
    {
        nanosleep(&buffer_sleep, NULL);

        /* check fill level */
        const size_t fill = asc_thread_buffer_count(buf->output);

        if(state == STATE_LOW && fill >= thresh_norm)
        {
            /* low => normal */
            asc_log_debug(MSG("buffering complete"));
            state = STATE_NORMAL;
        }
        else if(state == STATE_NORMAL && fill >= thresh_high)
        {
            /* normal => high */
            asc_log_debug(MSG("fill level too high, increase mux rate"));
            state = STATE_HIGH;
#ifdef BUFFER_DEBUG
            show_fill = 0;
#endif /* BUFFER_DEBUG */
        }
        else if(state == STATE_HIGH && fill <= thresh_high)
        {
            /* high => normal */
            asc_log_debug(MSG("resuming normal operation"));
            state = STATE_NORMAL;
        }

        /* measure elapsed time */
        const uint64_t now = asc_utime();
        uint64_t elapsed = (now - last);
        last = now;

#ifdef BUFFER_DEBUG
        if((now - show_fill) > 1000000) /* 1 sec */
        {
            const char *str = NULL;
            show_fill = now;

            if(state == STATE_LOW)
                str = "low";
            else if(state == STATE_NORMAL)
                str = "normal";
            else if(state == STATE_HIGH)
                str = "high";

            asc_log_debug(MSG("buffer fill: %llu/%llu (%u%%, %s)")
                          , fill, buf->size, (fill * 100) / buf->size, str);
        }
#endif /* BUFFER_DEBUG */

        /* dequeue packets */
        switch(state)
        {
            case STATE_LOW:
                break;

            case STATE_NORMAL:
                pending += (elapsed * buf->rate) / 1000000;
                break;

            case STATE_HIGH:
                pending = (fill - thresh_norm);
                break;
        }

        while(pending >= TS_PACKET_SIZE)
        {
            const ssize_t bytes
                = asc_thread_buffer_read(buf->output, buf_ts, TS_PACKET_SIZE);

            if(bytes != TS_PACKET_SIZE)
            {
                pending = 0;
                state = STATE_LOW;

                asc_log_error(MSG("buffer empty, output suspended"));
                break;
            }

            buf->callback(buf->cb_arg, buf_ts);
            pending -= TS_PACKET_SIZE;
        }
    }
}

static void on_thread_close(void *arg)
{
    remux_buffer_t *const buf = arg;

    buf->is_thread_started = false;

    if(buf->thread)
    {
        asc_thread_destroy(buf->thread);
        buf->thread = NULL;
    }

    if(buf->output)
    {
        asc_thread_buffer_destroy(buf->output);
        buf->output = NULL;
    }
}

remux_buffer_t *remux_buffer_init(const char *name, uint64_t rate)
{
    /* create buffer context */
    remux_buffer_t *buf = calloc(1, sizeof(*buf));
    asc_assert(buf != NULL, "[remux] calloc() failed");

    buf->name = name;
    buf->rate = (rate / 8);
    buf->size = (buf->rate * BUFFER_SECS);

    /* create thread */
    buf->thread = asc_thread_init(buf);
    buf->output = asc_thread_buffer_init(buf->size);

    asc_thread_start(buf->thread, thread_loop, NULL
                     , buf->output, on_thread_close);

    return buf;
}

void remux_buffer_push(remux_buffer_t *buf, const uint8_t *ts)
{
    const ssize_t bytes
        = asc_thread_buffer_write(buf->output, ts, TS_PACKET_SIZE);

    if(bytes != TS_PACKET_SIZE)
    {
        asc_log_error(MSG("buffer full, resetting"));
        asc_thread_buffer_flush(buf->output);
    }
}

void remux_buffer_destroy(remux_buffer_t *buf)
{
    if(buf)
    {
        on_thread_close(buf);
        free(buf);
    }
}
