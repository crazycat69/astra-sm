/*
 * Astra Module: IT95x (Lua interface)
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
 *      it95x_output
 *
 * Module Role:
 *      Sink, no demux
 *
 * Module Options:
 *      upstream    - object, stream module instance
 *      name        - string, instance identifier for logging
 *      adapter     - number, device index
 *      devpath     - string, unique OS-specific device path
 *      sync        - boolean, enable or disable buffering (default is true)
 *      sync_opts   - string, sync buffer options
 *
 *      frequency   - number, carrier frequency in kHz
 *      bandwidth   - number, channel bandwidth in kHz
 *      coderate    - string, FEC code rate (1/2, 2/3, 3/4, 5/6, 7/8)
 *      tx_mode     - string, transmission mode (2K, 8K, 4K)
 *                      NOTE: 4K is only supported for ISDB-T
 *      constellation
 *                  - string, modulation constellation (QPSK, 16QAM, 64QAM)
 *      guardinterval
 *                  - string, guard interval (1/32, 1/16, 1/8, 1/4)
 *      cell_id     - number, DVB-T TPS cell ID (default is 0)
 *      gain        - number, gain or attenuation in dB
 *      dc_i, dc_q  - number, DC offset compensation for I/Q
 *      iq_table    - table, I/Q calibration table
 *
 *      *** Following options are only supported by IT9517:
 *      system      - string, delivery system ("DVBT" or "ISDBT")
 *                      default is "DVBT"
 *      b_coderate  - string, FEC code rate for ISDB-T layer B
 *      b_constellation
 *                  - string, constellation for ISDB-T layer B
 *      sysid       - string, ISDB-T system identification
 *                      supported values: "ARIB-STD-B31" (default), "ISDB-TSB"
 *      partial     - boolean, enable ISDB-T partial reception
 *      pid_list    - table, PID filter list for partial reception
 *      pid_layer   - string, enable PID filtering for these layers
 *                      supported values: "false" (default), "A", "B", "AB"
 *      ofs_i, ofs_q
 *                  - number, OFS calibration values for I/Q
 *      pcr_mode    - number, PCR restamping mode (1-3, 0 = disable)
 *      tps_crypt   - number, TPS encryption key (0 = disable)
 *
 * Module Methods:
 *      bitrate     - return maximum input bitrate based on user settings
 */

#include "it95x.h"

// TODO: on_ts goes here
static
void on_ts(module_data_t *mod, const uint8_t *ts)
{
    // TODO: implement transmit buffering
    ASC_UNUSED(mod);
    ASC_UNUSED(ts);
}

/*
 * module initialization and methods
 */

static
int method_bitrate(lua_State *L, module_data_t *mod)
{
    if (mod->bitrate[1] == 0)
    {
        /* DVB-T or ISDB-T full transmission (13 segments) */
        lua_pushinteger(L, mod->bitrate[0]);
    }
    else
    {
        /* ISDB-T partial reception */
        lua_newtable(L);
        lua_pushinteger(L, mod->bitrate[0]);
        lua_rawseti(L, -2, 1); /* t[1] = layer A bitrate */
        lua_pushinteger(L, mod->bitrate[1]);
        lua_rawseti(L, -2, 2); /* t[2] = layer B bitrate */
    }

    return 1;
}

static
void module_init(lua_State *L, module_data_t *mod)
{
    asc_mutex_init(&mod->mutex);
    asc_cond_init(&mod->cond);

    /* get instance name */
    if (!module_option_string(L, "name", &mod->name, NULL))
        luaL_error(L, "[it95x] option 'name' is required");

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

    /* apply modulation settings */
    it95x_parse_opts(L, mod);

    module_option_boolean(L, "debug", &mod->debug);
    if (mod->debug)
        it95x_dump_opts(mod);

    // TODO: create TX ring

    /* create sync buffer */
    bool sync_on = true;
    module_option_boolean(L, "sync", &sync_on);

    if (sync_on)
    {
        const char *sync_opts = NULL;
        module_option_string(L, "sync_opts", &sync_opts, NULL);

        // TODO: create sync ctx
    }

    // TODO: create send timer (10-15 ms)

    /* start dedicated worker thread for modulator */
    module_stream_init(L, mod, on_ts);
    module_demux_set(mod, NULL, NULL);

    // TODO: start worker thread
}

static
void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);

    // TODO: kill retry and send timers

    // TODO: signal worker thread to quit

    // TODO: join worker thread

    // TODO: free TX ring

    // TODO: free sync ctx

    asc_cond_destroy(&mod->cond);
    asc_mutex_destroy(&mod->mutex);
}

static
const module_method_t module_methods[] =
{
    { "bitrate", method_bitrate },
    { NULL, NULL },
};

STREAM_MODULE_REGISTER(it95x_output)
{
    .init = module_init,
    .destroy = module_destroy,
    .methods = module_methods,
};
