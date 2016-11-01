/*
 * Astra Module: Remux (Service Information)
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

#define LIST_APPEND(__list, __cnt, __val, __type) \
    do { \
        bool __found = false; \
        for(size_t __i = 0; __i < __cnt; __i++) \
        { \
            if(__list[__i] == __val) \
            { \
                __found = true; \
                break; \
            } \
        } \
        if(!__found) \
        { \
            void *const __tmp = realloc(__list, sizeof(*__list) * (__cnt + 1)); \
            asc_assert(__tmp != NULL, MSG("realloc() failed")); \
            __list = (__type *)__tmp; \
            __list[__cnt++] = __val; \
        } \
    } while (0)

static inline bool list_contains_pid(const uint16_t list[]
                                     , size_t cnt
                                     , uint16_t pid)
{
    for(size_t i = 0; i < cnt; i++)
        if(pid == list[i])
            return true;

    return false;
}

static inline bool list_contains_item(const void *list
                                      , size_t cnt
                                      , const void *ptr)
{
    for(size_t i = 0; i < cnt; i++)
        if(ptr == ((void **)list)[i])
            return true;

    return false;
}

static inline void copy_psi(mpegts_psi_t *dst
                            , const mpegts_psi_t *src)
{
    dst->buffer_size = src->buffer_size;
    memcpy(dst->buffer, src->buffer, PSI_MAX_SIZE);
}

static void stream_reload(module_data_t *mod)
{
    /* garbage collection */
    for(size_t pid = 16; pid < TS_NULL_PID; pid++)
    {
        /* try to find pid's owner */
        if((mod->stream[pid] != MPEGTS_PACKET_NIT
            && pid < 32)
           || (mod->stream[pid] == MPEGTS_PACKET_NIT
               && pid == mod->nit_pid))
            /* NIT or pre-defined SI pid */
            goto found;

        else if(mod->stream[pid] == MPEGTS_PACKET_CA
                && list_contains_pid(mod->emms, mod->emm_cnt, pid))
            /* pid contains CAS EMM's */
            goto found;

        else
            /* scan TS programs */
            for(size_t i = 0; i < mod->prog_cnt; i++)
            {
                ts_program_t *const prog = mod->progs[i];

                if(prog->pmt_pid == pid)
                    /* pid belongs to a PMT */
                    goto found;

                else
                    /* check program's pid list */
                    if(list_contains_pid(prog->pids, prog->pid_cnt, pid))
                        /* pid belongs to an ES */
                        goto found;
            }

        /* into the trash it goes */
        if(mod->stream[pid])
        {
            asc_log_debug(MSG("deregistering pid %zu"), pid);
            mod->stream[pid] = MPEGTS_PACKET_UNKNOWN;
        }

        if(mod->pes[pid])
        {
            asc_log_debug(MSG("deleting PES muxer on pid %zu"), pid);

            mpegts_pes_destroy(mod->pes[pid]);
            mod->pes[pid] = NULL;
        }

    found:
        /* suppresses compiler warning */
        continue;
    }

    /* update PCR pid list */
    pcr_stream_t **list = NULL;
    size_t cnt = 0;

    for(size_t i = 0; i < mod->prog_cnt; i++)
    {
        const uint16_t pid = mod->progs[i]->pcr_pid;

        if(pid == TS_NULL_PID)
            /* PCR pid not yet assigned */
            continue;

        pcr_stream_t *pcr = pcr_stream_find(mod, pid);
        if(!pcr)
        {
            asc_log_debug(MSG("adding PCR to pid %hu"), pid);
            pcr = pcr_stream_init(pid);
        }

        LIST_APPEND(list, cnt, pcr, pcr_stream_t *);
    }

    for(size_t i = 0; i < mod->pcr_cnt; i++)
    {
        pcr_stream_t *pcr = mod->pcrs[i];

        if(!list_contains_item(list, cnt, pcr))
        {
            asc_log_debug(MSG("stopping PCR on pid %hu"), pcr->pid);
            pcr_stream_destroy(pcr);
        }
    }

    /* replace list pointer */
    free(mod->pcrs);
    mod->pcr_cnt = cnt;
    mod->pcrs = list;
}

void remux_pat(void *arg, mpegts_psi_t *psi)
{
    module_data_t *const mod = (module_data_t *)arg;

    /* check CRC */
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == mod->pat->crc32)
        /* PAT unchanged */
        return;

    if(crc32 != PSI_CALC_CRC32(psi))
    {
        asc_log_error(MSG("PAT checksum error"));
        return;
    }

    /* store new checksum */
    if(mod->pat->crc32 != 0)
        /* don't report first PAT */
        asc_log_debug(MSG("PAT changed, updating program list"));

    mod->pat->crc32 = crc32;

    /* rebuild program list */
    ts_program_t **list = NULL;
    size_t cnt = 0;

    const uint8_t *ptr;
    PAT_ITEMS_FOREACH(psi, ptr)
    {
        const uint16_t pnr = PAT_ITEM_GET_PNR(psi, ptr);
        const uint16_t pid = PAT_ITEM_GET_PID(psi, ptr);

        if(pnr && pid >= 32 && pid <= 8190)
        {
            /* PMT */
            mod->stream[pid] = MPEGTS_PACKET_PMT;
            ts_program_t *prog = ts_program_find(mod, pid);

            if(!prog)
            {
                /* have to create a new one */
                prog = ts_program_init(pnr, pid);
                asc_log_debug(MSG("created program %hu (PMT %hu)")
                                  , prog->pnr, prog->pmt_pid);
            }
            else if(pnr != prog->pnr)
            {
                /* extremely unlikely to happen */
                asc_log_debug(MSG("pnr change: %hu => %hu (PMT %hu)")
                              , prog->pnr, pnr, prog->pmt_pid);

                prog->pnr = pnr;
            }

            LIST_APPEND(list, cnt, prog, ts_program_t *);
        }
        else if(!pnr && pid >= 16 && pid <= 31)
        {
            /* NIT */
            mod->stream[pid] = MPEGTS_PACKET_NIT;
            mod->nit_pid = pid;
        }
    }

    /* kill off stale programs */
    for(size_t i = 0; i < mod->prog_cnt; i++)
    {
        ts_program_t *const prog = mod->progs[i];

        if(!list_contains_item(list, cnt, prog))
        {
            /* not listed in new PAT */
            asc_log_debug(MSG("deleting program %hu (PMT %hu)")
                          , prog->pnr, prog->pmt_pid);

            ts_program_destroy(prog);
        }
    }

    /* replace list pointer */
    free(mod->progs);
    mod->prog_cnt = cnt;
    mod->progs = list;

    /* clean up pids and muxers */
    stream_reload(mod);

    /* copy data to output PAT */
    copy_psi(mod->custom_pat, psi);
}

void remux_cat(void *arg, mpegts_psi_t *psi)
{
    module_data_t *const mod = (module_data_t *)arg;

    /* check CRC */
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == mod->cat->crc32)
        /* CAT unchanged */
        return;

    if(crc32 != PSI_CALC_CRC32(psi))
    {
        asc_log_error(MSG("CAT checksum error"));
        return;
    }

    /* store new checksum */
    if(mod->cat->crc32 != 0)
        /* don't report first CAT */
        asc_log_debug(MSG("CAT changed, updating EMM pid list"));

    mod->cat->crc32 = crc32;

    /* update EMM pid list */
    uint16_t *list = NULL;
    size_t cnt = 0;

    const uint8_t *desc;
    CAT_DESC_FOREACH(psi, desc)
    {
        const uint16_t pid = DESC_CA_PID(desc);

        if(desc[0] != 0x9 || pid < 32 || pid > 8190)
            /* non-CAS data or invalid pid */
            continue;

        mod->stream[pid] = MPEGTS_PACKET_CA;
        LIST_APPEND(list, cnt, pid, uint16_t);
    }

    /* replace list pointer */
    free(mod->emms);
    mod->emm_cnt = cnt;
    mod->emms = list;

    /* clean up pids and muxers */
    stream_reload(mod);

    /* copy data to output CAT */
    copy_psi(mod->custom_cat, psi);
}

void remux_sdt(void *arg, mpegts_psi_t *psi)
{
    module_data_t *const mod = (module_data_t *)arg;

    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == mod->sdt->crc32)
        /* SDT unchanged */
        return;

    mod->sdt->crc32 = crc32;

    /* copy data to output SDT */
    copy_psi(mod->custom_sdt, psi);
}

void remux_pmt(void *arg, mpegts_psi_t *psi)
{
    module_data_t *const mod = (module_data_t *)arg;
    ts_program_t *const prog = ts_program_find(mod, psi->pid);
    if(!prog)
        /* stray PMT; shouldn't happen */
        return;

    /* check CRC */
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == prog->pmt_crc32)
        /* PMT unchanged */
        return;

    if(crc32 != PSI_CALC_CRC32(psi))
    {
        asc_log_error(MSG("PMT checksum error, pid %hu"), psi->pid);
        return;
    }

    /* check program number */
    const uint16_t pnr = PMT_GET_PNR(psi);
    if(pnr != prog->pnr)
        return;

    /* store new checksum */
    if(prog->pmt_crc32 != 0)
        /* don't report program's first PMT */
        asc_log_debug(MSG("PMT changed at program no. %hu"), pnr);

    prog->pmt_crc32 = crc32;

    /* update stream map */
    uint16_t *list = NULL;
    size_t cnt = 0;

    const uint8_t *desc;
    PMT_DESC_FOREACH(psi, desc)
    {
        if(desc[0] == 0x9)
        {
            /* FIXME: magic constant */
            const uint16_t ca_pid = DESC_CA_PID(desc);

            if(ca_pid < 32 || ca_pid > 8190)
                /* invalid pid */
                continue;

            /* add ECM pid */
            mod->stream[ca_pid] = MPEGTS_PACKET_CA;
            LIST_APPEND(list, cnt, ca_pid, uint16_t);
        }
    }

    const uint8_t *item;
    PMT_ITEMS_FOREACH(psi, item)
    {
        const uint16_t pid = PMT_ITEM_GET_PID(psi, item);
        const uint8_t item_type = PMT_ITEM_GET_TYPE(psi, item);

        const stream_type_t *const st = mpegts_stream_type(item_type);
        mpegts_packet_type_t ts_type = st->pkt_type;

        if(pid < 32 || pid > 8190)
            /* invalid pid */
            continue;

        /* list associated data */
        PMT_ITEM_DESC_FOREACH(item, desc)
        {
            /* FIXME: magic constant */
            if(desc[0] == 0x9)
            {
                const uint16_t ca_pid = DESC_CA_PID(desc);

                if(ca_pid < 32 || ca_pid > 8190)
                    /* invalid pid */
                    continue;

                /* add ECM pid */
                mod->stream[ca_pid] = MPEGTS_PACKET_CA;
                LIST_APPEND(list, cnt, ca_pid, uint16_t);
            }
            /* FIXME: ditto */
            else if(item_type == 0x06 && ts_type == MPEGTS_PACKET_DATA)
                ts_type = mpegts_priv_type(desc[0]);
        }

        /* add elementary stream */
        mod->stream[pid] = ts_type;
        LIST_APPEND(list, cnt, pid, uint16_t);

        if(ts_type != MPEGTS_PACKET_DATA && !mod->pes[pid])
        {
            /* create muxers for A/V streams */
            asc_log_debug(MSG("creating PES muxer on pid %hu"), pid);

            mpegts_pes_t * const pes = mpegts_pes_init(pid);
            pes->on_pes = remux_pes;
            pes->on_ts = remux_ts_out;
            pes->cb_arg = mod;

            mod->pes[pid] = pes;
        }
    }

    /* update PCR pid */
    uint16_t pcr_pid = PMT_GET_PCR(psi);

    if(pcr_pid < 32 || pcr_pid > 8190)
    {
        /* invalid pid or no PCR */
        pcr_pid = TS_NULL_PID;

        /* TODO: enable PCR recovery using PTS */
        asc_log_info(MSG("program %hu (PMT %hu) has no PCR")
                     , prog->pnr, prog->pmt_pid);
    }
    else if(!list_contains_pid(list, cnt, pcr_pid))
    {
        /* in case PCR is in its own pid */
        mod->stream[pcr_pid] = MPEGTS_PACKET_DATA;
        LIST_APPEND(list, cnt, pcr_pid, uint16_t);

        if(mod->pes[pcr_pid])
        {
            /* shouldn't happen */
            asc_log_debug(MSG("deleting PES muxer on PCR-only pid %hu")
                          , pcr_pid);

            mpegts_pes_destroy(mod->pes[pcr_pid]);
            mod->pes[pcr_pid] = NULL;
        }
    }

    prog->pcr_pid = pcr_pid;

    /* replace list pointer */
    free(prog->pids);
    prog->pid_cnt = cnt;
    prog->pids = list;

    /* clean up pids and muxers */
    stream_reload(mod);

    /* copy data to output PMT */
    copy_psi(prog->custom_pmt, psi);
}
