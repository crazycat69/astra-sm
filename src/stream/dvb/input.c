/*
 * Astra Module: DVB
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
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

#include "dvb.h"
#include <core/event.h>
#include <core/thread.h>
#include <core/timer.h>
#include <mpegts/t2mi.h>

#define MSG(_msg) "[dvb_input %d:%d] " _msg, mod->adapter, mod->device

#define DVR_RETRY 10

struct module_data_t
{
    MODULE_STREAM_DATA();

    int adapter;
    int device;

    /* Base */
    asc_thread_t *thread;
    bool is_thread_started;

    asc_timer_t *retry_timer;
    asc_timer_t *status_timer;
    int idx_callback;

    /* DVR Config */
    bool no_dvr;
    int dvr_buffer_size;

    /* DVR Base */
    int dvr_fd;
    asc_event_t *dvr_event;
    uint8_t dvr_buffer[1022 * TS_PACKET_SIZE];

    uint32_t dvr_read;

    mpegts_psi_t *pat;
    int pat_error;

    /* DMX config */
    bool dmx_budget;

    /* DMX Base */
    char dmx_dev_name[32];
    int *dmx_fd_list;

    int do_bounce;

    /* T2-MI */
    struct
    {
        bool on;
        unsigned pnr;
        unsigned pid;
        unsigned plp;

        mpegts_t2mi_t *ctx;
    } t2mi;

    dvb_fe_t *fe;
    dvb_ca_t *ca;

    ts_callback_t send_ts;
    void *send_arg;
};

#define THREAD_DELAY_FE (1 * 1000 * 1000)
#define THREAD_DELAY_DMX (200 * 1000)
#define THREAD_DELAY_CA (1 * 1000 * 1000)
#define THREAD_DELAY_DVR (2 * 1000 * 1000)

/*
 * ooooooooo  ooooo  oooo oooooooooo
 *  888    88o 888    88   888    888
 *  888    888  888  88    888oooo88
 *  888    888   88888     888  88o
 * o888ooo88      888     o888o  88o8
 *
 */

static void dvr_open(module_data_t *mod);
static void dvr_close(module_data_t *mod);
static void on_thread_close(void *arg);
static void thread_loop(void *arg);
static void thread_loop_slave(void *arg);

static void on_pat(void *arg, mpegts_psi_t *psi)
{
    module_data_t *mod = (module_data_t *)arg;

    if(psi->buffer[0] != 0x00)
        return;

    // check changes
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == psi->crc32)
    {
        mod->pat_error = 0;
        return;
    }

    // check crc
    if(crc32 != PSI_CALC_CRC32(psi))
    {
        if(mod->pat_error >= 3)
        {
            asc_log_error(MSG("dvr checksum error, try to reopen"));
            if(mod->fe->type != DVB_TYPE_UNKNOWN)
                mod->fe->do_retune = 1;
            mod->do_bounce = 1;
            mod->pat_error = 0;
            dvr_close(mod);
            dvr_open(mod);
        }
        else
        {
            mod->pat_error = mod->pat_error + 1;
        }
        return;
    }

    psi->crc32 = crc32;
}

static void dvr_on_retry(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    if(!mod->no_dvr)
    {
        dvr_open(mod);
        if(mod->dvr_fd == 0)
        {
            asc_log_info(MSG("retrying in %d seconds"), DVR_RETRY);
            mod->retry_timer = asc_timer_one_shot(DVR_RETRY * 1000, dvr_on_retry, mod);
            return;
        }
        else
        {
            mod->retry_timer = NULL;
        }
    }

    mod->thread = asc_thread_init(mod);

    thread_callback_t loop;
    if(mod->fe->type != DVB_TYPE_UNKNOWN)
    {
        loop = thread_loop;
    }
    else
    {
        loop = thread_loop_slave;
    }

    asc_thread_start(mod->thread, loop, NULL, NULL, on_thread_close);
    while(!mod->is_thread_started)
        asc_usleep(500);
}

static void dvr_on_error(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;
    asc_log_error(MSG("dvr read error, try to reopen [%s]"), strerror(errno));
    dvr_close(mod);
    dvr_open(mod);
}

static void dvr_on_read(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    const ssize_t len = read(mod->dvr_fd, mod->dvr_buffer, sizeof(mod->dvr_buffer));
    if(len <= 0)
    {
        dvr_on_error(mod);
        return;
    }
    mod->dvr_read += len;

    for(int i = 0; i < len; i += TS_PACKET_SIZE)
    {
        const uint8_t *ts = &mod->dvr_buffer[i];

        if(mod->ca->ca_fd > 0)
            ca_on_ts(mod->ca, ts);

        mod->send_ts(mod->send_arg, ts);

        if(TS_IS_SYNC(ts) && TS_GET_PID(ts) == 0)
            mpegts_psi_mux(mod->pat, ts, on_pat, mod);
    }
}

static void dvr_open(module_data_t *mod)
{
    char dev_name[64];
    snprintf(dev_name, sizeof(dev_name), "/dev/dvb/adapter%d/dvr%d"
             , mod->adapter, mod->device);

    mod->dvr_fd = open(dev_name, O_RDONLY | O_NONBLOCK);
    if(mod->dvr_fd <= 0)
    {
        asc_log_error(MSG("failed to open dvr [%s]"), strerror(errno));
        mod->dvr_fd = 0;

        return;
    }

    if(mod->dvr_buffer_size > 0)
    {
        const uint64_t buffer_size = mod->dvr_buffer_size * 10 * 188 * 1024;
        if(ioctl(mod->dvr_fd, DMX_SET_BUFFER_SIZE, buffer_size) < 0)
        {
            asc_log_error(MSG("DMX_SET_BUFFER_SIZE failed [%s]"), strerror(errno));
            astra_abort();
        }
    }

    mod->dvr_event = asc_event_init(mod->dvr_fd, mod);
    asc_event_set_on_read(mod->dvr_event, dvr_on_read);
    asc_event_set_on_error(mod->dvr_event, dvr_on_error);
}

static void dvr_close(module_data_t *mod)
{
    mod->dvr_read = 0;

    if(mod->dvr_fd == 0)
        return;

    ASC_FREE(mod->dvr_event, asc_event_close);

    close(mod->dvr_fd);
    mod->dvr_fd = 0;
}

/*
 * ooooooooo  ooooooooooo oooo     oooo ooooo  oooo ooooo  oooo
 *  888    88o 888    88   8888o   888   888    88    888  88
 *  888    888 888ooo8     88 888o8 88   888    88      888
 *  888    888 888    oo   88  888  88   888    88     88 888
 * o888ooo88  o888ooo8888 o88o  8  o88o   888oo88   o88o  o888o
 *
 */

static void __dmx_join_pid(module_data_t *mod, int fd, uint16_t pid)
{
    struct dmx_pes_filter_params pes_filter;
    memset(&pes_filter, 0, sizeof(pes_filter));
    pes_filter.pid = pid;
    pes_filter.input = DMX_IN_FRONTEND;
    pes_filter.output = DMX_OUT_TS_TAP;
    pes_filter.pes_type = DMX_PES_OTHER;
    pes_filter.flags = DMX_IMMEDIATE_START;

    if(ioctl(fd, DMX_SET_PES_FILTER, &pes_filter) < 0)
    {
        asc_log_error(MSG("DMX_SET_PES_FILTER failed [%s]"), strerror(errno));
        astra_abort();
    }
}

static int __dmx_open(module_data_t *mod)
{
    const int fd = open(mod->dmx_dev_name, O_WRONLY);
    if(fd <= 0)
    {
        asc_log_error(MSG("failed to open demux [%s]"), strerror(errno));
        astra_abort();
    }

    return fd;
}

static void dmx_set_pid(module_data_t *mod, uint16_t pid, int is_set)
{
    if(mod->dmx_budget)
        return;

    if(pid >= MAX_PID)
    {
        asc_log_error(MSG("demux: PID value must be less then %d"), MAX_PID);
        astra_abort();
    }

    if(!mod->dmx_fd_list)
    {
        asc_log_error(MSG("demux: not initialized"));
        return;
    }

    if(is_set)
    {
        if(!mod->dmx_fd_list[pid])
        {
            mod->dmx_fd_list[pid] = __dmx_open(mod);
            __dmx_join_pid(mod, mod->dmx_fd_list[pid], pid);
        }
    }
    else
    {
        if(mod->dmx_fd_list[pid])
        {
            close(mod->dmx_fd_list[pid]);
            mod->dmx_fd_list[pid] = 0;
        }
    }
}

static void dmx_bounce(module_data_t *mod)
{
    if(!mod->dmx_fd_list)
        return;

    const int fd_max = (mod->dmx_budget) ? 1 : MAX_PID;
    for(int i = 0; i < fd_max; ++i)
    {
        if(mod->dmx_fd_list[i])
        {
            ioctl(mod->dmx_fd_list[i], DMX_STOP);
            ioctl(mod->dmx_fd_list[i], DMX_START);
        }
    }
}

static void dmx_open(module_data_t *mod)
{
    sprintf(mod->dmx_dev_name, "/dev/dvb/adapter%d/demux%d", mod->adapter, mod->device);

    const int fd = __dmx_open(mod);
    if(fd <= 0)
    {
        asc_log_error(MSG("failed to open demux [%s]"), strerror(errno));
        return;
    }

    if(mod->dmx_budget)
    {
        mod->dmx_fd_list = (int *)calloc(1, sizeof(int));
        mod->dmx_fd_list[0] = fd;
        __dmx_join_pid(mod, fd, MAX_PID);
    }
    else
    {
        close(fd);
        mod->dmx_fd_list = (int *)calloc(MAX_PID, sizeof(int));
    }
}

static void dmx_close(module_data_t *mod)
{
    if(!mod->dmx_fd_list)
        return;

    const int fd_max = (mod->dmx_budget) ? 1 : MAX_PID;
    for(int i = 0; i < fd_max; ++i)
    {
        if(mod->dmx_fd_list[i])
            close(mod->dmx_fd_list[i]);
    }
    ASC_FREE(mod->dmx_fd_list, free);
}

/*
 *   ooooooo  oooooooooo  ooooooooooo ooooo  ooooooo  oooo   oooo oooooooo8
 * o888   888o 888    888 88  888  88  888 o888   888o 8888o  88 888
 * 888     888 888oooo88      888      888 888     888 88 888o88  888oooooo
 * 888o   o888 888            888      888 888o   o888 88   8888         888
 *   88ooo88  o888o          o888o    o888o  88ooo88  o88o    88 o88oooo888
 *
 */

static void option_required(module_data_t *mod, const char *name)
{
    asc_log_error(MSG("option '%s' is required"), name);
    astra_abort();
}

static void option_unknown_type(module_data_t *mod, const char *name, const char *value)
{
    asc_log_error(MSG("unknown type of the '%s': %s"), name, value);
    astra_abort();
}

static void module_option_fec(lua_State *L, module_data_t *mod)
{
    const char *string_val;
    static const char __fec[] = "fec";
    if(module_option_string(L, __fec, &string_val, NULL))
    {
        if(!strcasecmp(string_val, "NONE")) mod->fe->fec = FEC_NONE;
        else if(!strcasecmp(string_val, "AUTO")) mod->fe->fec = FEC_AUTO;
        else if(!strcasecmp(string_val, "1/2")) mod->fe->fec = FEC_1_2;
        else if(!strcasecmp(string_val, "2/3")) mod->fe->fec = FEC_2_3;
        else if(!strcasecmp(string_val, "3/4")) mod->fe->fec = FEC_3_4;
        else if(!strcasecmp(string_val, "4/5")) mod->fe->fec = FEC_4_5;
        else if(!strcasecmp(string_val, "5/6")) mod->fe->fec = FEC_5_6;
        else if(!strcasecmp(string_val, "6/7")) mod->fe->fec = FEC_6_7;
        else if(!strcasecmp(string_val, "7/8")) mod->fe->fec = FEC_7_8;
        else if(!strcasecmp(string_val, "8/9")) mod->fe->fec = FEC_8_9;
        else if(!strcasecmp(string_val, "3/5")) mod->fe->fec = FEC_3_5;
        else if(!strcasecmp(string_val, "9/10")) mod->fe->fec = FEC_9_10;
        else
            option_unknown_type(mod, __fec, string_val);
    }
    else
        mod->fe->fec = FEC_AUTO;
}

/*
 * ooooooooo  ooooo  oooo oooooooooo           oooooooo8
 *  888    88o 888    88   888    888         888
 *  888    888  888  88    888oooo88 ooooooooo 888oooooo
 *  888    888   88888     888    888                 888
 * o888ooo88      888     o888ooo888          o88oooo888
 *
 */

static void module_options_s(lua_State *L, module_data_t *mod)
{
    const char *string_val;

    /* Transponder options */
    mod->fe->tone = SEC_TONE_OFF;
    mod->fe->voltage = SEC_VOLTAGE_OFF;

    static const char __polarization[] = "polarization";
    if(!module_option_string(L, __polarization, &string_val, NULL))
        option_required(mod, __polarization);

    const char pol = (string_val[0] > 'Z') ? (string_val[0] - ('z' - 'Z')) : string_val[0];
    if(pol == 'V' || pol == 'R')
        mod->fe->voltage = SEC_VOLTAGE_13;
    else if(pol == 'H' || pol == 'L')
        mod->fe->voltage = SEC_VOLTAGE_18;

    /* LNB options */
    int lof1 = 0, lof2 = 0, slof = 0;

    module_option_integer(L, "lof1", &lof1);
    if(lof1 > 0)
    {
        module_option_integer(L, "lof2", &lof2);
        module_option_integer(L, "slof", &slof);

        if(slof > 0 && lof2 > 0 && mod->fe->frequency >= slof)
        {
            // hiband
            mod->fe->frequency = mod->fe->frequency - lof2;
            mod->fe->tone = SEC_TONE_ON;
        }
        else
        {
            if(mod->fe->frequency < lof1)
                mod->fe->frequency = lof1 - mod->fe->frequency;
            else
                mod->fe->frequency = mod->fe->frequency - lof1;
        }
    }
    else
    {
        if(mod->fe->frequency >= 950 && mod->fe->frequency <= 2150)
            ;
        else if(mod->fe->frequency >= 2500 && mod->fe->frequency <= 2700)
            mod->fe->frequency = 3650 - mod->fe->frequency;
        else if(mod->fe->frequency >= 3400 && mod->fe->frequency <= 4200)
            mod->fe->frequency = 5150 - mod->fe->frequency;
        else if(mod->fe->frequency >= 4500 && mod->fe->frequency <= 4800)
            mod->fe->frequency = 5950 - mod->fe->frequency;
        else if(mod->fe->frequency >= 10700 && mod->fe->frequency < 11700)
            mod->fe->frequency = mod->fe->frequency - 9750;
        else if(mod->fe->frequency >= 11700 && mod->fe->frequency < 13250)
        {
            mod->fe->frequency = mod->fe->frequency - 10600;
            mod->fe->tone = SEC_TONE_ON;
        }
        else
        {
            asc_log_error(MSG("option 'frequency' has wrong value"));
            astra_abort();
        }
    }
    mod->fe->frequency *= 1000;

    static const char __symbolrate[] = "symbolrate";
    if(!module_option_integer(L, __symbolrate, &mod->fe->symbolrate))
        option_required(mod, __symbolrate);
    mod->fe->symbolrate *= 1000;

    bool force_tone = false;
    module_option_boolean(L, "tone", &force_tone);
    if(force_tone)
    {
        mod->fe->tone = SEC_TONE_ON;
    }

    bool lnb_sharing = false;
    module_option_boolean(L, "lnb_sharing", &lnb_sharing);
    if(lnb_sharing)
    {
        mod->fe->tone = SEC_TONE_OFF;
        mod->fe->voltage = SEC_VOLTAGE_OFF;
    }

    module_option_integer(L, "diseqc", &mod->fe->diseqc);

    module_option_integer(L, "uni_frequency", &mod->fe->uni_frequency);
    module_option_integer(L, "uni_scr", &mod->fe->uni_scr);

    static const char __rolloff[] = "rolloff";
    if(module_option_string(L, __rolloff, &string_val, NULL))
    {
        if(!strcasecmp(string_val, "AUTO")) mod->fe->rolloff = ROLLOFF_AUTO;
        else if(!strcasecmp(string_val, "35")) mod->fe->rolloff = ROLLOFF_35;
        else if(!strcasecmp(string_val, "20")) mod->fe->rolloff = ROLLOFF_20;
        else if(!strcasecmp(string_val, "25")) mod->fe->rolloff = ROLLOFF_25;
        else
            option_unknown_type(mod, __rolloff, string_val);
    }
    else
        mod->fe->rolloff = ROLLOFF_35;

    module_option_fec(L, mod);

    mod->fe->stream_id = -1;
    module_option_integer(L, "stream_id", &mod->fe->stream_id);
}

/*
 * ooooooooo  ooooo  oooo oooooooooo       ooooooooooo
 *  888    88o 888    88   888    888      88  888  88
 *  888    888  888  88    888oooo88 ooooooooo 888
 *  888    888   88888     888    888          888
 * o888ooo88      888     o888ooo888          o888o
 *
 */

static void module_options_t(lua_State *L, module_data_t *mod)
{
    const char *string_val;

    if(mod->fe->frequency < 1000)
        mod->fe->frequency *= 1000000;

    static const char __bandwidth[] = "bandwidth";
    if(module_option_string(L, __bandwidth, &string_val, NULL))
    {
        if(!strcasecmp(string_val, "AUTO")) mod->fe->bandwidth = BANDWIDTH_AUTO;
        else if(!strcasecmp(string_val, "8MHZ")) mod->fe->bandwidth = BANDWIDTH_8_MHZ;
        else if(!strcasecmp(string_val, "7MHZ")) mod->fe->bandwidth = BANDWIDTH_7_MHZ;
        else if(!strcasecmp(string_val, "6MHZ")) mod->fe->bandwidth = BANDWIDTH_6_MHZ;
        else
            option_unknown_type(mod, __bandwidth, string_val);
    }
    else
        mod->fe->bandwidth = BANDWIDTH_AUTO;

    static const char __guardinterval[] = "guardinterval";
    if(module_option_string(L, __guardinterval, &string_val, NULL))
    {
        if(!strcasecmp(string_val, "AUTO")) mod->fe->guardinterval = GUARD_INTERVAL_AUTO;
        else if(!strcasecmp(string_val, "1/32")) mod->fe->guardinterval = GUARD_INTERVAL_1_32;
        else if(!strcasecmp(string_val, "1/16")) mod->fe->guardinterval = GUARD_INTERVAL_1_16;
        else if(!strcasecmp(string_val, "1/8")) mod->fe->guardinterval = GUARD_INTERVAL_1_8;
        else if(!strcasestr(string_val, "1/4")) mod->fe->guardinterval = GUARD_INTERVAL_1_4;
        else
            option_unknown_type(mod, __guardinterval, string_val);
    }
    else
        mod->fe->guardinterval = GUARD_INTERVAL_AUTO;

    static const char __transmitmode[] = "transmitmode";
    if(module_option_string(L, __transmitmode, &string_val, NULL))
    {
        if(!strcasecmp(string_val, "AUTO")) mod->fe->transmitmode = TRANSMISSION_MODE_AUTO;
        else if(!strcasecmp(string_val, "2K")) mod->fe->transmitmode = TRANSMISSION_MODE_2K;
        else if(!strcasecmp(string_val, "8K")) mod->fe->transmitmode = TRANSMISSION_MODE_8K;
        else if(!strcasecmp(string_val, "4K")) mod->fe->transmitmode = TRANSMISSION_MODE_4K;
#ifdef HAVE_DVBAPI_T2
        else if(!strcasecmp(string_val, "1K")) mod->fe->transmitmode = TRANSMISSION_MODE_1K;
        else if(!strcasecmp(string_val, "16K")) mod->fe->transmitmode = TRANSMISSION_MODE_16K;
        else if(!strcasecmp(string_val, "32K")) mod->fe->transmitmode = TRANSMISSION_MODE_32K;
#endif /* HAVE_DVBAPI_T2 */
        else
            option_unknown_type(mod, __transmitmode, string_val);
    }
    else
        mod->fe->transmitmode = TRANSMISSION_MODE_AUTO;

    static const char __hierarchy[] = "hierarchy";
    if(module_option_string(L, __hierarchy, &string_val, NULL))
    {
        if(!strcasecmp(string_val, "AUTO")) mod->fe->hierarchy = HIERARCHY_AUTO;
        else if(!strcasecmp(string_val, "NONE")) mod->fe->hierarchy = HIERARCHY_NONE;
        else if(!strcasecmp(string_val, "1")) mod->fe->hierarchy = HIERARCHY_1;
        else if(!strcasecmp(string_val, "2")) mod->fe->hierarchy = HIERARCHY_2;
        else if(!strcasecmp(string_val, "4")) mod->fe->hierarchy = HIERARCHY_4;
        else
            option_unknown_type(mod, __hierarchy, string_val);
    }
    else
        mod->fe->hierarchy = HIERARCHY_AUTO;

    mod->fe->stream_id = -1;
    module_option_integer(L, "stream_id", &mod->fe->stream_id);
}

/*
 * ooooooooo  ooooo  oooo oooooooooo             oooooooo8
 *  888    88o 888    88   888    888          o888     88
 *  888    888  888  88    888oooo88 ooooooooo 888
 *  888    888   88888     888    888          888o     oo
 * o888ooo88      888     o888ooo888            888oooo88
 *
 */


static void module_options_c(lua_State *L, module_data_t *mod)
{
    if(mod->fe->frequency < 1000)
        mod->fe->frequency *= 1000000;

    static const char __symbolrate[] = "symbolrate";
    if(!module_option_integer(L, __symbolrate, &mod->fe->symbolrate))
        option_required(mod, __symbolrate);
    mod->fe->symbolrate *= 1000;

    module_option_fec(L, mod);
}

/*
 * oooooooooo      o       oooooooo8 ooooooooooo
 *  888    888    888     888         888    88
 *  888oooo88    8  88     888oooooo  888ooo8
 *  888    888  8oooo88           888 888    oo
 * o888ooo888 o88o  o888o o88oooo888 o888ooo8888
 *
 */

static void module_options(lua_State *L, module_data_t *mod)
{
    static const char __adapter[] = "adapter";
    if(!module_option_integer(L, __adapter, &mod->adapter))
        option_required(mod, __adapter);
    module_option_integer(L, "device", &mod->device);

    mod->fe->adapter = mod->adapter;
    mod->ca->adapter = mod->adapter;
    mod->fe->device = mod->device;
    mod->ca->device = mod->device;

    const char *string_val = NULL;

    static const char __type[] = "type";
    module_option_string(L, __type, &string_val, NULL);

    if(string_val == NULL)
    {
        ;
    }
    else if(!strcasecmp(string_val, "S"))
    {
        mod->fe->type = DVB_TYPE_S;
        mod->fe->delivery_system = SYS_DVBS;
    }
    else if(!strcasecmp(string_val, "S2"))
    {
        mod->fe->type = DVB_TYPE_S;
        mod->fe->delivery_system = SYS_DVBS2;
    }
    else if(!strcasecmp(string_val, "T"))
    {
        mod->fe->type = DVB_TYPE_T;
        mod->fe->delivery_system = SYS_DVBT;
    }
#ifdef HAVE_DVBAPI_T2
    else if(!strcasecmp(string_val, "T2"))
    {
        mod->fe->type = DVB_TYPE_T;
        mod->fe->delivery_system = SYS_DVBT2;
    }
#endif /* HAVE_DVBAPI_T2 */
    else if(!strcasecmp(string_val, "C"))
    {
        mod->fe->type = DVB_TYPE_C;
        mod->fe->delivery_system = SYS_DVBC_ANNEX_AC;
    }
    else if(!strcasecmp(string_val, "C/AC"))
    {
        mod->fe->type = DVB_TYPE_C;
        mod->fe->delivery_system = SYS_DVBC_ANNEX_AC;
    }
    else if(!strcasecmp(string_val, "C/B"))
    {
        mod->fe->type = DVB_TYPE_C;
        mod->fe->delivery_system = SYS_DVBC_ANNEX_B;
    }
    else if(!strcasecmp(string_val, "C/A"))
    {
        mod->fe->type = DVB_TYPE_C;
        mod->fe->delivery_system = SYS_DVBC_ANNEX_A;
    }
    else if(!strcasecmp(string_val, "C/C"))
    {
        mod->fe->type = DVB_TYPE_C;
        mod->fe->delivery_system = SYS_DVBC_ANNEX_C;
    }
    else if(!strcasecmp(string_val, "ATSC"))
    {
        mod->fe->type = DVB_TYPE_ATSC;
        mod->fe->delivery_system = SYS_ATSC;
    }
    else
        option_unknown_type(mod, __type, string_val);

    static const char __frequency[] = "frequency";
    module_option_integer(L, __frequency, &mod->fe->frequency);
    if(mod->fe->frequency == 0 && mod->fe->type != DVB_TYPE_UNKNOWN)
        option_required(mod, __frequency);

    module_option_boolean(L, "raw_signal", &mod->fe->raw_signal);
    module_option_boolean(L, "budget", &mod->dmx_budget);
    module_option_boolean(L, "log_signal", &mod->fe->log_signal);

    if(mod->fe->type == DVB_TYPE_UNKNOWN)
        module_option_boolean(L, "no_dvr", &mod->no_dvr);

    module_option_integer(L, "buffer_size", &mod->dvr_buffer_size);
    if(mod->dvr_buffer_size > 200)
        asc_log_warning(MSG("buffer_size value is too large"));

    static const char __modulation[] = "modulation";
    if(module_option_string(L, __modulation, &string_val, NULL))
    {
        if(!strcasecmp(string_val, "AUTO")) mod->fe->default_modulation = true;
        else if(!strcasecmp(string_val, "QPSK")) mod->fe->modulation = QPSK;
        else if(!strcasecmp(string_val, "QAM16")) mod->fe->modulation = QAM_16;
        else if(!strcasecmp(string_val, "QAM32")) mod->fe->modulation = QAM_32;
        else if(!strcasecmp(string_val, "QAM64")) mod->fe->modulation = QAM_64;
        else if(!strcasecmp(string_val, "QAM128")) mod->fe->modulation = QAM_128;
        else if(!strcasecmp(string_val, "QAM256")) mod->fe->modulation = QAM_256;
        else if(!strcasecmp(string_val, "QAM")) mod->fe->modulation = QAM_AUTO;
        else if(!strcasecmp(string_val, "VSB8")) mod->fe->modulation = VSB_8;
        else if(!strcasecmp(string_val, "VSB16")) mod->fe->modulation = VSB_16;
        else if(!strcasecmp(string_val, "PSK8")) mod->fe->modulation = PSK_8;
        else if(!strcasecmp(string_val, "APSK16")) mod->fe->modulation = APSK_16;
        else if(!strcasecmp(string_val, "APSK32")) mod->fe->modulation = APSK_32;
        else if(!strcasecmp(string_val, "DQPSK")) mod->fe->modulation = DQPSK;
        else
            option_unknown_type(mod, __modulation, string_val);
    }
    else
        mod->fe->default_modulation = true;

    mod->fe->timeout = 5;
    module_option_integer(L, "timeout", &mod->fe->timeout);

    int ca_pmt_delay = 3;
    module_option_integer(L, "ca_pmt_delay", &ca_pmt_delay);
    if(ca_pmt_delay > 120)
    {
        asc_log_error(MSG("ca_pmt_delay value is too large"));
        astra_abort();
    }
    mod->ca->pmt_delay = ca_pmt_delay * 1000 * 1000;

    module_option_boolean(L, "t2mi", &mod->t2mi.on);
    if (mod->t2mi.on)
    {
        mod->t2mi.plp = T2MI_PLP_AUTO;

        module_option_integer(L, "t2mi_plp", (int *)&mod->t2mi.plp);
        module_option_integer(L, "t2mi_pnr", (int *)&mod->t2mi.pnr);
        module_option_integer(L, "t2mi_pid", (int *)&mod->t2mi.pid);
    }

    switch(mod->fe->type)
    {
        case DVB_TYPE_UNKNOWN:
            break;
        case DVB_TYPE_S:
            module_options_s(L, mod);
            break;
        case DVB_TYPE_T:
            module_options_t(L, mod);
            break;
        case DVB_TYPE_C:
            module_options_c(L, mod);
            break;
        case DVB_TYPE_ATSC:
            if(mod->fe->frequency < 1000)
                mod->fe->frequency *= 1000000;
            break;
        default:
            break;
    }
}

/*
 * ooooooooooo ooooo ooooo oooooooooo  ooooooooooo      o      ooooooooo
 * 88  888  88  888   888   888    888  888    88      888      888    88o
 *     888      888ooo888   888oooo88   888ooo8       8  88     888    888
 *     888      888   888   888  88o    888    oo    8oooo88    888    888
 *    o888o    o888o o888o o888o  88o8 o888ooo8888 o88o  o888o o888ooo88
 *
 */

static void on_thread_close(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    mod->is_thread_started = false;
    ASC_FREE(mod->thread, asc_thread_destroy);
}

static void thread_loop(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    fe_open(mod->fe);
    ca_open(mod->ca);
    dmx_open(mod);

    nfds_t nfds = 0;

    struct pollfd fds[2];
    memset(fds, 0, sizeof(fds));

    fds[nfds].fd = mod->fe->fe_fd;
    fds[nfds].events = POLLIN;
    ++nfds;

    if(mod->ca->ca_fd)
    {
        fds[nfds].fd = mod->ca->ca_fd;
        fds[nfds].events = POLLIN;
        ++nfds;
    }

    mod->is_thread_started = true;

    uint64_t current_time = asc_utime();
    uint64_t fe_check_timeout = current_time;
    uint64_t dmx_check_timeout = current_time;
    uint64_t ca_check_timeout = current_time;
    uint64_t dvr_check_timeout = current_time;

    while(mod->is_thread_started)
    {
        const int ret = poll(fds, nfds, 100);

        if(!mod->is_thread_started)
            break;

        if(ret < 0)
        {
            asc_log_error(MSG("poll() failed [%s]"), strerror(errno));
            astra_abort();
        }

        if(ret > 0)
        {
            if(fds[0].revents)
                fe_loop(mod->fe, fds[0].revents & (POLLPRI | POLLIN));
            if(mod->ca->ca_fd && fds[1].revents)
                ca_loop(mod->ca, fds[1].revents & (POLLPRI | POLLIN));
        }

        current_time = asc_utime();

        if(current_time >= fe_check_timeout + THREAD_DELAY_FE)
        {
            fe_check_timeout = current_time;
            fe_loop(mod->fe, 0);
        }

        if(mod->do_bounce)
        {
            dmx_bounce(mod);
            mod->do_bounce = 0;
        }

        if(!mod->dmx_budget && mod->dmx_fd_list &&
            current_time >= dmx_check_timeout + THREAD_DELAY_DMX)
        {
            dmx_check_timeout = current_time;

            for(int i = 0; i < MAX_PID; ++i)
            {
                if((mod->__stream.pid_list[i] > 0) && (mod->dmx_fd_list[i] == 0))
                    dmx_set_pid(mod, i, 1);
                else if((mod->__stream.pid_list[i] == 0) && (mod->dmx_fd_list[i] > 0))
                    dmx_set_pid(mod, i, 0);
            }
        }

        if(mod->ca->ca_fd > 0 && current_time >= ca_check_timeout + THREAD_DELAY_CA)
        {
            ca_check_timeout = current_time;
            ca_loop(mod->ca, 0);
        }

        if(current_time >= dvr_check_timeout + THREAD_DELAY_DVR)
        {
            dvr_check_timeout = current_time;
            if(mod->fe->status & FE_HAS_LOCK)
            {
                if(mod->dvr_read == 0)
                    dmx_bounce(mod);
                else
                    mod->dvr_read = 0;
            }
        }
    }

    fe_close(mod->fe);
    ca_close(mod->ca);
    dmx_close(mod);
}

static void thread_loop_slave(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    fe_open(mod->fe);
    if(!mod->no_dvr)
    {
        dmx_open(mod);
    }

    mod->is_thread_started = true;

    uint64_t current_time = asc_utime();
    uint64_t fe_check_timeout = current_time;
    uint64_t dmx_check_timeout = current_time;
    uint64_t dvr_check_timeout = current_time;

    while(mod->is_thread_started)
    {
        asc_usleep(100 * 1000);

        if(!mod->is_thread_started)
            break;

        current_time = asc_utime();

        if(current_time >= fe_check_timeout + THREAD_DELAY_FE)
        {
            fe_check_timeout = current_time;
            fe_loop(mod->fe, 0);
        }

        if(mod->no_dvr)
            continue;

        if(mod->do_bounce)
        {
            dmx_bounce(mod);
            mod->do_bounce = 0;
        }

        if(!mod->dmx_budget && mod->dmx_fd_list &&
            current_time >= dmx_check_timeout + THREAD_DELAY_DMX)
        {
            dmx_check_timeout = current_time;

            for(int i = 0; i < MAX_PID; ++i)
            {
                if((mod->__stream.pid_list[i] > 0) && (mod->dmx_fd_list[i] == 0))
                    dmx_set_pid(mod, i, 1);
                else if((mod->__stream.pid_list[i] == 0) && (mod->dmx_fd_list[i] > 0))
                    dmx_set_pid(mod, i, 0);
            }
        }

        if(current_time >= dvr_check_timeout + THREAD_DELAY_DVR)
        {
            dvr_check_timeout = current_time;
            if(mod->fe->status & FE_HAS_LOCK)
            {
                if(mod->dvr_read == 0)
                    dmx_bounce(mod);
                else
                    mod->dvr_read = 0;
            }
        }
    }

    fe_close(mod->fe);
    dmx_close(mod);
}

/*
 * oooo     oooo  ooooooo  ooooooooo  ooooo  oooo ooooo       ooooooooooo
 *  8888o   888 o888   888o 888    88o 888    88   888         888    88
 *  88 888o8 88 888     888 888    888 888    88   888         888ooo8
 *  88  888  88 888o   o888 888    888 888    88   888      o  888    oo
 * o88o  8  o88o  88ooo88  o888ooo88    888oo88   o888ooooo88 o888ooo8888
 *
 */

static void on_status_timer(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    lua_rawgeti(lua, LUA_REGISTRYINDEX, mod->idx_callback);
    lua_newtable(lua);
    lua_pushinteger(lua, mod->fe->status);
    lua_setfield(lua, -2, "status");
    lua_pushinteger(lua, mod->fe->signal);
    lua_setfield(lua, -2, "signal");
    lua_pushinteger(lua, mod->fe->snr);
    lua_setfield(lua, -2, "snr");
    lua_pushinteger(lua, mod->fe->ber);
    lua_setfield(lua, -2, "ber");
    lua_pushinteger(lua, mod->fe->unc);
    lua_setfield(lua, -2, "unc");
    lua_call(lua, 1, 0);
}

static int method_ca_set_pnr(lua_State *L, module_data_t *mod)
{
    if(!mod->ca || !mod->ca->ca_fd)
        return 0;

    const uint16_t pnr = lua_tointeger(L, 2);
    const bool is_set = lua_toboolean(L, 3);
    ((is_set) ? ca_append_pnr : ca_remove_pnr)(mod->ca, pnr);
    return 0;
}

static int method_close(lua_State *L, module_data_t *mod)
{
    if (mod->t2mi.ctx)
    {
        mpegts_t2mi_set_demux(mod->t2mi.ctx, NULL, NULL, NULL);
        ASC_FREE(mod->t2mi.ctx, mpegts_t2mi_destroy);
    }

    dvr_close(mod);
    on_thread_close(mod);

    ASC_FREE(mod->pat, mpegts_psi_destroy);
    ASC_FREE(mod->fe, free);
    ASC_FREE(mod->ca, free);
    ASC_FREE(mod->retry_timer, asc_timer_destroy);
    ASC_FREE(mod->status_timer, asc_timer_destroy);

    if(mod->idx_callback)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, mod->idx_callback);
        mod->idx_callback = 0;
    }

    module_stream_destroy(mod);

    return 0;
}

static void join_pid(void *arg, uint16_t pid)
{
    module_data_t *const mod = (module_data_t *)arg;

    ++mod->__stream.pid_list[pid];
}

static void leave_pid(void *arg, uint16_t pid)
{
    module_data_t *const mod = (module_data_t *)arg;

    --mod->__stream.pid_list[pid];
}

static void module_init(lua_State *L, module_data_t *mod)
{
    module_stream_init(mod, NULL);

    mod->fe = (dvb_fe_t *)calloc(1, sizeof(dvb_fe_t));
    mod->ca = (dvb_ca_t *)calloc(1, sizeof(dvb_ca_t));

    module_options(L, mod);

    if (mod->t2mi.on)
    {
        module_stream_demux_set(mod, NULL, NULL);

        mod->t2mi.ctx = mpegts_t2mi_init();
        mpegts_t2mi_set_fname(mod->t2mi.ctx, "dvb_input %d:%d"
                              , mod->adapter, mod->device);

        mpegts_t2mi_set_demux(mod->t2mi.ctx, mod, join_pid, leave_pid);
        mpegts_t2mi_set_payload(mod->t2mi.ctx, mod->t2mi.pnr, mod->t2mi.pid);
        mpegts_t2mi_set_plp(mod->t2mi.ctx, mod->t2mi.plp);

        /* put received TS through decapsulator */
        mod->send_ts = (ts_callback_t)mpegts_t2mi_decap;
        mod->send_arg = mod->t2mi.ctx;

        mpegts_t2mi_set_callback(mod->t2mi.ctx
                                 , __module_stream_send
                                 , &mod->__stream);
    }
    else
    {
        module_stream_demux_set(mod, join_pid, leave_pid);

        mod->send_ts = __module_stream_send;
        mod->send_arg = &mod->__stream;
    }

    lua_getfield(L, MODULE_OPTIONS_IDX, "callback");
    if(lua_isfunction(L, -1))
    {
        mod->idx_callback = luaL_ref(L, LUA_REGISTRYINDEX);
        mod->status_timer = asc_timer_init(1000, on_status_timer, mod);
    }
    else
        lua_pop(L, 1);

    mod->pat = mpegts_psi_init(MPEGTS_PACKET_PAT, 0);

    dvr_on_retry(mod);
}

static void module_destroy(lua_State *L, module_data_t *mod)
{
    method_close(L, mod);
}

MODULE_STREAM_METHODS()
MODULE_LUA_METHODS()
{
    MODULE_STREAM_METHODS_REF(),
    { "ca_set_pnr", method_ca_set_pnr },
    { "close", method_close },
};
MODULE_LUA_REGISTER(dvb_input)
