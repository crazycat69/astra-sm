/*
 * Astra Module: MPEG-TS (PCR headers)
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

#ifndef _TS_PCR_
#define _TS_PCR_ 1

/* PCR frequency, Hz */
#define PCR_TIME_BASE 27000000LL

/* position of last byte in PCR field */
#define PCR_LAST_BYTE 11

/* maximum possible PCR value */
#define PCR_MAX ((0x1FFFFFFFFLL * 300) + 299)

/* absent timestamp marker */
#define XTS_NONE UINT64_MAX

/* whether TS packet contains PCR */
#define TS_IS_PCR(__x) \
    ( \
        (TS_IS_SYNC(__x)) && \
        (TS_IS_AF(__x)) && \
        (__x[4] >= 7) && /* AF length */ \
        (__x[5] & 0x10) /* PCR flag */ \
    )

/* extract PCR base (90KHz) */
#define TS_PCR_BASE(__x) \
    ( \
        ( \
            (uint64_t)((__x)[6]) << 25 \
        ) | ( \
            (uint64_t)((__x)[7]) << 17 \
        ) | ( \
            (uint64_t)((__x)[8]) << 9 \
        ) | ( \
            (uint64_t)((__x)[9]) << 1 \
        ) | ( \
            (uint64_t)((__x)[10]) >> 7 \
        ) \
    )

/* extract PCR extension (27MHz) */
#define TS_PCR_EXT(__x) \
    ( \
        ( \
            ((uint64_t)((__x)[10]) & 0x1) << 8 \
        ) | ( \
            (uint64_t)((__x)[11]) \
        ) \
    )

/* set PCR fields in a packet, separately */
#define TS_SET_PCR_FIELDS(__x, __b, __e) \
    do { \
        (__x)[6] = ((__b) >> 25) & 0xff; \
        (__x)[7] = ((__b) >> 17) & 0xff; \
        (__x)[8] = ((__b) >> 9) & 0xff; \
        (__x)[9] = ((__b) >> 1) & 0xff; \
        (__x)[10] = 0x7e | (((__b) << 7) & 0x80) | (((__e) >> 8) & 0x1); \
        (__x)[11] = (__e) & 0xff; \
    } while(0);

/* get PCR value */
#define TS_GET_PCR(__x) \
    ( \
        (TS_PCR_BASE(__x) * 300) + TS_PCR_EXT(__x) \
    )

/* set PCR value */
#define TS_SET_PCR(__x, __v) \
    TS_SET_PCR_FIELDS(__x, ((__v) / 300), ((__v) % 300));

/* usecs between two PCR values */
uint64_t mpegts_pcr_block_us(uint64_t *pcr_last, const uint64_t *pcr_current);

#endif /* _TS_PCR_ */
