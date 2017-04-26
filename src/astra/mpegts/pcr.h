/*
 * Astra TS Library (PCR)
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

#ifndef _TS_PCR_
#define _TS_PCR_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

/* PCR frequency, Hz */
#define TS_PCR_FREQ 27000000LL

/* position of last byte in PCR field */
#define TS_PCR_LAST_BYTE 11

/* maximum possible PCR value */
#define TS_PCR_MAX (0x200000000LL * 300)

/* absent timestamp marker */
#define TS_TIME_NONE UINT64_MAX

/* extract PCR base (90KHz) */
#define TS_PCR_BASE(_ts) \
    ( \
        ( \
            (uint64_t)((_ts)[6]) << 25 \
        ) | ( \
            (uint64_t)((_ts)[7]) << 17 \
        ) | ( \
            (uint64_t)((_ts)[8]) << 9 \
        ) | ( \
            (uint64_t)((_ts)[9]) << 1 \
        ) | ( \
            (uint64_t)((_ts)[10]) >> 7 \
        ) \
    )

/* extract PCR extension (27MHz) */
#define TS_PCR_EXT(_ts) \
    ( \
        ( \
            ((uint64_t)((_ts)[10]) & 0x1) << 8 \
        ) | ( \
            (uint64_t)((_ts)[11]) \
        ) \
    )

/* set PCR fields in a packet, separately */
#define TS_SET_PCR_FIELDS(_ts, _base, _ext) \
    do { \
        (_ts)[6] = ((_base) >> 25) & 0xff; \
        (_ts)[7] = ((_base) >> 17) & 0xff; \
        (_ts)[8] = ((_base) >> 9) & 0xff; \
        (_ts)[9] = ((_base) >> 1) & 0xff; \
        (_ts)[10] = 0x7e | (((_base) << 7) & 0x80) | (((_ext) >> 8) & 0x1); \
        (_ts)[11] = (_ext) & 0xff; \
    } while (0)

/* get PCR value */
#define TS_GET_PCR(_ts) \
    ( \
        (TS_PCR_BASE(_ts) * 300LL) + TS_PCR_EXT(_ts) \
    )

/* set PCR value */
#define TS_SET_PCR(_ts, _val) \
    TS_SET_PCR_FIELDS((_ts), ((_val) / 300LL), ((_val) % 300LL));

/* get delta between two PCR values */
#define TS_PCR_DELTA(_a, _b) \
    ( \
        ((_b) >= (_a)) ? ((_b) - (_a)) : (((_b) - (_a)) + TS_PCR_MAX) \
    )

/* convert milliseconds to packet count based on TS bitrate */
#define TS_PCR_PACKETS(_ms, _rate) \
    ( \
        ((_ms) * ((_rate) / 1000)) / (TS_PACKET_SIZE * 8) \
    )

/* calculate PCR value based on offset and bitrate */
#define TS_PCR_CALC(_offset, _rate) \
    ( \
        (((_offset) + TS_PCR_LAST_BYTE) * TS_PCR_FREQ * 8) / (_rate) \
    )

/* usecs between two PCR values (deprecated) */
uint64_t ts_pcr_block_us(uint64_t *pcr_last
                         , const uint64_t *pcr_current) __asc_result;

#endif /* _TS_PCR_ */
