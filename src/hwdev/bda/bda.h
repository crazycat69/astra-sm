/*
 * Astra Module: BDA
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

#ifndef _HWDEV_BDA_H_
#define _HWDEV_BDA_H_ 1

#include <astra/astra.h>
#include <astra/luaapi/stream.h>
#include <astra/core/list.h>
#include <astra/core/thread.h>
#include <astra/core/timer.h>
#include <astra/core/mutex.h>

#include "../dshow/dshow.h"
#include <tuner.h>

#define MSG(_msg) "[dvb_input %s] " _msg, mod->name

/*
 * user commands for controlling the tuner
 */

/* DiSEqC command length, bytes */
#define BDA_DISEQC_LEN 6

/* maximum DiSEqC sequence size */
#define BDA_DISEQC_MAX_SEQ 64

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
    LNB_Source lnb_source;
    Polarisation polarization;
    SpectralInversion inversion;
    RollOff rolloff;
    Pilot pilot;
    int pls_code;
    int pls_mode;

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

typedef enum
{
    BDA_EXT_LNBPOWER_NOT_SET = -1,
    BDA_EXT_LNBPOWER_NOT_DEFINED = 0,
    BDA_EXT_LNBPOWER_OFF,
    BDA_EXT_LNBPOWER_ON, /* auto voltage */
    BDA_EXT_LNBPOWER_18V,
    BDA_EXT_LNBPOWER_13V,
} bda_lnbpower_mode_t;

typedef enum
{
    BDA_EXT_22K_NOT_SET = -1,
    BDA_EXT_22K_NOT_DEFINED = 0,
    BDA_EXT_22K_OFF,
    BDA_EXT_22K_ON,
} bda_22k_mode_t;

typedef enum
{
    BDA_EXT_TONEBURST_NOT_SET = -1,
    BDA_EXT_TONEBURST_NOT_DEFINED = 0,
    BDA_EXT_TONEBURST_OFF,
    BDA_EXT_TONEBURST_UNMODULATED, /* mini-A */
    BDA_EXT_TONEBURST_MODULATED, /* mini-B */
} bda_toneburst_mode_t;

typedef struct
{
    uint8_t data[BDA_DISEQC_LEN];
    unsigned int data_len;

    bda_lnbpower_mode_t lnbpower;
    bda_22k_mode_t t22k;
    bda_toneburst_mode_t toneburst;

    int delay;
} bda_diseqc_seq_t;

typedef struct
{
    bda_command_t cmd;

    /* table syntax: a:diseqc({{...},{...}}) */
    bda_diseqc_seq_t seq[BDA_DISEQC_MAX_SEQ];
    unsigned int seq_size;

    /* number syntax: a:diseqc(n) */
    LNB_Source port;
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
    const CLSID *loc_fallback;  /* fallback locator for older systems */
    const CLSID *tuning_space;  /* tuning space object for this network */
    const CLSID *network_type;  /* GUID to assign to tuning spaces */

    /* tuning space initializer */
    HRESULT (*init_space)(ITuningSpace *);

    /* tune request initializers */
    HRESULT (*set_space)(const bda_tune_cmd_t *, ITuningSpace *);
    HRESULT (*set_request)(const bda_tune_cmd_t *, ITuneRequest *);
    HRESULT (*set_locator)(const bda_tune_cmd_t *, ILocator *);
};

extern const bda_network_t bda_net_atsc;
extern const bda_network_t bda_net_cqam;
extern const bda_network_t bda_net_dvbc;
extern const bda_network_t bda_net_dvbs;
extern const bda_network_t bda_net_dvbs2;
extern const bda_network_t bda_net_dvbt;
extern const bda_network_t bda_net_dvbt2;
extern const bda_network_t bda_net_isdbs;
extern const bda_network_t bda_net_isdbt;
extern const bda_network_t *const bda_network_list[];

HRESULT bda_net_provider(const bda_network_t *net, IBaseFilter **out);
HRESULT bda_tuning_space(const bda_network_t *net, ITuningSpace **out);
HRESULT bda_tune_request(const bda_tune_cmd_t *cmd, ITuneRequest **out);

/*
 * vendor extensions
 */

/* extension types */
enum
{
    BDA_EXT_DISEQC    = 0x00000001, /* send DiSEqC raw command */
    BDA_EXT_LNBPOWER  = 0x00000002, /* set LNB power and voltage */
    BDA_EXT_22K       = 0x00000004, /* switch 22kHz tone on and off */
    BDA_EXT_TONEBURST = 0x00000008, /* switch mini-DiSEqC input (A/B) */
    BDA_EXT_CA        = 0x00000010, /* CI CAM slot support */
};

/* tuning data hooks */
typedef enum
{
    BDA_TUNE_PRE = 0,   /* call before starting the graph */
    BDA_TUNE_POST,      /* call after the graph has started */
} bda_tune_hook_t;

typedef struct
{
    const char *name;
    const char *description;
    uint32_t flags;

    HRESULT (*init)(IBaseFilter *[], void **);
    void (*destroy)(void *);

    /* called before and after starting the graph */
    HRESULT (*tune_pre)(void *, const bda_tune_cmd_t *);
    HRESULT (*tune_post)(void *, const bda_tune_cmd_t *);

    /* send DiSEqC command */
    HRESULT (*diseqc)(void *, const uint8_t *, unsigned int);

    /* LNB power and voltage */
    HRESULT (*lnbpower)(void *, bda_lnbpower_mode_t);

    /* 22kHz tone */
    HRESULT (*t22k)(void *, bda_22k_mode_t);
    HRESULT (*toneburst)(void *, bda_toneburst_mode_t);

    void *data;
} bda_extension_t;

HRESULT bda_ext_init(module_data_t *mod, IBaseFilter *filters[]);
void bda_ext_destroy(module_data_t *mod);

HRESULT bda_ext_tune(module_data_t *mod, const bda_tune_cmd_t *tune
                     , bda_tune_hook_t when);
HRESULT bda_ext_diseqc(module_data_t *mod, const uint8_t *cmd
                       , unsigned int len);
HRESULT bda_ext_lnbpower(module_data_t *mod, bda_lnbpower_mode_t mode);
HRESULT bda_ext_22k(module_data_t *mod, bda_22k_mode_t mode);
HRESULT bda_ext_toneburst(module_data_t *mod, bda_toneburst_mode_t mode);

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
    bda_diseqc_cmd_t diseqc;
    bool joined_pids[TS_MAX_PID];
    bool ca_pmts[TS_MAX_PNR];

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

#define BDA_MODULE_PFX "dvb_input "
#define BDA_MODULE_ID mod->name

/* log formatted error message */
#define __BDA_LOG(_hr, _type, ...) \
    do { \
        bda_log_hr(BDA_MODULE_PFX, BDA_MODULE_ID, _hr, _type, __VA_ARGS__); \
    } while (0)

#define BDA_ERROR(_hr, ...) \
    __BDA_LOG(_hr, ASC_LOG_ERROR, __VA_ARGS__)
#define BDA_ERROR_D(_hr, ...) \
    __BDA_LOG(_hr, ASC_LOG_DEBUG, __VA_ARGS__)

/* go to cleanup, unconditionally */
#define __BDA_THROW(_hr, _type, ...) \
    do { \
        __BDA_LOG(_hr, _type, __VA_ARGS__); \
        if (SUCCEEDED(_hr)) \
            _hr = E_FAIL; \
        goto out; \
    } while (0)

#define BDA_THROW(_hr, ...) \
    __BDA_THROW(_hr, ASC_LOG_ERROR, __VA_ARGS__)
#define BDA_THROW_D(_hr, ...) \
    __BDA_THROW(_hr, ASC_LOG_DEBUG, __VA_ARGS__)

/* go to cleanup if HRESULT indicates failure */
#define __BDA_CKHR(_hr, _type, ...) \
    do { \
        if (FAILED(_hr)) \
            __BDA_THROW(_hr, _type, __VA_ARGS__); \
    } while (0)

#define BDA_CKHR(_hr, ...) \
    __BDA_CKHR(_hr, ASC_LOG_ERROR, __VA_ARGS__)
#define BDA_CKHR_D(_hr, ...) \
    __BDA_CKHR(_hr, ASC_LOG_DEBUG, __VA_ARGS__)

/* go to cleanup if a NULL pointer is detected */
#define __BDA_CKPTR(_hr, _type, _ptr, ...) \
    do { \
        ASC_WANT_PTR(_hr, _ptr); \
        __BDA_CKHR(_hr, _type, __VA_ARGS__); \
    } while (0)

#define BDA_CKPTR(_hr, _ptr, ...) \
    __BDA_CKPTR(_hr, ASC_LOG_ERROR, _ptr, __VA_ARGS__)
#define BDA_CKPTR_D(_hr, _ptr, ...) \
    __BDA_CKPTR(_hr, ASC_LOG_DEBUG, _ptr, __VA_ARGS__)

void bda_dump_request(ITuneRequest *request);
void bda_log_hr(const char *pfx, const char *id
                , HRESULT hr, asc_log_type_t type
                , const char *fmt, ...) __fmt_printf(5, 6);

#endif /* _HWDEV_BDA_H_ */
