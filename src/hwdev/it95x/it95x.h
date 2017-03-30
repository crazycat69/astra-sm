/*
 * Astra Module: IT95x
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

#ifndef _HWDEV_IT95X_H_
#define _HWDEV_IT95X_H_ 1

#include <astra/astra.h>
#include <astra/luaapi/stream.h>
#include <astra/core/thread.h>
#include <astra/core/mutex.h>
#include <astra/core/cond.h>
#include <astra/core/timer.h>
#include <astra/mpegts/sync.h>

#include "api.h"

#define MSG(_msg) "[it95x %s] " _msg, mod->name

struct module_data_t
{
    STREAM_MODULE_DATA();

    /* module configuration */
    const char *name;
    int adapter;
    const char *devpath;
    bool debug;

    /* generic modulator options */
    uint32_t frequency;
    uint32_t bandwidth;

    int gain;
    int dc_i;
    int dc_q;
    unsigned int ofs_i;
    unsigned int ofs_q;

    it95x_iq_t iq_table[IT95X_IQ_TABLE_SIZE];
    size_t iq_size;

    it95x_pcr_mode_t pcr_mode;
    it95x_system_t system;

    /* DVB-T specific */
    it95x_dvbt_t dvbt;
    it95x_tps_t tps;

    uint32_t tps_crypt;

    /* ISDB-T specific */
    it95x_isdbt_t isdbt;
    it95x_tmcc_t tmcc;

    it95x_pid_t pid_list[IT95X_PID_LIST_SIZE];
    size_t pid_cnt;
    it95x_layer_t pid_layer;

    /* channel bitrate (per layer for partial RX) */
    uint32_t bitrate[2];

    /* transmit ring */
    struct
    {
        it95x_tx_block_t *blocks;
        // XXX: it95x_tx_block_t *current;
        size_t size;

        size_t head;
        size_t tail;
        // XXX: claim?
    } buf;

    mpegts_sync_t *sync;
    asc_timer_t *retry_timer;
    asc_timer_t *send_timer;

    /* worker thread */
    asc_thread_t *thread;
    asc_cond_t cond;
    asc_mutex_t mutex;
    bool running;
};

void it95x_parse_opts(lua_State *L, module_data_t *mod);
void it95x_dump_opts(const module_data_t *mod);

void it95x_worker_loop(void *arg);

#endif /* _HWDEV_IT95X_H_ */
