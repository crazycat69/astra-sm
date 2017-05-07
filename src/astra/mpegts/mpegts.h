/*
 * Astra TS Library (Base definitions)
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

#ifndef _TS_MPEGTS_
#define _TS_MPEGTS_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

/* sizes and limits */
#define TS_PACKET_SIZE 188
#define TS_PACKET_BITS 1504
#define TS_HEADER_SIZE 4
#define TS_BODY_SIZE 184
#define TS_MAX_PIDS 8192
#define TS_MAX_PROGS 65536
#define TS_NULL_PID 0x1fff

typedef void (*ts_callback_t)(void *, const uint8_t *);

/* bounds checking for PIDs and program numbers */
static inline
bool ts_pid_valid(int pid)
{
    return (pid >= 0 && pid < TS_MAX_PIDS);
}

static inline
bool ts_pnr_valid(int pnr)
{
    return (pnr >= 1 && pnr < TS_MAX_PROGS);
}

/*
 * An array type representing a 188-byte TS packet, meant for ring
 * buffers and the like. That way there's no need to manually calculate
 * byte offsets for each packet in the buffer.
 */
typedef uint8_t ts_packet_t[TS_PACKET_SIZE];

/*
 * TS header
 */

/* scrambling control values */
enum
{
    TS_SC_NONE      = 0,
    TS_SC_RESERVED  = 1,
    TS_SC_EVEN      = 2,
    TS_SC_ODD       = 3,
};

/* initialize TS packet header */
#define TS_INIT(_ts) \
    do { \
        (_ts)[0] = 0x47; \
        (_ts)[1] = 0x0; \
        (_ts)[2] = 0x0; \
        (_ts)[3] = 0x0; \
    } while (0)

/* test for sync byte (ASCII 'G') */
#define TS_IS_SYNC(_ts) \
    ((_ts)[0] == 0x47)

/* transport error indicator (TEI) */
#define TS_IS_ERROR(_ts) \
    (((_ts)[1] & 0x80) != 0)

#define TS_SET_ERROR(_ts, _on) \
    do { \
        if (_on) \
            (_ts)[1] |= 0x80; \
        else \
            (_ts)[1] &= ~0x80; \
    } while (0)

/* payload presence bit */
#define TS_IS_PAYLOAD(_ts) \
    (((_ts)[3] & 0x10) != 0)

#define TS_SET_PAYLOAD(_ts, _on) \
    do { \
        if (_on) \
            (_ts)[3] |= 0x10; \
        else \
            (_ts)[3] &= ~0x10; \
    } while (0)

/* payload unit start indicator (PUSI) */
#define TS_IS_PUSI(_ts) \
    (TS_IS_PAYLOAD(_ts) && ((_ts)[1] & 0x40) != 0)

#define TS_SET_PUSI(_ts, _on) \
    do { \
        if (_on) \
            (_ts)[1] |= 0x40; \
        else \
            (_ts)[1] &= ~0x40; \
    } while (0)

/* transport priority bit */
#define TS_IS_PRIORITY(_ts) \
    (((_ts)[1] & 0x20) != 0)

#define TS_SET_PRIORITY(_ts, _on) \
    do { \
        if (_on) \
            (_ts)[1] |= 0x20; \
        else \
            (_ts)[1] &= ~0x20; \
    } while (0)

/* packet identifier (PID) */
#define TS_GET_PID(_ts) \
    ((((_ts)[1] << 8) & 0x1f00) | (_ts)[2])

#define TS_SET_PID(_ts, _pid) \
    do { \
        (_ts)[1] = ((_ts)[1] & ~0x1f) | (((_pid) >> 8) & 0x1f); \
        (_ts)[2] = (_pid) & 0xff; \
    } while (0)

/* scrambling control (SC) */
#define TS_GET_SC(_ts) \
    (((_ts)[3] >> 6) & 0x3)

#define TS_SET_SC(_ts, _sc) \
    do { \
        (_ts)[3] = ((_ts)[3] & ~0xc0) | (((_sc) << 6) & 0xc0); \
    } while (0)

/* continuity counter (CC) */
#define TS_GET_CC(_ts) \
    ((_ts)[3] & 0xf)

#define TS_SET_CC(_ts, _cc) \
    do { \
        (_ts)[3] = ((_ts)[3] & ~0xf) | ((_cc) & 0xf); \
    } while (0)

/* adaptation field presence bit */
#define TS_IS_AF(_ts) \
    (((_ts)[3] & 0x20) != 0)

#define TS_SET_AF(_ts, _len) \
    do { \
        (_ts)[3] |= 0x20; /* AF presence bit */ \
        (_ts)[4] = (_len) & 0xff; /* AF length (183 max) */ \
        if ((_len) > 0) \
            (_ts)[5] = 0x0; /* AF flags */ \
        if ((_len) > 1) \
            memset(&(_ts)[6], 0xff, (_len) - 1); /* stuffing */ \
    } while (0)

#define TS_CLEAR_AF(_ts) \
    do { \
        (_ts)[3] &= ~0x20; \
    } while (0)

/*
 * NOTE: AF length must be 183 bytes in a packet without payload,
 *       or up to 182 bytes if payload is present (leaving 1 byte
 *       for the payload).
 */

/*
 * Adaptation field
 */

/* get adaptation field length */
#define TS_AF_LEN(_ts) \
    (TS_IS_AF(_ts) ? (_ts)[4] : -1)

/* discontinuity indicator */
#define TS_IS_DISCONT(_ts) \
    (TS_AF_LEN(_ts) > 0 && ((_ts)[5] & 0x80) != 0)

#define TS_SET_DISCONT(_ts, _on) \
    do { \
        if (_on) \
            (_ts)[5] |= 0x80; \
        else \
            (_ts)[5] &= ~0x80; \
    } while (0)

/* random access indicator */
#define TS_IS_RANDOM(_ts) \
    (TS_AF_LEN(_ts) > 0 && ((_ts)[5] & 0x40) != 0)

#define TS_SET_RANDOM(_ts, _on) \
    do { \
        if (_on) \
            (_ts)[5] |= 0x40; \
        else \
            (_ts)[5] &= ~0x40; \
    } while (0)

/* ES priority indicator */
#define TS_IS_ES_PRIO(_ts) \
    (TS_AF_LEN(_ts) > 0 && ((_ts)[5] & 0x20) != 0)

#define TS_SET_ES_PRIO(_ts, _on) \
    do { \
        if (_on) \
            (_ts)[5] |= 0x20; \
        else \
            (_ts)[5] &= ~0x20; \
    } while (0)

/* PCR presence bit */
#define TS_IS_PCR(_ts) \
    (TS_AF_LEN(_ts) >= 7 && ((_ts)[5] & 0x10) != 0)

#define TS_CLEAR_PCR(_ts) \
    do { \
        (_ts)[5] &= ~0x10; \
    } while (0)

/*
 * TS payload
 */

/* get TS payload length */
static inline
unsigned int ts_payload_len(const uint8_t *ts, const uint8_t *payload)
{
    if ((uintptr_t)payload > (uintptr_t)ts)
        return TS_PACKET_SIZE - (payload - ts);
    else
        return 0;
}

/* get pointer to TS payload */
#define TS_GET_PAYLOAD(_ts) \
    ( \
        (TS_IS_PAYLOAD(_ts)) ? ( \
            (TS_IS_AF(_ts)) ? ( \
                (TS_AF_LEN(_ts) < TS_BODY_SIZE - 1) ? ( \
                    /* skip AF */ \
                    &(_ts)[TS_HEADER_SIZE + 1 + TS_AF_LEN(_ts)] \
                ) : ( \
                    /* invalid AF length */ \
                    NULL \
                ) \
            ) : ( \
                /* no AF, payload only */ \
                &(_ts)[TS_HEADER_SIZE] \
            ) \
        ) : ( \
            /* no payload */ \
            NULL \
        ) \
    )

#endif /* _TS_MPEGTS_ */
