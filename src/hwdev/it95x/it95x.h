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
    bool no_sync;
    // sync opts

    uint32_t frequency;
    uint16_t bandwidth;
    uint16_t cell_id;
    int gain;

    int dc_i;
    int dc_q;
    unsigned int ofs_i;
    unsigned int ofs_q;

    it95x_system_t system;
    it95x_dvbt_t dvbt;
    it95x_isdbt_t isdbt;

    // TODO: isdb pid filter config
    // TODO: IQ calibration table config

    it95x_pcr_mode_t pcr_mode;
    uint8_t tps_key[4];
    bool tps_crypt;

    /* transmit ring */
    // TODO

    mpegts_sync_t *sync;
    asc_timer_t *timer;

    /* modulator thread */
    asc_thread_t *thr;
    //asc_cond_t cond;
    //asc_mutex_t mutex;

    /* device context */
    it95x_device_t *dev;
};

#endif /* _HWDEV_IT95X_H_ */
