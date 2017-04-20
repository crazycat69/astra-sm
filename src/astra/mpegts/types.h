/*
 * Astra TS Library (Type definitions)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
 *               2015-2017, Artem Kharitonov <artem@3phase.pw>
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

#ifndef _TS_TYPES_
#define _TS_TYPES_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

/* TS packet type */
typedef enum
{
    TS_TYPE_UNKNOWN = 0x00000000,

    /* Program Specific Information */
    TS_TYPE_PSI     = 0x00100000,
    TS_TYPE_PAT     = TS_TYPE_PSI | 0x01,
    TS_TYPE_CAT     = TS_TYPE_PSI | 0x02,
    TS_TYPE_PMT     = TS_TYPE_PSI | 0x04,

    /* Service Information */
    TS_TYPE_SI      = 0x00200000,
    TS_TYPE_NIT     = TS_TYPE_SI | 0x01,
    TS_TYPE_SDT     = TS_TYPE_SI | 0x02,
    TS_TYPE_EIT     = TS_TYPE_SI | 0x04,
    TS_TYPE_TDT     = TS_TYPE_SI | 0x08,

    /* Conditional Access */
    TS_TYPE_CA      = 0x00400000,
    TS_TYPE_ECM     = TS_TYPE_CA | 0x01,
    TS_TYPE_EMM     = TS_TYPE_CA | 0x02,

    /* Elementary Stream */
    TS_TYPE_PES     = 0x00800000,
    TS_TYPE_VIDEO   = TS_TYPE_PES | 0x01,
    TS_TYPE_AUDIO   = TS_TYPE_PES | 0x02,
    TS_TYPE_SUB     = TS_TYPE_PES | 0x04,

    TS_TYPE_DATA    = 0x01000000,
    TS_TYPE_NULL    = 0x02000000
} ts_type_t;

/* mapping between PMT stream_type and TS packet type */
typedef struct
{
    uint8_t type_id;
    ts_type_t pkt_type;
    const char *description;
} ts_stream_type_t;

/* pre-defined null packet */
static
const uint8_t ts_null_pkt[TS_PACKET_SIZE] =
{
    /*
     * PID 8191 (0x1FFF), CC 0
     * Payload all zeroes
     */
    0x47, 0x1f, 0xff, 0x10
};

const ts_stream_type_t *ts_stream_type(uint8_t type_id) __asc_result;
ts_type_t ts_priv_type(uint8_t desc_type) __asc_result;
const char *ts_type_name(ts_type_t type) __asc_result;

#endif /* _TS_TYPES_ */
