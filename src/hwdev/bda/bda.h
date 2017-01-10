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

#include <astra/astra.h>
#include <astra/luaapi/stream.h>
#include <astra/core/list.h>
#include <astra/core/thread.h>
#include <astra/core/timer.h>
#include <astra/core/mutex.h>
#include <astra/core/mainloop.h>

#include "../dshow/dshow.h"
#include <tuner.h>

#define MSG(_msg) "[dvb_input %s] " _msg, mod->name

/*
 * user commands for controlling the tuner
 */

typedef enum
{
    BDA_COMMAND_TUNE = 0,   /* tune the device and begin receiving TS */
    BDA_COMMAND_CLOSE,      /* tear down BDA graph and close the device */
    BDA_COMMAND_DEMUX,      /* ask pid filter to join or leave pid */
    BDA_COMMAND_CA,         /* control CI CAM program descrambling */
    BDA_COMMAND_DISEQC,     /* send DiSEqC command to driver */
    BDA_COMMAND_QUIT,       /* clean up and exit the BDA thread */
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
    int country_code;
    TunerInputType input_type;

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

    bool join;
    uint16_t pid;
} bda_demux_cmd_t;

typedef struct
{
    bda_command_t cmd;

    bool enable;
    uint16_t pnr;
} bda_ca_cmd_t;

typedef struct
{
    bda_command_t cmd;

    /* TODO: add diseqc command sequence */
} bda_diseqc_cmd_t;

typedef union
{
    bda_command_t cmd;

    bda_tune_cmd_t tune;
    bda_demux_cmd_t demux;
    bda_ca_cmd_t ca;
    bda_diseqc_cmd_t diseqc;
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
    HRESULT (*init_default_locator)(ILocator *);
    HRESULT (*init_space)(ITuningSpace *);

    /* tune request initializers */
    HRESULT (*set_space)(const bda_tune_cmd_t *, ITuningSpace *);
    HRESULT (*set_request)(const bda_tune_cmd_t *, ITuneRequest *);
    HRESULT (*set_locator)(const bda_tune_cmd_t *, ILocator *);
};

extern const bda_network_t *const bda_network_list[];
HRESULT bda_net_provider(const bda_network_t *net, IBaseFilter **out);
HRESULT bda_tuning_space(const bda_network_t *net, ITuningSpace **out);
HRESULT bda_tune_request(const bda_tune_cmd_t *cmd, ITuneRequest **out);

/*
 * vendor extensions
 */

/* DiSEqC command length, bytes */
#define BDA_DISEQC_LEN 6

enum
{
    BDA_EXTENSION_DISEQC    = 0x00000001, /* send DiSEqC raw command */
    BDA_EXTENSION_CA        = 0x00000002, /* CI CAM slot support */

    // TODO:
    // LNB power on/off
    // 22khz tone on/off
    // toneburst
};

typedef struct
{
    const char *name;
    const char *description;
    uint32_t flags;

    HRESULT (*init)(IBaseFilter *[], void **);
    void (*destroy)(void *);

    HRESULT (*tune)(void *, const bda_tune_cmd_t *);
    HRESULT (*diseqc)(void *, const uint8_t[BDA_DISEQC_LEN]);
    HRESULT (*lnb)(void *, bool);
    HRESULT (*t22khz)(void *, bool);
    // XXX: voltage/polarization ?

    void *data;
} bda_extension_t;

HRESULT bda_ext_init(module_data_t *mod, IBaseFilter *filters[]);
void bda_ext_destroy(module_data_t *mod);

HRESULT bda_ext_tune(module_data_t *mod, const bda_tune_cmd_t *tune);
HRESULT bda_ext_diseqc(module_data_t *mod, const uint8_t cmd[BDA_DISEQC_LEN]);

/*
 * BDA graph
 */

typedef enum
{
    BDA_STATE_INIT = 0,     /* control thread shall attempt tuner init */
    BDA_STATE_RUNNING,      /* tuner open, graph is working properly */
    BDA_STATE_STOPPED,      /* tuner device closed by user command */
    BDA_STATE_ERROR,        /* graph stopped due to error; awaiting reinit */
} bda_state_t;

typedef struct
{
    BOOLEAN locked;
    BOOLEAN present;
    LONG quality;
    LONG strength;
} bda_signal_stats_t;

struct module_data_t
{
    STREAM_MODULE_DATA();

    /* module configuration */
    const char *name;
    int adapter;
    const char *devpath;
    int idx_callback;
    int buffer_size;
    bool budget;
    bool debug;
    bool log_signal;
    bool no_dvr;
    int timeout;

    asc_timer_t *status_timer;

    /*
     * TODO
     *
     * Linux DVB input module also has these extra options.
     * Do we need to implement them?
     *
     * ca_pmt_delay
     * raw_signal - nope. no way to switch signal readout format
     * tone - 22khz tone. vendor specific
     * lnb_sharing - LNB power ctl. vendor specific
     * uni_frequency, uni_scr - Unicable, controlled via Diseqc
     */

    /* dedicated graph thread */
    asc_thread_t *thr;
    asc_list_t *queue;
    asc_mutex_t queue_lock;
    HANDLE queue_evt;

    /* TS ring buffer */
    struct {
        ts_packet_t *data;
        asc_mutex_t lock;
        size_t size;

        size_t head;
        size_t claim;
        size_t tail;

        unsigned int pending;
        unsigned int dropped;
    } buf;

    uint8_t frag[TS_PACKET_SIZE];
    size_t frag_pos;

    /* module state and tuning data */
    bda_tune_cmd_t tune;
    bool joined_pids[TS_MAX_PID];
    bool ca_pmts[TS_MAX_PNR];
    /* TODO: add diseqc sequence */

    bda_state_t state;
    unsigned int tunefail;
    int cooldown;

    bda_signal_stats_t signal_stats;
    asc_mutex_t signal_lock;

    /* COM objects */
    IFilterGraph2 *graph;
    IMediaEvent *event;
    IBaseFilter *provider;
    IMPEG2PIDMap *pidmap;
    IBDA_SignalStatistics *signal;

    HANDLE graph_evt;
    DWORD rot_reg;

    /* vendor extensions */
    asc_list_t *extensions;
    uint32_t ext_flags;
};

void bda_graph_loop(void *arg);

void bda_buffer_pop(void *arg);

/*
 * error handling (a.k.a. the joys of working with COM in plain C)
 */

/* log formatted error message */
#define __BDA_LOG(_level, ...) \
    do { bda_log_hr(mod, hr, _level, __VA_ARGS__); } while (0)

#define BDA_ERROR(...) __BDA_LOG(ASC_LOG_ERROR, __VA_ARGS__)
#define BDA_DEBUG(...) __BDA_LOG(ASC_LOG_DEBUG, __VA_ARGS__)

/* go to cleanup, unconditionally */
#define __BDA_THROW(_level, ...) \
    do { \
        __BDA_LOG(_level, __VA_ARGS__); \
        if (SUCCEEDED(hr)) \
            hr = E_FAIL; \
        goto out; \
    } while (0)

#define BDA_THROW(...) __BDA_THROW(ASC_LOG_ERROR, __VA_ARGS__)
#define BDA_THROW_D(...) __BDA_THROW(ASC_LOG_DEBUG, __VA_ARGS__)

/* go to cleanup if HRESULT indicates failure */
#define __BDA_CKHR(_level, ...) \
    do { \
        if (FAILED(hr)) \
            __BDA_THROW(_level, __VA_ARGS__); \
    } while (0)

#define BDA_CKHR(...) __BDA_CKHR(ASC_LOG_ERROR, __VA_ARGS__)
#define BDA_CKHR_D(...) __BDA_CKHR(ASC_LOG_DEBUG, __VA_ARGS__)

/* go to cleanup if a NULL pointer is detected */
#define __BDA_CKPTR(_level, _ptr, ...) \
    do { \
        ASC_WANT_PTR(hr, _ptr); \
        __BDA_CKHR(_level, __VA_ARGS__); \
    } while (0)

#define BDA_CKPTR(_ptr, ...) __BDA_CKPTR(ASC_LOG_ERROR, _ptr, __VA_ARGS__)
#define BDA_CKPTR_D(_ptr, ...) __BDA_CKPTR(ASC_LOG_DEBUG, _ptr, __VA_ARGS__)

void bda_dump_request(ITuneRequest *request);
void bda_log_hr(const module_data_t *mod, HRESULT hr, asc_log_type_t level
                , const char *msg, ...) __fmt_printf(4, 5);

#endif /* _HWDEV_BDA_H_ */
