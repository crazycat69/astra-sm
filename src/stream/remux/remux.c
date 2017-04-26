/*
 * Astra Module: Remux
 *
 * Copyright (C) 2014-2016, Artem Kharitonov <artem@3phase.pw>
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
 * Module Role:
 *      Output stage, no demux
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
static inline uint64_t get_pcr_value(const module_data_t *mod
                                    , const pcr_stream_t *pcr)
{
    return pcr->base + TS_PCR_CALC(mod->offset, mod->rate);
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
    callback(mod, ts_null_pkt);
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
    module_data_t *const mod = (module_data_t *)arg;

    /* write early; PCR gets messed up otherwise */
    mod->offset += TS_PACKET_SIZE;
    module_stream_send(mod, ts);

    /* insert SI */
    if(can_insert(&mod->pat_count, mod->pat_interval))
    {
        /* PAT */
        ts_psi_demux(mod->custom_pat, remux_ts_out, mod);

        /* PMT */
        for(size_t i = 0; i < mod->prog_cnt; i++)
            ts_psi_demux(mod->progs[i]->custom_pmt, remux_ts_out, mod);
    }

    /* CAT */
    if(can_insert(&mod->cat_count, mod->cat_interval))
        ts_psi_demux(mod->custom_cat, remux_ts_out, mod);

    /* SDT */
    if(can_insert(&mod->sdt_count, mod->sdt_interval))
        ts_psi_demux(mod->custom_sdt, remux_ts_out, mod);

    /* PCR */
    for(size_t i = 0; i < mod->pcr_cnt; i++)
    {
        pcr_stream_t *const pcr = mod->pcrs[i];

        if(can_insert(&pcr->count, mod->pcr_interval))
            insert_pcr_packet(mod, pcr, remux_ts_out);
    }
}

void remux_pes(void *arg, ts_pes_t *pes)
{
    /*
     * PES output hook
     */
    module_data_t *const mod = (module_data_t *)arg;

    /* reset error stats */
    if(pes->truncated || pes->dropped)
    {
        /* don't report initial packet loss */
        if(pes->sent > 0)
        {
            char str[128];

            size_t pos = snprintf(str, sizeof(str), "pid: %hu", pes->pid);
            if (pes->dropped)
            {
                pos += snprintf(&str[pos], sizeof(str) - pos
                                , ", TS dropped: %u", pes->dropped);
            }

            if (pes->truncated)
            {
                pos += snprintf(&str[pos], sizeof(str) - pos
                                , ", PES truncated: %u", pes->truncated);
            }

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
        pes->pcr = TS_TIME_NONE;

    /*
     * TODO
     *
     *  - PCR recovery using PTS/DTS values
     *    (in case original PCR is missing/invalid)
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
            if(pcr->last == TS_TIME_NONE)
                break;

            const int64_t delta =
                (int64_t)(pcr->last - get_pcr_value(mod, pcr));

            if(pcr->base == TS_TIME_NONE || llabs(delta) > PCR_DRIFT)
            {
                asc_log_debug(MSG("reset time base on PCR pid %hu"), pcr->pid);

                pcr->base = pcr->last - TS_PCR_CALC(mod->offset, mod->rate);
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
        case TS_TYPE_VIDEO:
        case TS_TYPE_AUDIO:
        case TS_TYPE_SUB:
            /* elementary stream */
            if(TS_GET_SC(ts) == TS_SC_NONE && mod->pes[pid])
            {
                /* pass it on for reassembly */
                ts_pes_mux(mod->pes[pid], ts);
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

        case TS_TYPE_CA:
        case TS_TYPE_EIT:
        case TS_TYPE_NIT:
        case TS_TYPE_DATA:
            /* non-PES/scrambled payload, EIT, NIT, etc */
            remux_ts_out(mod, ts);
            break;

        case TS_TYPE_PAT:
            /* global program list */
            ts_psi_mux(mod->pat, ts, remux_pat, mod);
            break;

        case TS_TYPE_CAT:
            /* conditional access table */
            ts_psi_mux(mod->cat, ts, remux_cat, mod);
            break;

        case TS_TYPE_SDT:
            /* service description table */
            ts_psi_mux(mod->sdt, ts, remux_sdt, mod);
            break;

        case TS_TYPE_PMT:
            /* stream list, program-specific */
            mod->pmt->pid = pid;
            ts_psi_mux(mod->pmt, ts, remux_pmt, mod);
            break;

        default:
            /* drop padding and unknown pids */
            break;
    }
}

/*
 * module init/deinit
 */
static void module_init(lua_State *L, module_data_t *mod)
{
    /* channel name */
    module_option_string(L, "name", &mod->name, NULL);
    if(mod->name == NULL)
        luaL_error(L, "[remux] option 'name' is required");

    /* mux rate, bps */
    module_option_integer(L, "rate", (int *)&mod->rate);
    if(mod->rate <= 1000)
        mod->rate *= 1000000;

    if(!(mod->rate >= 1000000 && mod->rate <= 1000000000))
        luaL_error(L, MSG("rate must be between 1 and 1000 mbps"));

    /* PCR interval, ms */
    if(!module_option_integer(L, "pcr_interval", (int *)&mod->pcr_interval))
        mod->pcr_interval = PCR_INTERVAL;

    if(!(mod->pcr_interval >= 20 && mod->pcr_interval <= 100))
        luaL_error(L, MSG("pcr interval must be between 20 and 100 ms"));

    /* PCR delay, ms */
    if(!module_option_integer(L, "pcr_delay", &mod->pcr_delay))
        mod->pcr_delay = PCR_DELAY;

    if(!(mod->pcr_delay >= -5000 && mod->pcr_delay <= 5000))
        luaL_error(L, MSG("pcr delay must be between -5000 and 5000 ms"));

    mod->pcr_delay *= (TS_PCR_FREQ / 1000);

    /* packet intervals */
    mod->pcr_interval = TS_PCR_PACKETS(mod->pcr_interval, mod->rate);
    /* SI (non-configurable) */
    mod->pat_interval = TS_PCR_PACKETS(PAT_INTERVAL, mod->rate);
    mod->cat_interval = TS_PCR_PACKETS(CAT_INTERVAL, mod->rate);
    mod->sdt_interval = TS_PCR_PACKETS(SDT_INTERVAL, mod->rate);

    /* PSI init */
    mod->pat = ts_psi_init(TS_TYPE_PAT, 0x00);
    mod->cat = ts_psi_init(TS_TYPE_CAT, 0x01);
    mod->sdt = ts_psi_init(TS_TYPE_SDT, 0x11);

    mod->custom_pat = ts_psi_init(TS_TYPE_PAT, 0x00);
    mod->custom_cat = ts_psi_init(TS_TYPE_CAT, 0x01);
    mod->custom_sdt = ts_psi_init(TS_TYPE_SDT, 0x11);

    mod->pmt = ts_psi_init(TS_TYPE_PMT, 0);

    /* pid list init */
    mod->stream[0x00] = TS_TYPE_PAT;
    mod->stream[0x01] = TS_TYPE_CAT;
    mod->stream[0x02] = TS_TYPE_DATA; /* TSDT */
    mod->stream[0x11] = TS_TYPE_SDT;
    mod->stream[0x12] = TS_TYPE_EIT;
    mod->stream[0x13] = TS_TYPE_DATA; /* RST */
    mod->stream[0x14] = TS_TYPE_DATA; /* TDT, TOT */

    module_stream_init(L, mod, remux_ts_in);
    module_demux_set(mod, NULL, NULL);
}

static void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);

    /* PSI deinit */
    ts_psi_destroy(mod->pat);
    ts_psi_destroy(mod->cat);
    ts_psi_destroy(mod->sdt);

    ts_psi_destroy(mod->custom_pat);
    ts_psi_destroy(mod->custom_cat);
    ts_psi_destroy(mod->custom_sdt);

    ts_psi_destroy(mod->pmt);

    /* pid list deinit */
    for(size_t i = 0; i < TS_MAX_PIDS; i++)
    {
        mod->stream[i] = TS_TYPE_UNKNOWN;

        if(mod->pes[i])
            ts_pes_destroy(mod->pes[i]);
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

STREAM_MODULE_REGISTER(remux)
{
    .init = module_init,
    .destroy = module_destroy,
};
