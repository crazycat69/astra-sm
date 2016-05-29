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
#include <mpegts/pes.h>
#include <mpegts/pcr.h>

#define MSG(_msg) "[pes] %s(): " _msg, __func__

mpegts_pes_t *mpegts_pes_init(uint16_t pid)
{
    mpegts_pes_t *const pes = ASC_ALLOC(1, mpegts_pes_t);

    pes->pid = pid;
    pes->o_cc = 15; /* wraps over to zero */

    return pes;
}

void mpegts_pes_destroy(mpegts_pes_t *pes)
{
    free(pes);
}

static void pes_demux(mpegts_pes_t *pes, bool fast);

bool mpegts_pes_mux(mpegts_pes_t *pes, const uint8_t *ts)
{
    /* locate payload */
    const uint8_t *payload = TS_GET_PAYLOAD(ts);
    size_t paylen = TS_PACKET_SIZE - (payload - ts);

    if (!payload || paylen > TS_BODY_SIZE)
        /* no payload/invalid size */
        return false;

    /* check continuity, propagate CC errors */
    const uint8_t cc = TS_GET_CC(ts);

    if (pes->expect_size > 0 && cc != ((pes->i_cc + 1) & 0xf))
        pes->o_cc++;

    pes->i_cc = cc;

    /* flush buffer on packet start */
    if (TS_IS_PAYLOAD_START(ts))
    {
        if (pes->expect_size > 0 && pes->buf_read < pes->buf_write)
        {
            /* push-out TS path; used by both modes */
            pes_demux(pes, false);

            if (pes->buf_write != pes->buf_read)
                asc_log_error(MSG("BUG: didn't send whole buffer"));
        }

        pes->expect_size = pes->buf_write = pes->buf_read = 0;
        pes->pcr = pes->pts = pes->dts = XTS_NONE;

        /* check payload length and start code */
        if (paylen < PES_HEADER_SIZE
            || PES_BUFFER_GET_HEADER(payload) != 0x000001)
        {
            return false;
        }

        memcpy(&pes->ext, &payload[PES_HDR_BASIC], PES_HDR_EXT);

        const size_t hdrlen = PES_HEADER_SIZE + pes->ext.hdrlen;
        if (paylen < hdrlen)
            return false;

        /* parse headers */
        pes->key = TS_IS_RAI(ts); /* random access indicator */
        pes->expect_size = PES_BUFFER_GET_SIZE(payload);
        pes->stream_id = PES_BUFFER_GET_SID(payload);

        if (pes->ext.pts)
        {
            pes->pts = PES_GET_PTS(payload);
            if (pes->ext.dts)
                pes->dts = PES_GET_DTS(payload);
        }

        if (TS_IS_PCR(ts))
            pes->pcr = TS_GET_PCR(ts);

        /* adjust expected packet size */
        if (pes->expect_size <= hdrlen)
            /* variable length (e.g. video) */
            pes->expect_size = PES_MAX_BUFFER;
        else
            /* fixed length */
            pes->expect_size -= hdrlen;

        /* cut off PES header before buffering */
        payload += hdrlen;
        paylen -= hdrlen;
    }

    if (pes->expect_size > 0)
    {
        memcpy(&pes->buffer[pes->buf_write], payload, paylen);
        pes->buf_write += paylen;

        if (pes->buf_write > PES_MAX_BUFFER - TS_BODY_SIZE)
        {
            /* avoid buffer overflow on next write */
            asc_log_error(MSG("packet is over %u bytes, truncating. pid: %hu")
                          , PES_MAX_BUFFER, pes->pid);

            pes->expect_size = pes->buf_write;
            pes->truncated++;

            pes_demux(pes, false);

            pes->o_cc++;
        }
        else if (pes->expect_size == pes->buf_write)
        {
            /* fixed-length TS path; used by both modes */
            pes_demux(pes, false);
        }
        else if (pes->mode == PES_MODE_FAST)
        {
            /* fast TS path; send output as soon as possible */
            pes_demux(pes, true);
        }
    }
    else
    {
        /* got nowhere to send this */
        pes->dropped++;
    }

    return true;
}

static
void pes_demux(mpegts_pes_t *pes, bool fast)
{
    uint8_t *const ts = pes->ts;

    while (pes->buf_read < pes->buf_write)
    {
        const bool is_start = (pes->buf_read == 0);
        const size_t remain = (pes->buf_write - pes->buf_read);

        if (fast && remain < TS_BODY_SIZE)
            /* wait for more data */
            break;

        uint8_t *pay = &ts[TS_HEADER_SIZE];
        uint8_t *pes_h = NULL;
        size_t space = TS_BODY_SIZE;

        /* write TS header */
        ts[0] = 0x47;
        ts[1] = pes->pid >> 8;
        ts[2] = pes->pid;
        if (is_start)
            /* set PUSI */
            ts[1] |= 0x40;

        /* CC counter and payload flag */
        pes->o_cc = (pes->o_cc + 1) & 0xf;
        ts[3] = (0x10 | pes->o_cc);

        /* AF and PES header, first packet only */
        size_t af_size = 0;
        size_t pes_hlen = 0;

        if (is_start)
        {
            /* callback might change header, so call it first */
            if (pes->on_pes)
                pes->on_pes(pes->cb_arg, pes);

            /* set random access on key frames */
            if (pes->key)
            {
                ts[5] |= 0x40;
                af_size = 2;
            }

            /* add PCR if requested */
            if (pes->pcr != XTS_NONE)
            {
                ts[5] |= 0x10;
                TS_SET_PCR(ts, pes->pcr);
                af_size = 8;
            }

            /* drop extra fields */
            memset((uint8_t *)(&pes->ext) + 1, 0, sizeof(pes->ext) - 1);

            /* calculate PES header size */
            if (pes->pts != XTS_NONE)
            {
                pes->ext.pts = 1;
                pes_hlen += 5;
                if (pes->dts != XTS_NONE)
                {
                    pes->ext.dts = 1;
                    pes_hlen += 5;
                }
            }
            pes->ext.marker = 2;
            pes->ext.hdrlen = pes_hlen;
            pes_hlen += PES_HEADER_SIZE;

            /* alloc */
            pes_h = ASC_ALLOC(pes_hlen, uint8_t);

            /* basic part */
            pes_h[2] = 0x01; /* start code */
            pes_h[3] = pes->stream_id;

            /* recalculate packet size */
            if (!fast)
                pes->expect_size = pes->buf_write;

            if (pes->expect_size != PES_MAX_BUFFER)
            {
                /* NOTE: packet length doesn't include basic 6-byte header */
                const size_t pktlen =
                    pes->expect_size + pes_hlen - PES_HDR_BASIC;

                if (pktlen <= 0xFFFF)
                {
                    /* can't define length for larger packets */
                    pes_h[4] = pktlen >> 8;
                    pes_h[5] = pktlen;
                }
            }

            /* extension */
            memcpy(&pes_h[PES_HDR_BASIC], &pes->ext, sizeof(pes->ext));
            if (pes->ext.pts)
            {
                PES_SET_PTS(pes_h, pes->pts);
                if (pes->ext.dts)
                {
                    PES_SET_DTS(pes_h, pes->dts);
                    pes_h[9] |= 0x10;
                }
            }

            space -= (af_size + pes_hlen);
        }

        /* pad last TS packet via AF */
        if (remain < space)
        {
            const size_t stuffing = (space - remain);
            memset(&pay[af_size], 0xff, stuffing);
            if (!af_size)
            {
                /* dummy AF; clear all flags */
                ts[5] = 0;
            }
            af_size += stuffing;
            space = remain;
        }

        /* finalize AF */
        if (af_size)
        {
            ts[3] |= 0x20;       /* AF flag */
            ts[4] = af_size - 1; /* AF length */
            pay += af_size;
        }

        /* write PES header */
        if (pes_hlen)
        {
            memcpy(pay, pes_h, pes_hlen);
            free(pes_h);
            pay += pes_hlen;
        }

        memcpy(pay, &pes->buffer[pes->buf_read], space);
        if (pes->on_ts)
            pes->on_ts(pes->cb_arg, ts);

        pes->sent++;
        pes->buf_read += space;
    }

    if (!fast)
    {
        if (pes->expect_size != PES_MAX_BUFFER
            && pes->buf_write != pes->expect_size)
        {
            /* happens with crappy streams */
            asc_log_error(MSG("wrong size: expected %zu, got %zu, pid: %hu")
                          , pes->expect_size, pes->buf_write, pes->pid);
        }

        pes->expect_size = 0;
    }
}
