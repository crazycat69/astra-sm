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
#include <mpegts/t2mi.h>
#include <mpegts/psi.h>
#include <utils/crc8.h>

#define MSG(_msg) "[t2mi/%s] " _msg, mi->name

/* sizes and lengths */
#define PLP_LIST_SIZE 0x100
#define T2MI_BUFFER_SIZE 0x3000

#define T2MI_HEADER_SIZE 6
#define T2MI_BBFRAME_HEADER_SIZE 3
#define T2MI_L1_CURRENT_HEADER_SIZE 2

#define BBFRAME_HEADER_SIZE 10

#define L1_CURRENT_PRE_SIZE 21
#define L1_CURRENT_MAX_FREQS 8
#define L1_CURRENT_MAX_AUX 16

#define asc_log_error_once(...) \
    do { \
        if (!mi->warned) \
        { \
            asc_log_error(__VA_ARGS__); \
            mi->warned = false; \
        } \
    } while (0)

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

/* PLP types */
enum
{
    PLP_TYPE_COMMON = 0x0, /* Common PLP */
    PLP_TYPE_DATA_1 = 0x1, /* Data PLP Type 1 */
    PLP_TYPE_DATA_2 = 0x2, /* Data PLP Type 2 */
};

/* BBframe stream formats */
enum
{
    BBFRAME_FORMAT_GFPS = 0x0, /* Generic Packetized Stream */
    BBFRAME_FORMAT_GCS  = 0x1, /* Generic Continuous Stream */
    BBFRAME_FORMAT_GSE  = 0x2, /* Generic Encapsulated Stream */
    BBFRAME_FORMAT_TS   = 0x3, /* MPEG Transport Stream */
};

/* BBframe modes */
enum
{
    BBFRAME_MODE_NORMAL = 0x0, /* Normal Mode */
    BBFRAME_MODE_HEM    = 0x1, /* High Effeciency Mode */
};

/* Baseband frame */
typedef struct t2_plp_t t2_plp_t;

typedef struct
{
    bool     intl_frame_start;

    unsigned format;
    unsigned input_stream_id;
    bool     single_input;
    bool     constant_coding;
    bool     issy;
    bool     npd;

    unsigned upl;
    unsigned dfl;
    unsigned sync;
    unsigned syncd;

    /* mode is XOR'ed with CRC-8 */
    unsigned crc8;
    unsigned mode;

    /* points to reassembly buffer */
    uint8_t *header;
    uint8_t *data;
    const uint8_t *end;

    size_t up_offset;
    size_t up_size;
    size_t df_size;

    t2_plp_t *plp;
} bb_frame_t;

/* Physical layer pipe */
struct t2_plp_t
{
    unsigned id;
    bool     present;
    bool     active;

    /* l1conf */
    unsigned type;
    unsigned payload_type;
    bool     ff_flag;
    unsigned first_rf_idx;
    unsigned first_frame_idx;
    unsigned group_id;
    unsigned cod;
    unsigned mod;
    bool     rotation;
    unsigned fec_type;
    unsigned num_blocks_max;
    unsigned frame_interval;
    unsigned time_il_length;
    unsigned time_il_type;
    bool     in_band_a;

    /* l1dyn */
    uint32_t plp_start;
    unsigned num_blocks;

    size_t frag_skip;
    uint8_t frag[T2MI_BUFFER_SIZE];
};

/* Auxiliary stream */
typedef struct
{
    unsigned type;

    uint32_t priv_conf;
    uint64_t priv_dyn;
} t2mi_aux_t;

/* L1-current */
typedef struct
{
    /* l1pre */
    unsigned type;
    bool     bwt_ext;
    unsigned s1;
    unsigned s2;
    bool     repetition_flag;
    unsigned guard_interval;
    unsigned papr;
    unsigned mod;
    unsigned cod;
    unsigned fec_type;
    uint32_t post_size;
    uint32_t post_info_size;
    unsigned pilot_pattern;
    unsigned tx_id_availability;
    unsigned cell_id;
    unsigned network_id;
    unsigned t2_system_id;
    unsigned num_t2_frames;
    unsigned num_data_symbols;
    unsigned regen_flag;
    bool     post_extension;
    unsigned num_rf;
    unsigned current_rf_idx;
    unsigned t2_version;

    /* l1conf */
    size_t   l1conf_pos;
    unsigned sub_slices;
    unsigned num_plp;
    unsigned num_aux;

    unsigned fef_type;
    uint32_t fef_length;
    unsigned fef_interval;

    /* l1dyn */
    size_t   l1dyn_pos;
    uint32_t sub_slice_interval;
    uint32_t type_2_start;
    unsigned change_counter;
    unsigned start_rf_idx;

    /* l1ext */
    size_t   l1ext_pos;

    uint32_t frequencies[L1_CURRENT_MAX_FREQS];
    t2mi_aux_t aux[L1_CURRENT_MAX_AUX];

    unsigned cksum;
    const uint8_t *data;
} l1_current_t;

/* T2-MI packet */
typedef struct
{
    unsigned packet_type;
    unsigned packet_count;
    unsigned superframe_idx;
    unsigned stream_id;
    uint32_t crc32;

    unsigned frame_idx;

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
    unsigned prefer_pnr;
    unsigned prefer_plp;

    mpegts_psi_t *pat;
    mpegts_psi_t *pmt;

    mpegts_packet_type_t streams[MAX_PID];
    unsigned pmt_pid;
    unsigned payload_pid;
    unsigned last_cc;

    uint8_t buffer[T2MI_BUFFER_SIZE];
    size_t skip;

    t2mi_packet_t packet;
    t2_plp_t *plps[PLP_LIST_SIZE];
    l1_current_t l1_current;
    unsigned last_pkt_count;

    demux_callback_t join_pid;
    demux_callback_t leave_pid;
    module_data_t *demux_mod;

    ts_callback_t on_ts;
    void *arg;

    bool warned;
    bool seen_pkts;
    bool error;
};

/*
 * bit juggling
 */

static
uint64_t read_bit_field(const uint8_t **ptr, unsigned *off, unsigned size)
{
    static const unsigned masks[8][8] = {
        { 0xff, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe },
        { 0x7f, 0x40, 0x60, 0x70, 0x78, 0x7c, 0x7e, 0x7f },
        { 0x3f, 0x20, 0x30, 0x38, 0x3c, 0x3e, 0x3f, 0x3f },
        { 0x1f, 0x10, 0x18, 0x1c, 0x1e, 0x1f, 0x1f, 0x1f },
        { 0x0f, 0x08, 0x0c, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f },
        { 0x07, 0x04, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07 },
        { 0x03, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 },
        { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
    };

    uint64_t value = 0;
    *off %= 8;

    while (*off + size >= 8)
    {
        const unsigned bits = 8 - *off;

        size -= bits;
        value |= (**ptr & masks[*off][0]) << size;

        *off = (*off + bits) % 8;
        (*ptr)++;
    }

    if (size > 0)
    {
        const unsigned right = 8 - *off - size;
        value |= (**ptr & masks[*off][size]) >> right;
        *off += size;
    }

    return value;
}

#define BIT_SET_PTR(__data) \
    do { \
        ptr = (__data); \
        off = 0; \
    } while (0)

#define BIT_FIELD_FUNC(__size) \
    read_bit_field(&ptr, &off, (__size))

#define BIT_FIELD(__where, __size) \
    (__where) = BIT_FIELD_FUNC(__size)

#define BIT_SKIP(__size) \
    do { \
        off += (__size); \
        while (off >= 8) \
        { \
            ptr++; \
            off -= 8; \
        } \
    } while (0)

/* round up bit length to nearest byte */
#define BITS_TO_BYTES(__data_bits) \
    (((__data_bits) + 7) / 8)

/* read 32-bit integer from offset, MSB first */
#define GET_UINT32(x) \
    (uint32_t)(((x)[0] << 24) | ((x)[1] << 16) | ((x)[2] << 8) | ((x)[3]))

/*
 * string values for header fields
 */

static inline __func_const
const char *bb_format_name(unsigned fmt)
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

static inline __func_const
const char *plp_type_name(unsigned type)
{
    switch (type)
    {
        case PLP_TYPE_COMMON: return "Common Type";
        case PLP_TYPE_DATA_1: return "Data Type 1";
        case PLP_TYPE_DATA_2: return "Data Type 2";
    }

    return "Unknown";
}

/*
 * demux callback wrappers
 */

static inline
void outer_join_pid(const mpegts_t2mi_t *mi, uint16_t pid)
{
    if (mi->join_pid != NULL)
        mi->join_pid(mi->demux_mod, pid);
}

static inline
void outer_leave_pid(const mpegts_t2mi_t *mi, uint16_t pid)
{
    if (mi->leave_pid != NULL)
        mi->leave_pid(mi->demux_mod, pid);
}

/*
 * init/destroy
 */

mpegts_t2mi_t *mpegts_t2mi_init(void)
{
    mpegts_t2mi_t *const mi = ASC_ALLOC(1, mpegts_t2mi_t);

    static const char def_name[] = "t2mi";
    memcpy(mi->name, def_name, sizeof(def_name));

    mi->pat = mpegts_psi_init(MPEGTS_PACKET_PAT, 0);
    mi->pmt = mpegts_psi_init(MPEGTS_PACKET_PMT, 0);

    mi->prefer_plp = T2MI_PLP_AUTO;

    return mi;
}

void mpegts_t2mi_destroy(mpegts_t2mi_t *mi)
{
    for (size_t i = 0; i < MAX_PID; i++)
    {
        if (mi->streams[i] != MPEGTS_PACKET_UNKNOWN)
            outer_leave_pid(mi, i);
    }

    mpegts_psi_destroy(mi->pat);
    mpegts_psi_destroy(mi->pmt);

    for (size_t i = 0; i < PLP_LIST_SIZE; i++)
        free(mi->plps[i]);

    free(mi);
}

/*
 * setters
 */

void mpegts_t2mi_set_fname(mpegts_t2mi_t *mi, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    vsnprintf(mi->name, sizeof(mi->name), format, ap);

    va_end(ap);
}

void mpegts_t2mi_set_callback(mpegts_t2mi_t *mi, ts_callback_t cb, void *arg)
{
    mi->on_ts = cb;
    mi->arg = arg;
}

void mpegts_t2mi_set_plp(mpegts_t2mi_t *mi, unsigned plp_id)
{
    if (plp_id > T2MI_PLP_AUTO)
        plp_id = T2MI_PLP_AUTO;

    mi->prefer_plp = plp_id;
    mi->l1_current.cksum = 0;
}

void mpegts_t2mi_set_demux(mpegts_t2mi_t *mi, module_data_t *mod
                           , demux_callback_t join_pid
                           , demux_callback_t leave_pid)
{
    mi->demux_mod = mod;
    mi->join_pid = join_pid;
    mi->leave_pid = leave_pid;
}

void mpegts_t2mi_set_payload(mpegts_t2mi_t *mi, uint16_t pnr, uint16_t pid)
{
    /* sanitize input */
    pid &= 0x1FFF;

    if (pnr != 0)
        /* PNR implies parsing PAT/PMT */
        pid = 0;
    else if (pid != 0)
        pnr = 0;

    mi->prefer_pnr = pnr;

    /* clear SI state */
    mi->pat->crc32 = mi->pmt->crc32 = 0;
    mi->payload_pid = mi->pmt_pid = 0;

    /* reset pid map */
    for (size_t i = 0; i < MAX_PID; i++)
    {
        if (mi->streams[i] != MPEGTS_PACKET_UNKNOWN)
        {
            outer_leave_pid(mi, i);
            mi->streams[i] = MPEGTS_PACKET_UNKNOWN;
        }
    }

    if (pid == 0)
    {
        /* auto pid discovery through SI */
        mi->streams[0] = MPEGTS_PACKET_PAT;
        outer_join_pid(mi, 0);
    }
    else
    {
        /* force payload pid */
        mi->streams[pid] = MPEGTS_PACKET_DATA;
        outer_join_pid(mi, pid);

        asc_log_debug(MSG("set payload pid to %hu"), pid);
    }

    // XXX: do we need this?
    mi->streams[1] = MPEGTS_PACKET_CAT;
    outer_join_pid(mi, 1);
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
    t2_plp_t *const plp = pkt->bb.plp;
    if (!plp->frag_skip)
        return false;

    const size_t frag_skip = plp->frag_skip;
    plp->frag_skip = 0;

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
            memcpy(&plp->frag[frag_skip], bb->data, bb->up_offset);
            plp->frag_skip = frag_skip + bb->up_offset;
            // XXX: needs testing
        }
        else
        {
            asc_log_debug(MSG("reassembled UP has wrong size (expected %zu, got %zu)")
                          , bb->up_size, len);
        }

        return false;
    }

    memcpy(&plp->frag[frag_skip], bb->data, bb->up_offset);
    return true;
}

static inline
void bb_reinsert_null(mpegts_t2mi_t *mi, size_t dnp)
{
    for (size_t i = 0; i < dnp; i++)
        mi->on_ts(mi->arg, ts_null_pkt);
}

static
bool on_bbframe_ts(mpegts_t2mi_t *mi, t2mi_packet_t *pkt)
{
    bb_frame_t *const bb = &pkt->bb;
    t2_plp_t *const plp = bb->plp;

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
        mi->on_ts(mi->arg, plp->frag);
        if (bb->npd)
            bb_reinsert_null(mi, plp->frag[TS_PACKET_SIZE]);
    }

    /* zero-copy path */
    uint8_t *ptr = bb->data + bb->up_offset;
    while (ptr + bb->up_size <= bb->end)
    {
        uint8_t *const ts = ptr - 1;
        ts[0] = bb->sync;

        if (mi->on_ts)
        {
            mi->on_ts(mi->arg, ts);
            if (bb->npd)
                // TODO: insert Common PLP packets instead of null ones
                bb_reinsert_null(mi, ptr[bb->up_size - 1]);
        }

        ptr += bb->up_size;
    }

    /* store TS fragments until next time */
    const size_t left = bb->end - ptr;
    if (left > 0)
    {
        plp->frag_skip = 1 + left;
        plp->frag[0] = bb->sync;
        memcpy(&plp->frag[1], ptr, left);
    }

    return true;
}

/*
 * BBframe header inspection
 *
 * ===> Baseband Frames     ^
 *      ...                 |
 */

static
bool on_bbframe(mpegts_t2mi_t *mi, t2mi_packet_t *pkt)
{
    bb_frame_t *const bb = &pkt->bb;
    uint8_t *const ptr = bb->header;

    /* BB header, DVB-T2 part */
    bb->format = (ptr[0] & 0xC0) >> 6;
    bb->single_input = ptr[0] & 0x20;
    bb->constant_coding = ptr[0] & 0x10;
    bb->issy = ptr[0] & 0x08;
    bb->npd = ptr[0] & 0x04;

    bb->input_stream_id = ptr[1];

    bb->upl = (ptr[2] << 8) | ptr[3];
    bb->dfl = (ptr[4] << 8) | ptr[5];
    bb->syncd = (ptr[7] << 8) | ptr[8];
    bb->sync = ptr[6];

    bb->data = &ptr[BBFRAME_HEADER_SIZE];

    /* verify CRC-8 */
    bb->crc8 = au_crc8(ptr, BBFRAME_HEADER_SIZE - 1);
    bb->mode = ptr[9] ^ bb->crc8;

    if (bb->mode & ~0x1)
    {
        /* unknown mode; assume corrupt frame */
        asc_log_debug(MSG("CRC-8 error, dropping BBframe"));
        return false;
    }

    /* check DF length */
    bb->df_size = BITS_TO_BYTES(bb->dfl);
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
        // XXX: does non 8-bit aligned UP ever happen?
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
 * PLP enumeration
 *
 * ===> L1-current          ^
 *      ...                 |
 */

static
bool on_l1_current(mpegts_t2mi_t *mi, const t2mi_packet_t *pkt)
{
    l1_current_t *const l1 = &mi->l1_current;

    const uint8_t *ptr;
    unsigned off;

    /*
     * L1 pre-signaling
     */
    BIT_SET_PTR(l1->data);
    l1->l1conf_pos = L1_CURRENT_PRE_SIZE;

    if (&l1->data[l1->l1conf_pos] >= pkt->end)
    {
        asc_log_error(MSG("L1 pre-signaling length out of bounds"));
        return false;
    }

    BIT_FIELD(l1->type, 8);
    BIT_FIELD(l1->bwt_ext, 1);
    BIT_FIELD(l1->s1, 3);
    BIT_FIELD(l1->s2, 4);
    BIT_FIELD(l1->repetition_flag, 1);
    BIT_FIELD(l1->guard_interval, 3);
    BIT_FIELD(l1->papr, 4);
    BIT_FIELD(l1->mod, 4);
    BIT_FIELD(l1->cod, 2);
    BIT_FIELD(l1->fec_type, 2);
    BIT_FIELD(l1->post_size, 18);
    BIT_FIELD(l1->post_info_size, 18);
    BIT_FIELD(l1->pilot_pattern, 4);
    BIT_FIELD(l1->tx_id_availability, 8);
    BIT_FIELD(l1->cell_id, 16);
    BIT_FIELD(l1->network_id, 16);
    BIT_FIELD(l1->t2_system_id, 16);
    BIT_FIELD(l1->num_t2_frames, 8);
    BIT_FIELD(l1->num_data_symbols, 12);
    BIT_FIELD(l1->regen_flag, 3);
    BIT_FIELD(l1->post_extension, 1);
    BIT_FIELD(l1->num_rf, 3);
    BIT_FIELD(l1->current_rf_idx, 3);
    BIT_FIELD(l1->t2_version, 4);

    /*
     * L1 configurable signaling
     */
    BIT_SET_PTR(&l1->data[l1->l1conf_pos]);

    const unsigned l1conf_bits = BIT_FIELD_FUNC(16);
    l1->l1dyn_pos = l1->l1conf_pos + BITS_TO_BYTES(l1conf_bits + 16);

    if (&l1->data[l1->l1dyn_pos] >= pkt->end)
    {
        asc_log_error(MSG("L1 configurable signaling length out of bounds"));
        return false;
    }

    BIT_FIELD(l1->sub_slices, 15);
    BIT_FIELD(l1->num_plp, 8);
    BIT_FIELD(l1->num_aux, 4);
    BIT_SKIP(8);

    for (size_t i = 0; i < l1->num_rf; i++)
    {
        /* frequency listing */
        const unsigned rf_idx = BIT_FIELD_FUNC(3);
        BIT_FIELD(l1->frequencies[rf_idx], 32);
    }

    if (l1->s2 & 0x1)
    {
        /* FEF data */
        BIT_FIELD(l1->fef_type, 4);
        BIT_FIELD(l1->fef_length, 22);
        BIT_FIELD(l1->fef_interval, 8);
    }

    /* update PLP list */
    for (size_t i = 0; i < PLP_LIST_SIZE; i++)
    {
        t2_plp_t *const plp = mi->plps[i];
        if (plp != NULL)
            plp->active = plp->present = false;
    }

    t2_plp_t *selected = NULL;
    const bool auto_plp = (mi->prefer_plp == T2MI_PLP_AUTO);

    for (size_t i = 0; i < l1->num_plp; i++)
    {
        const unsigned plp_id = BIT_FIELD_FUNC(8);

        t2_plp_t *plp = mi->plps[plp_id];
        if (plp == NULL)
        {
            plp = mi->plps[plp_id] = ASC_ALLOC(1, t2_plp_t);
            asc_log_debug(MSG("added PLP %u"), plp_id);
        }

        plp->id = plp_id;
        plp->present = true;

        BIT_FIELD(plp->type, 3);
        BIT_FIELD(plp->payload_type, 5);
        BIT_FIELD(plp->ff_flag, 1);
        BIT_FIELD(plp->first_rf_idx, 3);
        BIT_FIELD(plp->first_frame_idx, 8);
        BIT_FIELD(plp->group_id, 8);
        BIT_FIELD(plp->cod, 3);
        BIT_FIELD(plp->mod, 3);
        BIT_FIELD(plp->rotation, 1);
        BIT_FIELD(plp->fec_type, 2);
        BIT_FIELD(plp->num_blocks_max, 10);
        BIT_FIELD(plp->frame_interval, 8);
        BIT_FIELD(plp->time_il_length, 8);
        BIT_FIELD(plp->time_il_type, 1);
        BIT_FIELD(plp->in_band_a, 1);
        BIT_SKIP(16);

        if (selected == NULL
            && (plp->type == PLP_TYPE_DATA_1 || plp->type == PLP_TYPE_DATA_2)
            && (auto_plp || mi->prefer_plp == plp->id))
        {
            plp->active = true;
            selected = plp;
        }
    }

    for (size_t i = 0; i < PLP_LIST_SIZE; i++)
    {
        const t2_plp_t *const plp = mi->plps[i];
        if (plp != NULL && !plp->present)
        {
            asc_log_debug(MSG("removing PLP %u"), plp->id);
            ASC_FREE(mi->plps[i], free);
        }
    }

    for (size_t i = 0; i < PLP_LIST_SIZE; i++)
    {
        t2_plp_t *const plp = mi->plps[i];
        if (plp == NULL)
            continue;

        /* look for Common Type PLP(s) in the same group */
        if (selected != NULL
            && plp->type == PLP_TYPE_COMMON && plp->group_id == selected->group_id)
        {
            plp->active = true;
        }

        if (!plp->active && plp->frag_skip > 0)
        {
            asc_log_debug(MSG("dropping UP fragments on non-active PLP %u (%zu bytes)")
                          , plp->id, plp->frag_skip);

            plp->frag_skip = 0;
        }

        /* output list of PLP's found */
        asc_log_info(MSG("L1-current: PLP %u (%s), group %u%s")
                     , plp->id, plp_type_name(plp->type), plp->group_id
                     , plp->active ? " (*)" : "");
    }

    if (selected != NULL)
    {
        asc_log_info(MSG("selected data PLP %u%s")
                     , selected->id, auto_plp ? " (auto)" : "");
    }
    else if (!auto_plp)
        asc_log_error(MSG("data PLP with ID %u not found"), mi->prefer_plp);
    else
        asc_log_error(MSG("no suitable data PLP's found"));

    /* L1 configurable, cont'd */
    BIT_SKIP(32);

    for (size_t i = 0; i < l1->num_aux; i++)
    {
        BIT_FIELD(l1->aux[i].type, 4);
        BIT_FIELD(l1->aux[i].priv_conf, 28);
    }

    /*
     * L1 dynamic signaling
     */
    BIT_SET_PTR(&l1->data[l1->l1dyn_pos]);

    const unsigned l1dyn_bits = BIT_FIELD_FUNC(16);
    l1->l1ext_pos = l1->l1dyn_pos + BITS_TO_BYTES(l1dyn_bits + 16);

    if (&l1->data[l1->l1ext_pos] >= pkt->end)
    {
        asc_log_error(MSG("L1 dynamic signaling length out of bounds"));
        return false;
    }

    BIT_SKIP(8);
    BIT_FIELD(l1->sub_slice_interval, 22);
    BIT_FIELD(l1->type_2_start, 22);
    BIT_FIELD(l1->change_counter, 8);
    BIT_FIELD(l1->start_rf_idx, 3);
    BIT_SKIP(8);

    for (size_t i = 0; i < l1->num_plp; i++)
    {
        const unsigned plp_id = BIT_FIELD_FUNC(8);
        t2_plp_t *const plp = mi->plps[plp_id];

        if (plp == NULL)
        {
            asc_log_error(MSG("L1 dynamic signaling refers to non-existent PLP %u"), plp_id);
            return false;
        }

        BIT_FIELD(plp->plp_start, 22);
        BIT_FIELD(plp->num_blocks, 10);
        BIT_SKIP(8);
    }

    return true;
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
    const unsigned expect = (mi->last_pkt_count + 1) & 0xff;
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

    /* pass it on based on packet type */
    bb_frame_t *const bb = &pkt->bb;
    l1_current_t *const l1_c = &mi->l1_current;

    unsigned cksum;

    switch (pkt->packet_type)
    {
        case T2MI_TYPE_BASEBAND_FRAME:
            /* BB header, T2-MI part */
            bb->plp = mi->plps[ptr[1]];
            if (bb->plp == NULL || !bb->plp->active)
                /* not in PLP whitelist */
                return true;

            pkt->frame_idx = ptr[0];
            bb->intl_frame_start = ptr[2] & 0x80;
            bb->header = &ptr[T2MI_BBFRAME_HEADER_SIZE];

            return on_bbframe(mi, pkt);

        case T2MI_TYPE_L1_CURRENT:
            /* L1-current header */
            pkt->frame_idx = ptr[0];
            l1_c->data = &ptr[T2MI_L1_CURRENT_HEADER_SIZE];

            cksum = au_crc8(l1_c->data, L1_CURRENT_PRE_SIZE - 1);
            if (l1_c->cksum != cksum)
            {
                if (l1_c->cksum != 0)
                    asc_log_info(MSG("L1 configuration changed"));

                l1_c->cksum = cksum;
                return on_l1_current(mi, pkt);
            }

        default:
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
    const unsigned cc = TS_GET_CC(ts);
    const unsigned expect = (mi->last_cc + 1) & 0xf;

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
        const unsigned bits = (buf[4] << 8) | buf[5];
        const size_t pay_size = BITS_TO_BYTES(bits);

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

        pkt->payload_size = pay_size;
        pkt->total_size = want;

        /* check CRC32 */
        const size_t crc32_pos = want - CRC32_SIZE;
        pkt->crc32 = GET_UINT32(&buf[crc32_pos]);
        const uint32_t calc_crc32 = au_crc32b(buf, crc32_pos);

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
 *      MPEG TS / GSE       ^
 *      L1 / BBframes       |
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
        asc_log_info(MSG("%s changed, checking %s"), psi_type, psi_ref);

    psi->crc32 = crc32;

    /* list contents */
    unsigned *next_pid;
    unsigned new_pid = 0;
    size_t item_count = 0;

    const uint8_t *item;
    if (psi_is_pat)
    {
        /* PAT */
        PAT_ITEMS_FOREACH(psi, item)
        {
            const unsigned pnr = PAT_ITEM_GET_PNR(psi, item);
            const unsigned pid = PAT_ITEM_GET_PID(psi, item);

            if (!(pnr > 0 && pid >= 32 && pid <= 8190))
                /* not a valid program */
                return;

            if (mi->prefer_pnr == pnr || !new_pid)
                new_pid = pid;

            asc_log_debug(MSG("PAT: pnr %u, PMT pid %u"), pnr, pid);
            item_count++;
        }

        next_pid = &mi->pmt_pid;
    }
    else
    {
        /* PMT */
        PMT_ITEMS_FOREACH(psi, item)
        {
            const unsigned pid = PMT_ITEM_GET_PID(psi, item);
            const unsigned item_type = PMT_ITEM_GET_TYPE(psi, item);

            if (!(pid >= 32 && pid <= 8190))
                /* not a valid ES pid */
                return;

            if (!new_pid && item_type == 0x06)
                /* pick first stream with type 0x06 (as per TS 102 773) */
                new_pid = pid;

            asc_log_debug(MSG("PMT: pid %u, type 0x%02x"), pid, item_type);
            item_count++;
        }

        next_pid = &mi->payload_pid;
    }

    /* update pid map */
    if (*next_pid != 0)
    {
        if (new_pid == *next_pid)
        {
            asc_log_debug(MSG("%s unchanged (%u)"), psi_ref, *next_pid);
            return;
        }

        asc_log_debug(MSG("discarding old %s %u"), psi_ref, *next_pid);

        mi->streams[*next_pid] = MPEGTS_PACKET_UNKNOWN;
        outer_leave_pid(mi, *next_pid);
    }

    if (new_pid)
    {
        asc_log_debug(MSG("%s: selected %s %u"), psi_type, psi_ref, new_pid);

        *next_pid = new_pid;
        mi->streams[new_pid] = psi_next_type;
        outer_join_pid(mi, new_pid);
    }
    else
        asc_log_error(MSG("%s: no valid %s found"), psi_type, psi_ref);
}

/*
 * input function
 */

void mpegts_t2mi_decap(mpegts_t2mi_t *mi, const uint8_t *ts)
{
    const unsigned pid = TS_GET_PID(ts);

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
            break;
    }
}
