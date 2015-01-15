/*
 * Astra Module: MPEG-TS (PES processing)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *               2014-2015, Artem Kharitonov <artem@sysert.ru>
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

#define MSG(_msg) "[pes] %s(): " _msg, __func__

mpegts_pes_t *mpegts_pes_init(uint16_t pid)
{
    mpegts_pes_t *pes = calloc(1, sizeof(*pes));
    asc_assert(pes != NULL, MSG("calloc() failed"));

    pes->pid = pid;
    pes->o_cc = 15; /* wraps over to zero */

    return pes;
}

void mpegts_pes_destroy(mpegts_pes_t *pes)
{
    free(pes);
}

bool mpegts_pes_mux(mpegts_pes_t *pes, const uint8_t *ts
                    , pes_callback_t callback, void *arg)
{
    bool result = false;

    /* locate payload */
    const uint8_t *payload = TS_GET_PAYLOAD(ts);
    const size_t paylen = (size_t)((ts + TS_PACKET_SIZE) - payload);

    if(!payload || paylen > TS_BODY_SIZE)
        /* no payload/invalid size */
        return false;

    /* check continuity */
    const uint8_t cc = TS_GET_CC(ts);
    if(pes->buffer_skip && cc != ((pes->i_cc + 1) & 0xf))
    {
        pes->buffer_size = pes->buffer_skip;
        pes->truncated++;
    }
    pes->i_cc = cc;

    /* packet start flag */
    const bool is_start = (TS_IS_PAYLOAD_START(ts)
        && (PES_BUFFER_GET_HEADER(payload) == 0x1)
        && (paylen >= PES_HEADER_SIZE));

    if(pes->buffer_skip)
    {
        uint8_t *dst = &pes->buffer[pes->buffer_skip];
        size_t remain = (pes->buffer_size - pes->buffer_skip);

        if(is_start)
        {
            if(pes->buffer_size != PES_MAX_BUFFER
               && pes->buffer_size != pes->buffer_skip)
                /* broken header */
                asc_log_error(MSG("size mismatch: %zu != %zu, pid: %hu")
                              , pes->buffer_size, pes->buffer_skip, pes->pid);

            /* got startcode for next packet */
            pes->buffer_size = pes->buffer_skip;
            remain = 0;
        }

        if(remain > paylen)
            remain = paylen;
        else
            /* no more data */
            pes->buffer_skip = 0;

        memcpy(dst, payload, remain);

        if(!pes->buffer_skip)
        {
            /* assembled full PES packet */
            callback(arg, pes);
            pes->sent++;
            result = true;
        }
        else
            pes->buffer_skip += remain;
    }

    if(is_start)
    {
        /* new packet; reset buffer */
        pes->buffer_size = pes->buffer_skip = 0;
        pes->pcr = pes->pts = pes->dts = XTS_NONE;
        pes->received++;

        /* determine buffer size */
        size_t bufsize = PES_BUFFER_GET_SIZE(payload);
        if(bufsize <= PES_HEADER_SIZE) /* bufsize is at most 0xFFFF+6 */
            /* unknown packet length */
            bufsize = PES_MAX_BUFFER;

        /* parse headers */
        pes->key = TS_IS_RAI(ts); /* random access indicator */
        pes->stream_id = PES_BUFFER_GET_SID(payload);
        memcpy(&pes->ext, &payload[PES_HDR_BASIC], PES_HDR_EXT);

        if(pes->ext.pts)
        {
            pes->pts = PES_GET_PTS(payload);
            if(pes->ext.dts)
                pes->dts = PES_GET_DTS(payload);
        }

        if(TS_IS_PCR(ts))
            pes->pcr = TS_GET_PCR(ts);

        /* copy first data portion */
        pes->buffer_size = bufsize;
        if(bufsize > paylen)
            /* PES split over several TS packets */
            pes->buffer_skip = bufsize = paylen;

        memcpy(pes->buffer, payload, bufsize);

        if(!pes->buffer_skip)
        {
            /* payload smaller than one TS packet */
            callback(arg, pes);
            pes->sent++;
            result = true;
        }
    }

    if(!result && !pes->buffer_skip)
        /* TS packet out of sequence */
        pes->dropped++;

    return result;
}

void mpegts_pes_demux(mpegts_pes_t *pes, ts_callback_t callback, void *arg)
{
    uint8_t *ts = pes->ts;

    bool is_start = 1;
    size_t skip = PES_HEADER_SIZE + pes->ext.hdrlen;

    if(skip > pes->buffer_size)
    {
        /* header longer than packet itself */
        asc_log_error(MSG("oversized PES header: %zu > %zu, pid: %hu")
                      , skip, pes->buffer_size, pes->pid);

        skip = pes->buffer_size;
    }

    while(skip < pes->buffer_size)
    {
        uint8_t *pay = &pes->ts[TS_HEADER_SIZE];
        uint8_t *pes_h = NULL;
        size_t space = TS_BODY_SIZE;

        /* write TS header */
        ts[0] = 0x47;
        ts[1] = pes->pid >> 8;
        ts[2] = pes->pid;
        if(is_start)
            /* set PUSI */
            ts[1] |= 0x40;

        /* CC counter and payload flag */
        pes->o_cc = (pes->o_cc + 1) & 0xf;
        ts[3] = (0x10 | pes->o_cc);

        /* AF and PES header, first packet only */
        size_t af_size = 0;
        size_t pes_hlen = 0;
        if(is_start)
        {
            is_start = 0;

            /* set random access on key frames */
            if(pes->key)
            {
                ts[5] |= 0x40;
                af_size = 2;
            }

            /* add PCR if requested */
            if(pes->pcr != XTS_NONE)
            {
                ts[5] |= 0x10;
                TS_SET_PCR(ts, pes->pcr);
                af_size = 8;
            }

            /* drop extra fields */
            memset((uint8_t *)(&pes->ext) + 1, 0, sizeof(pes->ext) - 1);

            /* calculate PES header size */
            if(pes->pts != XTS_NONE)
            {
                pes->ext.pts = 1;
                pes_hlen += 5;
                if(pes->dts != XTS_NONE)
                {
                    pes->ext.dts = 1;
                    pes_hlen += 5;
                }
            }
            pes->ext.marker = 2;
            pes->ext.hdrlen = pes_hlen;
            pes_hlen += PES_HEADER_SIZE;

            /* alloc */
            pes_h = calloc(1, pes_hlen);
            asc_assert(pes_h != NULL, MSG("calloc() failed"));

            /* basic part */
            pes_h[2] = 0x01; /* start code */
            pes_h[3] = pes->stream_id;

            /* recalculate packet size */
            const size_t pktlen = (pes->buffer_size - skip
                + pes_hlen - PES_HDR_BASIC);

            if(pktlen <= 0xFFFF)
            {
                /* can't define length for larger packets */
                pes_h[4] = pktlen >> 8;
                pes_h[5] = pktlen;
            }

            /* extension */
            memcpy(&pes_h[PES_HDR_BASIC], &pes->ext, sizeof(pes->ext));
            if(pes->ext.pts)
            {
                PES_SET_PTS(pes_h, pes->pts);
                if(pes->ext.dts)
                {
                    PES_SET_DTS(pes_h, pes->dts);
                    pes_h[9] |= 0x10;
                }
            }

            space = (space - af_size - pes_hlen);
        }

        /* pad last TS packet via AF */
        const size_t remain = (pes->buffer_size - skip);
        if(remain < space)
        {
            const size_t stuffing = (space - remain);
            memset(&pay[af_size], 0xff, stuffing);
            if(!af_size)
            {
                /* dummy AF; clear all flags */
                ts[5] = 0;
            }
            af_size += stuffing;
            space = remain;
        }

        /* finalize AF */
        if(af_size)
        {
            ts[3] |= 0x20;       /* AF flag */
            ts[4] = af_size - 1; /* AF length */
            pay += af_size;
        }

        /* write PES header */
        if(pes_hlen)
        {
            memcpy(pay, pes_h, pes_hlen);
            free(pes_h);
            pay += pes_hlen;
        }

        memcpy(pay, &pes->buffer[skip], space);
        callback(arg, ts);

        skip += space;
    }

    if(skip != pes->buffer_size)
    {
        /* shouldn't happen */
        asc_log_error(MSG("size mismatch: %zu != %zu, pid: %hu")
                      , skip, pes->buffer_size, pes->pid);
    }
}
