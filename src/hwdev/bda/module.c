/*
 * Astra Module: BDA (Lua interface)
 *
 * Copyright (C) 2016-2017, Artem Kharitonov <artem@3phase.pw>
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
 *      dvb_input
 *
 * Module Role:
 *      Source, demux endpoint
 *
 * Module Options:
 *      name        - string, instance identifier for logging
 *      adapter     - number, device index
 *      devpath     - string, unique Windows device path
 *      budget      - boolean, disable PID filter (get whole transponder)
 *      log_signal  - boolean, log signal statistics every second
 *      no_dvr      - boolean, monitoring mode (no tuning or TS reception)
 *      timeout     - number, how long to wait for lock before retuning
 *                      defaults to 5 seconds
 *      diseqc      - table, command sequence to send on tuner init
 *             - OR - number, DiSEqC 1.0 port number (alternate syntax)
 *
 *      *** Following options are also valid for the tune() method:
 *      type        - string, digital network type. supported types:
 *                      atsc, cqam, c, s, s2, t, t2, isdbs, isdbt
 *
 *      frequency   - number, carrier frequency in MHz
 *      symbolrate  - number, symbol rate in KS/s
 *      stream_id   - number, ISI, PLP ID or physical channel number
 *      modulation  - string, modulation type
 *      fec         - string, inner FEC rate
 *      outer_fec   - string, outer FEC rate
 *      fec_mode    - string, inner FEC mode
 *      outer_fec_mode
 *                  - string, outer FEC mode
 *
 *      *** Options specific to ATSC and CQAM:
 *      major_channel
 *                  - number, major channel number
 *      minor_channel
 *                  - number, minor channel number
 *      virtual_channel
 *                  - number, virtual channel number for CQAM
 *      input_type
 *                  - string, tuner input type: cable or antenna
 *      country_code
 *                  - number, country/region code
 *
 *      *** Options specific to DVB-S/S2:
 *      lof1        - number, low oscillator frequency in MHz
 *      lof2        - number, high oscillator frequency in MHz
 *      slof        - number, LNB switch frequency in MHz
 *      polarization
 *                  - string, signal polarization (H, V, L, R)
 *      inversion   - boolean, spectral inversion (or AUTO)
 *      rolloff     - number, DVB-S2 roll-off factor (20, 25, 35)
 *      pilot       - boolean, DVB-S2 pilot mode
 *      pls_code   -  number, Physical Layer Scrambling code
 *      pls_mode   -  number, Physical Layer Scrambling mode
 *
 *      *** Options specific to DVB-T/T2:
 *      bandwidth   - number, signal bandwidth in MHz (normally 6, 7 or 8)
 *      guardinterval
 *                  - string, guard interval
 *      transmitmode
 *                  - string, transmission mode
 *      hierarchy   - number, hierarchy alpha
 *      lp_fec      - string, low-priority stream inner FEC rate
 *      lp_fec_mode - string, low-priority stream inner FEC mode
 *
 * Module Methods:
 *      tune({options})
 *                  - set tuning settings, opening the device if needed
 *      close()
 *                  - stop receiving TS and close the tuner device
 *      ca_set_pnr(pnr, is_set)
 *                  - enable or disable CAM descrambling for a PNR
 *      diseqc({ {cmd1}, {cmd2}, ... })
 *                  - send DiSEqC command sequence when tuner is ready
 *      diseqc(port)
 *                  - set DiSEqC 1.0 port number (alternate syntax)
 *
 * DiSEqC Commands:
 *      data        - string, hex DiSEqC command (6 bytes/12 chars max)
 *      lnbpower    - number/boolean, LNB power setting (true, false, 13, 18)
 *      t22k        - boolean, enable or disable 22kHz tone
 *      toneburst   - number/boolean, mini-DiSEqC port (false, 1-2, A-B)
 *      delay       - number, insert sleep (milliseconds, no more than 500)
 *
 * DiSEqC Examples:
 *      --
 *      -- #1: port number
 *      --
 *
 *      -- set input at module initialization time:
 *      local a = dvb_input({
 *          ...
 *          diseqc = "A",
 *          ...
 *      })
 *
 *      -- change input at run time (restarts tuning process):
 *      a:diseqc(2) -- same as "B"
 *
 *      -- remove port setting and restart tuning:
 *      a:diseqc("auto")
 *
 *      -- remove port setting without restarting:
 *      -- (future tuning attempts will not set DiSEqC port)
 *      a:diseqc()
 *
 *      --
 *      -- #2: command sequence
 *      --
 *      -- 64 commands max. Last used sequence is reissued every time
 *      -- tuning process is restarted. Not all commands are supported
 *      -- by every adapter and OS version.
 *      --
 *      -- 15ms sleep is inserted automatically after each command.
 *      -- Add delay commands if you need longer sleep periods.
 *      --
 *
 *      -- set sequence at module initialization time:
 *      local a = dvb_input({
 *          ...
 *          diseqc = {
 *              { toneburst = "B" },
 *          },
 *          ...
 *      })
 *
 *      -- issue commands at run time:
 *      a:diseqc({
 *          { toneburst = false },
 *          { t22k = false },
 *          { lnbpower = 13 },
 *          { data = "e01038f0" },
 *          { delay = 150 },
 *      })
 *
 *      -- erase stored sequence and restart tuning:
 *      a:diseqc("auto")
 *
 *      -- same, without restarting:
 *      a:diseqc()
 */

#include "bda.h"
#include <astra/core/mainloop.h>
#include <astra/utils/strhex.h>

/* default buffer size, MiB */
#define BDA_BUFFER_SIZE 4

/* default retune timeout, seconds */
#define BDA_RETUNE_TIMEOUT 5

/*
 * BDA thread communication
 */

/* submit command to thread */
static
void graph_submit(module_data_t *mod, const bda_user_cmd_t *cmd)
{
    asc_mutex_lock(&mod->queue_lock);

    bda_user_cmd_t *const item = ASC_ALLOC(1, bda_user_cmd_t);
    memcpy(item, cmd, sizeof(*item));
    asc_list_insert_tail(mod->queue, item);

    asc_mutex_unlock(&mod->queue_lock);
    SetEvent(mod->queue_evt);
}

/* push Lua table containing signal statistics */
static
void push_signal_stats(lua_State *L, module_data_t *mod)
{
    asc_mutex_lock(&mod->signal_lock);
    lua_newtable(L);
    lua_pushboolean(L, mod->signal_stats.present);
    lua_setfield(L, -2, "present");
    lua_pushboolean(L, mod->signal_stats.locked);
    lua_setfield(L, -2, "locked");
    lua_pushinteger(L, mod->signal_stats.strength);
    lua_setfield(L, -2, "strength");
    lua_pushinteger(L, mod->signal_stats.quality);
    lua_setfield(L, -2, "quality");
    asc_mutex_unlock(&mod->signal_lock);
}

/* signal statistics timer callback */
static
void on_stats_timer(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    if (mod->idx_callback != LUA_REFNIL)
    {
        lua_State *const L = module_lua(mod);
        lua_rawgeti(L, LUA_REGISTRYINDEX, mod->idx_callback);

        push_signal_stats(L, mod);
        if (lua_tr_call(L, 1, 0) != 0)
            lua_err_log(L);
    }
}

/* thread exit callback */
static
void on_thread_close(void *arg)
{
    /* shouldn't happen, ever */
    module_data_t *const mod = (module_data_t *)arg;
    asc_log_error(MSG("BUG: BDA thread exited on its own"));
}

/* called when there's packets queued in the ring buffer */
void bda_buffer_pop(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    asc_mutex_lock(&mod->buf.lock);

    const size_t tail = mod->buf.tail = mod->buf.claim;
    const size_t claim = mod->buf.claim = mod->buf.head;
    const unsigned int dropped = mod->buf.dropped;
    mod->buf.dropped = 0;

    asc_mutex_unlock(&mod->buf.lock);

    if (dropped > 0)
    {
        asc_log_error(MSG("dropped %u packets due to buffer overflow")
                      , dropped);
    }

    /* dequeue claimed packets */
    for (size_t i = tail; i != claim; i++, i %= mod->buf.size)
    {
        /*
         * TODO: parse PAT and PMTs for hardware CAMs.
         *
         *       on_pat(): list programs and create PSI objects for req'd PNRs
         *       on_pmt(): send pid list to ctl thread via CA user command
         *
         *       Control thread then talks to CAM via vendor extension.
         */
        module_stream_send(mod, mod->buf.data[i]);
    }
}

/*
 * option parsing
 */

static
BinaryConvolutionCodeRate val_fec(const char *str)
{
    if (!strcmp(str, "1/2"))            return BDA_BCC_RATE_1_2;
    else if (!strcmp(str, "2/3"))       return BDA_BCC_RATE_2_3;
    else if (!strcmp(str, "3/4"))       return BDA_BCC_RATE_3_4;
    else if (!strcmp(str, "3/5"))       return BDA_BCC_RATE_3_5;
    else if (!strcmp(str, "4/5"))       return BDA_BCC_RATE_4_5;
    else if (!strcmp(str, "5/6"))       return BDA_BCC_RATE_5_6;
    else if (!strcmp(str, "5/11"))      return BDA_BCC_RATE_5_11;
    else if (!strcmp(str, "7/8"))       return BDA_BCC_RATE_7_8;
    else if (!strcmp(str, "1/4"))       return BDA_BCC_RATE_1_4;
    else if (!strcmp(str, "1/3"))       return BDA_BCC_RATE_1_3;
    else if (!strcmp(str, "2/5"))       return BDA_BCC_RATE_2_5;
    else if (!strcmp(str, "6/7"))       return BDA_BCC_RATE_6_7;
    else if (!strcmp(str, "8/9"))       return BDA_BCC_RATE_8_9;
    else if (!strcmp(str, "9/10"))      return BDA_BCC_RATE_9_10;
    else if (!strcasecmp(str, "AUTO"))  return BDA_BCC_RATE_NOT_SET;

    return BDA_BCC_RATE_NOT_DEFINED;
}

static
FECMethod val_fec_mode(const char *str)
{
    if (!strcasecmp(str, "Viterbi"))    return BDA_FEC_VITERBI;
    else if (!strcmp(str, "204/188"))   return BDA_FEC_RS_204_188;
    else if (!strcasecmp(str, "LDPC"))  return BDA_FEC_LDPC;
    else if (!strcasecmp(str, "BCH"))   return BDA_FEC_BCH;
    else if (!strcmp(str, "147/130"))   return BDA_FEC_RS_147_130;
    else if (!strcasecmp(str, "AUTO"))  return BDA_FEC_METHOD_NOT_SET;

    return BDA_FEC_METHOD_NOT_DEFINED;
}

static
GuardInterval val_guardinterval(const char *str)
{
    if (!strcmp(str, "1/32"))           return BDA_GUARD_1_32;
    else if (!strcmp(str, "1/16"))      return BDA_GUARD_1_16;
    else if (!strcmp(str, "1/8"))       return BDA_GUARD_1_8;
    else if (!strcmp(str, "1/4"))       return BDA_GUARD_1_4;
    else if (!strcmp(str, "1/128"))     return BDA_GUARD_1_128;
    else if (!strcmp(str, "19/128"))    return BDA_GUARD_19_128;
    else if (!strcmp(str, "19/256"))    return BDA_GUARD_19_256;
    else if (!strcasecmp(str, "AUTO"))  return BDA_GUARD_NOT_SET;

    return BDA_GUARD_NOT_DEFINED;
}

static
HierarchyAlpha val_hierarchy(const char *str)
{
    if (!strcmp(str, "1"))              return BDA_HALPHA_1;
    else if (!strcmp(str, "2"))         return BDA_HALPHA_2;
    else if (!strcmp(str, "4"))         return BDA_HALPHA_4;
    else if (!strcasecmp(str, "AUTO"))  return BDA_HALPHA_NOT_SET;

    return BDA_HALPHA_NOT_DEFINED;
}

static
ModulationType val_modulation(const char *str)
{
    if (!strcasecmp(str, "QAM16"))          return BDA_MOD_16QAM;
    else if (!strcasecmp(str, "QAM32"))     return BDA_MOD_32QAM;
    else if (!strcasecmp(str, "QAM64"))     return BDA_MOD_64QAM;
    else if (!strcasecmp(str, "QAM80"))     return BDA_MOD_80QAM;
    else if (!strcasecmp(str, "QAM96"))     return BDA_MOD_96QAM;
    else if (!strcasecmp(str, "QAM112"))    return BDA_MOD_112QAM;
    else if (!strcasecmp(str, "QAM128"))    return BDA_MOD_128QAM;
    else if (!strcasecmp(str, "QAM160"))    return BDA_MOD_160QAM;
    else if (!strcasecmp(str, "QAM192"))    return BDA_MOD_192QAM;
    else if (!strcasecmp(str, "QAM224"))    return BDA_MOD_224QAM;
    else if (!strcasecmp(str, "QAM256"))    return BDA_MOD_256QAM;
    else if (!strcasecmp(str, "QAM320"))    return BDA_MOD_320QAM;
    else if (!strcasecmp(str, "QAM384"))    return BDA_MOD_384QAM;
    else if (!strcasecmp(str, "QAM448"))    return BDA_MOD_448QAM;
    else if (!strcasecmp(str, "QAM512"))    return BDA_MOD_512QAM;
    else if (!strcasecmp(str, "QAM640"))    return BDA_MOD_640QAM;
    else if (!strcasecmp(str, "QAM768"))    return BDA_MOD_768QAM;
    else if (!strcasecmp(str, "QAM896"))    return BDA_MOD_896QAM;
    else if (!strcasecmp(str, "QAM1024"))   return BDA_MOD_1024QAM;
    else if (!strcasecmp(str, "QPSK"))      return BDA_MOD_QPSK;
    else if (!strcasecmp(str, "BPSK"))      return BDA_MOD_BPSK;
    else if (!strcasecmp(str, "OQPSK"))     return BDA_MOD_OQPSK;
    else if (!strcasecmp(str, "VSB8"))      return BDA_MOD_8VSB;
    else if (!strcasecmp(str, "VSB16"))     return BDA_MOD_16VSB;
    else if (!strcasecmp(str, "PSK8"))      return BDA_MOD_8PSK;
    else if (!strcasecmp(str, "APSK16"))    return BDA_MOD_16APSK;
    else if (!strcasecmp(str, "APSK32"))    return BDA_MOD_32APSK;
    else if (!strcasecmp(str, "NBC-QPSK"))  return BDA_MOD_NBC_QPSK;
    else if (!strcasecmp(str, "NBC-8PSK"))  return BDA_MOD_NBC_8PSK;
    else if (!strcasecmp(str, "TMCC-T"))    return BDA_MOD_ISDB_T_TMCC;
    else if (!strcasecmp(str, "TMCC-S"))    return BDA_MOD_ISDB_S_TMCC;
    else if (!strcasecmp(str, "QAM"))       return BDA_MOD_64QAM;
    else if (!strcasecmp(str, "AUTO"))      return BDA_MOD_NOT_SET;

    return BDA_MOD_NOT_DEFINED;
}

static
LNB_Source val_lnb_source(const char *str)
{
    if (!strcmp(str, "1"))              return BDA_LNB_SOURCE_A;
    else if (!strcasecmp(str, "A"))     return BDA_LNB_SOURCE_A;
    else if (!strcmp(str, "2"))         return BDA_LNB_SOURCE_B;
    else if (!strcasecmp(str, "B"))     return BDA_LNB_SOURCE_B;
    else if (!strcmp(str, "3"))         return BDA_LNB_SOURCE_C;
    else if (!strcasecmp(str, "C"))     return BDA_LNB_SOURCE_C;
    else if (!strcmp(str, "4"))         return BDA_LNB_SOURCE_D;
    else if (!strcasecmp(str, "D"))     return BDA_LNB_SOURCE_D;
    else if (!strcasecmp(str, "AUTO"))  return BDA_LNB_SOURCE_NOT_SET;

    return BDA_LNB_SOURCE_NOT_DEFINED;
}

static
Polarisation val_polarization(const char *str)
{
    if (!strcasecmp(str, "H"))          return BDA_POLARISATION_LINEAR_H;
    else if (!strcasecmp(str, "V"))     return BDA_POLARISATION_LINEAR_V;
    else if (!strcasecmp(str, "L"))     return BDA_POLARISATION_CIRCULAR_L;
    else if (!strcasecmp(str, "R"))     return BDA_POLARISATION_CIRCULAR_R;
    else if (!strcasecmp(str, "AUTO"))  return BDA_POLARISATION_NOT_SET;

    return BDA_POLARISATION_NOT_DEFINED;
}

static
SpectralInversion val_inversion(const char *str)
{
    if (!strcasecmp(str, "true"))       return BDA_SPECTRAL_INVERSION_INVERTED;
    else if (!strcasecmp(str, "false")) return BDA_SPECTRAL_INVERSION_NORMAL;
    else if (!strcasecmp(str, "AUTO"))
    {
        /*
         * NOTE: unlike other enumeration types in here, this one has an
         *       explicit auto setting.
         */
        return BDA_SPECTRAL_INVERSION_AUTOMATIC;
    }

    return BDA_SPECTRAL_INVERSION_NOT_DEFINED;
}

static
TransmissionMode val_transmitmode(const char *str)
{
    if (!strcasecmp(str, "2K"))         return BDA_XMIT_MODE_2K;
    else if (!strcasecmp(str, "8K"))    return BDA_XMIT_MODE_8K;
    else if (!strcasecmp(str, "4K"))    return BDA_XMIT_MODE_4K;
    else if (!strcasecmp(str, "2KI"))   return BDA_XMIT_MODE_2K_INTERLEAVED;
    else if (!strcasecmp(str, "4KI"))   return BDA_XMIT_MODE_4K_INTERLEAVED;
    else if (!strcasecmp(str, "1K"))    return BDA_XMIT_MODE_1K;
    else if (!strcasecmp(str, "16K"))   return BDA_XMIT_MODE_16K;
    else if (!strcasecmp(str, "32K"))   return BDA_XMIT_MODE_32K;
    else if (!strcasecmp(str, "AUTO"))  return BDA_XMIT_MODE_NOT_SET;

    return BDA_XMIT_MODE_NOT_DEFINED;
}

static
RollOff val_rolloff(const char *str)
{
    if (!strcmp(str, "20"))             return BDA_ROLL_OFF_20;
    else if (!strcmp(str, "25"))        return BDA_ROLL_OFF_25;
    else if (!strcmp(str, "35"))        return BDA_ROLL_OFF_35;
    else if (!strcasecmp(str, "AUTO"))  return BDA_ROLL_OFF_NOT_SET;

    return BDA_ROLL_OFF_NOT_DEFINED;
}

static
Pilot val_pilot(const char *str)
{
    if (!strcasecmp(str, "true"))       return BDA_PILOT_ON;
    else if (!strcasecmp(str, "false")) return BDA_PILOT_OFF;
    else if (!strcasecmp(str, "AUTO"))  return BDA_PILOT_NOT_SET;

    return BDA_PILOT_NOT_DEFINED;
}

static
bda_lnbpower_mode_t val_lnbpower(const char *str)
{
    if (!strcasecmp(str, "true"))       return BDA_EXT_LNBPOWER_ON;
    else if (!strcasecmp(str, "false")) return BDA_EXT_LNBPOWER_OFF;
    else if (!strcmp(str, "13"))        return BDA_EXT_LNBPOWER_13V;
    else if (!strcmp(str, "18"))        return BDA_EXT_LNBPOWER_18V;
    else if (!strcasecmp(str, "AUTO"))  return BDA_EXT_LNBPOWER_NOT_SET;

    return BDA_EXT_LNBPOWER_NOT_DEFINED;
}

static
bda_22k_mode_t val_t22k(const char *str)
{
    if (!strcasecmp(str, "true"))       return BDA_EXT_22K_ON;
    else if (!strcasecmp(str, "false")) return BDA_EXT_22K_OFF;
    else if (!strcasecmp(str, "AUTO"))  return BDA_EXT_22K_NOT_SET;

    return BDA_EXT_22K_NOT_DEFINED;
}

static
bda_toneburst_mode_t val_toneburst(const char *str)
{
    if (!strcasecmp(str, "false"))      return BDA_EXT_TONEBURST_OFF;
    else if (!strcmp(str, "1"))         return BDA_EXT_TONEBURST_UNMODULATED;
    else if (!strcasecmp(str, "A"))     return BDA_EXT_TONEBURST_UNMODULATED;
    else if (!strcmp(str, "2"))         return BDA_EXT_TONEBURST_MODULATED;
    else if (!strcasecmp(str, "B"))     return BDA_EXT_TONEBURST_MODULATED;
    else if (!strcasecmp(str, "AUTO"))  return BDA_EXT_TONEBURST_NOT_SET;

    return BDA_EXT_TONEBURST_NOT_DEFINED;
}

/* parse Lua table at stack index 2 containing tuning data */
static
void parse_tune_options(lua_State *L, const module_data_t *mod
                        , bda_tune_cmd_t *tune)
{
    /* get network type */
    const char *opt = NULL;
    if (!module_option_string(L, "type", &opt, NULL) || opt == NULL)
        luaL_error(L, MSG("option 'type' is required"));

    for (size_t i = 0; bda_network_list[i] != NULL; i++)
    {
        const bda_network_t *const net = bda_network_list[i];

        for (size_t j = 0; j < ASC_ARRAY_SIZE(net->name); j++)
        {
            if (net->name[j] == NULL)
                break;

            if (!strcasecmp(opt, net->name[j]))
                tune->net = net;
        }

        if (tune->net != NULL)
            break;
    }

    if (tune->net == NULL)
        luaL_error(L, MSG("unknown network type '%s'"), opt);

    /*
     * generic settings
     */

    /* frequency: carrier frequency of the RF signal, kHz */
    tune->frequency = -1;
    if (module_option_integer(L, "frequency", &tune->frequency))
    {
        if (tune->frequency <= 0)
            luaL_error(L, MSG("frequency must be greater than zero"));

        if (tune->frequency <= 100000) /* MHz */
            tune->frequency *= 1000;
        else if (tune->frequency >= 1000000) /* Hz */
            tune->frequency /= 1000;
    }

    /* symbolrate: symbol rate, symbols per second */
    tune->symbolrate = -1;
    if (module_option_integer(L, "symbolrate", &tune->symbolrate))
    {
        if (tune->symbolrate <= 0)
            luaL_error(L, MSG("symbol rate must be greater than zero"));

        if (tune->symbolrate < 1000000) /* KS/s */
            tune->symbolrate *= 1000;
    }

    /* stream_id: ISI, PLP ID or physical channel number */
    tune->stream_id = -1;
    if (module_option_integer(L, "stream_id", &tune->stream_id))
    {
        if (tune->stream_id < 0)
            luaL_error(L, MSG("stream ID can't be negative"));
    }

    /* modulation: modulation type */
    tune->modulation = BDA_MOD_NOT_SET;
    if (module_option_string(L, "modulation", &opt, NULL))
    {
        tune->modulation = val_modulation(opt);
        if (tune->modulation == BDA_MOD_NOT_DEFINED)
            luaL_error(L, MSG("invalid modulation: '%s'"), opt);
    }

    /* fec: inner FEC rate */
    tune->fec = BDA_BCC_RATE_NOT_SET;
    if (module_option_string(L, "fec", &opt, NULL))
    {
        tune->fec = val_fec(opt);
        if (tune->fec == BDA_BCC_RATE_NOT_DEFINED)
            luaL_error(L, MSG("invalid inner FEC rate: '%s'"), opt);
    }

    /* outer_fec: outer FEC rate */
    tune->outer_fec = BDA_BCC_RATE_NOT_SET;
    if (module_option_string(L, "outer_fec", &opt, NULL))
    {
        tune->outer_fec = val_fec(opt);
        if (tune->outer_fec == BDA_BCC_RATE_NOT_DEFINED)
            luaL_error(L, MSG("invalid outer FEC rate: '%s'"), opt);
    }

    /* fec_mode: inner FEC mode */
    tune->fec_mode = BDA_FEC_METHOD_NOT_SET;
    if (module_option_string(L, "fec_mode", &opt, NULL))
    {
        tune->fec_mode = val_fec_mode(opt);
        if (tune->fec_mode == BDA_FEC_METHOD_NOT_DEFINED)
            luaL_error(L, MSG("invalid inner FEC mode: '%s'"), opt);
    }

    /* outer_fec_mode: outer FEC mode */
    tune->outer_fec_mode = BDA_FEC_METHOD_NOT_SET;
    if (module_option_string(L, "outer_fec_mode", &opt, NULL))
    {
        tune->outer_fec_mode = val_fec_mode(opt);
        if (tune->outer_fec_mode == BDA_FEC_METHOD_NOT_DEFINED)
            luaL_error(L, MSG("invalid outer FEC mode: '%s'"), opt);
    }

    /*
     * ATSC and CQAM
     */

    /* major_channel: major channel number */
    tune->major_channel = -1;
    if (module_option_integer(L, "major_channel", &tune->major_channel))
    {
        if (tune->major_channel < 0)
            luaL_error(L, MSG("major channel can't be negative"));
    }

    /* minor_channel: minor channel number */
    tune->minor_channel = -1;
    if (module_option_integer(L, "minor_channel", &tune->minor_channel))
    {
        if (tune->minor_channel < 0)
            luaL_error(L, MSG("minor channel can't be negative"));
    }

    /* virtual_channel: virtual channel number for CQAM */
    tune->virtual_channel = -1;
    if (module_option_integer(L, "virtual_channel", &tune->virtual_channel))
    {
        if (tune->virtual_channel < 0)
            luaL_error(L, MSG("virtual channel can't be negative"));
    }

    /* input_type: tuner input type */
    tune->input_type = TunerInputCable;
    if (module_option_string(L, "input_type", &opt, NULL))
    {
        if (!strcasecmp(opt, "cable"))
            ; /* do nothing */
        else if (!strcasecmp(opt, "antenna"))
            tune->input_type = TunerInputAntenna;
        else
            luaL_error(L, MSG("invalid input type: '%s'"), opt);
    }

    /* country_code: country/region code */
    tune->country_code = -1;
    if (module_option_integer(L, "country_code", &tune->country_code))
    {
        if (tune->country_code < 0)
            luaL_error(L, MSG("country code can't be negative"));
    }

    /*
     * DVB-S
     */

    /* lof1: low oscillator frequency, kHz */
    tune->lof1 = -1;
    if (module_option_integer(L, "lof1", &tune->lof1))
    {
        if (tune->lof1 <= 0)
            luaL_error(L, MSG("LO frequency must be greater than zero"));

        if (tune->lof1 <= 100000) /* MHz */
            tune->lof1 *= 1000;
    }

    /* lof2: high oscillator frequency, kHz */
    tune->lof2 = -1;
    if (module_option_integer(L, "lof2", &tune->lof2))
    {
        if (tune->lof2 <= 0)
            luaL_error(L, MSG("LO frequency must be greater than zero"));

        if (tune->lof2 <= 100000) /* MHz */
            tune->lof2 *= 1000;
    }

    /* slof: LNB switch frequency, kHz */
    tune->slof = -1;
    if (module_option_integer(L, "slof", &tune->slof))
    {
        if (tune->slof <= 0)
            luaL_error(L, MSG("LNB switch freq must be greater than zero"));

        if (tune->slof <= 100000) /* MHz */
            tune->slof *= 1000;
    }

    /* lnb_source: DiSEqC input source (simple) */
    tune->lnb_source = BDA_LNB_SOURCE_NOT_SET;
    /* NOTE: this is filled in by control thread */

    /* polarization: signal polarization */
    tune->polarization = BDA_POLARISATION_NOT_SET;
    if (module_option_string(L, "polarization", &opt, NULL))
    {
        tune->polarization = val_polarization(opt);
        if (tune->polarization == BDA_POLARISATION_NOT_DEFINED)
            luaL_error(L, MSG("invalid polarization: '%s'"), opt);
    }

    /* inversion: spectral inversion */
    tune->inversion = BDA_SPECTRAL_INVERSION_NOT_SET;
    if (module_option_string(L, "inversion", &opt, NULL))
    {
        tune->inversion = val_inversion(opt);
        if (tune->inversion == BDA_SPECTRAL_INVERSION_NOT_DEFINED)
            luaL_error(L, MSG("invalid inversion setting: '%s'"), opt);
    }

    /* rolloff: DVB-S2 roll-off factor */
    tune->rolloff = BDA_ROLL_OFF_NOT_SET;
    if (module_option_string(L, "rolloff", &opt, NULL))
    {
        tune->rolloff = val_rolloff(opt);
        if (tune->rolloff == BDA_ROLL_OFF_NOT_DEFINED)
            luaL_error(L, MSG("invalid roll-off setting: '%s'"), opt);
    }

    /* pilot: DVB-S2 pilot mode */
    tune->pilot = BDA_PILOT_NOT_SET;
    if (module_option_string(L, "pilot", &opt, NULL))
    {
        tune->pilot = val_pilot(opt);
        if (tune->pilot == BDA_PILOT_NOT_DEFINED)
            luaL_error(L, MSG("invalid pilot setting: '%s'"), opt);
    }

    /* pls_code: Physical Layer Scrambling code */
    tune->pls_code = -1;
    if (module_option_integer(L, "pls_code", &tune->pls_code))
    {
        if (tune->pls_code < 0 || tune->pls_code > 262143)
            luaL_error(L, MSG("PLS code must be 0-262143"));
    }

    /* pls_mode: Physical Layer Scrambling mode */
    tune->pls_mode = -1;
    if (module_option_integer(L, "pls_mode", &tune->pls_mode))
    {
        if (tune->pls_mode < 0 || tune->pls_mode > 2)
            luaL_error(L, MSG("PLS mode must be 0-2"));
    }

    /*
     * DVB-T
     */

    /* bandwidth: signal bandwidth, MHz */
    tune->bandwidth = -1;
    if (module_option_string(L, "bandwidth", &opt, NULL)
        && strcasecmp(opt, "AUTO"))
    {
        tune->bandwidth = atoi(opt);
        if (tune->bandwidth <= 0)
            luaL_error(L, MSG("bandwidth must be greater than zero"));
    }

    /* guardinterval: guard interval */
    tune->guardinterval = BDA_GUARD_NOT_SET;
    if (module_option_string(L, "guardinterval", &opt, NULL))
    {
        tune->guardinterval = val_guardinterval(opt);
        if (tune->guardinterval == BDA_GUARD_NOT_DEFINED)
            luaL_error(L, MSG("invalid guard interval: '%s'"), opt);
    }

    /* transmitmode: transmission mode */
    tune->transmitmode = BDA_XMIT_MODE_NOT_SET;
    if (module_option_string(L, "transmitmode", &opt, NULL))
    {
        tune->transmitmode = val_transmitmode(opt);
        if (tune->transmitmode == BDA_XMIT_MODE_NOT_DEFINED)
            luaL_error(L, MSG("invalid transmission mode: '%s'"), opt);
    }

    /* hierarchy: hierarchy alpha */
    tune->hierarchy = BDA_HALPHA_NOT_SET;
    if (module_option_string(L, "hierarchy", &opt, NULL))
    {
        tune->hierarchy = val_hierarchy(opt);
        if (tune->hierarchy == BDA_HALPHA_NOT_DEFINED)
            luaL_error(L, MSG("invalid hierarchy alpha setting: '%s'"), opt);
    }

    /* lp_fec: low-priority stream inner FEC rate */
    tune->lp_fec = BDA_BCC_RATE_NOT_SET;
    if (module_option_string(L, "lp_fec", &opt, NULL))
    {
        tune->lp_fec = val_fec(opt);
        if (tune->lp_fec == BDA_BCC_RATE_NOT_DEFINED)
            luaL_error(L, MSG("invalid LP inner FEC rate: '%s'"), opt);
    }

    /* lp_fec_mode: low-priority stream inner FEC mode */
    tune->lp_fec_mode = BDA_FEC_METHOD_NOT_SET;
    if (module_option_string(L, "lp_fec_mode", &opt, NULL))
    {
        tune->lp_fec_mode = val_fec_mode(opt);
        if (tune->lp_fec_mode == BDA_FEC_METHOD_NOT_DEFINED)
            luaL_error(L, MSG("invalid LP inner FEC mode: '%s'"), opt);
    }
}

/* parse Lua table at stack index 2 containing DiSEqC command */
static
void parse_diseqc_options(lua_State *L, const module_data_t *mod
                          , bda_diseqc_seq_t *seq)
{
    /* data: hex DiSEqC command */
    const char *opt = NULL;
    size_t opt_len = 0;

    if (module_option_string(L, "data", &opt, &opt_len))
    {
        if (opt_len % 2)
            luaL_error(L, MSG("command must have an even number of digits"));
        else if (opt_len < 2 || opt_len > (BDA_DISEQC_LEN * 2))
            luaL_error(L, MSG("command must be 1 to 6 bytes long"));

        au_str2hex(opt, seq->data, sizeof(seq->data));
        seq->data_len = opt_len / 2;
    }

    /* lnbpower: LNB power setting */
    seq->lnbpower = BDA_EXT_LNBPOWER_NOT_SET;
    if (module_option_string(L, "lnbpower", &opt, NULL))
    {
        seq->lnbpower = val_lnbpower(opt);
        if (seq->lnbpower == BDA_EXT_LNBPOWER_NOT_DEFINED)
            luaL_error(L, MSG("invalid LNB power setting: '%s'"), opt);
    }

    /* t22k: enable or disable 22kHz tone */
    seq->t22k = BDA_EXT_22K_NOT_SET;
    if (module_option_string(L, "t22k", &opt, NULL))
    {
        seq->t22k = val_t22k(opt);
        if (seq->t22k == BDA_EXT_22K_NOT_DEFINED)
            luaL_error(L, MSG("invalid 22kHz tone setting: '%s'"), opt);
    }

    /* toneburst: mini-DiSEqC port */
    seq->toneburst = BDA_EXT_TONEBURST_NOT_SET;
    if (module_option_string(L, "toneburst", &opt, NULL))
    {
        seq->toneburst = val_toneburst(opt);
        if (seq->toneburst == BDA_EXT_TONEBURST_NOT_DEFINED)
            luaL_error(L, MSG("invalid mini-DiSEqC port: '%s'"), opt);
    }

    /* delay: insert sleep */
    if (module_option_integer(L, "delay", &seq->delay))
    {
        if (seq->delay < 0 || seq->delay > 500)
            luaL_error(L, MSG("delay must be 0-500 ms"));
    }
}

/*
 * module methods
 */

static
int method_tune(lua_State *L, module_data_t *mod)
{
    /* fix up Lua stack for option getters */
    if (lua_gettop(L) < MODULE_OPTIONS_IDX)
    {
        lua_pushnil(L);
        lua_insert(L, 1);
    }
    luaL_checktype(L, MODULE_OPTIONS_IDX, LUA_TTABLE);

    /* generate tuning command */
    bda_user_cmd_t cmd =
    {
        .cmd = BDA_COMMAND_TUNE,
    };

    parse_tune_options(L, mod, &cmd.tune);
    graph_submit(mod, &cmd);

    return 0;
}

static
int method_close(lua_State *L, module_data_t *mod)
{
    __uarg(L);

    const bda_user_cmd_t cmd =
    {
        .cmd = BDA_COMMAND_CLOSE,
    };
    graph_submit(mod, &cmd);

    return 0;
}

static
int method_ca(lua_State *L, module_data_t *mod)
{
    luaL_checktype(L, -1, LUA_TBOOLEAN);
    const bool enable = lua_toboolean(L, -1);
    const int pnr = luaL_checkinteger(L, -2);

    if (pnr < 1 || pnr >= TS_MAX_PNR)
        luaL_error(L, MSG("program number %d out of range"), pnr);

    const bda_user_cmd_t cmd =
    {
        .ca = {
            .cmd = BDA_COMMAND_CA,
            .enable = enable,
            .pnr = pnr,
        },
    };
    graph_submit(mod, &cmd);

    return 0;
}

static
int method_diseqc(lua_State *L, module_data_t *mod)
{
    if (lua_gettop(L) < 2)
    {
        /* called with no arguments */
        lua_pushnil(L);
    }

    bda_user_cmd_t cmd =
    {
        .diseqc = {
            .cmd = BDA_COMMAND_DISEQC,
            .port = BDA_LNB_SOURCE_NOT_DEFINED,
        },
    };

    if (lua_istable(L, -1))
    {
        /* a:diseqc({{...},{...}}) */
        unsigned int size = 0;

        lua_foreach(L, -2)
        {
            if (!lua_istable(L, -1))
                luaL_error(L, MSG("invalid format for DiSEqC sequence"));
            else if (size >= ASC_ARRAY_SIZE(cmd.diseqc.seq))
                luaL_error(L, MSG("DiSEqC sequence is too long"));

            lua_insert(L, MODULE_OPTIONS_IDX);

            parse_diseqc_options(L, mod, &cmd.diseqc.seq[size]);
            size++;

            lua_pushvalue(L, MODULE_OPTIONS_IDX);
            lua_remove(L, MODULE_OPTIONS_IDX);
        }

        cmd.diseqc.seq_size = size;
    }
    else if (!lua_isnil(L, -1))
    {
        /* a:diseqc(n) */
        const char *const str = luaL_checkstring(L, -1);
        const LNB_Source val = val_lnb_source(str);

        if (val == BDA_LNB_SOURCE_NOT_DEFINED)
            luaL_error(L, MSG("invalid DiSEqC port number: '%s'"), str);

        cmd.diseqc.port = val;
    }

    graph_submit(mod, &cmd);

    return 0;
}

static
int method_stats(lua_State *L, module_data_t *mod)
{
    push_signal_stats(L, mod);
    return 1;
}

/*
 * demux control
 */

static
void set_pid(module_data_t *mod, uint16_t pid, bool join)
{
    const bda_user_cmd_t cmd =
    {
        .demux = {
            .cmd = BDA_COMMAND_DEMUX,
            .join = join,
            .pid = pid,
        },
    };

    graph_submit(mod, &cmd);
}

static
void join_pid(module_data_t *mod, uint16_t pid)
{
    if (!module_demux_check(mod, pid))
        set_pid(mod, pid, true);

    module_demux_join(mod, pid);
}

static
void leave_pid(module_data_t *mod, uint16_t pid)
{
    module_demux_leave(mod, pid);

    if (!module_demux_check(mod, pid))
        set_pid(mod, pid, false);
}

/*
 * module init/destroy
 */

static
void module_init(lua_State *L, module_data_t *mod)
{
    mod->idx_callback = LUA_REFNIL;

    /* create command queue */
    asc_mutex_init(&mod->buf.lock);
    asc_mutex_init(&mod->signal_lock);
    asc_mutex_init(&mod->queue_lock);
    mod->queue = asc_list_init();
    mod->extensions = asc_list_init();

    asc_wake_open();

    mod->queue_evt = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (mod->queue_evt == NULL)
    {
        luaL_error(L, "[dvb_input] CreateEvent() failed: %s"
                   , asc_error_msg());
    }

    /* get instance name */
    module_option_string(L, "name", &mod->name, NULL);
    if (mod->name == NULL)
        luaL_error(L, "[dvb_input] option 'name' is required");

    /* get device identifier */
    mod->adapter = -1;
    if (module_option_integer(L, "adapter", &mod->adapter))
    {
        /* device index */
        if (mod->adapter < 0)
            luaL_error(L, MSG("adapter number can't be negative"));
    }
    else if (module_option_string(L, "devpath", &mod->devpath, NULL))
    {
        /* unique device path */
        if (strlen(mod->devpath) == 0)
            luaL_error(L, MSG("device path can't be empty"));
    }
    else
    {
        luaL_error(L, MSG("either adapter or devpath must be set"));
    }

    /* get signal stats callback */
    lua_getfield(L, MODULE_OPTIONS_IDX, "callback");
    if (!lua_isnil(L, -1))
    {
        luaL_checktype(L, -1, LUA_TFUNCTION);

        mod->idx_callback = luaL_ref(L, LUA_REGISTRYINDEX);
        mod->stats_timer = asc_timer_init(1000, on_stats_timer, mod);
    }
    else
    {
        lua_pop(L, 1);
    }

    /* create TS buffer */
    mod->buffer_size = BDA_BUFFER_SIZE;
    module_option_integer(L, "buffer_size", &mod->buffer_size);
    if (mod->buffer_size < 1 || mod->buffer_size > 1024)
        luaL_error(L, MSG("buffer size out of range"));

    mod->buf.size = (mod->buffer_size * 1024UL * 1024UL) / TS_PACKET_SIZE;
    asc_assert(mod->buf.size > 0, MSG("invalid buffer size"));

    mod->buf.data = ASC_ALLOC(mod->buf.size, ts_packet_t);

    /* miscellaneous options */
    module_option_boolean(L, "budget", &mod->budget);
    module_option_boolean(L, "debug", &mod->debug);
    module_option_boolean(L, "log_signal", &mod->log_signal);
    module_option_boolean(L, "no_dvr", &mod->no_dvr);

    mod->timeout = BDA_RETUNE_TIMEOUT;
    module_option_integer(L, "timeout", &mod->timeout);
    if (mod->timeout < 1)
        luaL_error(L, MSG("retune timeout can't be less than a second"));

    /* send diseqc first to avoid tuning twice */
    lua_getfield(L, MODULE_OPTIONS_IDX, "diseqc");
    method_diseqc(L, mod);
    lua_pop(L, 1);

    /* send initial tuning data */
    method_tune(L, mod);

    /* start dedicated thread for BDA graph */
    module_stream_init(L, mod, NULL);
    module_demux_set(mod, join_pid, leave_pid);

    mod->thr = asc_thread_init();
    asc_thread_start(mod->thr, mod, bda_graph_loop, on_thread_close);
}

static
void module_destroy(module_data_t *mod)
{
    if (mod->thr != NULL)
    {
        const bda_user_cmd_t cmd =
        {
            .cmd = BDA_COMMAND_QUIT,
        };
        graph_submit(mod, &cmd);

        ASC_FREE(mod->thr, asc_thread_join);
    }

    if (mod->idx_callback != LUA_REFNIL)
    {
        luaL_unref(module_lua(mod), LUA_REGISTRYINDEX, mod->idx_callback);
        mod->idx_callback = LUA_REFNIL;
    }

    if (mod->extensions != NULL)
    {
        asc_list_clear(mod->extensions)
        {
            asc_log_warning(MSG("BUG: expected extension list to be empty"));
        }

        asc_list_destroy(mod->extensions);
    }

    if (mod->queue != NULL)
    {
        /* clean up leftovers from failed module init */
        const size_t cnt = asc_list_size(mod->queue);
        if (cnt > 0)
            asc_log_debug(MSG("cleaning up %zu stale user commands"), cnt);

        asc_list_clear(mod->queue)
        {
            free(asc_list_data(mod->queue));
        }

        asc_list_destroy(mod->queue);
    }

    asc_wake_close();
    asc_job_prune(mod);

    ASC_FREE(mod->buf.data, free);
    ASC_FREE(mod->stats_timer, asc_timer_destroy);
    ASC_FREE(mod->queue_evt, CloseHandle);

    asc_mutex_destroy(&mod->queue_lock);
    asc_mutex_destroy(&mod->signal_lock);
    asc_mutex_destroy(&mod->buf.lock);

    module_stream_destroy(mod);
}

static
const module_method_t module_methods[] =
{
    { "tune", method_tune },
    { "close", method_close },
    { "ca_set_pnr", method_ca },
    { "diseqc", method_diseqc },
    { "stats", method_stats },
    { NULL, NULL },
};

STREAM_MODULE_REGISTER(dvb_input)
{
    .init = module_init,
    .destroy = module_destroy,
    .methods = module_methods,
};
