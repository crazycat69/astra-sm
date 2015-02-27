/*
 * Astra Module: Remux
 *
 * Copyright (C) 2014-2015, Artem Kharitonov <artem@sysert.ru>
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

/*
 * Module Name:
 *      remux
 *
 * Module Options:
 *      rate - target bitrate, bits per second
 *      pcr_interval - PCR insertion interval, ms
 *      pcr_delay - delay to apply to PCR value, ms
 */

#include "remux.h"

/*
 * stuffing and PCR restamping
 */
static inline uint64_t offset_to_pcr(const module_data_t *mod)
{
    /*
     * number of bytes written rescaled to PCR time base
     * (see libavformat/mpegtsenc.c)
     */
    return ((mod->offset + PCR_LAST_BYTE) * 8 * PCR_TIME_BASE) / mod->rate;
}

static inline uint64_t get_pcr_value(const module_data_t *mod
                                    , const pcr_stream_t *pcr)
{
    return pcr->base + offset_to_pcr(mod);
}

static inline void insert_pcr_packet(module_data_t *mod
                                     , const pcr_stream_t *pcr
                                     , ts_callback_t callback)
{
    /* add stuffing */
    uint8_t ts[TS_PACKET_SIZE];
    memset(ts, 0xff, sizeof(ts));

    /* if on A/V pid, use last CC */
    uint8_t cc = 0;
    if(mod->pes[pcr->pid])
        /* NOTE: CC is not incremented on AF-only packets */
        cc = mod->pes[pcr->pid]->o_cc;

    /* write TS header */
    ts[0] = 0x47;
    ts[1] = pcr->pid >> 8;
    ts[2] = pcr->pid;
    ts[3] = 0x20 | (cc & 0xf); /* AF only */
    ts[4] = TS_BODY_SIZE - 1;  /* AF length */
    ts[5] = 0x10;              /* PCR present */

    /* write PCR bits */
    TS_SET_PCR(ts, get_pcr_value(mod, pcr));

    callback(mod, ts);
}

static inline void insert_null_packet(module_data_t *mod
                                      , ts_callback_t callback)
{
    static const uint8_t ts[TS_PACKET_SIZE] = {
        /*
         * pid 0x1fff, cc 0
         * payload all zeroes
         */
        0x47, 0x1f, 0xff, 0x10
    };

    callback(mod, ts);
}

static inline unsigned msecs_to_pkts(unsigned rate, unsigned msec)
{
    return (msec * rate) / (TS_PACKET_SIZE * 8 * 1000);
}

static inline bool can_insert(unsigned *count, unsigned interval)
{
    if(++*count > interval)
        *count = 0;

    return !(*count);
}

/*
 * TS datapath
 */
void remux_ts_out(void *arg, const uint8_t *ts)
{
    /*
     * TS output hook
     */
    module_data_t *mod = (module_data_t *)arg;

    /*
     * TODO
     *
     *  - software scrambling using libdvbcsa BS mode
     *  - BISS w/CAT and CA_descriptor in PMT
     *  - inserting ECM and EMM into output stream
     */

    /* write early; PCR gets messed up otherwise */
    mod->offset += TS_PACKET_SIZE;
    module_stream_send(mod, ts);

    /* insert SI */
    if(can_insert(&mod->pat_count, mod->pat_interval))
    {
        /* PAT */
        mpegts_psi_demux(mod->custom_pat, remux_ts_out, mod);

        /* PMT */
        for(size_t i = 0; i < mod->prog_cnt; i++)
            mpegts_psi_demux(mod->progs[i]->custom_pmt, remux_ts_out, mod);
    }

    /* CAT */
    if(can_insert(&mod->cat_count, mod->cat_interval))
        mpegts_psi_demux(mod->custom_cat, remux_ts_out, mod);

    /* SDT */
    if(can_insert(&mod->sdt_count, mod->sdt_interval))
        mpegts_psi_demux(mod->custom_sdt, remux_ts_out, mod);

    /* PCR */
    for(size_t i = 0; i < mod->pcr_cnt; i++)
    {
        pcr_stream_t *const pcr = mod->pcrs[i];

        if(can_insert(&pcr->count, mod->pcr_interval))
            insert_pcr_packet(mod, pcr, remux_ts_out);
    }
}

void remux_pes(void *arg, mpegts_pes_t *pes)
{
    /*
     * PES output hook
     */
    module_data_t *mod = (module_data_t *)arg;

    /* reset error stats */
    if(pes->truncated || pes->dropped)
    {
        /* don't report initial packet loss */
        if(pes->sent > 0)
        {
            char str[128];
            unsigned pos = 0;

            pos += sprintf(&str[pos], "pid: %hu", pes->pid);
            if (pes->dropped)
                pos += sprintf(&str[pos], ", TS dropped: %u", pes->dropped);

            if (pes->truncated)
                pos += sprintf(&str[pos], ", PES truncated: %u", pes->truncated);

            asc_log_error(MSG("%s"), str);
        }

        pes->truncated = pes->dropped = 0;
    }

    /* add PCR to keyframes on PCR pids */
    pcr_stream_t *pcr;
    if(pes->key && (pcr = pcr_stream_find(mod, pes->pid)))
    {
        pes->pcr = get_pcr_value(mod, pcr);
        pcr->count = 0;
    } else
        pes->pcr = XTS_NONE;

    /*
     * TODO
     *
     *  - PCR recovery using PTS/DTS values
     *    (in case original PCR is missing/invalid)
     *
     *  - add audio mixing/recoding
     */
}

void remux_ts_in(module_data_t *mod, const uint8_t *orig_ts)
{
    /*
     * TS input hook
     */
    const uint8_t *ts = orig_ts;
    const uint16_t pid = TS_GET_PID(ts);

    /*
     * TODO
     *
     *  - use first PCR pid as clock reference
     *  - in case of faulty PCR, switch over to next pid
     *  - if no valid PCR is available, recreate it from PTS
     */

    pcr_stream_t *const pcr = pcr_stream_find(mod, pid);

    if(pcr)
    {
        if(TS_IS_PCR(ts))
            pcr->last = TS_GET_PCR(ts) - (unsigned)mod->pcr_delay;

        while(1)
        {
            if(pcr->last == XTS_NONE)
                break;

            const int64_t delta =
                (int64_t)(pcr->last - get_pcr_value(mod, pcr));

            if(pcr->base == XTS_NONE || llabs(delta) > PCR_DRIFT)
            {
                asc_log_debug(MSG("reset time base on PCR pid %hu"), pcr->pid);

                pcr->base = pcr->last - offset_to_pcr(mod);
                continue;
            }

            if(delta < 0)
                break;
            else
                insert_null_packet(mod, remux_ts_out);
        }
    }

    switch(mod->stream[pid])
    {
        case MPEGTS_PACKET_VIDEO:
        case MPEGTS_PACKET_AUDIO:
        case MPEGTS_PACKET_SUB:
            /* elementary stream */
            if(!TS_IS_SCRAMBLED(ts) && mod->pes[pid])
            {
                /* pass it on for reassembly */
                mpegts_pes_mux(mod->pes[pid], ts);
                break;
            }
            else if(TS_IS_PCR(ts))
            {
                /* got PCR in a scrambled packet */
                uint8_t *const copy = mod->buf;
                memcpy(copy, ts, TS_PACKET_SIZE);

                /* clear PCR flag and field */
                copy[5] &= ~0x10;

                const size_t af_len = ts[4];
                if(af_len < TS_BODY_SIZE)
                {
                    memset(&copy[6], 0xff, af_len - 1);

                    /* 7 = 1 + 6 (flags + PCR field) */
                    if(af_len > 7)
                        /* move remaining AF bytes */
                        memcpy(&copy[6], &ts[12], af_len - 7);
                }

                ts = copy;
            }

        /* pass these through for now */
        case MPEGTS_PACKET_CA:
            /* TODO: drop_ca option */

        case MPEGTS_PACKET_EIT:
        case MPEGTS_PACKET_NIT:
        case MPEGTS_PACKET_DATA:
            /* non-PES/scrambled payload, EIT, NIT, etc */
            remux_ts_out(mod, ts);
            break;

        case MPEGTS_PACKET_PAT:
            /* global program list */
            mpegts_psi_mux(mod->pat, ts, remux_pat, mod);
            break;

        case MPEGTS_PACKET_CAT:
            /* conditional access table */
            mpegts_psi_mux(mod->cat, ts, remux_cat, mod);
            break;
            /* TODO: drop_ca option */

        case MPEGTS_PACKET_SDT:
            /* service description table */
            mpegts_psi_mux(mod->sdt, ts, remux_sdt, mod);
            break;

        case MPEGTS_PACKET_PMT:
            /* stream list, program-specific */
            mod->pmt->pid = pid;
            mpegts_psi_mux(mod->pmt, ts, remux_pmt, mod);
            break;

        default:
            /* drop padding and unknown pids */
            break;
    }
}

/*
 * module init/deinit
 */
static void module_init(module_data_t *mod)
{
    /* channel name */
    module_option_string("name", &mod->name, NULL);
    asc_assert(mod->name != NULL, "[remux] option 'name' is required");

    /* mux rate, bps */
    module_option_number("rate", (int *)&mod->rate);
    asc_assert(mod->rate >= 1000000 && mod->rate <= 1000000000
               , MSG("rate must be between 1 and 1000 mbps"));

    /* PCR interval, ms */
    if(!module_option_number("pcr_interval", (int *)&mod->pcr_interval))
        mod->pcr_interval = PCR_INTERVAL;

    asc_assert(mod->pcr_interval >= 20 && mod->pcr_interval <= 100
               , MSG("pcr interval must be between 20 and 100 ms"));

    /* PCR delay, ms */
    if(!module_option_number("pcr_delay", &mod->pcr_delay))
        mod->pcr_delay = PCR_DELAY;

    asc_assert(mod->pcr_delay >= -5000 && mod->pcr_delay <= 5000
               , MSG("pcr delay must be between -5000 and 5000 ms"));

    mod->pcr_delay *= (PCR_TIME_BASE / 1000);

    /* packet intervals */
    mod->pcr_interval = msecs_to_pkts(mod->rate, mod->pcr_interval);
    /* SI (non-configurable) */
    mod->pat_interval = msecs_to_pkts(mod->rate, PAT_INTERVAL);
    mod->cat_interval = msecs_to_pkts(mod->rate, CAT_INTERVAL);
    mod->sdt_interval = msecs_to_pkts(mod->rate, SDT_INTERVAL);

    /* PSI init */
    mod->pat = mpegts_psi_init(MPEGTS_PACKET_PAT, 0x00);
    mod->cat = mpegts_psi_init(MPEGTS_PACKET_CAT, 0x01);
    mod->sdt = mpegts_psi_init(MPEGTS_PACKET_SDT, 0x11);

    mod->custom_pat = mpegts_psi_init(MPEGTS_PACKET_PAT, 0x00);
    mod->custom_cat = mpegts_psi_init(MPEGTS_PACKET_CAT, 0x01);
    mod->custom_sdt = mpegts_psi_init(MPEGTS_PACKET_SDT, 0x11);

    mod->pmt = mpegts_psi_init(0, 0);

    /* pid list init */
    mod->stream[0x00] = MPEGTS_PACKET_PAT;
    mod->stream[0x01] = MPEGTS_PACKET_CAT;
    mod->stream[0x02] = MPEGTS_PACKET_DATA; /* TSDT */
    mod->stream[0x11] = MPEGTS_PACKET_SDT;
    mod->stream[0x12] = MPEGTS_PACKET_EIT;
    mod->stream[0x13] = MPEGTS_PACKET_DATA; /* RST */
    mod->stream[0x14] = MPEGTS_PACKET_DATA; /* TDT, TOT */

    module_stream_init(mod, remux_ts_in);
}

static void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);

    /* PSI deinit */
    mpegts_psi_destroy(mod->pat);
    mpegts_psi_destroy(mod->cat);
    mpegts_psi_destroy(mod->sdt);

    mpegts_psi_destroy(mod->custom_pat);
    mpegts_psi_destroy(mod->custom_cat);
    mpegts_psi_destroy(mod->custom_sdt);

    mpegts_psi_destroy(mod->pmt);

    /* pid list deinit */
    for(size_t i = 0; i < MAX_PID; i++)
    {
        mod->stream[i] = MPEGTS_PACKET_UNKNOWN;

        if(mod->pes[i])
            mpegts_pes_destroy(mod->pes[i]);
    }

    /* free structs */
    for(size_t i = 0; i < mod->prog_cnt; i++)
        ts_program_destroy(mod->progs[i]);

    for(size_t i = 0; i < mod->pcr_cnt; i++)
        pcr_stream_destroy(mod->pcrs[i]);

    mod->nit_pid = 0;
    mod->prog_cnt = 0;
    mod->emm_cnt = 0;

    free(mod->progs);
    free(mod->emms);

    mod->progs = NULL;
    mod->emms = NULL;
}

MODULE_STREAM_METHODS()
MODULE_LUA_METHODS()
{
    MODULE_STREAM_METHODS_REF()
};
MODULE_LUA_REGISTER(remux)
