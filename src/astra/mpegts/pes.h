/*
 * Astra Module: MPEG-TS (PES headers)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
 *               2014-2015, Artem Kharitonov <artem@sysert.ru>
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

#ifndef _TS_PES_
#define _TS_PES_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

/* header sizes */
#define PES_HDR_BASIC   6U
#define PES_HDR_EXT     3U
#define PES_HEADER_SIZE (PES_HDR_BASIC + PES_HDR_EXT)
#define PES_MAX_BUFFER  524288U

/* start code */
#define PES_BUFFER_GET_HEADER(_pes) \
    ( \
        (_pes[0] << 16) | (_pes[1] << 8) | (_pes[2]) \
    )

/* stream ID */
#define PES_BUFFER_GET_SID(_pes) (_pes[3])

/* packet length */
#define PES_BUFFER_GET_SIZE(_pes) \
    ( \
        (size_t)( \
            (_pes[4] << 8) | _pes[5] \
        ) + PES_HDR_BASIC \
    )

/* get PTS/DTS values */
#define PES_GET_XTS(__x, __p) \
    ( \
        ( \
            (uint64_t)((__x)[(__p) + 0] & 0xe) << 29 \
        ) | ( \
            (uint64_t)((__x)[(__p) + 1]) << 22 \
        ) | ( \
            (uint64_t)((__x)[(__p) + 2] & 0xfe) << 14 \
        ) | ( \
            (uint64_t)((__x)[(__p) + 3]) << 7 \
        ) | ( \
            (uint64_t)((__x)[(__p) + 4]) >> 1 \
        ) \
    )
/* wrappers */
#define PES_GET_PTS(__x) PES_GET_XTS(__x, 9)
#define PES_GET_DTS(__x) PES_GET_XTS(__x, 14)

/* set PTS/DTS values */
#define PES_SET_XTS(__x, __o, __p, __v) \
    do \
    { \
        (__x)[(__o) + 0] = ((__p) << 4) | (((__v) >> 29) & 0xe) | 0x1; \
        (__x)[(__o) + 1] = (__v) >> 22; \
        (__x)[(__o) + 2] = ((__v) >> 14) | 0x1; \
        (__x)[(__o) + 3] = (__v) >> 7; \
        (__x)[(__o) + 4] = ((__v) << 1) | 0x1; \
    } while(0);
/* wrappers */
#define PES_SET_PTS(__x, __v) PES_SET_XTS(__x, 9, 0x2, __v)
#define PES_SET_DTS(__x, __v) PES_SET_XTS(__x, 14, 0x1, __v)

typedef struct mpegts_pes_t mpegts_pes_t;
typedef void (*pes_callback_t)(void *, mpegts_pes_t *);

/* preferred PES buffering mode */
typedef enum {
    /* output as soon as possible (default) */
    PES_MODE_FAST = 0,

    /* wait until we have the whole packet */
    PES_MODE_WHOLE,
} mpegts_pes_mode_t;

/* extension header struct */
typedef struct __attribute__((__packed__)) {
#if defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN
    unsigned original  : 1;
    unsigned copyright : 1;
    unsigned alignment : 1;
    unsigned priority  : 1;
    unsigned scrambled : 2;
    unsigned marker    : 2;
    /* byte 6 */
    unsigned extension : 1;
    unsigned crc       : 1;
    unsigned copy_info : 1;
    unsigned dsm_trick : 1;
    unsigned es_rate   : 1;
    unsigned escr      : 1;
    unsigned dts       : 1;
    unsigned pts       : 1;
    /* byte 7 */
#elif defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN
    /* byte 6 */
    unsigned marker    : 2;
    unsigned scrambled : 2;
    unsigned priority  : 1;
    unsigned alignment : 1;
    unsigned copyright : 1;
    unsigned original  : 1;
    /* byte 7 */
    unsigned pts       : 1;
    unsigned dts       : 1;
    unsigned escr      : 1;
    unsigned es_rate   : 1;
    unsigned dsm_trick : 1;
    unsigned copy_info : 1;
    unsigned crc       : 1;
    unsigned extension : 1;
#else
#   error "Please fix BYTE_ORDER defines"
#endif /* BYTE_ORDER */
    /* byte 8 */
    unsigned hdrlen    : 8;
} mpegts_pes_ext_t;

/* (de)muxer context struct */
struct mpegts_pes_t
{
    /* TS header */
    uint16_t pid;
    uint8_t i_cc; /* input CC */
    uint8_t o_cc; /* output CC */
    bool key;     /* random access */

    /* PES header */
    uint8_t stream_id;
    size_t expect_size;

    /* PES extension header */
    mpegts_pes_ext_t ext;

    /* timing data */
    uint64_t pts;
    uint64_t dts;
    uint64_t pcr;

    /* packet counters */
    unsigned sent;
    unsigned truncated;
    unsigned dropped;

    /* mux buffer */
    uint8_t buffer[PES_MAX_BUFFER];
    size_t buf_read;
    size_t buf_write;

    /* demux buffer */
    uint8_t ts[TS_PACKET_SIZE];

    /* output mode */
    mpegts_pes_mode_t mode;

    /* callbacks */
    void *cb_arg;
    pes_callback_t on_pes;
    ts_callback_t on_ts;
};

mpegts_pes_t *mpegts_pes_init(uint16_t pid) __wur;
void mpegts_pes_destroy(mpegts_pes_t *pes);

bool mpegts_pes_mux(mpegts_pes_t *pes, const uint8_t *ts);

#endif /* _TS_PES_ */
