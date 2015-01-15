/*
 * Astra Module: Remux
 *
 * Copyright (C) 2014, Artem Kharitonov <artem@sysert.ru>
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

#ifndef _REMUX_H_
#define _REMUX_H_ 1

/*
 * TS program
 */
typedef struct
{
    uint16_t pnr;
    uint16_t pmt_pid;
    uint16_t pcr_pid;

    /* PSI */
    uint32_t pmt_crc32;
    mpegts_psi_t *custom_pmt;

    /* ES pid list */
    uint16_t *pids;
    unsigned pid_cnt;
} ts_program_t;

ts_program_t *ts_program_init(uint16_t pnr, uint16_t pid);
ts_program_t *ts_program_find(const module_data_t *mod, uint16_t pid);
void ts_program_destroy(ts_program_t *p);

/*
 * PCR stream
 */
typedef struct
{
    uint16_t pid;

    uint64_t base;
    uint64_t last;

    unsigned count;
} pcr_stream_t;

pcr_stream_t *pcr_stream_init(uint16_t pid);
pcr_stream_t *pcr_stream_find(const module_data_t *mod, uint16_t pid);
void pcr_stream_destroy(pcr_stream_t *p);

/*
 * output buffer
 */
typedef struct
{
    const char *name;
    uint64_t rate;

    size_t size;

    asc_thread_t *thread;
    asc_thread_buffer_t *output;
    bool is_thread_started;

    ts_callback_t callback;
    void *cb_arg;
} remux_buffer_t;

/* thread loop wake up interval, msecs */
#define BUFFER_USLEEP 5

/* buffer size, seconds */
#define BUFFER_SECS 4

/* start output at this fill level */
#define BUFFER_NORM 25

/* when to dump buffer contents */
#define BUFFER_HIGH 75

remux_buffer_t *remux_buffer_init(const char *name, uint64_t rate);
void remux_buffer_push(remux_buffer_t *buf, const uint8_t *ts);
void remux_buffer_destroy(remux_buffer_t *buf);

/*
 * module instance
 */
struct module_data_t
{
    MODULE_STREAM_DATA();

    /* module config */
    const char *name;
    unsigned rate;
    int pcr_delay;
    bool no_buffer;

    /* output bytes */
    uint64_t offset;

    /* PSI */
    mpegts_psi_t *pat;
    mpegts_psi_t *cat;
    mpegts_psi_t *sdt;

    mpegts_psi_t *custom_pat;
    mpegts_psi_t *custom_cat;
    mpegts_psi_t *custom_sdt;

    mpegts_psi_t *pmt;

    /* packet intervals */
    unsigned pcr_interval;
    unsigned pat_interval;
    unsigned cat_interval;
    unsigned sdt_interval;

    /* packet counters */
    unsigned pat_count;
    unsigned cat_count;
    unsigned sdt_count;

    /* TS data */
    mpegts_packet_type_t stream[MAX_PID];
    mpegts_pes_t *pes[MAX_PID];
    uint16_t nit_pid;
    uint8_t buf[TS_PACKET_SIZE];

    ts_program_t **progs;
    unsigned prog_cnt;

    pcr_stream_t **pcrs;
    unsigned pcr_cnt;

    uint16_t *emms;
    unsigned emm_cnt;

    remux_buffer_t *buffer;
};

/* default PCR insertion interval, ms */
#define PCR_INTERVAL 20

/* default PCR delay, ms */
#define PCR_DELAY 250

/* maximum permissible PCR drift */
#define PCR_DRIFT 27000000 /* 1 sec */

/*
 * service information
 */
void remux_pat(void *arg, mpegts_psi_t *psi);
void remux_cat(void *arg, mpegts_psi_t *psi);
void remux_sdt(void *arg, mpegts_psi_t *psi);
void remux_pmt(void *arg, mpegts_psi_t *psi);

/* SI intervals, ms */
#define PAT_INTERVAL 100
#define CAT_INTERVAL 500
#define SDT_INTERVAL 500

#endif /* _REMUX_H_ */
