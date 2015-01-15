/*
 * Astra Module: MPEG-TS (PCR headers)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
 *                    2015, Artem Kharitonov <artem@sysert.ru>
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

/*
 * oooooooooo    oooooooo8 oooooooooo
 *  888    888 o888     88  888    888
 *  888oooo88  888          888oooo88
 *  888        888o     oo  888  88o
 * o888o        888oooo88  o888o  88o8
 *
 */

#define TS_IS_PCR(_ts)                                                                          \
    (                                                                                           \
        (_ts[0] == 0x47) &&                                                                     \
        (TS_IS_AF(_ts)) &&              /* adaptation field */                                  \
        (_ts[4] >= 7) &&                /* adaptation field length */                           \
        (_ts[5] & 0x10)                 /* PCR_flag */                                          \
    )

#define TS_GET_PCR(_ts) \
    (( \
        ((uint64_t)(_ts)[6] << 25) | \
        ((uint64_t)(_ts)[7] << 17) | \
        ((uint64_t)(_ts)[8] << 9 ) | \
        ((uint64_t)(_ts)[9] << 1 ) | \
        ((uint64_t)(_ts)[10] >> 7) \
    ) * 300 + ( \
        (((uint64_t)(_ts)[10] & 0x01) << 8) | ((uint64_t)(_ts)[11]) \
    ))

#define TS_SET_PCR(_ts, _pcr)                                                                   \
    {                                                                                           \
        uint8_t *const __ts = _ts;                                                              \
        const uint64_t __pcr = _pcr;                                                            \
        const uint64_t __pcr_base = __pcr / 300;                                                \
        const uint64_t __pcr_ext = __pcr % 300;                                                 \
        __ts[6] = (__pcr_base >> 25) & 0xFF;                                                    \
        __ts[7] = (__pcr_base >> 17) & 0xFF;                                                    \
        __ts[8] = (__pcr_base >> 9 ) & 0xFF;                                                    \
        __ts[9] = (__pcr_base >> 1 ) & 0xFF;                                                    \
        __ts[10] = ((__pcr_base << 7) & 0x80) | 0x7E | ((__pcr_ext >> 8) & 0x01);               \
        __ts[11] = __pcr_ext & 0xFF;                                                            \
    }

uint64_t mpegts_pcr_block_us(uint64_t *pcr_last, const uint64_t *pcr_current);

#endif /* _TS_PCR_ */
