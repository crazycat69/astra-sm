/*
 * Astra Module: MPEG-TS (Analyze)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
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
 *      analyze
 *
 * Module Role:
 *      Input stage or sink, requests pids optionally
 *
 * Module Options:
 *      upstream    - object, stream instance returned by module_instance:stream()
 *      name        - string, analyzer name
 *      rate_stat   - boolean, dump bitrate with 10ms interval
 *      join_pid    - boolean, request all SI tables on the upstream module
 *      callback    - function(data), events callback:
 *                    data.error    - string,
 *                    data.psi      - table, psi information (PAT, PMT, CAT, SDT)
 *                    data.analyze  - table, per pid information: errors, bitrate
 *                    data.on_air   - boolean, comes with data.analyze, stream status
 *                    data.rate     - table, rate_stat array
 */

#include <astra/astra.h>
#include <astra/core/timer.h>
#include <astra/luaapi/stream.h>
#include <astra/mpegts/descriptors.h>
#include <astra/mpegts/pes.h>
#include <astra/mpegts/psi.h>

typedef struct
{
    ts_type_t type;

    uint8_t cc;

    uint32_t packets;

    // errors
    uint32_t cc_error;  // Continuity Counter
    uint32_t sc_error;  // Scrambled
    uint32_t pes_error; // PES header
} analyze_item_t;

typedef struct
{
    uint16_t pnr;
    uint32_t crc;
} pmt_checksum_t;

struct module_data_t
{
    STREAM_MODULE_DATA();

    const char *name;
    bool rate_stat;
    int cc_limit;
    int bitrate_limit;
    bool join_pid;

    bool cc_check; // to skip initial cc errors
    bool video_check; // increase bitrate_limit for channel with video stream

    int idx_callback;

    uint16_t tsid;

    asc_timer_t *check_stat;
    analyze_item_t *stream[TS_MAX_PID];

    ts_psi_t *pat;
    ts_psi_t *cat;
    ts_psi_t *pmt;
    ts_psi_t *sdt;

    int pmt_ready;
    int pmt_count;
    pmt_checksum_t *pmt_checksum_list;

    uint8_t sdt_max_section_id;
    uint32_t *sdt_checksum_list;

    // rate_stat
    uint64_t last_ts;
    uint32_t ts_count;
    int rate_count;
    int rate[10];
};

#define MSG(_msg) "[analyze %s] " _msg, mod->name

static const char __pid[] = "pid";
static const char __crc32[] = "crc32";
static const char __pnr[] = "pnr";
static const char __tsid[] = "tsid";
static const char __descriptors[] = "descriptors";
static const char __psi[] = "psi";
static const char __err[] = "error";
static const char __callback[] = "callback";

static void callback(lua_State *L, module_data_t *mod)
{
    if(lua_type(L, -1) != LUA_TTABLE)
        asc_log_error(MSG("BUG: table required for callback!"));

    lua_rawgeti(L, LUA_REGISTRYINDEX, mod->idx_callback);
    lua_insert(L, -2);
    if (lua_tr_call(L, 1, 0) != 0)
        lua_err_log(L);
}

/*
 * oooooooooo   o   ooooooooooo
 *  888    888 888  88  888  88
 *  888oooo88 8  88     888
 *  888      8oooo88    888
 * o888o   o88o  o888o o888o
 *
 */

static void on_pat(void *arg, ts_psi_t *psi)
{
    module_data_t *const mod = (module_data_t *)arg;
    lua_State *const L = module_lua(mod);

    if(psi->buffer[0] != 0x00)
        return;

    // check changes
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == psi->crc32)
        return;

    lua_newtable(L);

    lua_pushinteger(L, psi->pid);
    lua_setfield(L, -2, __pid);

    // check crc
    if(crc32 != PSI_CALC_CRC32(psi))
    {
        lua_pushstring(L, "PAT checksum error");
        lua_setfield(L, -2, __err);
        callback(L, mod);
        return;
    }

    psi->crc32 = crc32;
    mod->tsid = PAT_GET_TSID(psi);

    lua_pushstring(L, "pat");
    lua_setfield(L, -2, __psi);

    lua_pushnumber(L, psi->crc32);
    lua_setfield(L, -2, __crc32);

    lua_pushinteger(L, mod->tsid);
    lua_setfield(L, -2, __tsid);

    mod->pmt_ready = 0;
    mod->pmt_count = 0;

    lua_newtable(L);
    const uint8_t *pointer;
    PAT_ITEMS_FOREACH(psi, pointer)
    {
        const uint16_t pnr = PAT_ITEM_GET_PNR(psi, pointer);
        const uint16_t pid = PAT_ITEM_GET_PID(psi, pointer);

        if(!pid || pid >= TS_NULL_PID)
            continue;

        const int item_count = luaL_len(L, -1) + 1;
        lua_pushinteger(L, item_count);
        lua_newtable(L);
        lua_pushinteger(L, pnr);
        lua_setfield(L, -2, __pnr);
        lua_pushinteger(L, pid);
        lua_setfield(L, -2, __pid);
        lua_settable(L, -3); // append to the "programs" table

        if(!mod->stream[pid])
            mod->stream[pid] = ASC_ALLOC(1, analyze_item_t);

        if(pnr != 0)
        {
            mod->stream[pid]->type = TS_TYPE_PMT;
            if(mod->join_pid)
                module_demux_join(mod, pid);
            ++ mod->pmt_count;
        }
        else
        {
            mod->stream[pid]->type = TS_TYPE_NIT;
            if(mod->join_pid)
                module_demux_join(mod, pid);
        }
    }
    lua_setfield(L, -2, "programs");

    ASC_FREE(mod->pmt_checksum_list, free);
    if(mod->pmt_count > 0)
        mod->pmt_checksum_list = ASC_ALLOC(mod->pmt_count, pmt_checksum_t);

    callback(L, mod);
}

/*
 *   oooooooo8     o   ooooooooooo
 * o888     88    888  88  888  88
 * 888           8  88     888
 * 888o     oo  8oooo88    888
 *  888oooo88 o88o  o888o o888o
 *
 */

static void on_cat(void *arg, ts_psi_t *psi)
{
    module_data_t *const mod = (module_data_t *)arg;
    lua_State *const L = module_lua(mod);

    if(psi->buffer[0] != 0x01)
        return;

    // check changes
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == psi->crc32)
        return;

    lua_newtable(L);

    lua_pushinteger(L, psi->pid);
    lua_setfield(L, -2, __pid);

    // check crc
    if(crc32 != PSI_CALC_CRC32(psi))
    {
        lua_pushstring(L, "CAT checksum error");
        lua_setfield(L, -2, __err);
        callback(L, mod);
        return;
    }
    psi->crc32 = crc32;

    lua_pushstring(L, "cat");
    lua_setfield(L, -2, __psi);

    lua_pushnumber(L, psi->crc32);
    lua_setfield(L, -2, __crc32);

    int descriptors_count = 1;
    lua_newtable(L);
    const uint8_t *desc_pointer = CAT_DESC_FIRST(psi);
    while(!CAT_DESC_EOL(psi, desc_pointer))
    {
        lua_pushinteger(L, descriptors_count++);
        ts_desc_to_lua(L, desc_pointer);
        lua_settable(L, -3); // append to the "descriptors" table

        CAT_DESC_NEXT(psi, desc_pointer);
    }
    lua_setfield(L, -2, __descriptors);

    callback(L, mod);
}

/*
 * oooooooooo oooo     oooo ooooooooooo
 *  888    888 8888o   888  88  888  88
 *  888oooo88  88 888o8 88      888
 *  888        88  888  88      888
 * o888o      o88o  8  o88o    o888o
 *
 */

static void on_pmt(void *arg, ts_psi_t *psi)
{
    module_data_t *const mod = (module_data_t *)arg;
    lua_State *const L = module_lua(mod);

    if(psi->buffer[0] != 0x02)
        return;

    const uint32_t crc32 = PSI_GET_CRC32(psi);

    // check crc
    if(crc32 != PSI_CALC_CRC32(psi))
    {
        lua_newtable(L);

        lua_pushinteger(L, psi->pid);
        lua_setfield(L, -2, __pid);

        lua_pushstring(L, "PMT checksum error");
        lua_setfield(L, -2, __err);
        callback(L, mod);
        return;
    }

    const uint16_t pnr = PMT_GET_PNR(psi);

    // check changes
    for(int i = 0; i < mod->pmt_count; ++i)
    {
        if(mod->pmt_checksum_list[i].pnr == pnr)
        {
            if(mod->pmt_checksum_list[i].crc == crc32)
                return;

            -- mod->pmt_ready;
            mod->pmt_checksum_list[i].pnr = 0;
            break;
        }
    }

    for(int i = 0; i < mod->pmt_count; ++i)
    {
        if(mod->pmt_checksum_list[i].pnr == 0)
        {
            ++ mod->pmt_ready;
            mod->pmt_checksum_list[i].pnr = pnr;
            mod->pmt_checksum_list[i].crc = crc32;
            break;
        }
    }

    mod->video_check = false;

    lua_newtable(L);

    lua_pushinteger(L, psi->pid);
    lua_setfield(L, -2, __pid);

    lua_pushstring(L, "pmt");
    lua_setfield(L, -2, __psi);

    lua_pushnumber(L, crc32);
    lua_setfield(L, -2, __crc32);

    lua_pushinteger(L, pnr);
    lua_setfield(L, -2, __pnr);

    int descriptors_count = 1;
    lua_newtable(L);
    const uint8_t *desc_pointer = PMT_DESC_FIRST(psi);
    while(!PMT_DESC_EOL(psi, desc_pointer))
    {
        lua_pushinteger(L, descriptors_count++);
        ts_desc_to_lua(L, desc_pointer);
        lua_settable(L, -3); // append to the "descriptors" table

        PMT_DESC_NEXT(psi, desc_pointer);
    }
    lua_setfield(L, -2, __descriptors);

    lua_pushinteger(L, PMT_GET_PCR(psi));
    lua_setfield(L, -2, "pcr");

    int streams_count = 1;
    lua_newtable(L);
    const uint8_t *pointer;
    PMT_ITEMS_FOREACH(psi, pointer)
    {
        const uint16_t pid = PMT_ITEM_GET_PID(psi, pointer);
        const uint8_t type = PMT_ITEM_GET_TYPE(psi, pointer);

        if(!pid || pid >= TS_NULL_PID)
            continue;

        lua_pushinteger(L, streams_count++);
        lua_newtable(L);

        if(!mod->stream[pid])
            mod->stream[pid] = ASC_ALLOC(1, analyze_item_t);

        const ts_stream_type_t *const st = ts_stream_type(type);
        mod->stream[pid]->type = st->pkt_type;

        lua_pushinteger(L, pid);
        lua_setfield(L, -2, __pid);

        descriptors_count = 1;
        lua_newtable(L);
        PMT_ITEM_DESC_FOREACH(pointer, desc_pointer)
        {
            lua_pushinteger(L, descriptors_count++);
            ts_desc_to_lua(L, desc_pointer);
            lua_settable(L, -3); // append to the "streams[X].descriptors" table

            if(type == 0x06 && mod->stream[pid]->type == TS_TYPE_DATA)
                mod->stream[pid]->type = ts_priv_type(desc_pointer[0]);
        }
        lua_setfield(L, -2, __descriptors);

        lua_pushstring(L, ts_type_name(mod->stream[pid]->type));
        lua_setfield(L, -2, "type_name");

        lua_pushinteger(L, type);
        lua_setfield(L, -2, "type_id");

        lua_pushstring(L, st->description);
        lua_setfield(L, -2, "type_description");

        lua_settable(L, -3); // append to the "streams" table

        if(mod->stream[pid]->type == TS_TYPE_VIDEO)
            mod->video_check = true;
    }
    lua_setfield(L, -2, "streams");

    callback(L, mod);
}

/*
 *  oooooooo8 ooooooooo   ooooooooooo
 * 888         888    88o 88  888  88
 *  888oooooo  888    888     888
 *         888 888    888     888
 * o88oooo888 o888ooo88      o888o
 *
 */

static void on_sdt(void *arg, ts_psi_t *psi)
{
    module_data_t *const mod = (module_data_t *)arg;
    lua_State *const L = module_lua(mod);

    if(psi->buffer[0] != 0x42)
        return;

    if(mod->tsid != SDT_GET_TSID(psi))
        return;

    const uint32_t crc32 = PSI_GET_CRC32(psi);

    // check crc
    if(crc32 != PSI_CALC_CRC32(psi))
    {
        lua_newtable(L);

        lua_pushinteger(L, psi->pid);
        lua_setfield(L, -2, __pid);

        lua_pushstring(L, "SDT checksum error");
        lua_setfield(L, -2, __err);
        callback(L, mod);
        return;
    }

    // check changes
    if(!mod->sdt_checksum_list)
    {
        const uint8_t max_section_id = SDT_GET_LAST_SECTION_NUMBER(psi);
        mod->sdt_max_section_id = max_section_id;
        mod->sdt_checksum_list = ASC_ALLOC(max_section_id + 1, uint32_t);
    }
    const uint8_t section_id = SDT_GET_SECTION_NUMBER(psi);
    if(section_id > mod->sdt_max_section_id)
    {
        asc_log_warning(MSG("SDT: section_number is greater then section_last_number"));
        return;
    }
    if(mod->sdt_checksum_list[section_id] == crc32)
        return;

    if(mod->sdt_checksum_list[section_id] != 0)
    {
        // Reload stream
        free(mod->sdt_checksum_list);
        mod->sdt_checksum_list = NULL;
        return;
    }

    mod->sdt_checksum_list[section_id] = crc32;

    lua_newtable(L);

    lua_pushinteger(L, psi->pid);
    lua_setfield(L, -2, __pid);

    lua_pushstring(L, "sdt");
    lua_setfield(L, -2, __psi);

    lua_pushnumber(L, crc32);
    lua_setfield(L, -2, __crc32);

    lua_pushinteger(L, mod->tsid);
    lua_setfield(L, -2, __tsid);

    int descriptors_count;
    int services_count = 1;
    lua_newtable(L);
    const uint8_t *pointer;
    SDT_ITEMS_FOREACH(psi, pointer)
    {
        const uint16_t sid = SDT_ITEM_GET_SID(psi, pointer);

        lua_pushinteger(L, services_count++);

        lua_newtable(L);
        lua_pushinteger(L, sid);
        lua_setfield(L, -2, "sid");

        descriptors_count = 1;
        lua_newtable(L);
        const uint8_t *desc_pointer;
        SDT_ITEM_DESC_FOREACH(pointer, desc_pointer)
        {
            lua_pushinteger(L, descriptors_count++);
            ts_desc_to_lua(L, desc_pointer);
            lua_settable(L, -3);
        }
        lua_setfield(L, -2, __descriptors);

        lua_settable(L, -3); // append to the "services[X].descriptors" table
    }
    lua_setfield(L, -2, "services");

    callback(L, mod);
}

/*
 * ooooooooooo  oooooooo8
 * 88  888  88 888
 *     888      888oooooo
 *     888             888
 *    o888o    o88oooo888
 *
 */

static void append_rate(module_data_t *mod, int rate)
{
    lua_State *const L = module_lua(mod);

    mod->rate[mod->rate_count] = rate;
    ++mod->rate_count;
    if(mod->rate_count >= (int)(sizeof(mod->rate)/sizeof(*mod->rate)))
    {
        lua_newtable(L);
        lua_newtable(L);
        for(int i = 0; i < mod->rate_count; ++i)
        {
            lua_pushinteger(L, i + 1);
            lua_pushinteger(L, mod->rate[i]);
            lua_settable(L, -3);
        }
        lua_setfield(L, -2, "rate");
        callback(L, mod);
        mod->rate_count = 0;
    }
}

static void on_ts(module_data_t *mod, const uint8_t *ts)
{
    if(mod->rate_stat)
    {
        ++mod->ts_count;

        uint64_t diff_interval = 0;
        const uint64_t cur = asc_utime() / 10000;

        if(cur != mod->last_ts)
        {
            if(mod->last_ts != 0 && cur > mod->last_ts)
                diff_interval = cur - mod->last_ts;

            mod->last_ts = cur;
        }

        if(diff_interval > 0)
        {
            if(diff_interval > 1)
            {
                for(; diff_interval > 0; --diff_interval)
                    append_rate(mod, 0);
            }

            append_rate(mod, mod->ts_count);
            mod->ts_count = 0;
        }
    }

    const uint16_t pid = TS_GET_PID(ts);
    analyze_item_t *item = NULL;
    if(ts[0] == 0x47 && pid < TS_MAX_PID)
        item = mod->stream[pid];
    if(!item)
        item = mod->stream[TS_NULL_PID];

    ++item->packets;

    if(item->type == TS_TYPE_NULL)
        return;

    if(item->type & (TS_TYPE_PSI | TS_TYPE_SI))
    {
        switch(item->type)
        {
            case TS_TYPE_PAT:
                ts_psi_mux(mod->pat, ts, on_pat, mod);
                break;
            case TS_TYPE_CAT:
                ts_psi_mux(mod->cat, ts, on_cat, mod);
                break;
            case TS_TYPE_PMT:
                mod->pmt->pid = pid;
                ts_psi_mux(mod->pmt, ts, on_pmt, mod);
                break;
            case TS_TYPE_SDT:
                ts_psi_mux(mod->sdt, ts, on_sdt, mod);
                break;
            default:
                break;
        }
    }

    // Analyze

    // skip packets without payload
    if(!TS_IS_PAYLOAD(ts))
        return;

    const uint8_t cc = TS_GET_CC(ts);
    const uint8_t last_cc = (item->cc + 1) & 0x0F;
    item->cc = cc;

    if(cc != last_cc)
        ++item->cc_error;

    if(TS_IS_SCRAMBLED(ts))
        ++item->sc_error;

    if(!(item->type & TS_TYPE_PES))
        return;

    if(item->type == TS_TYPE_VIDEO && TS_IS_PAYLOAD_START(ts))
    {
        const uint8_t *payload = TS_GET_PAYLOAD(ts);
        if(payload && PES_BUFFER_GET_HEADER(payload) != 0x000001)
            ++item->pes_error;
    }
}

/*
 *  oooooooo8 ooooooooooo   o   ooooooooooo
 * 888        88  888  88  888  88  888  88
 *  888oooooo     888     8  88     888
 *         888    888    8oooo88    888
 * o88oooo888    o888o o88o  o888o o888o
 *
 */

static void on_check_stat(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;
    lua_State *const L = module_lua(mod);

    int items_count = 1;
    lua_newtable(L);

    bool on_air = true;

    uint32_t bitrate = 0;
    uint32_t cc_errors = 0;
    uint32_t pes_errors = 0;
    bool scrambled = false;

    const uint32_t bitrate_limit = (mod->bitrate_limit > 0)
                                 ? ((uint32_t)mod->bitrate_limit)
                                 : ((mod->video_check) ? 256 : 32);

    lua_newtable(L);
    for(int i = 0; i < TS_MAX_PID; ++i)
    {
        analyze_item_t *item = mod->stream[i];

        if(!item)
            continue;

        if(!mod->cc_check)
            item->cc_error = 0;

        lua_pushinteger(L, items_count++);
        lua_newtable(L);

        lua_pushinteger(L, i);
        lua_setfield(L, -2, __pid);

        const uint32_t item_bitrate =
            ((uint64_t)item->packets * TS_PACKET_SIZE * 8) / 1000;
        bitrate += item_bitrate;

        lua_pushinteger(L, item_bitrate);
        lua_setfield(L, -2, "bitrate");

        lua_pushinteger(L, item->cc_error);
        lua_setfield(L, -2, "cc_error");
        lua_pushinteger(L, item->sc_error);
        lua_setfield(L, -2, "sc_error");
        lua_pushinteger(L, item->pes_error);
        lua_setfield(L, -2, "pes_error");

        cc_errors += item->cc_error;
        pes_errors += item->pes_error;

        if(item->type == TS_TYPE_VIDEO || item->type == TS_TYPE_AUDIO)
        {
            if(item->sc_error)
            {
                scrambled = true;
                on_air = false;
            }
            if(item->pes_error > 2)
                on_air = false;
        }

        item->packets = 0;
        item->cc_error = 0;
        item->sc_error = 0;
        item->pes_error = 0;

        lua_settable(L, -3);
    }
    lua_setfield(L, -2, "analyze");

    lua_newtable(L);
    {
        lua_pushinteger(L, bitrate);
        lua_setfield(L, -2, "bitrate");
        lua_pushinteger(L, cc_errors);
        lua_setfield(L, -2, "cc_errors");
        lua_pushinteger(L, pes_errors);
        lua_setfield(L, -2, "pes_errors");
        lua_pushboolean(L, scrambled);
        lua_setfield(L, -2, "scrambled");
    }
    lua_setfield(L, -2, "total");

    if(!mod->cc_check)
        mod->cc_check = true;

    if(bitrate < bitrate_limit)
        on_air = false;
    if(mod->cc_limit > 0 && cc_errors >= (uint32_t)mod->cc_limit)
        on_air = false;
    if(mod->pmt_ready == 0 || mod->pmt_ready != mod->pmt_count)
        on_air = false;

    lua_pushboolean(L, on_air);
    lua_setfield(L, -2, "on_air");

    callback(L, mod);
}

/*
 * oooo     oooo  ooooooo  ooooooooo  ooooo  oooo ooooo       ooooooooooo
 *  8888o   888 o888   888o 888    88o 888    88   888         888    88
 *  88 888o8 88 888     888 888    888 888    88   888         888ooo8
 *  88  888  88 888o   o888 888    888 888    88   888      o  888    oo
 * o88o  8  o88o  88ooo88  o888ooo88    888oo88   o888ooooo88 o888ooo8888
 *
 */

static void module_init(lua_State *L, module_data_t *mod)
{
    module_option_string(L, "name", &mod->name, NULL);
    if(mod->name == NULL)
        luaL_error(L, "[analyze] option 'name' is required");

    lua_getfield(L, MODULE_OPTIONS_IDX, __callback);
    if(!lua_isfunction(L, -1))
        luaL_error(L, MSG("option 'callback' is required"));

    mod->idx_callback = luaL_ref(L, LUA_REGISTRYINDEX);

    module_option_boolean(L, "rate_stat", &mod->rate_stat);
    module_option_integer(L, "cc_limit", &mod->cc_limit);
    module_option_integer(L, "bitrate_limit", &mod->bitrate_limit);
    module_option_boolean(L, "join_pid", &mod->join_pid);

    module_stream_init(L, mod, on_ts);
    module_demux_set(mod, NULL, NULL);
    if(mod->join_pid)
    {
        module_demux_join(mod, 0x00);
        module_demux_join(mod, 0x01);
        module_demux_join(mod, 0x11);
        module_demux_join(mod, 0x12);
    }

    // PAT
    mod->stream[0x00] = ASC_ALLOC(1, analyze_item_t);
    mod->stream[0x00]->type = TS_TYPE_PAT;
    mod->pat = ts_psi_init(TS_TYPE_PAT, 0x00);
    // CAT
    mod->stream[0x01] = ASC_ALLOC(1, analyze_item_t);
    mod->stream[0x01]->type = TS_TYPE_CAT;
    mod->cat = ts_psi_init(TS_TYPE_CAT, 0x01);
    // SDT
    mod->stream[0x11] = ASC_ALLOC(1, analyze_item_t);
    mod->stream[0x11]->type = TS_TYPE_SDT;
    mod->sdt = ts_psi_init(TS_TYPE_SDT, 0x11);
    // EIT
    mod->stream[0x12] = ASC_ALLOC(1, analyze_item_t);
    mod->stream[0x12]->type = TS_TYPE_EIT;
    // PMT
    mod->pmt = ts_psi_init(TS_TYPE_PMT, TS_MAX_PID);
    // NULL
    mod->stream[TS_NULL_PID] = ASC_ALLOC(1, analyze_item_t);
    mod->stream[TS_NULL_PID]->type = TS_TYPE_NULL;

    mod->check_stat = asc_timer_init(1000, on_check_stat, mod);
}

static void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);

    if(mod->idx_callback)
    {
        luaL_unref(module_lua(mod), LUA_REGISTRYINDEX, mod->idx_callback);
        mod->idx_callback = 0;
    }

    for(int i = 0; i < TS_MAX_PID; ++i)
    {
        if(mod->stream[i])
            free(mod->stream[i]);
    }

    ts_psi_destroy(mod->pat);
    ts_psi_destroy(mod->cat);
    ts_psi_destroy(mod->sdt);
    ts_psi_destroy(mod->pmt);

    ASC_FREE(mod->check_stat, asc_timer_destroy);

    free(mod->pmt_checksum_list);
    free(mod->sdt_checksum_list);
}

STREAM_MODULE_REGISTER(analyze)
{
    .init = module_init,
    .destroy = module_destroy,
};
