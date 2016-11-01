/*
 * Astra Module: Remux (Utilities)
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

#include "remux.h"

/*
 * TS program
 */
ts_program_t *ts_program_init(uint16_t pnr, uint16_t pid)
{
    ts_program_t *const prog = ASC_ALLOC(1, ts_program_t);

    prog->pnr = pnr;
    prog->pmt_pid = pid;
    prog->pcr_pid = TS_NULL_PID;

    prog->custom_pmt = mpegts_psi_init(MPEGTS_PACKET_PMT, pid);

    return prog;
}

ts_program_t *ts_program_find(const module_data_t *mod, uint16_t pid)
{
    for(size_t i = 0; i < mod->prog_cnt; i++)
        if(mod->progs[i]->pmt_pid == pid)
            return mod->progs[i];

    return NULL;
}

void ts_program_destroy(ts_program_t *p)
{
    if(p)
    {
        mpegts_psi_destroy(p->custom_pmt);
        free(p->pids);
    }

    free(p);
}

/*
 * PCR stream
 */
pcr_stream_t *pcr_stream_init(uint16_t pid)
{
    pcr_stream_t *const st = ASC_ALLOC(1, pcr_stream_t);

    st->pid = pid;

    st->base = XTS_NONE;
    st->last = XTS_NONE;

    return st;
}

pcr_stream_t *pcr_stream_find(const module_data_t *mod, uint16_t pid)
{
    for(size_t i = 0; i < mod->pcr_cnt; i++)
        if(mod->pcrs[i]->pid == pid)
            return mod->pcrs[i];

    return NULL;
}

void pcr_stream_destroy(pcr_stream_t *p)
{
    free(p);
}
