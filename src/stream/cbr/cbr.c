/*
 * Astra Module: Constant bitrate muxer
 *
 * Copyright (C) 2017, Artem Kharitonov <artem@3phase.pw>
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
 *      ts_cbr
 *
 * Module Role:
 *      Input or output stage, forwards pid requests
 *
 * Module Options:
 *      upstream    - object, stream module instance
 *      name        - string, instance identifier for logging
 *      rate        - number, target bitrate in bits per second
 *      pcr_interval - number, maximum PCR insertion interval in milliseconds
 *                      default is 35 ms for DVB compliance
 *      pcr_delay   - number, apply delay in milliseconds to output PCRs
 *                      can be negative, disabled by default
 *      buffer_size - number, buffer size in milliseconds at target bitrate
 *                      default is 150 ms (e.g. ~187 KiB if rate is 10 Mbit)
 */

#include <astra/astra.h>
#include <astra/core/list.h>
#include <astra/luaapi/stream.h>
#include <astra/mpegts/pcr.h>
#include <astra/mpegts/psi.h>

#define MSG(_msg) "[cbr %s] " _msg, mod->name

/* default PCR insertion interval, milliseconds */
#define DEFAULT_PCR_INTERVAL 35

/* default buffer size, milliseconds */
#define DEFAULT_BUFFER_SIZE 150

/* maximum allowed PCR delta on receive */
#define MAX_PCR_DELTA ((TS_PCR_FREQ / 1000) * 120) /* 120ms */

typedef struct
{
    unsigned int pnr;
    unsigned int pid;
    unsigned int pcr_pid;
} pmt_item_t;

typedef struct
{
    unsigned int pid;
    unsigned int insert;
    uint64_t last;
    size_t offset;
} pcr_item_t;

struct module_data_t
{
    STREAM_MODULE_DATA();

    const char *name;
    unsigned int rate;
    unsigned int pcr_interval;
    int pcr_delay;

    ts_type_t stream[TS_MAX_PIDS];
    ts_psi_t *psi[TS_MAX_PIDS];

    asc_list_t *pmt_list;
    pmt_item_t *pmt[TS_MAX_PIDS];

    asc_list_t *pcr_list;
    pcr_item_t *pcr[TS_MAX_PIDS];
    size_t pcr_rr;

    ts_packet_t *buf;
    size_t buf_size;
    size_t buf_fill;

    uint64_t master_pcr_last;
    unsigned int master_pcr_pid;
    double pending;
    double feedback;
    //
    // TODO: use integers
    //
};

/*
 * PCR PID discovery
 */

static
void next_master_pcr(module_data_t *mod)
{
    mod->master_pcr_pid = TS_NULL_PID;
    mod->master_pcr_last = TS_TIME_NONE;
    mod->pending = mod->feedback = 0;

    const size_t pcr_cnt = asc_list_count(mod->pcr_list);
    size_t i = 0, rr = 0;

    if (pcr_cnt > 0)
        rr = mod->pcr_rr++ % pcr_cnt;

    asc_list_for(mod->pcr_list)
    {
        pcr_item_t *const pcr = (pcr_item_t *)asc_list_data(mod->pcr_list);

        if (i++ == rr)
        {
            mod->master_pcr_pid = pcr->pid;
            asc_log_debug(MSG("selected PCR PID %u as master clock")
                          , pcr->pid);
        }

        /* reset restamping state */
        pcr->insert = mod->pcr_interval;
        pcr->last = TS_TIME_NONE;
        pcr->offset = 0;
    }
}

static
void buffer_reset(module_data_t *mod);

static
void update_pcr_list(module_data_t *mod)
{
    bool pcr_pids[TS_MAX_PIDS] = { false };

    asc_list_for(mod->pmt_list)
    {
        pmt_item_t *const pmt = (pmt_item_t *)asc_list_data(mod->pmt_list);
        const unsigned int pid = pmt->pcr_pid;

        if (pid == TS_NULL_PID)
            continue; /* no PCR PID or haven't received PMT yet */

        if (mod->pcr[pid] == NULL)
        {
            /* new PCR PID */
            pcr_item_t *const pcr = ASC_ALLOC(1, pcr_item_t);
            pcr->pid = pid;
            pcr->insert = mod->pcr_interval;
            pcr->last = TS_TIME_NONE;

            mod->pcr[pid] = pcr;
            asc_list_insert_tail(mod->pcr_list, pcr);

            asc_log_debug(MSG("added PCR PID %u (program %u)")
                          , pid, pmt->pnr);
        }

        pcr_pids[pid] = true;
    }

    asc_list_first(mod->pcr_list);
    while (!asc_list_eol(mod->pcr_list))
    {
        pcr_item_t *const pcr = (pcr_item_t *)asc_list_data(mod->pcr_list);
        const unsigned int pid = pcr->pid;

        if (!pcr_pids[pid])
        {
            /* remove orphaned PCR PID */
            if (mod->master_pcr_pid == pid)
            {
                mod->master_pcr_pid = TS_NULL_PID;
                buffer_reset(mod);

                asc_log_debug(MSG("master PCR PID %u has gone away"), pid);
            }

            mod->pcr[pid] = NULL;
            asc_list_remove_current(mod->pcr_list);
            free(pcr);

            asc_log_debug(MSG("removed PCR PID %u"), pid);
        }
        else
        {
            asc_list_next(mod->pcr_list);
        }
    }

    if (mod->master_pcr_pid == TS_NULL_PID)
        next_master_pcr(mod);
}

static
void on_pmt(module_data_t *mod, ts_psi_t *psi)
{
    pmt_item_t *const pmt = mod->pmt[psi->pid];
    const unsigned int pcr_pid = PMT_GET_PCR(psi);

    if (pcr_pid != pmt->pcr_pid)
    {
        pmt->pcr_pid = pcr_pid;
        update_pcr_list(mod);
    }
}

static
void on_pat(module_data_t *mod, ts_psi_t *psi)
{
    bool pmt_pids[TS_MAX_PIDS] = { false };
    const uint8_t *ptr = NULL;

    PAT_ITEMS_FOREACH(psi, ptr)
    {
        const unsigned int pnr = PAT_ITEM_GET_PNR(psi, ptr);
        const unsigned int pid = PAT_ITEM_GET_PID(psi, ptr);

        if (!ts_pnr_valid(pnr) || !(pid >= 32 && pid < TS_NULL_PID))
            continue; /* invalid program no. or illegal PMT PID */

        if (mod->pmt[pid] == NULL)
        {
            /* program added */
            pmt_item_t *const pmt = ASC_ALLOC(1, pmt_item_t);
            pmt->pnr = pnr;
            pmt->pid = pid;
            pmt->pcr_pid = TS_NULL_PID;

            mod->stream[pid] = TS_TYPE_PMT;
            mod->psi[pid] = ts_psi_init(TS_TYPE_PMT, pid);

            mod->pmt[pid] = pmt;
            asc_list_insert_tail(mod->pmt_list, pmt);

            asc_log_debug(MSG("added PMT for program %u on PID %u")
                          , pnr, pid);
        }

        pmt_pids[pid] = true;
    }

    asc_list_first(mod->pmt_list);
    while (!asc_list_eol(mod->pmt_list))
    {
        pmt_item_t *const pmt = (pmt_item_t *)asc_list_data(mod->pmt_list);
        const unsigned int pnr = pmt->pnr;
        const unsigned int pid = pmt->pid;

        if (!pmt_pids[pid])
        {
            /* program gone */
            mod->stream[pid] = TS_TYPE_UNKNOWN;
            ASC_FREE(mod->psi[pid], ts_psi_destroy);

            mod->pmt[pid] = NULL;
            asc_list_remove_current(mod->pmt_list);
            free(pmt);

            asc_log_debug(MSG("removed PMT for program %u on PID %u")
                          , pnr, pid);
        }
        else
        {
            asc_list_next(mod->pmt_list);
        }
    }

    update_pcr_list(mod);
}

static
void on_psi(void *arg, ts_psi_t *psi)
{
    module_data_t *const mod = (module_data_t *)arg;

    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if (crc32 == psi->crc32 || crc32 != PSI_CALC_CRC32(psi))
        return;

    psi->crc32 = crc32;

    if (psi->pid == 0)
        on_pat(mod, psi);
    else
        on_pmt(mod, psi);
}

/*
 * null padding and PCR restamping
 */

static
void restamp_pcr(module_data_t *mod, uint8_t *ts)
{
    //
    // TODO: implement PCR restamping
    //

    if (ts != NULL)
        module_stream_send(mod, ts);
    else
        module_stream_send(mod, ts_null_pkt);
}

static
void buffer_pad(module_data_t *mod, double ratio)
{
    for (size_t i = 0; i < mod->buf_fill; i++)
    {
        restamp_pcr(mod, mod->buf[i]);

        mod->pending += ratio;
        while (mod->pending >= TS_PACKET_SIZE * 8)
        {
            mod->pending -= TS_PACKET_SIZE * 8;
            restamp_pcr(mod, NULL);
        }
    }

    mod->buf_fill = 0;
}

static
void buffer_reset(module_data_t *mod)
{
    for (size_t i = 0; i < mod->buf_fill; i++)
        module_stream_send(mod, mod->buf[i]);

    mod->buf_fill = 0;
}

static
void buffer_push(module_data_t *mod, const uint8_t *ts)
{
    if (mod->buf_fill >= mod->buf_size)
    {
        asc_log_error(MSG("buffer overflow, resetting master clock"));

        next_master_pcr(mod);
        buffer_reset(mod);
    }

    memcpy(mod->buf[mod->buf_fill], ts, TS_PACKET_SIZE);
    mod->buf_fill++;
}

static
void receive_pcr(module_data_t *mod, const uint8_t *ts)
{
    const uint64_t pcr_now = TS_GET_PCR(ts);
    const uint64_t pcr_last = mod->master_pcr_last;
    mod->master_pcr_last = pcr_now;

    const uint64_t delta = TS_PCR_DELTA(pcr_last, pcr_now);
    if (delta > 0 && delta < MAX_PCR_DELTA)
    {
        /* calculate padding bits per packet */
        const double got = mod->buf_fill * TS_PACKET_SIZE * 8;
        const double want = (mod->rate * delta) / (double)TS_PCR_FREQ;

        //
        // TODO: implement feedback
        // want += mod->feedback;
        //

        double ratio = 0;
        if (want > got && mod->buf_fill > 0)
        {
            ratio = (want - got) / (double)mod->buf_fill;
        }

        //
        // TODO: use integers
        //

        if (got > want)
        {
            const unsigned int in_rate = (got * TS_PCR_FREQ) / delta;
            asc_log_warning(MSG("input bitrate exceeds configured target "
                                "(%u bps > %u bps)"), in_rate, mod->rate);
        }

        /* distribute padding evenly among buffered packets */
        buffer_pad(mod, ratio);
    }
    else
    {
        /*
         * PCR delta is out of range. If this isn't the first PCR packet
         * received, treat it as an error and switch to next available
         * PCR PID. Buffered data is sent out unaltered, since there's
         * no way to estimate the correct amount of padding.
         */
        if (pcr_last != TS_TIME_NONE)
        {
            const unsigned int ms = delta / (TS_PCR_FREQ / 1000);
            asc_log_debug(MSG("PCR discontinuity (%ums) on master PCR PID %u, "
                              "resetting clock"), ms, mod->master_pcr_pid);

            next_master_pcr(mod);
        }

        buffer_reset(mod);
    }
}

static
void on_ts(module_data_t *mod, const uint8_t *ts)
{
    const unsigned int pid = TS_GET_PID(ts);

    switch (mod->stream[pid])
    {
        case TS_TYPE_NULL:
            /* drop any existing padding */
            return;

        case TS_TYPE_PAT:
        case TS_TYPE_PMT:
            ts_psi_mux(mod->psi[pid], ts, on_psi, mod);
            /* fallthrough */

        default:
            break;
    }

    if (mod->master_pcr_pid != TS_NULL_PID)
    {
        if (pid == mod->master_pcr_pid && TS_IS_PCR(ts))
        {
            receive_pcr(mod, ts);
        }

        buffer_push(mod, ts);
    }
    else
    {
        module_stream_send(mod, ts);
    }
}

/*
 * module init/destroy
 */

static
void module_init(lua_State *L, module_data_t *mod)
{
    /* instance name */
    module_option_string(L, "name", &mod->name, NULL);
    if (mod->name == NULL)
        luaL_error(L, "[cbr] option 'name' is required");

    /* target bitrate, bps */
    int opt = 0;
    if (module_option_integer(L, "rate", &opt))
    {
        if (opt < 0)
            luaL_error(L, MSG("bitrate cannot be a negative number"));

        if (opt <= 1000) /* in case rate is in mbps */
            opt *= 1000000;

        if (!(opt >= 100000 && opt <= 1000000000))
            luaL_error(L, MSG("bitrate must be between 100 Kbps and 1 Gbps"));

        mod->rate = opt;
    }
    else
    {
        luaL_error(L, MSG("option 'rate' is required"));
    }

    /* maximum PCR interval, ms */
    opt = DEFAULT_PCR_INTERVAL;
    module_option_integer(L, "pcr_interval", &opt);
    if (!(opt >= 10 && opt <= 100))
        luaL_error(L, MSG("PCR interval must be between 10 and 100 ms"));

    mod->pcr_interval = TS_PCR_PACKETS(opt, mod->rate);
    if (mod->pcr_interval <= 1)
        luaL_error(L, MSG("PCR interval is too small for configured bitrate"));

    /* PCR delay, ms */
    opt = 0;
    module_option_integer(L, "pcr_delay", &opt);
    if (!(opt >= -10000 && opt <= 10000))
        luaL_error(L, MSG("PCR delay cannot exceed 10 seconds"));

    mod->pcr_delay = opt * (TS_PCR_FREQ / 1000);

    /* buffer size, ms */
    opt = DEFAULT_BUFFER_SIZE;
    module_option_integer(L, "buffer_size", &opt);
    if (!(opt >= 100 && opt <= 1000))
        luaL_error(L, MSG("buffer size must be between 100 and 1000 ms"));

    mod->buf_size = ((mod->rate / 1000) * opt) / (TS_PACKET_SIZE * 8);
    ASC_ASSERT(mod->buf_size > 0, MSG("invalid buffer size"));

    mod->buf = ASC_ALLOC(mod->buf_size, ts_packet_t);

    /* set up PCR PID discovery via PMT */
    mod->stream[0x00] = TS_TYPE_PAT;
    mod->stream[TS_NULL_PID] = TS_TYPE_NULL;

    mod->psi[0x00] = ts_psi_init(TS_TYPE_PAT, 0x00);

    mod->pmt_list = asc_list_init();
    mod->pcr_list = asc_list_init();

    mod->master_pcr_pid = TS_NULL_PID;
    mod->master_pcr_last = TS_TIME_NONE;

    module_stream_init(L, mod, on_ts);
}

static
void module_destroy(module_data_t *mod)
{
    for (size_t i = 0; i < TS_MAX_PIDS; i++)
    {
        ASC_FREE(mod->psi[i], ts_psi_destroy);
        ASC_FREE(mod->pmt[i], free);
        ASC_FREE(mod->pcr[i], free);
    }

    ASC_FREE(mod->pmt_list, asc_list_destroy);
    ASC_FREE(mod->pcr_list, asc_list_destroy);
    ASC_FREE(mod->buf, free);

    module_stream_destroy(mod);
}

STREAM_MODULE_REGISTER(ts_cbr)
{
    .init = module_init,
    .destroy = module_destroy,
};
