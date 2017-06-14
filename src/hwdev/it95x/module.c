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
 *      buffer_size - number, buffer size in megabytes (default is 1 MiB)
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
 *
 * I/Q Calibration Table Syntax:
 *      iq_table = {
 *          { <frequency>, <amp>, <phi> },
 *          -- Up to 65536 entries.
 *          -- See doc/it95x_iq for converter tool.
 *      },
 *
 * PID Filter Syntax for ISDB-T:
 *      pid_layer = <layer>,
 *      pid_list = {
 *          { <pid_1>, <layer> },
 *          { <pid_2>, <layer> },
 *          -- Up to 31 PIDs.
 *      },
 */

#include "it95x.h"

/* default buffer size, MiB */
#define DEFAULT_BUFFER_SIZE 1

/* device restart interval, seconds */
#define RESTART_TIMER_SEC 10

/*
 * buffering and worker thread communication
 */

/* thread restart callback */
static
void on_worker_close(void *arg);

static
void on_worker_restart(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    if (mod->restart_timer != NULL)
    {
        asc_log_debug(MSG("attempting to reinitialize device"));
        mod->restart_timer = NULL;
    }

    mod->thread = asc_thread_init(mod, it95x_worker_loop, on_worker_close);
}

/* called whenever the worker thread returns */
static
void on_worker_close(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    ASC_FREE(mod->thread, asc_thread_join);
    mod->transmitting = mod->quitting = false;
    mod->tx_tail = mod->tx_head;

    /* schedule modulator restart */
    asc_log_warning(MSG("reopening device in %u seconds")
                    , RESTART_TIMER_SEC);

    const unsigned int ms = RESTART_TIMER_SEC * 1000;
    mod->restart_timer = asc_timer_one_shot(ms, on_worker_restart, mod);
}

/* queue current block for transmission and start filling up the next one */
static
void next_block(module_data_t *mod)
{
    asc_mutex_lock(&mod->mutex);

    size_t filled = mod->tx_head - mod->tx_tail;
    if (mod->tx_head < mod->tx_tail)
        filled += mod->tx_size;

    if (mod->transmitting)
    {
        size_t next = (mod->tx_head + 1) % mod->tx_size;
        if (next == mod->tx_tail)
        {
            asc_log_error(MSG("transmit ring full, resetting"));
            next = (mod->tx_tail + 1) % mod->tx_size;
        }

        mod->tx_head = next;

        if (filled > 1)
            asc_cond_signal(&mod->cond);
    }

    mod->tx_ring[mod->tx_head].size = 0;

    asc_mutex_unlock(&mod->mutex);

#ifdef IT95X_DEBUG
    if (mod->debug)
    {
        const time_t now = time(NULL);

        if (now - mod->last_report >= 60) /* 1 min */
        {
            asc_log_debug(MSG("transmit ring fill: %zu/%zu")
                          , filled, mod->tx_size);

            mod->last_report = now;
        }

        if (mod->transmitting && filled == 0)
        {
            asc_log_debug(MSG("transmit ring is empty"));
        }
    }
#endif /* IT95X_DEBUG */
}

/* copy single TS packet into current TX block */
static
void on_ts(module_data_t *mod, const uint8_t *ts)
{
    it95x_tx_block_t *const blk = &mod->tx_ring[mod->tx_head];

    memcpy(&blk->data[blk->size], ts, TS_PACKET_SIZE);
    blk->size += TS_PACKET_SIZE;

    if (blk->size >= IT95X_TX_BLOCK_SIZE)
        next_block(mod);
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
    module_option_string(L, "name", &mod->name, NULL);
    if (mod->name == NULL)
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

    /* validate modulation settings */
    it95x_parse_opts(L, mod);

#ifdef IT95X_DEBUG
    /* get debug mode */
    module_option_boolean(L, "debug", &mod->debug);

    if (mod->debug)
        it95x_dump_opts(mod);
#endif

    /* create transmit ring */
    int opt = DEFAULT_BUFFER_SIZE;
    module_option_integer(L, "buffer_size", &opt);
    if (!(opt >= 1 && opt <= 100))
        luaL_error(L, MSG("buffer size must be between 1 and 100 MiB"));

    mod->tx_size = ((size_t)opt * 1024UL * 1024UL) / sizeof(*mod->tx_ring);
    ASC_ASSERT(mod->tx_size > 0, MSG("invalid buffer size"));

    mod->tx_ring = ASC_ALLOC(mod->tx_size, it95x_tx_block_t);
    asc_log_debug(MSG("using transmit ring of %zu blocks (%zu bytes each)")
                  , mod->tx_size, sizeof(*mod->tx_ring));

    /* start dedicated thread for sending data to modulator */
    module_stream_init(L, mod, on_ts);
    module_demux_set(mod, NULL, NULL);

    on_worker_restart(mod);
}

static
void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);

    if (mod->thread != NULL)
    {
        asc_mutex_lock(&mod->mutex);
        mod->quitting = true;
        asc_cond_signal(&mod->cond);
        asc_mutex_unlock(&mod->mutex);

        asc_thread_join(mod->thread);
    }

    ASC_FREE(mod->restart_timer, asc_timer_destroy);
    ASC_FREE(mod->tx_ring, free);

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
