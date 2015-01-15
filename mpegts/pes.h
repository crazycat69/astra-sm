/*
 * Astra Module: MPEG-TS (PES headers)
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

#ifndef _TS_PES_
#define _TS_PES_ 1

/*
 * oooooooooo ooooooooooo  oooooooo8
 *  888    888 888    88  888
 *  888oooo88  888ooo8     888oooooo
 *  888        888    oo          888
 * o888o      o888ooo8888 o88oooo888
 *
 */

#define PES_MAX_SIZE 0x000A0000

#define PES_HEADER_SIZE 6

#define PES_BUFFER_GET_SIZE(_b) (((_b[4] << 8) | _b[5]) + 6)
#define PES_BUFFER_GET_HEADER(_b) ((_b[0] << 16) | (_b[1] << 8) | (_b[2]))

typedef struct
{
    mpegts_packet_type_t type;
    uint16_t pid;
    uint8_t cc;

    uint64_t block_time_begin;
    uint64_t block_time_total;

    // demux
    uint8_t ts[TS_PACKET_SIZE];

    uint32_t pcr_interval;
    uint64_t pcr_time;
    uint64_t pcr_time_offset;

    // mux
    uint32_t buffer_size;
    uint32_t buffer_skip;
    uint8_t buffer[PES_MAX_SIZE];
} mpegts_pes_t;

typedef void (*pes_callback_t)(void *, mpegts_pes_t *);

mpegts_pes_t * mpegts_pes_init(mpegts_packet_type_t type, uint16_t pid, uint32_t pcr_interval);
void mpegts_pes_destroy(mpegts_pes_t *pes);

void mpegts_pes_mux(mpegts_pes_t *pes, const uint8_t *ts, pes_callback_t callback, void *arg);
void mpegts_pes_demux(mpegts_pes_t *pes, ts_callback_t callback, void *arg);

#define PES_IS_SYNTAX_SPEC(_pes)                                                                \
    (                                                                                           \
        _pes->buffer[3] != 0xBC && /* program_stream_map */                                     \
        _pes->buffer[3] != 0xBE && /* padding_stream */                                         \
        _pes->buffer[3] != 0xBF && /* private_stream_2 */                                       \
        _pes->buffer[3] != 0xF0 && /* ECM */                                                    \
        _pes->buffer[3] != 0xF1 && /* EMM */                                                    \
        _pes->buffer[3] != 0xF2 && /* DSMCC_stream */                                           \
        _pes->buffer[3] != 0xF8 && /* ITU-T Rec. H.222.1 type E */                              \
        _pes->buffer[3] != 0xFF    /* program_stream_directory */                               \
    )

#define PES_INIT(_pes, _stream_id, _is_pts, _is_dts)                                            \
    {                                                                                           \
        const uint8_t __stream_id = _stream_id;                                                 \
        _pes->buffer[0] = 0x00;                                                                 \
        _pes->buffer[1] = 0x00;                                                                 \
        _pes->buffer[2] = 0x01;                                                                 \
        _pes->buffer[3] = __stream_id;                                                          \
        _pes->buffer[4] = 0x00;                                                                 \
        _pes->buffer[5] = 0x00;                                                                 \
        _pes->buffer_size = PES_HEADER_SIZE;                                                    \
        if(PES_IS_SYNTAX_SPEC(_pes))                                                            \
        {                                                                                       \
            _pes->buffer[6] = 0x80;                                                             \
            _pes->buffer[7] = 0x00;                                                             \
            _pes->buffer[8] = 0;                                                                \
            _pes->buffer_size += 3;                                                             \
            if(_is_pts)                                                                         \
            {                                                                                   \
                _pes->buffer[7] = _pes->buffer[7] | 0x80;                                       \
                _pes->buffer[8] += 5;                                                           \
                _pes->buffer_size += 5;                                                         \
                if(_is_dts)                                                                     \
                {                                                                               \
                    _pes->buffer[7] = _pes->buffer[7] | 0x40;                                   \
                    _pes->buffer[8] += 5;                                                       \
                    _pes->buffer_size += 5;                                                     \
                }                                                                               \
            }                                                                                   \
        }                                                                                       \
    }

#define __PES_IS_PTS(_pes) (PES_IS_SYNTAX_SPEC(_pes) && (_pes->buffer[7] & 0x80))

#define PES_GET_PTS(_pes)                                                                       \
    ((!__PES_IS_PTS(_pes)) ? (0) : (                                                            \
        (uint64_t)((_pes->buffer[9 ] & 0x0E) << 29) |                                           \
                  ((_pes->buffer[10]       ) << 22) |                                           \
                  ((_pes->buffer[11] & 0xFE) << 14) |                                           \
                  ((_pes->buffer[12]       ) << 7 ) |                                           \
                  ((_pes->buffer[13]       ) >> 1 )                                             \
    ))

#define PES_SET_PTS(_pes, _pts)                                                                 \
    {                                                                                           \
        asc_assert(__PES_IS_PTS(_pes), "PTS flag is not set");                                  \
        const uint64_t __pts = _pts;                                                            \
        _pes->buffer[9] = 0x20 | ((__pts >> 29) & 0x0E) | 0x01;                                 \
        _pes->buffer[10] = ((__pts >> 22) & 0xFF);                                              \
        _pes->buffer[11] = ((__pts >> 14) & 0xFE) | 0x01;                                       \
        _pes->buffer[12] = ((__pts >> 7 ) & 0xFF);                                              \
        _pes->buffer[13] = ((__pts << 1 ) & 0xFE) | 0x01;                                       \
    }

#define __PES_IS_DTS(_pes) (PES_IS_SYNTAX_SPEC(_pes) && (_pes->buffer[7] & 0x40))

#define PES_GET_DTS(_pes)                                                                       \
    ((!__PES_IS_DTS(_pes)) ? (0) : (                                                            \
        (uint64_t)((_pes->buffer[14] & 0x0E) << 29) |                                           \
                  ((_pes->buffer[15]       ) << 22) |                                           \
                  ((_pes->buffer[16] & 0xFE) << 14) |                                           \
                  ((_pes->buffer[17]       ) << 7 ) |                                           \
                  ((_pes->buffer[18]       ) >> 1 )                                             \
    ))

#define PES_SET_DTS(_pes, _dts)                                                                 \
    {                                                                                           \
        asc_assert(__PES_IS_DTS(_pes), "DTS flag is not set");                                  \
        const uint64_t __dts = _dts;                                                            \
        _pes->buffer[9] = _pes->buffer[9] | 0x10;                                               \
        _pes->buffer[14] = 0x10 | ((__dts >> 29) & 0x0E) | 0x01;                                \
        _pes->buffer[15] = ((__dts >> 22) & 0xFF);                                              \
        _pes->buffer[16] = ((__dts >> 14) & 0xFE) | 0x01;                                       \
        _pes->buffer[17] = ((__dts >> 7 ) & 0xFF);                                              \
        _pes->buffer[18] = ((__dts << 1 ) & 0xFE) | 0x01;                                       \
    }

#define PES_SET_SIZE(_pes)                                                                      \
    {                                                                                           \
        if(_pes->type != MPEGTS_PACKET_VIDEO)                                                   \
        {                                                                                       \
            const uint16_t __size = _pes->buffer_size - PES_HEADER_SIZE;                        \
            _pes->buffer[4] = (__size >> 8) & 0xFF;                                             \
            _pes->buffer[5] = (__size     ) & 0xFF;                                             \
        }                                                                                       \
        else                                                                                    \
        {                                                                                       \
            _pes->buffer[4] = 0x00;                                                             \
            _pes->buffer[5] = 0x00;                                                             \
        }                                                                                       \
    }

#endif /* _TS_PES_ */
