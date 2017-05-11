/*
 * Astra TS Library (PES processing)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
 *               2014-2017, Artem Kharitonov <artem@3phase.pw>
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
#define PES_HDR_BASIC 6U
#define PES_HDR_EXT 3U
#define PES_HEADER_SIZE (PES_HDR_BASIC + PES_HDR_EXT)
#define PES_MAX_BUFFER 524288U

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

#endif /* _TS_PES_ */
