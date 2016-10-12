/*
 * Astra Module: BDA
 *
 * Copyright (C) 2016, Artem Kharitonov <artem@3phase.pw>
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

#ifndef _HWDEV_BDA_H_
#define _HWDEV_BDA_H_ 1

// FIXME: move these to astra.h
#define UNICODE
#define _UNICODE
// FIXME

#include "../hwdev.h"
#include "../win32/guids.h"
#include "../win32/dshow.h"

#include <tuner.h>

/*
 * user commands for controlling the tuner
 */

typedef enum
{
    BDA_COMMAND_TUNE = 0,   /* tune the device and begin receiving TS */
    BDA_COMMAND_CLOSE,      /* tear down BDA graph and close the device */
    BDA_COMMAND_DEMUX,      /* ask pid filter to join or leave pid */
    BDA_COMMAND_DISEQC,     /* send DiSEqC command to driver */
    BDA_COMMAND_CA,         /* control CI CAM program descrambling */
} bda_command_t;

typedef struct bda_network_t bda_network_t;

typedef struct
{
    bda_command_t cmd;

    /* generic settings */
    const bda_network_t *net;
    int frequency;
    int symbolrate;
    int stream_id;
    ModulationType modulation;
    BinaryConvolutionCodeRate fec;
    BinaryConvolutionCodeRate outer_fec;
    FECMethod fec_mode;
    FECMethod outer_fec_mode;

    /* atsc and cqam */
    int major_channel;
    int minor_channel;
    int virtual_channel;

    /* dvb-s */
    int lof1;
    int lof2;
    int slof;
    Polarisation polarization;
    SpectralInversion inversion;
    RollOff rolloff;
    Pilot pilot;

    /* dvb-t */
    int bandwidth;
    GuardInterval guardinterval;
    TransmissionMode transmitmode;
    HierarchyAlpha hierarchy;
    BinaryConvolutionCodeRate lp_fec;
    FECMethod lp_fec_mode;
} bda_tune_cmd_t;

typedef struct
{
    bda_command_t cmd;
} bda_close_cmd_t;

typedef struct
{
    bda_command_t cmd;

    bool join;
    uint16_t pid;
} bda_demux_cmd_t;

typedef struct
{
    bda_command_t cmd;

    // TODO: diseqc commands
} bda_diseqc_cmd_t;

typedef struct
{
    bda_command_t cmd;

    // TODO: CA PMT commands
} bda_ca_cmd_t;

typedef union
{
    bda_command_t cmd;

    bda_tune_cmd_t tune;
    bda_close_cmd_t close;
    bda_diseqc_cmd_t diseqc;
    bda_ca_cmd_t ca;
} bda_user_cmd_t;

/*
 * networks and tuning requests
 */

struct bda_network_t
{
    const char *name[4];        /* up to 4 short names */

    const CLSID *provider;      /* fallback provider for older systems */
    const CLSID *locator;       /* locator object for this network */
    const CLSID *tuning_space;  /* tuning space object for this network */
    const CLSID *network_type;  /* GUID to assign to tuning spaces */

    /* tuning space initializers */
    HRESULT (*init_locator)(ILocator *);
    HRESULT (*init_tuning_space)(ITuningSpace *);

    /* tune request initializers */
    HRESULT (*set_locator)(const bda_tune_cmd_t *, ILocator *);
    HRESULT (*set_tuning_space)(const bda_tune_cmd_t *, ITuningSpace *);
    HRESULT (*set_tune_request)(const bda_tune_cmd_t *, ITuneRequest *);
};

extern const bda_network_t *const bda_network_list[];
HRESULT bda_tuning_space(const bda_network_t *net, ITuningSpace **out);
/*
 * TODO
 *
 * HRESULT bda_tune_request(const bda_tune_cmd_t *cmd, ITuneRequest **out);
 */

/*
 * BDA graph
 */

struct hw_device_t
{
    // module config
    int adapter; // default = -1

/*
 *  TODO
 *
 *  const char *displayname; // use strdup?
 *  bool budget;
 *
 *  // extra options from the linux module
 *  // do we need to implement these?
 *  buffer_size
 *  log_signal
 *  timeout
 *  ca_pmt_delay
 *  raw_signal
 *
 *  // graph thread
 *  asc_thread_t *thr;
 *
 *  asc_list_t *commands;
 *  asc_mutex_t *cmd_lock;
 *
 *  // keep demux pid state in an array
 *  // restore on graph restart
 *
 *  // graph objects
 *  IBlaBla *bla;
 */
};

/*
 * TODO
 *
 * void bda_thread_loop(void *arg);
 */

/*
 * Lua methods
 */

int bda_enumerate(lua_State *L);

/*
 * TODO
 *
 * int bda_method_tune(lua_State *L, module_data_t *mod);
 * int bda_method_close(lua_State *L, module_data_t *mod);
 * int bda_method_diseqc(lua_State *L, module_data_t *mod);
 * int bda_method_ca(lua_State *L, module_data_t *mod);
 */

#endif /* _HWDEV_BDA_H_ */
