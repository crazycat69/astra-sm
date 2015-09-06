/*
 * Astra Module: MPEG-TS (basic definitions)
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

#ifndef _TS_CORE_
#define _TS_CORE_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

#define TS_PACKET_SIZE 188
#define TS_HEADER_SIZE 4
#define TS_BODY_SIZE (TS_PACKET_SIZE - TS_HEADER_SIZE)

#define M2TS_PACKET_SIZE 192

#define MAX_PID 8192
#define NULL_TS_PID (MAX_PID - 1)
#define DESC_MAX_SIZE 1024

#define TS_IS_SYNC(_ts) ((_ts[0] == 0x47))
#define TS_IS_PAYLOAD(_ts) ((_ts[3] & 0x10))
#define TS_IS_PAYLOAD_START(_ts) ((TS_IS_PAYLOAD(_ts) && (_ts[1] & 0x40)))
#define TS_IS_AF(_ts) ((_ts[3] & 0x20))
#define TS_IS_SCRAMBLED(_ts) ((_ts[3] & 0xC0))

#define TS_IS_RAI(__x) \
    ( \
        (TS_IS_SYNC(__x)) && \
        (TS_IS_AF(__x)) && \
        (__x[5] & 0x40) /* random access flag */ \
    )

#define TS_GET_PID(_ts) ((uint16_t)(((_ts[1] & 0x1F) << 8) | _ts[2]))
#define TS_SET_PID(_ts, _pid) \
    do { \
        uint8_t *__ts = _ts; \
        const uint16_t __pid = _pid; \
        __ts[1] = (__ts[1] & ~0x1F) | ((__pid >> 8) & 0x1F); \
        __ts[2] = __pid & 0xFF; \
    } while (0)

#define TS_GET_CC(_ts) (_ts[3] & 0x0F)
#define TS_SET_CC(_ts, _cc) \
    do { \
        _ts[3] = (_ts[3] & 0xF0) | ((_cc) & 0x0F); \
    } while (0)

#define TS_GET_PAYLOAD(_ts) ( \
    (!TS_IS_PAYLOAD(_ts)) ? (NULL) : ( \
        (!TS_IS_AF(_ts)) ? (&_ts[TS_HEADER_SIZE]) : ( \
            (_ts[4] > TS_BODY_SIZE - 1) ? (NULL) : (&_ts[TS_HEADER_SIZE + 1 + _ts[4]])) \
        ) \
    )

typedef void (*ts_callback_t)(void *, const uint8_t *);

#endif /* _TS_CORE_ */
