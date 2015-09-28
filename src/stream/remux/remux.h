/*
 * Astra Module: Remux
 *
 * Copyright (C) 2014-2015, Artem Kharitonov <artem@sysert.ru>
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

#include <astra.h>
#include <core/stream.h>
#include <mpegts/pcr.h>
#include <mpegts/pes.h>
#include <mpegts/psi.h>

#define MSG(_msg) "[remux %s] " _msg, mod->name
#define MSGF(_msg) "[remux] %s(): " _msg, __func__

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
    size_t pid_cnt;
} ts_program_t;

ts_program_t *ts_program_init(uint16_t pnr, uint16_t pid) __wur;
ts_program_t *ts_program_find(const module_data_t *mod, uint16_t pid) __func_pure __wur;
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
pcr_stream_t *pcr_stream_find(const module_data_t *mod, uint16_t pid) __func_pure;
void pcr_stream_destroy(pcr_stream_t *p);

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
    size_t prog_cnt;

    pcr_stream_t **pcrs;
    size_t pcr_cnt;

    uint16_t *emms;
    size_t emm_cnt;
};

void remux_ts_out(void *arg, const uint8_t *ts);
void remux_pes(void *arg, mpegts_pes_t *pes);
void remux_ts_in(module_data_t *mod, const uint8_t *orig_ts);

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
