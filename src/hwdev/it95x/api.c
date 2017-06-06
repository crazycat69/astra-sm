/*
 * Astra Module: IT95x (Modulator API)
 *
 * Copyright (C) 2017, Artem Kharitonov <artem@3phase.pw>
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

#include "api.h"

#ifdef _WIN32
#   define IT95X_INVALID_DATA (E_INVALIDARG)
#else
#   define IT95X_INVALID_DATA (-EINVAL)
#endif

/* calculate (bw * fec * constellation * interval) */
static
uint32_t base_rate(uint32_t bandwidth
                   , it95x_constellation_t constellation
                   , it95x_guardinterval_t guardinterval
                   , it95x_coderate_t coderate)
{
    uint32_t x = bandwidth * 1000;

    switch (constellation)
    {
        case IT95X_CONSTELLATION_QPSK:  x *= 2; break;
        case IT95X_CONSTELLATION_16QAM: x *= 4; break;
        case IT95X_CONSTELLATION_64QAM: x *= 6; break;
        default:
            return 0;
    }

    switch (guardinterval)
    {
        case IT95X_GUARD_1_32: x = (x * 32) / 33; break;
        case IT95X_GUARD_1_16: x = (x * 16) / 17; break;
        case IT95X_GUARD_1_8:  x = (x * 8) / 9;   break;
        case IT95X_GUARD_1_4:  x = (x * 4) / 5;   break;
        default:
            return 0;
    }

    switch (coderate)
    {
        case IT95X_CODERATE_1_2: x /= 2; break;
        case IT95X_CODERATE_2_3: x = (x * 2) / 3; break;
        case IT95X_CODERATE_3_4: x = (x * 3) / 4; break;
        case IT95X_CODERATE_5_6: x = (x * 5) / 6; break;
        case IT95X_CODERATE_7_8: x = (x * 7) / 8; break;
        default:
            return 0;
    }

    return x;
}

int it95x_bitrate_dvbt(uint32_t bandwidth, const it95x_dvbt_t *dvbt
                       , uint32_t *bitrate)
{
    /* 64-bit to avoid integer overflow */
    const uint64_t rate = base_rate(bandwidth
                                    , dvbt->constellation
                                    , dvbt->guardinterval
                                    , dvbt->coderate);

    /* 1512/2048 * 188/204 * 64/56 = 423/544 */
    *bitrate = (rate * 423ULL) / 544;

    /*
     * NOTE: There's a driver (?) issue where if the input TS is null-padded
     *       exactly to the channel rate, transmit latencies can add up
     *       and eventually cause an overflow in the transmit ring. Make
     *       the advertised rate slightly lower to compensate.
     */
    *bitrate -= TS_PACKET_BITS;

    if (*bitrate == 0)
        return IT95X_INVALID_DATA;
    else
        return 0;
}

int it95x_bitrate_isdbt(uint32_t bandwidth, const it95x_isdbt_t *isdbt
                        , uint32_t *a_bitrate, uint32_t *b_bitrate)
{
    const uint64_t a_rate = base_rate(bandwidth
                                      , isdbt->a.constellation
                                      , isdbt->guardinterval
                                      , isdbt->a.coderate);

    if (isdbt->partial)
    {
        /*
         * NOTE: Segment count is hardcoded and can't be changed.
         *       Layer C transmission is not supported by the IT9517.
         */
        const uint64_t a_seg = 1;
        const uint64_t b_seg = 12;

        const uint64_t b_rate = base_rate(bandwidth
                                          , isdbt->b.constellation
                                          , isdbt->guardinterval
                                          , isdbt->b.coderate);

        *b_bitrate = (b_rate * 188ULL * b_seg) / 3213;
        *a_bitrate = (a_rate * 188ULL * a_seg) / 3213;
        /* FIXME: add latency compensation? */

        if (*a_bitrate == 0 || *b_bitrate == 0)
            return IT95X_INVALID_DATA;
    }
    else
    {
        *b_bitrate = 0;
        *a_bitrate = (a_rate * 188ULL * 13ULL) / 3213; /* all 13 segments */

        /* see DVB-T bitrate function for explanation */
        *a_bitrate -= TS_PACKET_BITS;

        if (*a_bitrate == 0)
            return IT95X_INVALID_DATA;
    }

    return 0;
}
