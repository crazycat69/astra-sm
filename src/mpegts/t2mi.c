/*
 * Astra Module: MPEG-TS (T2-MI de-encapsulator)
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

#define MSG(_msg) "[t2mi/%s] " _msg, mi->name

/* sizes and lengths */
#define T2MI_BUFFER_SIZE 0x3000

#define T2MI_HEADER_SIZE 6
#define BBFRAME_T2MI_HEADER_SIZE 3
#define BBFRAME_DVB_HEADER_SIZE 10

/* read 32-bit integer from offset, MSB first */
#define T2MI_GET_CRC32(x) \
    (uint32_t)(((x)[0] << 24) | ((x)[1] << 16) | ((x)[2] << 8) | ((x)[3]))

#define asc_log_error_once(...) \
    { \
        if (!mi->warned) \
        { \
            asc_log_error(__VA_ARGS__); \
            mi->warned = false; \
        } \
    }

/* T2-MI packet types */
enum
{
    T2MI_TYPE_BASEBAND_FRAME   = 0x00, /* Baseband Frame */
    T2MI_TYPE_AUX_IQ_DATA      = 0x01, /* Auxiliary stream I/Q data */
    T2MI_TYPE_CELL_INSERT      = 0x02, /* Arbitrary cell insertion */
    T2MI_TYPE_L1_CURRENT       = 0x10, /* L1-current */
    T2MI_TYPE_L1_FUTURE        = 0x11, /* L1-future */
    T2MI_TYPE_P2_BIAS          = 0x12, /* P2 bias balancing cells */
    T2MI_TYPE_DVB_T2_TIMESTAMP = 0x20, /* DVB-T2 timestamp */
    T2MI_TYPE_INDIVIDUAL       = 0x21, /* Individual addressing */
    T2MI_TYPE_FEF_NULL         = 0x30, /* FEF part: Null */
    T2MI_TYPE_FEF_IQ_DATA      = 0x31, /* FEF part: I/Q data */
    T2MI_TYPE_FEF_COMPOSITE    = 0x32, /* FEF part: composite */
    T2MI_TYPE_FEF_SUB_PART     = 0x33, /* FEF sub-part */
};

/* BBframe stream formats */
enum
{
    BBFRAME_FORMAT_GFPS = 0x00, /* Generic Packetized Stream */
    BBFRAME_FORMAT_GCS  = 0x40, /* Generic Continuous Stream */
    BBFRAME_FORMAT_GSE  = 0x80, /* Generic Encapsulated Stream */
    BBFRAME_FORMAT_TS   = 0xC0, /* MPEG Transport Stream */
};

/* BBframe modes */
enum
{
    BBFRAME_MODE_NORMAL = 0x00, /* Normal Mode */
    BBFRAME_MODE_HEM    = 0x01, /* High Effeciency Mode */
};

/* structs for storing parsed packet headers */
typedef struct
{
    uint8_t frame_idx;
    uint8_t plp_id;
    bool intl_frame_start;

    uint8_t format;
    uint8_t input_stream_id;
    bool single_input;
    bool constant_coding;
    bool issy;
    bool npd;

    uint16_t upl;
    uint16_t dfl;
    uint16_t syncd;
    uint8_t sync;

    size_t up_offset;
    size_t up_size;
    size_t df_size;

    /* mode is XOR'ed with CRC-8 */
    uint8_t crc8;
    uint8_t mode;

    /* points to reassembly buffer */
    uint8_t *header;
    uint8_t *data;
    const uint8_t *end;
} bb_frame_t;

typedef struct
{
    uint8_t packet_type;
    uint8_t packet_count;
    uint8_t superframe_idx;
    uint8_t stream_id;
    uint8_t pad_bits;
    uint16_t payload_bits;
    uint32_t crc32;

    size_t payload_size;
    size_t total_size;

    /* points to reassembly buffer */
    uint8_t *data;
    const uint8_t *end;

    bool continuous;
    bb_frame_t bb;
} t2mi_packet_t;

/* decapsulator context */
struct mpegts_t2mi_t
{
    char name[128];
    uint8_t plp_id;

    mpegts_psi_t *pat;
    mpegts_psi_t *pmt;

    mpegts_packet_type_t streams[MAX_PID];
    uint16_t prefer_pnr;
    uint16_t pmt_pid;
    uint16_t payload_pid;
    uint8_t last_cc;

    uint8_t buffer[T2MI_BUFFER_SIZE];
    size_t skip;
    uint8_t last_pkt_count;
    t2mi_packet_t packet;

    size_t frag_skip;
    uint8_t frag[T2MI_BUFFER_SIZE];

    ts_callback_t on_ts;
    void *arg;

    bool warned;
    bool seen_pkts;
    bool error;
};

/*
 * init/destroy
 */
__asc_inline
mpegts_t2mi_t *mpegts_t2mi_init(void)
{
    mpegts_t2mi_t *mi = (mpegts_t2mi_t *)calloc(1, sizeof(*mi));
    asc_assert(mi != NULL, "[t2mi] calloc() failed");

    strcpy(mi->name, "t2mi");
    mi->pat = mpegts_psi_init(MPEGTS_PACKET_PAT, 0);
    mi->pmt = mpegts_psi_init(MPEGTS_PACKET_PMT, 0);

    mpegts_t2mi_set_payload(mi, 0, 0);

    return mi;
}

__asc_inline
void mpegts_t2mi_destroy(mpegts_t2mi_t *mi)
{
    mpegts_psi_destroy(mi->pat);
    mpegts_psi_destroy(mi->pmt);

    free(mi);
}

/*
 * setters
 */
__asc_inline
void mpegts_t2mi_set_name(mpegts_t2mi_t *mi, const char *name)
{
    snprintf(mi->name, sizeof(mi->name), "%s", name);
}

__asc_inline
void mpegts_t2mi_set_callback(mpegts_t2mi_t *mi, ts_callback_t cb, void *arg)
{
    mi->on_ts = cb;
    mi->arg = arg;
}

__asc_inline
void mpegts_t2mi_set_plp(mpegts_t2mi_t *mi, uint8_t plp_id)
{
    mi->plp_id = plp_id;
}

/*
 * TODO
 *
 * __asc_inline
 * void mpegts_t2mi_set_demux(mpegts_t2mi_t *mi, join_pid, leave_pid);
 * {
 * }
 */

void mpegts_t2mi_set_payload(mpegts_t2mi_t *mi, uint16_t pnr, uint16_t pid)
{
    if (pnr)
    {
        mi->prefer_pnr = pnr;

        /* we'll need to scan PAT/PMT */
        pid = 0;
    }
    else
        mi->prefer_pnr = 0;

    /* reset pid map */
    memset(mi->streams, 0, sizeof(mi->streams));
    // XXX: call `leave_pid' on joined pids?
    mi->streams[pid] = (pid != 0) ? MPEGTS_PACKET_DATA : MPEGTS_PACKET_PAT;
    mi->streams[1] = MPEGTS_PACKET_CAT; // XXX: do we need this?
}

/*
 * original TS extraction
 *
 * ===> MPEG TS             ^
 *      ...                 |
 */
static
bool bb_reassemble_up(mpegts_t2mi_t *mi, t2mi_packet_t *pkt)
{
    if (!mi->frag_skip)
        return false;

    const size_t frag_skip = mi->frag_skip;
    mi->frag_skip = 0;

    if (!pkt->continuous)
    {
        /* packet loss; frag buffer contents are unusable */
        asc_log_debug(MSG("dropping UP fragment due to discontinuity (%zu bytes)")
                      , frag_skip);

        return false;
    }

    /* check UP size */
    const bb_frame_t *const bb = &pkt->bb;
    const size_t len = frag_skip + bb->up_offset - 1;

    if (len != bb->up_size)
    {
        if (bb->syncd == 0xFFFF)
        {
            /* UP is larger than data field */
            memcpy(&mi->frag[frag_skip], bb->data, bb->up_offset);
            mi->frag_skip = frag_skip + bb->up_offset;
            // XXX: needs testing
        }
        else
        {
            asc_log_debug(MSG("reassembled UP has wrong size (expected %zu, got %zu)")
                          , bb->up_size, len);
        }

        return false;
    }

    memcpy(&mi->frag[frag_skip], bb->data, bb->up_offset);
    return true;
}

static inline
void bb_reinsert_null(mpegts_t2mi_t *mi, size_t dnp)
{
    for (size_t i = 0; i < dnp; i++)
        mi->on_ts(mi->arg, null_ts);
}

static
bool on_bbframe_ts(mpegts_t2mi_t *mi, t2mi_packet_t *pkt)
{
    bb_frame_t *const bb = &pkt->bb;

    /*
     * TODO: support normal mode
     */
    if (bb->mode == BBFRAME_MODE_NORMAL)
    {
        asc_log_error_once(MSG("BBframe normal mode is not supported"));
        return false;
    }

    /* HEM uses sync/upl fields for ISSY */
    bb->sync = 0x47;
    bb->up_size = TS_PACKET_SIZE - 1;

    if (bb->npd)
        /* additional byte for null packet counter */
        bb->up_size++;

    /* fragmented TS reassembly */
    if (bb_reassemble_up(mi, pkt) && mi->on_ts)
    {
        mi->on_ts(mi->arg, mi->frag);
        if (bb->npd)
            bb_reinsert_null(mi, mi->frag[TS_PACKET_SIZE]);
    }

    /* zero-copy path */
    uint8_t *ptr = bb->data + bb->up_offset;
    while (ptr + bb->up_size <= bb->end)
    {
        uint8_t *const ts = ptr - 1;
        ts[0] = 0x47;

        if (mi->on_ts)
        {
            mi->on_ts(mi->arg, ts);
            if (bb->npd)
                bb_reinsert_null(mi, ptr[bb->up_size - 1]);
        }

        ptr += bb->up_size;
    }

    /* store TS fragments until next time */
    const size_t left = bb->end - ptr;
    if (left > 0)
    {
        mi->frag_skip = 1 + left;
        mi->frag[0] = 0x47;
        memcpy(&mi->frag[1], ptr, left);
    }

    return true;
}

/*
 * BBframe header inspection
 *
 * ===> Baseband Frames     ^
 *      ...                 |
 */
static inline __func_const
const char *bb_format_name(uint8_t fmt)
{
    switch (fmt)
    {
        case BBFRAME_FORMAT_GFPS: return "Generic Packetized Stream";
        case BBFRAME_FORMAT_GCS:  return "Generic Continuous Stream";
        case BBFRAME_FORMAT_GSE:  return "Generic Encapsulated Stream";
        case BBFRAME_FORMAT_TS:   return "MPEG Transport Stream";
    }

    return "Unknown";
}

static
bool on_bbframe(mpegts_t2mi_t *mi, t2mi_packet_t *pkt)
{
    uint8_t *const ptr = pkt->bb.header;
    bb_frame_t *const bb = &pkt->bb;

    /* BB header, DVB-T2 part */
    bb->format = ptr[0] & 0xC0;
    bb->single_input = ptr[0] & 0x20;
    bb->constant_coding = ptr[0] & 0x10;
    bb->issy = ptr[0] & 0x08;
    bb->npd = ptr[0] & 0x04;

    bb->input_stream_id = ptr[1];

    bb->upl = (ptr[2] << 8) | ptr[3];
    bb->dfl = (ptr[4] << 8) | ptr[5];
    bb->syncd = (ptr[7] << 8) | ptr[8];
    bb->sync = ptr[6];

    bb->data = &ptr[BBFRAME_DVB_HEADER_SIZE];

    /* verify CRC-8 */
    bb->crc8 = crc8(ptr, BBFRAME_DVB_HEADER_SIZE - 1);
    bb->mode = ptr[9] ^ bb->crc8;

    if (bb->mode & ~0x1)
    {
        /* unknown mode; assume corrupt frame */
        asc_log_debug(MSG("CRC-8 error, dropping BBframe"));
        return false;
    }

    /* check DF length */
    bb->df_size = bb->dfl / 8;
    bb->end = bb->data + bb->df_size;

    if (bb->end > pkt->end)
    {
        asc_log_error(MSG("BBframe data field length out of bounds"));
        return false;
    }

    /* check syncd (offset of first UP) */
    if (bb->syncd == 0xFFFF)
    {
        /* no UP starts in this packet */
        bb->up_offset = bb->df_size;
    }
    else
    {
        bb->up_offset = bb->syncd / 8;
        if (bb->up_offset > bb->df_size)
        {
            asc_log_error(MSG("BBframe syncd value out of bounds"));
            return false;
        }
    }

    /* pass it on */
    switch (bb->format)
    {
        case BBFRAME_FORMAT_TS:
            return on_bbframe_ts(mi, pkt);

        default:
            asc_log_error_once(MSG("unsupported format: %s"), bb_format_name(bb->format));
            return true;
    }
}

/*
 * T2-MI header inspection
 *
 * ===> T2-MI Packets       ^
 *      ...                 |
 */
static
bool on_t2mi(mpegts_t2mi_t *mi, t2mi_packet_t *pkt)
{
    uint8_t *const ptr = &pkt->data[T2MI_HEADER_SIZE];

    /* check for errors */
    const uint8_t expect = mi->last_pkt_count + 1;
    mi->last_pkt_count = pkt->packet_count;

    if (pkt->packet_count != expect)
    {
        /* packet loss; don't report this on first packet */
        if (mi->seen_pkts)
        {
            asc_log_debug(MSG("T2-MI packet_count discontinuity (expect %u, got %u)")
                          , expect, pkt->packet_count);

            mi->seen_pkts = false;
        }
    }
    else if (mi->error)
    {
        /* previous packet was dropped due to an error */
        mi->error = false;
    }
    else
    {
        /* turn on no-error indication */
        pkt->continuous = true;

        if (!mi->seen_pkts)
            mi->seen_pkts = true;
    }

    switch (pkt->packet_type)
    {
        case T2MI_TYPE_BASEBAND_FRAME:
            /* BB header, T2-MI part */
            pkt->bb.frame_idx = ptr[0];
            pkt->bb.plp_id = ptr[1];
            pkt->bb.intl_frame_start = ptr[2] & 0x80;

            /* TODO: common PLP */
            if (pkt->bb.plp_id != mi->plp_id)
                /* silently drop wrong PLP frames */
                return true;

            /* pass it on */
            pkt->bb.header = &ptr[BBFRAME_T2MI_HEADER_SIZE];
            return on_bbframe(mi, pkt);

        /* TODO: get PLP list */

        default:
            /* ignore */
            return true;
    }
}

/*
 * T2-MI packet reassembly
 *
 * ===> DVB Data Piping     ^
 *      ...                 |
 */
static
void on_outer_ts(mpegts_t2mi_t *mi, const uint8_t *ts)
{
    /* look for TS payload */
    const uint8_t *pay = TS_GET_PAYLOAD(ts);
    size_t paylen = TS_PACKET_SIZE - (pay - ts);

    if (!pay || paylen > TS_BODY_SIZE)
        return;

    /* check CC, discard current T2-MI packet on error */
    const uint8_t cc = TS_GET_CC(ts);
    const uint8_t expect = (mi->last_cc + 1) & 0xf;

    if (cc != expect && mi->skip != 0)
    {
        asc_log_debug(MSG("CC error (expect %u, got %u), discarding T2-MI packet"), expect, cc);
        mi->skip = 0;
    }

    mi->last_cc = cc;

    /* locate T2-MI header in PUSI packets */
    bool new_packet = false;
    if (TS_IS_PAYLOAD_START(ts))
    {
        /* eat byte indicating header offset */
        size_t offset = 1;

        if (mi->skip == 0)
            /* discard tail if this is the first packet we see */
            offset += pay[0];

        if (offset >= paylen)
        {
            asc_log_error(MSG("header offset out of bounds (%zu > %zu)"), offset, paylen);
            mi->skip = 0;

            return;
        }

        new_packet = true;
        pay += offset;
        paylen -= offset;
    }

    /* append payload to reassembly buffer */
    if (mi->skip + paylen > T2MI_BUFFER_SIZE)
    {
        asc_log_error(MSG("packet too large, flushing buffer"));
        mi->skip = 0;
    }

    if (!new_packet && mi->skip == 0)
        return;

    memcpy(&mi->buffer[mi->skip], pay, paylen);
    mi->skip += paylen;

    /* look for completed packets */
    uint8_t *const buf = mi->buffer;
    t2mi_packet_t *const pkt = &mi->packet;

    while (mi->skip > 0)
    {
        const uint16_t bits = (buf[4] << 8) | buf[5];
        const uint8_t padding = (8 - (bits % 8)) & 0x7;
        const size_t pay_size = (bits + padding) / 8;

        size_t want = T2MI_HEADER_SIZE;
        if (mi->skip >= want)
            /* got header; lengths are valid */
            want += pay_size + CRC32_SIZE;

        if (mi->skip < want)
            /* no full packet yet */
            break;

        /* parse headers */
        memset(pkt, 0, sizeof(*pkt));

        pkt->packet_type = buf[0];
        pkt->packet_count = buf[1];
        pkt->superframe_idx = (buf[2] & 0xf0) >> 4;
        pkt->stream_id = buf[3] & 0x7;
        pkt->payload_bits = bits;
        pkt->pad_bits = padding;

        pkt->payload_size = pay_size;
        pkt->total_size = want;

        /* check CRC32 */
        const size_t crc32_pos = want - CRC32_SIZE;
        pkt->crc32 = T2MI_GET_CRC32(&buf[crc32_pos]);
        const uint32_t calc_crc32 = crc32b(buf, crc32_pos);

        if (pkt->crc32 != calc_crc32)
        {
            asc_log_debug(MSG("T2-MI CRC mismatch; dropping packet"));
            mi->skip = 0;

            break;
        }

        /* pass it on */
        pkt->data = buf;
        pkt->end = buf + crc32_pos;

        if (!on_t2mi(mi, pkt))
            /* save failure flag for next packet */
            mi->error = true;

        /* move remainder to the beginning of the buffer */
        mi->skip -= want;
        memmove(buf, &buf[want], mi->skip);
    }
}

/*
 * payload stream discovery
 *
 * Protocol stack:
 *
 *      MPEG TS             ^
 *      Baseband Frames     |
 *      T2-MI Packets       |
 *      DVB Data Piping     |
 * ===> MPEG TS (outer)     |
 *
 * T2-MI pid diagram:
 *
 * PAT (0)
 *  |
 *  \----- PMT
 *          |
 *          \----- Payload (type 0x06)
 */
#define psi_type \
    mpegts_type_name(psi->type)

#define psi_is_pat \
    ( psi->type == MPEGTS_PACKET_PAT )

#define psi_next_type \
    ( psi_is_pat ? MPEGTS_PACKET_PMT : MPEGTS_PACKET_DATA )

#define psi_ref \
    ( psi_is_pat ? "PMT pid" : "payload pid" )

static
void on_psi(void *arg, mpegts_psi_t *psi)
{
    mpegts_t2mi_t *const mi = (mpegts_t2mi_t *)arg;

    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if (crc32 == psi->crc32)
        /* PSI unchanged */
        return;

    if (crc32 != PSI_CALC_CRC32(psi))
    {
        asc_log_error(MSG("%s checksum error"), psi_type);
        return;
    }

    /* store new checksum */
    if (psi->crc32 != 0)
        asc_log_debug(MSG("%s changed, checking %s"), psi_type, psi_ref);

    psi->crc32 = crc32;

    /* list contents */
    uint16_t *next_pid;
    uint16_t new_pid = 0;
    size_t item_count = 0;

    const uint8_t *item;
    if (psi_is_pat)
    {
        /* PAT */
        PAT_ITEMS_FOREACH(psi, item)
        {
            const uint16_t pnr = PAT_ITEM_GET_PNR(psi, item);
            const uint16_t pid = PAT_ITEM_GET_PID(psi, item);

            if (!(pnr > 0 && pid >= 32 && pid <= 8190))
                /* not a valid program */
                return;

            if (mi->prefer_pnr == pnr || !new_pid)
                new_pid = pid;

            asc_log_debug(MSG("PAT: pnr %hu, PMT pid %hu"), pnr, pid);
            item_count++;
        }

        next_pid = &mi->pmt_pid;
    }
    else
    {
        /* PMT */
        PMT_ITEMS_FOREACH(psi, item)
        {
            const uint16_t pid = PMT_ITEM_GET_PID(psi, item);
            const uint8_t item_type = PMT_ITEM_GET_TYPE(psi, item);

            if (!(pid >= 32 && pid <= 8190))
                /* not a valid ES pid */
                return;

            if (!new_pid && item_type == 0x06)
                /* pick first stream with type 0x06 (as per TS 102 773) */
                new_pid = pid;

            asc_log_debug(MSG("PMT: pid %hu, type 0x%02x"), pid, item_type);
            item_count++;
        }

        next_pid = &mi->payload_pid;
    }

    /* update pid map */
    if (*next_pid != 0)
    {
        asc_log_debug(MSG("discarding old %s %u"), psi_ref, *next_pid);
        mi->streams[*next_pid] = MPEGTS_PACKET_UNKNOWN;
    }

    if (new_pid)
    {
        asc_log_debug(MSG("%s: selected %s %u"), psi_type, psi_ref, new_pid);

        mi->streams[new_pid] = psi_next_type;
        *next_pid = new_pid;
    }
    else
        asc_log_error(MSG("%s: no valid %s found"), psi_type, psi_ref);
}

/*
 * input function
 */
void mpegts_t2mi_decap(mpegts_t2mi_t *mi, const uint8_t *ts)
{
    const unsigned int pid = TS_GET_PID(ts);

    switch (mi->streams[pid])
    {
        case MPEGTS_PACKET_PAT:
            mpegts_psi_mux(mi->pat, ts, on_psi, mi);
            break;

        case MPEGTS_PACKET_PMT:
            mpegts_psi_mux(mi->pmt, ts, on_psi, mi);
            break;

        case MPEGTS_PACKET_DATA:
            on_outer_ts(mi, ts);
            break;

        /* XXX: should we pass through CAT/EMM pids? */

        default:
            /* ignore */
            break;
    }
}
