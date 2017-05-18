/*
 * Astra Module: IT95x (Option parsing)
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

#include "it95x.h"

/*
 * string to enum and vice versa
 */

static
it95x_coderate_t val_coderate(const char *str)
{
    if (!strcmp(str, "1/2"))            return IT95X_CODERATE_1_2;
    else if (!strcmp(str, "2/3"))       return IT95X_CODERATE_2_3;
    else if (!strcmp(str, "3/4"))       return IT95X_CODERATE_3_4;
    else if (!strcmp(str, "5/6"))       return IT95X_CODERATE_5_6;
    else if (!strcmp(str, "7/8"))       return IT95X_CODERATE_7_8;

    return IT95X_CODERATE_UNKNOWN;
}

static
const char *str_coderate(it95x_coderate_t val)
{
    switch (val)
    {
        case IT95X_CODERATE_1_2:        return "1/2";
        case IT95X_CODERATE_2_3:        return "2/3";
        case IT95X_CODERATE_3_4:        return "3/4";
        case IT95X_CODERATE_5_6:        return "5/6";
        case IT95X_CODERATE_7_8:        return "7/8";
        default:
            return NULL;
    }
}

static
it95x_constellation_t val_constellation(const char *str)
{
    if (!strcasecmp(str, "QPSK"))       return IT95X_CONSTELLATION_QPSK;
    else if (!strcasecmp(str, "16QAM")) return IT95X_CONSTELLATION_16QAM;
    else if (!strcasecmp(str, "64QAM")) return IT95X_CONSTELLATION_64QAM;

    return IT95X_CONSTELLATION_UNKNOWN;
}

static
const char *str_constellation(it95x_constellation_t val)
{
    switch (val)
    {
        case IT95X_CONSTELLATION_QPSK:  return "QPSK";
        case IT95X_CONSTELLATION_16QAM: return "16QAM";
        case IT95X_CONSTELLATION_64QAM: return "64QAM";
        default:
            return NULL;
    }
}

static
it95x_tx_mode_t val_tx_mode(const char *str)
{
    if (!strcasecmp(str, "2K"))         return IT95X_TX_MODE_2K;
    else if (!strcasecmp(str, "8K"))    return IT95X_TX_MODE_8K;
    else if (!strcasecmp(str, "4K"))    return IT95X_TX_MODE_4K;

    return IT95X_TX_MODE_UNKNOWN;
}

static
const char *str_tx_mode(it95x_tx_mode_t val)
{
    switch (val)
    {
        case IT95X_TX_MODE_2K:          return "2K";
        case IT95X_TX_MODE_8K:          return "8K";
        case IT95X_TX_MODE_4K:          return "4K";
        default:
            return NULL;
    }
}

static
it95x_guardinterval_t val_guardinterval(const char *str)
{
    if (!strcmp(str, "1/32"))           return IT95X_GUARD_1_32;
    else if (!strcmp(str, "1/16"))      return IT95X_GUARD_1_16;
    else if (!strcmp(str, "1/8"))       return IT95X_GUARD_1_8;
    else if (!strcmp(str, "1/4"))       return IT95X_GUARD_1_4;

    return IT95X_GUARD_UNKNOWN;
}

static
const char *str_guardinterval(it95x_guardinterval_t val)
{
    switch (val)
    {
        case IT95X_GUARD_1_32:          return "1/32";
        case IT95X_GUARD_1_16:          return "1/16";
        case IT95X_GUARD_1_8:           return "1/8";
        case IT95X_GUARD_1_4:           return "1/4";
        default:
            return NULL;
    }
}

static
it95x_pcr_mode_t val_pcr_mode(const char *str)
{
    if (!strcasecmp(str, "false"))      return IT95X_PCR_DISABLED;
    else if (!strcasecmp(str, "none"))  return IT95X_PCR_DISABLED;
    else if (!strcmp(str, "0"))         return IT95X_PCR_DISABLED;
    else if (!strcmp(str, "1"))         return IT95X_PCR_MODE1;
    else if (!strcmp(str, "2"))         return IT95X_PCR_MODE2;
    else if (!strcmp(str, "3"))         return IT95X_PCR_MODE3;

    return IT95X_PCR_UNKNOWN;
}

static
const char *str_pcr_mode(it95x_pcr_mode_t val)
{
    switch (val)
    {
        case IT95X_PCR_DISABLED:        return "none";
        case IT95X_PCR_MODE1:           return "1";
        case IT95X_PCR_MODE2:           return "2";
        case IT95X_PCR_MODE3:           return "3";
        default:
            return NULL;
    }
}

static
it95x_system_t val_system(const char *str)
{
    if (!strcasecmp(str, "DVBT"))       return IT95X_SYSTEM_DVBT;
    else if (!strcasecmp(str, "ISDBT")) return IT95X_SYSTEM_ISDBT;

    return IT95X_SYSTEM_UNKNOWN;
}

static
const char *str_system(it95x_system_t val)
{
    switch (val)
    {
        case IT95X_SYSTEM_DVBT:         return "DVBT";
        case IT95X_SYSTEM_ISDBT:        return "ISDBT";
        default:
            return NULL;
    }
}

static
it95x_layer_t val_layer(const char *str)
{
    if (!strcasecmp(str, "false"))      return IT95X_LAYER_NONE;
    else if (!strcasecmp(str, "none"))  return IT95X_LAYER_NONE;
    else if (!strcasecmp(str, "B"))     return IT95X_LAYER_B;
    else if (!strcasecmp(str, "A"))     return IT95X_LAYER_A;
    else if (!strcasecmp(str, "AB"))    return IT95X_LAYER_AB;

    return IT95X_LAYER_UNKNOWN;
}

static
const char *str_layer(it95x_layer_t val)
{
    switch (val)
    {
        case IT95X_LAYER_NONE:          return "none";
        case IT95X_LAYER_B:             return "B";
        case IT95X_LAYER_A:             return "A";
        case IT95X_LAYER_AB:            return "AB";
        default:
            return NULL;
    }
}

static
it95x_sysid_t val_sysid(const char *str)
{
    if (!strcasecmp(str, "ARIB-STD-B31"))   return IT95X_SYSID_ARIB_STD_B31;
    else if (!strcasecmp(str, "ISDB-TSB"))  return IT95X_SYSID_ISDB_TSB;

    return IT95X_SYSID_UNKNOWN;
}

static
const char *str_sysid(it95x_sysid_t val)
{
    switch (val)
    {
        case IT95X_SYSID_ARIB_STD_B31:  return "ARIB-STD-B31";
        case IT95X_SYSID_ISDB_TSB:      return "ISDB-TSB";
        default:
            return NULL;
    }
}

/*
 * configuration parsing
 */

static
void parse_dvbt(lua_State *L, module_data_t *mod)
{
    it95x_dvbt_t *const dvbt = &mod->dvbt;
    it95x_tps_t *const tps = &mod->tps;

    const char *sopt;
    int ret, iopt;

    /*
     * coderate: FEC code rate
     */
    if (module_option_string(L, "coderate", &sopt, NULL))
    {
        dvbt->coderate = val_coderate(sopt);
        if (dvbt->coderate == IT95X_CODERATE_UNKNOWN)
            luaL_error(L, MSG("invalid code rate: '%s'"), sopt);
    }
    else
    {
        luaL_error(L, MSG("option 'coderate' is required"));
    }

    /*
     * tx_mode: transmission mode
     */
    if (module_option_string(L, "tx_mode", &sopt, NULL))
    {
        dvbt->tx_mode = val_tx_mode(sopt);
        if (dvbt->tx_mode == IT95X_TX_MODE_UNKNOWN)
            luaL_error(L, MSG("invalid transmission mode: '%s'"), sopt);
        else if (dvbt->tx_mode == IT95X_TX_MODE_4K)
            luaL_error(L, MSG("TX mode '4K' is invalid for DVB-T"));
    }
    else
    {
        luaL_error(L, MSG("option 'tx_mode' is required"));
    }

    /*
     * constellation: modulation constellation
     */
    if (module_option_string(L, "constellation", &sopt, NULL))
    {
        dvbt->constellation = val_constellation(sopt);
        if (dvbt->constellation == IT95X_CONSTELLATION_UNKNOWN)
            luaL_error(L, MSG("invalid constellation: '%s'"), sopt);
    }
    else
    {
        luaL_error(L, MSG("option 'constellation' is required"));
    }

    /*
     * guardinterval: guard interval
     */
    if (module_option_string(L, "guardinterval", &sopt, NULL))
    {
        dvbt->guardinterval = val_guardinterval(sopt);
        if (dvbt->guardinterval == IT95X_GUARD_UNKNOWN)
            luaL_error(L, MSG("invalid guard interval: '%s'"), sopt);
    }
    else
    {
        luaL_error(L, MSG("option 'guardinterval' is required"));
    }

    /*
     * cell_id: DVB-T TPS cell ID
     */
    tps->cell_id = 0;
    if (module_option_integer(L, "cell_id", &iopt))
    {
        if (iopt < 0 || iopt > UINT16_MAX)
            luaL_error(L, MSG("invalid TPS cell ID: '%d'"), iopt);

        tps->cell_id = iopt;
    }

    /*
     * tps_crypt: TPS encryption key
     */
    mod->tps_crypt = 0;
    if (module_option_string(L, "tps_crypt", &sopt, NULL))
        mod->tps_crypt = atol(sopt);

    /* copy modulation settings to TPS */
    tps->high_coderate = tps->low_coderate = dvbt->coderate;
    tps->tx_mode = dvbt->tx_mode;
    tps->constellation = dvbt->constellation;
    tps->guardinterval = dvbt->guardinterval;

    /* calculate channel bitrate */
    memset(mod->bitrate, 0, sizeof(mod->bitrate));
    ret = it95x_bitrate_dvbt(mod->bandwidth, dvbt, &mod->bitrate[0]);

    if (ret != 0)
    {
        char *const err = it95x_strerror(ret);
        lua_pushfstring(L, MSG("failed to calculate bitrate: %s"), err);
        free(err);
        lua_error(L);
    }
}

static
void parse_pid_list(lua_State *L, module_data_t *mod)
{
    luaL_checktype(L, -1, LUA_TTABLE);
    lua_foreach(L, -2)
    {
        /* item structure: { <pid>, <layer> } */
        if (!lua_istable(L, -1) || luaL_len(L, -1) != 2)
        {
            luaL_error(L, MSG("invalid format for PID list"));
        }
        else if (mod->pid_cnt >= ASC_ARRAY_SIZE(mod->pid_list))
        {
            luaL_error(L, MSG("PID list is too large"));
        }

        /* add PID entry */
        it95x_pid_t *const pid = &mod->pid_list[mod->pid_cnt++];

        /* t[1]: pid */
        lua_rawgeti(L, -1, 1);
        const int iopt = luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        if (!ts_pid_valid(iopt))
            luaL_error(L, MSG("PID out of range: '%d'"), iopt);

        pid->pid = iopt;

        /* t[2]: layer */
        lua_rawgeti(L, -1, 2);
        const char *const sopt = luaL_checkstring(L, -1);

        pid->layer = val_layer(sopt);
        if (pid->layer == IT95X_LAYER_UNKNOWN)
            luaL_error(L, MSG("invalid layer for PID %d: '%s'"), iopt, sopt);

        lua_pop(L, 1);
    }
}

static
void parse_isdbt(lua_State *L, module_data_t *mod)
{
    it95x_isdbt_t *const isdbt = &mod->isdbt;
    it95x_tmcc_t *const tmcc = &mod->tmcc;

    const char *sopt;
    int ret;

    /*
     * tx_mode: transmission mode
     */
    if (module_option_string(L, "tx_mode", &sopt, NULL))
    {
        isdbt->tx_mode = val_tx_mode(sopt);
        if (isdbt->tx_mode == IT95X_TX_MODE_UNKNOWN)
            luaL_error(L, MSG("invalid transmission mode: '%s'"), sopt);
    }
    else
    {
        luaL_error(L, MSG("option 'tx_mode' is required"));
    }

    /*
     * guardinterval: guard interval
     */
    if (module_option_string(L, "guardinterval", &sopt, NULL))
    {
        isdbt->guardinterval = val_guardinterval(sopt);
        if (isdbt->guardinterval == IT95X_GUARD_UNKNOWN)
            luaL_error(L, MSG("invalid guard interval: '%s'"), sopt);
    }
    else
    {
        luaL_error(L, MSG("option 'guardinterval' is required"));
    }

    /*
     * coderate: FEC code rate for layer A
     */
    if (module_option_string(L, "coderate", &sopt, NULL))
    {
        isdbt->a.coderate = val_coderate(sopt);
        if (isdbt->a.coderate == IT95X_CODERATE_UNKNOWN)
            luaL_error(L, MSG("invalid layer A code rate: '%s'"), sopt);
    }
    else
    {
        luaL_error(L, MSG("option 'coderate' is required"));
    }

    /*
     * constellation: modulation constellation for layer A
     */
    if (module_option_string(L, "constellation", &sopt, NULL))
    {
        isdbt->a.constellation = val_constellation(sopt);
        if (isdbt->a.constellation == IT95X_CONSTELLATION_UNKNOWN)
            luaL_error(L, MSG("invalid layer A constellation: '%s'"), sopt);
    }
    else
    {
        luaL_error(L, MSG("option 'constellation' is required"));
    }

    /*
     * partial: partial reception (12+1 segments)
     */
    isdbt->partial = false;
    module_option_boolean(L, "partial", &isdbt->partial);

    if (isdbt->partial)
    {
        /*
         * b_coderate: FEC code rate for layer B
         */
        if (module_option_string(L, "b_coderate", &sopt, NULL))
        {
            isdbt->b.coderate = val_coderate(sopt);
            if (isdbt->b.coderate == IT95X_CODERATE_UNKNOWN)
                luaL_error(L, MSG("invalid layer B code rate: '%s'"), sopt);
        }
        else
        {
            luaL_error(L, MSG("option 'b_coderate' is required"));
        }

        /*
         * b_constellation: modulation constellation for layer B
         */
        if (module_option_string(L, "b_constellation", &sopt, NULL))
        {
            isdbt->b.constellation = val_constellation(sopt);
            if (isdbt->b.constellation == IT95X_CONSTELLATION_UNKNOWN)
            {
                luaL_error(L, MSG("invalid layer B constellation: '%s'")
                           , sopt);
            }
        }
        else
        {
            luaL_error(L, MSG("option 'b_constellation' is required"));
        }

        /*
         * pid_list: PID filter list
         */
        memset(mod->pid_list, 0, sizeof(mod->pid_list));
        mod->pid_cnt = 0;

        lua_getfield(L, MODULE_OPTIONS_IDX, "pid_list");
        if (!lua_isnil(L, -1))
            parse_pid_list(L, mod);

        lua_pop(L, 1);

        if (mod->pid_cnt == 0)
        {
            luaL_error(L, MSG("PID list cannot be empty when partial "
                              "reception is enabled"));
        }

        /*
         * pid_layer: layer(s) to enable PID filtering for
         */
        mod->pid_layer = IT95X_LAYER_NONE;
        if (module_option_string(L, "pid_layer", &sopt, NULL))
        {
            mod->pid_layer = val_layer(sopt);
            if (mod->pid_layer == IT95X_LAYER_UNKNOWN)
            {
                luaL_error(L, MSG("invalid PID filter layer setting: '%s'")
                           , sopt);
            }
            else if (mod->pid_layer == IT95X_LAYER_NONE)
            {
                luaL_error(L, MSG("cannot disable PID filter while partial "
                                  "reception is enabled"));
            }
        }
        else
        {
            luaL_error(L, MSG("option 'pid_layer' is required"));
        }
    }
    else
    {
        /* everything goes to layer A */
        isdbt->b = isdbt->a;

        memset(mod->pid_list, 0, sizeof(mod->pid_list));
        mod->pid_cnt = 0;
        mod->pid_layer = IT95X_LAYER_NONE;
    }

    /*
     * sysid: system identification
     */
    tmcc->sysid = IT95X_SYSID_ARIB_STD_B31;
    if (module_option_string(L, "sysid", &sopt, NULL))
    {
        tmcc->sysid = val_sysid(sopt);
        if (tmcc->sysid == IT95X_SYSID_UNKNOWN)
            luaL_error(L, MSG("invalid system ID: '%s'"), sopt);
    }

    /* copy modulation settings to TMCC */
    tmcc->partial = isdbt->partial;
    tmcc->a = isdbt->a;
    tmcc->b = isdbt->b;

    /* calculate channel bitrate */
    memset(mod->bitrate, 0, sizeof(mod->bitrate));
    ret = it95x_bitrate_isdbt(mod->bandwidth, isdbt
                              , &mod->bitrate[0], &mod->bitrate[1]);

    if (ret != 0)
    {
        char *const err = it95x_strerror(ret);
        lua_pushfstring(L, MSG("failed to calculate bitrate: %s"), err);
        free(err);
        lua_error(L);
    }
}

static
void parse_iq_table(lua_State *L, module_data_t *mod)
{
    luaL_checktype(L, -1, LUA_TTABLE);
    lua_foreach(L, -2)
    {
        /* item structure: { <freq>, <amp>, <phi> } */
        if (!lua_istable(L, -1) || luaL_len(L, -1) != 3)
        {
            luaL_error(L, MSG("invalid format for I/Q calibration table"));
        }
        else if (mod->iq_size >= ASC_ARRAY_SIZE(mod->iq_table))
        {
            luaL_error(L, MSG("I/Q calibration table is too large"));
        }

        /* add table entry */
        it95x_iq_t *const iq = &mod->iq_table[mod->iq_size++];

        /* t[1]: frequency */
        lua_rawgeti(L, -1, 1);
        const int opt = luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        if (opt < 0)
            luaL_error(L, MSG("invalid frequency for I/Q table: '%d'"), opt);

        iq->frequency = opt;
        if (iq->frequency <= 3000) /* MHz */
            iq->frequency *= 1000;
        else if (iq->frequency >= 3000000) /* Hz */
            iq->frequency /= 1000;

        /* t[2]: amp */
        lua_rawgeti(L, -1, 2);
        iq->amp = luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        /* t[3]: phi */
        lua_rawgeti(L, -1, 3);
        iq->phi = luaL_checkinteger(L, -1);
        lua_pop(L, 1);
    }
}

void it95x_parse_opts(lua_State *L, module_data_t *mod)
{
    const char *sopt;
    int iopt;

    /*
     * frequency: carrier frequency of the RF signal, kHz
     */
    if (!module_option_integer(L, "frequency", &iopt))
        luaL_error(L, MSG("option 'frequency' is required"));
    else if (iopt <= 0)
        luaL_error(L, MSG("invalid carrier frequency: '%d'"), iopt);

    mod->frequency = iopt;
    if (mod->frequency <= 3000) /* MHz */
        mod->frequency *= 1000;
    else if (mod->frequency >= 3000000) /* Hz */
        mod->frequency /= 1000;

    if (mod->frequency < 30000 || mod->frequency > 3000000)
    {
        luaL_error(L, MSG("carrier frequency out of range: %f kHz")
                   , (lua_Number)mod->frequency);
    }

    /*
     * bandwidth: channel bandwidth, kHz
     */
    if (!module_option_integer(L, "bandwidth", &iopt))
        luaL_error(L, MSG("option 'bandwidth' is required"));
    else if (iopt <= 0)
        luaL_error(L, MSG("invalid channel bandwidth: '%d'"), iopt);

    mod->bandwidth = iopt;
    if (mod->bandwidth <= 15) /* MHz */
        mod->bandwidth *= 1000;
    else if (mod->bandwidth >= 15000) /* Hz */
        mod->bandwidth /= 1000;

    if (mod->bandwidth < 1000 || mod->bandwidth > 15000)
    {
        luaL_error(L, MSG("channel bandwidth out of range: %f kHz")
                   , (lua_Number)mod->bandwidth);
    }

    /*
     * gain: gain or attenuation value, dB
     */
    mod->gain = 0;
    if (module_option_integer(L, "gain", &iopt))
    {
        if (iopt < INT8_MIN || iopt > INT8_MAX)
            luaL_error(L, MSG("invalid gain value: '%d'"), iopt);

        mod->gain = iopt;
    }

    /*
     * dc_i, dc_q: DC offset compensation for I/Q
     */
    mod->dc_i = 0;
    if (module_option_integer(L, "dc_i", &iopt))
    {
        if (iopt < INT8_MIN || iopt > INT8_MAX)
            luaL_error(L, MSG("invalid DC calibration value: '%d'"), iopt);

        mod->dc_i = iopt;
    }

    mod->dc_q = 0;
    if (module_option_integer(L, "dc_q", &iopt))
    {
        if (iopt < INT8_MIN || iopt > INT8_MAX)
            luaL_error(L, MSG("invalid DC calibration value: '%d'"), iopt);

        mod->dc_q = iopt;
    }

    /*
     * ofs_i, ofs_q: OFS calibration values for I/Q
     */
    mod->ofs_i = 0;
    if (module_option_integer(L, "ofs_i", &iopt))
    {
        if (iopt < 0 || iopt > UINT8_MAX)
            luaL_error(L, MSG("invalid OFS calibration value: '%d'"), iopt);

        mod->ofs_i = iopt;
    }

    mod->ofs_q = 0;
    if (module_option_integer(L, "ofs_q", &iopt))
    {
        if (iopt < 0 || iopt > UINT8_MAX)
            luaL_error(L, MSG("invalid OFS calibration value: '%d'"), iopt);

        mod->ofs_q = iopt;
    }

    /*
     * iq_table: I/Q calibration table
     */
    memset(mod->iq_table, 0, sizeof(mod->iq_table));
    mod->iq_size = 0;

    lua_getfield(L, MODULE_OPTIONS_IDX, "iq_table");
    if (!lua_isnil(L, -1))
        parse_iq_table(L, mod);

    lua_pop(L, 1);

    /*
     * pcr_mode: PCR restamping mode
     */
    mod->pcr_mode = IT95X_PCR_DISABLED;
    if (module_option_string(L, "pcr_mode", &sopt, NULL))
    {
        mod->pcr_mode = val_pcr_mode(sopt);
        if (mod->pcr_mode == IT95X_PCR_UNKNOWN)
            luaL_error(L, MSG("invalid PCR restamping mode: '%s'"), sopt);
    }

    /*
     * system: delivery system
     */
    mod->system = IT95X_SYSTEM_DVBT;
    if (module_option_string(L, "system", &sopt, NULL))
        mod->system = val_system(sopt);

    switch (mod->system)
    {
        case IT95X_SYSTEM_DVBT:  parse_dvbt(L, mod);  break;
        case IT95X_SYSTEM_ISDBT: parse_isdbt(L, mod); break;
        default:
            luaL_error(L, MSG("invalid delivery system: '%s'"), sopt);
    }
}

/*
 * configuration dumping
 */

#define CFG_DUMP(_indent, ...) \
    do { \
        cfg_dump(mod, _indent, __VA_ARGS__); \
    } while (0)

static __asc_printf(3, 4)
void cfg_dump(const module_data_t *mod, size_t indent, const char *fmt, ...)
{
    char buf[128] = { '\0' };
    char spaces[indent + 1];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    spaces[indent] = '\0';
    for (size_t i = 0; i < indent; i++)
        spaces[i] = ' ';

    asc_log_debug(MSG("%s%s"), spaces, buf);
}

static
void dump_dvbt(const module_data_t *mod)
{
    const it95x_dvbt_t *const dvbt = &mod->dvbt;
    const it95x_tps_t *const tps = &mod->tps;

    CFG_DUMP(2, "begin DVB-T modulation parameters");
    CFG_DUMP(4, "code rate: %s (%d)"
             , str_coderate(dvbt->coderate), dvbt->coderate);
    CFG_DUMP(4, "transmission mode: %s (%d)"
             , str_tx_mode(dvbt->tx_mode), dvbt->tx_mode);
    CFG_DUMP(4, "constellation: %s (%d)"
             , str_constellation(dvbt->constellation), dvbt->constellation);
    CFG_DUMP(4, "guard interval: %s (%d)"
             , str_guardinterval(dvbt->guardinterval), dvbt->guardinterval);
    CFG_DUMP(4, "channel bitrate: %lu bps", (long)mod->bitrate[0]);
    CFG_DUMP(2, "end DVB-T modulation parameters");

    CFG_DUMP(2, "begin DVB-T TPS parameters");
    CFG_DUMP(4, "high code rate: %s (%d)"
             , str_coderate(tps->high_coderate), tps->high_coderate);
    CFG_DUMP(4, "low code rate: %s (%d)"
             , str_coderate(tps->low_coderate), tps->low_coderate);
    CFG_DUMP(4, "transmission mode: %s (%d)"
             , str_tx_mode(tps->tx_mode), tps->tx_mode);
    CFG_DUMP(4, "constellation: %s (%d)"
             , str_constellation(tps->constellation), tps->constellation);
    CFG_DUMP(4, "guard interval: %s (%d)"
             , str_guardinterval(tps->guardinterval), tps->guardinterval);
    CFG_DUMP(4, "cell ID: %hu (0x%04hx)", tps->cell_id, tps->cell_id);
    CFG_DUMP(2, "end DVB-T TPS parameters");

    if (mod->tps_crypt != 0)
    {
        CFG_DUMP(2, "TPS encryption key: %lu (0x%08lx)"
                 , (long)mod->tps_crypt, (long)mod->tps_crypt);
    }
    else
    {
        CFG_DUMP(2, "TPS encryption is disabled");
    }
}

static
void dump_isdbt(const module_data_t *mod)
{
    const it95x_isdbt_t *const isdbt = &mod->isdbt;
    const it95x_tmcc_t *const tmcc = &mod->tmcc;

    CFG_DUMP(2, "begin ISDB-T modulation parameters");
    CFG_DUMP(4, "transmission mode: %s (%d)"
             , str_tx_mode(isdbt->tx_mode), isdbt->tx_mode);
    CFG_DUMP(4, "guard interval: %s (%d)"
             , str_guardinterval(isdbt->guardinterval), isdbt->guardinterval);
    CFG_DUMP(4, "partial reception: %s"
             , (isdbt->partial ? "enabled" : "disabled"));

    if (isdbt->partial)
    {
        CFG_DUMP(4, "code rate for layer A: %s (%d)"
                 , str_coderate(isdbt->a.coderate), isdbt->a.coderate);
        CFG_DUMP(4, "code rate for layer B: %s (%d)"
                 , str_coderate(isdbt->b.coderate), isdbt->b.coderate);

        CFG_DUMP(4, "constellation for layer A: %s (%d)"
                 , str_constellation(isdbt->a.constellation)
                 , isdbt->a.constellation);
        CFG_DUMP(4, "constellation for layer B: %s (%d)"
                 , str_constellation(isdbt->b.constellation)
                 , isdbt->b.constellation);

        CFG_DUMP(4, "PID filter layer setting: %s (%d)"
                 , str_layer(mod->pid_layer), mod->pid_layer);
        CFG_DUMP(4, "begin PID filter list (%zu/%u entries)"
                 , mod->pid_cnt, IT95X_PID_LIST_SIZE);

        for (size_t i = 0; i < mod->pid_cnt; i++)
        {
            const it95x_pid_t *const pid = &mod->pid_list[i];
            CFG_DUMP(6, "index: %zu, pid: %hu (0x%04hx), layer: %s (%d)"
                     , i + 1, pid->pid, pid->pid, str_layer(pid->layer)
                     , pid->layer);
        }

        CFG_DUMP(4, "end PID filter list");

        CFG_DUMP(4, "channel bitrate for layer A (1-segment): %lu bps"
                 , (long)mod->bitrate[0]);
        CFG_DUMP(4, "channel bitrate for layer B (12-segment): %lu bps"
                 , (long)mod->bitrate[1]);
    }
    else
    {
        CFG_DUMP(4, "code rate: %s (%d)"
                 , str_coderate(isdbt->a.coderate), isdbt->a.coderate);
        CFG_DUMP(4, "constellation: %s (%d)"
                 , str_constellation(isdbt->a.constellation)
                 , isdbt->a.constellation);
        CFG_DUMP(4, "channel bitrate (13-segment): %lu bps"
                 , (long)mod->bitrate[0]);
    }

    CFG_DUMP(2, "end ISDB-T modulation parameters");

    CFG_DUMP(2, "begin ISDB-T TMCC parameters");
    CFG_DUMP(4, "system identification: %s (%d)"
             , str_sysid(tmcc->sysid), tmcc->sysid);
    CFG_DUMP(4, "partial reception: %s"
             , (tmcc->partial ? "enabled" : "disabled"));

    if (tmcc->partial)
    {
        CFG_DUMP(4, "code rate for layer A: %s (%d)"
                 , str_coderate(tmcc->a.coderate), tmcc->a.coderate);
        CFG_DUMP(4, "code rate for layer B: %s (%d)"
                 , str_coderate(tmcc->b.coderate), tmcc->b.coderate);

        CFG_DUMP(4, "constellation for layer A: %s (%d)"
                 , str_constellation(tmcc->a.constellation)
                 , tmcc->a.constellation);
        CFG_DUMP(4, "constellation for layer B: %s (%d)"
                 , str_constellation(tmcc->b.constellation)
                 , tmcc->b.constellation);
    }
    else
    {
        CFG_DUMP(4, "code rate: %s (%d)"
                 , str_coderate(tmcc->a.coderate), tmcc->a.coderate);
        CFG_DUMP(4, "constellation: %s (%d)"
                 , str_constellation(tmcc->a.constellation)
                 , tmcc->a.constellation);
    }

    CFG_DUMP(2, "end ISDB-T TMCC parameters");
}

void it95x_dump_opts(const module_data_t *mod)
{
    CFG_DUMP(0, "begin configuration dump");
    CFG_DUMP(2, "delivery system: %s", str_system(mod->system));
    CFG_DUMP(2, "carrier frequency: %lu kHz", (long)mod->frequency);
    CFG_DUMP(2, "channel bandwidth: %lu kHz", (long)mod->bandwidth);
    CFG_DUMP(2, "gain: %d dB", mod->gain);
    CFG_DUMP(2, "DC compensation for I/Q: %d/%d", mod->dc_i, mod->dc_q);
    CFG_DUMP(2, "OFS calibration for I/Q: %u/%u", mod->ofs_i, mod->ofs_q);
    CFG_DUMP(2, "PCR restamping mode: %s (%d)"
             , str_pcr_mode(mod->pcr_mode), mod->pcr_mode);

    if (mod->iq_size > 0)
    {
        CFG_DUMP(2, "begin I/Q calibration table (%zu/%u entries)"
                 , mod->iq_size, IT95X_IQ_TABLE_SIZE);

        for (size_t i = 0; i < mod->iq_size; i++)
        {
            const it95x_iq_t *const iq = &mod->iq_table[i];
            CFG_DUMP(4, "frequency: %lu kHz, amp: %d, phi: %d"
                     , (long)iq->frequency, iq->amp, iq->phi);
        }

        CFG_DUMP(2, "end I/Q calibration table");
    }
    else
    {
        CFG_DUMP(2, "I/Q calibration table is not configured");
    }

    switch (mod->system)
    {
        case IT95X_SYSTEM_DVBT:  dump_dvbt(mod);  break;
        case IT95X_SYSTEM_ISDBT: dump_isdbt(mod); break;
        default:
            CFG_DUMP(2, "unknown delivery system");
            break;
    }

    CFG_DUMP(0, "end configuration dump");
}
