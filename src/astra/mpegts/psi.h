/*
 * Astra Module: MPEG-TS (PSI headers)
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

#ifndef _TS_PSI_
#define _TS_PSI_

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

#include <astra/utils/crc32b.h>

/*
 * common definitions
 */

#define PSI_MAX_SIZE 0x00000FFF
#define PSI_HEADER_SIZE 3
#define PSI_BUFFER_GET_SIZE(_b) \
    (PSI_HEADER_SIZE + (((_b[1] & 0x0f) << 8) | _b[2]))

typedef struct
{
    mpegts_packet_type_t type;
    uint16_t pid;
    uint8_t cc;

    uint32_t crc32;

    // demux
    uint8_t ts[TS_PACKET_SIZE];

    // mux
    uint16_t buffer_size;
    uint16_t buffer_skip;
    uint8_t buffer[PSI_MAX_SIZE];
} mpegts_psi_t;

typedef void (*psi_callback_t)(void *, mpegts_psi_t *);

mpegts_psi_t *mpegts_psi_init(mpegts_packet_type_t type, uint16_t pid) __wur;
void mpegts_psi_destroy(mpegts_psi_t *psi);

void mpegts_psi_mux(mpegts_psi_t *psi, const uint8_t *ts, psi_callback_t callback, void *arg);
void mpegts_psi_demux(mpegts_psi_t *psi, ts_callback_t callback, void *arg);

#define PSI_CALC_CRC32(_psi) \
    au_crc32b(_psi->buffer, _psi->buffer_size - CRC32_SIZE)

// with inline function we have nine more instructions
#define PSI_GET_CRC32(_psi) ( \
    (_psi->buffer[_psi->buffer_size - CRC32_SIZE + 0] << 24) | \
    (_psi->buffer[_psi->buffer_size - CRC32_SIZE + 1] << 16) | \
    (_psi->buffer[_psi->buffer_size - CRC32_SIZE + 2] << 8 ) | \
    (_psi->buffer[_psi->buffer_size - CRC32_SIZE + 3]      ) )

#define PSI_SET_CRC32(_psi) \
    do { \
        const uint32_t __crc = PSI_CALC_CRC32(_psi); \
        _psi->buffer[_psi->buffer_size - CRC32_SIZE + 0] = __crc >> 24; \
        _psi->buffer[_psi->buffer_size - CRC32_SIZE + 1] = __crc >> 16; \
        _psi->buffer[_psi->buffer_size - CRC32_SIZE + 2] = __crc >> 8; \
        _psi->buffer[_psi->buffer_size - CRC32_SIZE + 3] = __crc & 0xFF; \
    } while (0)

#define PSI_SET_SIZE(_psi) \
    do { \
        const uint16_t __size = _psi->buffer_size - PSI_HEADER_SIZE; \
        _psi->buffer[1] = (_psi->buffer[1] & 0xF0) | ((__size >> 8) & 0x0F); \
        _psi->buffer[2] = (__size & 0xFF); \
    } while (0)

/*
 * CA descriptors
 */

#define DESC_CA_CAID(_desc) ((_desc[2] << 8) | _desc[3])
#define DESC_CA_PID(_desc) (((_desc[4] & 0x1F) << 8) | _desc[5])

/*
 * PAT (Program Association Table)
 */

#define PAT_INIT(_psi, _tsid, _version) \
    do { \
        _psi->buffer[0] = 0x00; \
        _psi->buffer[1] = 0x80 | 0x30; \
        PAT_SET_TSID(_psi, _tsid); \
        _psi->buffer[5] = 0x01; \
        PAT_SET_VERSION(_psi, _version); \
        _psi->buffer[6] = 0x00; \
        _psi->buffer[7] = 0x00; \
        _psi->buffer_size = 8 + CRC32_SIZE; \
        PSI_SET_SIZE(_psi); \
    } while (0)

#define PAT_GET_TSID(_psi) ((_psi->buffer[3] << 8) | _psi->buffer[4])
#define PAT_SET_TSID(_psi, _tsid) \
    do { \
        const uint16_t __tsid = _tsid; \
        _psi->buffer[3] = __tsid >> 8; \
        _psi->buffer[4] = __tsid & 0xFF; \
    } while (0)

#define PAT_GET_VERSION(_psi) ((_psi->buffer[5] & 0x3E) >> 1)
#define PAT_SET_VERSION(_psi, _version) \
    do { \
        _psi->buffer[5] = 0xC0 | (((_version) << 1) & 0x3E) | (_psi->buffer[5] & 0x01); \
    } while (0)

#define PAT_ITEMS_FIRST(_psi) (&_psi->buffer[8])
#define PAT_ITEMS_EOL(_psi, _pointer) \
    ((_pointer - _psi->buffer) >= (_psi->buffer_size - CRC32_SIZE))
#define PAT_ITEMS_NEXT(_psi, _pointer) _pointer += 4

#define PAT_ITEMS_APPEND(_psi, _pnr, _pid) \
    do { \
        uint8_t *const __pointer_a = &_psi->buffer[_psi->buffer_size - CRC32_SIZE]; \
        PAT_ITEM_SET_PNR(_psi, __pointer_a, _pnr); \
        PAT_ITEM_SET_PID(_psi, __pointer_a, _pid); \
        _psi->buffer_size += 4; \
        PSI_SET_SIZE(_psi); \
    } while (0)

#define PAT_ITEMS_FOREACH(_psi, _ptr) \
    for(_ptr = PAT_ITEMS_FIRST(_psi) \
        ; !PAT_ITEMS_EOL(_psi, _ptr) \
        ; PAT_ITEMS_NEXT(_psi, _ptr))

#define PAT_ITEM_GET_PNR(_psi, _pointer) ((_pointer[0] << 8) | _pointer[1])
#define PAT_ITEM_GET_PID(_psi, _pointer) (((_pointer[2] & 0x1F) << 8) | _pointer[3])

#define PAT_ITEM_SET_PNR(_psi, _pointer, _pnr) \
    do { \
        uint8_t *const __pointer = _pointer; \
        const uint16_t __pnr = _pnr; \
        __pointer[0] = __pnr >> 8; \
        __pointer[1] = __pnr & 0xFF; \
    } while (0)

#define PAT_ITEM_SET_PID(_psi, _pointer, _pid) \
    do { \
        uint8_t *const __pointer = _pointer; \
        const uint16_t __pid = _pid; \
        __pointer[2] = 0xE0 | ((__pid >> 8) & 0x1F); \
        __pointer[3] = __pid & 0xFF; \
    } while (0)

/*
 * CAT (Conditional Access Table)
 */

#define CAT_GET_VERSION(_psi) PAT_GET_VERSION(_psi)
#define CAT_SET_VERSION(_psi, _version) PAT_SET_VERSION(_psi, _version)

#define CAT_DESC_FIRST(_psi) (&_psi->buffer[8])
#define CAT_DESC_EOL(_psi, _desc_pointer) PAT_ITEMS_EOL(_psi, _desc_pointer)
#define CAT_DESC_NEXT(_psi, _desc_pointer) _desc_pointer += 2 + _desc_pointer[1]

#define CAT_DESC_FOREACH(_psi, _ptr) \
    for(_ptr = CAT_DESC_FIRST(_psi) \
        ; !CAT_DESC_EOL(_psi, _ptr) \
        ; CAT_DESC_NEXT(_psi, _ptr))

/*
 * PMT (Program Map Table)
 */

#define PMT_INIT(_psi, _pnr, _version, _pcr, _desc, _desc_size) \
    do { \
        _psi->buffer[0] = 0x02; \
        _psi->buffer[1] = 0x80 | 0x30; \
        PMT_SET_PNR(_psi, _pnr); \
        _psi->buffer[5] = 0x01; \
        PMT_SET_VERSION(_psi, _version); \
        _psi->buffer[6] = 0x00; \
        _psi->buffer[7] = 0x00; \
        PMT_SET_PCR(_psi, _pcr); \
        const uint16_t __desc_size = _desc_size; \
        _psi->buffer[10] = 0xF0 | ((__desc_size >> 8) & 0x0F); \
        _psi->buffer[11] = __desc_size & 0xFF; \
        if(__desc_size > 0) \
        { \
            uint16_t __desc_skip = 0; \
            const uint8_t *const __desc = _desc; \
            while(__desc_skip < __desc_size) \
            { \
                memcpy(&_psi->buffer[12 + __desc_skip] \
                       , &__desc[__desc_skip] \
                       , 2 + __desc[__desc_skip + 1]); \
            } \
        } \
        _psi->buffer_size = 12 + __desc_size + CRC32_SIZE; \
        PSI_SET_SIZE(_psi); \
    } while (0)

#define PMT_GET_PNR(_psi) ((_psi->buffer[3] << 8) | _psi->buffer[4])
#define PMT_SET_PNR(_psi, _pnr) \
    do { \
        const uint16_t __pnr = _pnr; \
        _psi->buffer[3] = __pnr >> 8; \
        _psi->buffer[4] = __pnr & 0xFF; \
    } while (0)

#define PMT_GET_PCR(_psi) (((_psi->buffer[8] & 0x1F) << 8) | _psi->buffer[9])
#define PMT_SET_PCR(_psi, _pcr) \
    do { \
        const uint16_t __pcr = _pcr; \
        _psi->buffer[8] = 0xE0 | ((__pcr >> 8) & 0x1F); \
        _psi->buffer[9] = __pcr & 0xFF; \
    } while (0)

#define PMT_GET_VERSION(_psi) PAT_GET_VERSION(_psi)
#define PMT_SET_VERSION(_psi, _version) PAT_SET_VERSION(_psi, _version)

#define PMT_DESC_FIRST(_psi) (&_psi->buffer[12])
#define __PMT_DESC_SIZE(_psi) (((_psi->buffer[10] & 0x0F) << 8) | _psi->buffer[11])
#define PMT_DESC_EOL(_psi, _desc_pointer) \
    (_desc_pointer >= (PMT_DESC_FIRST(_psi) + __PMT_DESC_SIZE(_psi)))
#define PMT_DESC_NEXT(_psi, _desc_pointer) _desc_pointer += 2 + _desc_pointer[1]

#define PMT_DESC_FOREACH(_psi, _ptr) \
    for(_ptr = PMT_DESC_FIRST(_psi) \
        ; !PMT_DESC_EOL(_psi, _ptr) \
        ; PMT_DESC_NEXT(_psi, _ptr))

#define __PMT_ITEM_DESC_SIZE(_pointer) (((_pointer[3] & 0x0F) << 8) | _pointer[4])

#define PMT_ITEMS_FIRST(_psi) (PMT_DESC_FIRST(_psi) + __PMT_DESC_SIZE(_psi))
#define PMT_ITEMS_EOL(_psi, _pointer) PAT_ITEMS_EOL(_psi, _pointer)
#define PMT_ITEMS_NEXT(_psi, _pointer) _pointer += 5 + __PMT_ITEM_DESC_SIZE(_pointer)

#define PMT_ITEMS_APPEND(_psi, _type, _pid, _desc, _desc_size) \
    do { \
        uint8_t *const __pointer_a = &_psi->buffer[_psi->buffer_size - CRC32_SIZE]; \
        PMT_ITEM_SET_TYPE(_psi, __pointer_a, _type); \
        PMT_ITEM_SET_PID(_psi, __pointer_a, _pid); \
        const uint16_t __desc_size = _desc_size; \
        __pointer_a[3] = 0xF0 | ((__desc_size >> 8) & 0x0F); \
        __pointer_a[4] = __desc_size & 0xFF; \
        _psi->buffer_size += (__desc_size + 5); \
        PSI_SET_SIZE(_psi); \
    } while (0)

#define PMT_ITEMS_FOREACH(_psi, _ptr) \
    for(_ptr = PMT_ITEMS_FIRST(_psi) \
        ; !PMT_ITEMS_EOL(_psi, _ptr) \
        ; PMT_ITEMS_NEXT(_psi, _ptr))

#define PMT_ITEM_GET_TYPE(_psi, _pointer) _pointer[0]
#define PMT_ITEM_SET_TYPE(_psi, _pointer, _type) _pointer[0] = _type

#define PMT_ITEM_DESC_FIRST(_pointer) (&_pointer[5])
#define PMT_ITEM_DESC_EOL(_pointer, _desc_pointer) \
    (_desc_pointer >= PMT_ITEM_DESC_FIRST(_pointer) + __PMT_ITEM_DESC_SIZE(_pointer))
#define PMT_ITEM_DESC_NEXT(_pointer, _desc_pointer) _desc_pointer += 2 + _desc_pointer[1]

#define PMT_ITEM_DESC_FOREACH(_ptr, _desc_ptr) \
    for(_desc_ptr = PMT_ITEM_DESC_FIRST(_ptr) \
        ; !PMT_ITEM_DESC_EOL(_ptr, _desc_ptr) \
        ; PMT_ITEM_DESC_NEXT(_ptr, _desc_ptr))

#define PMT_ITEM_GET_PID(_psi, _pointer) (((_pointer[1] & 0x1F) << 8) | _pointer[2])
#define PMT_ITEM_SET_PID(_psi, _pointer, _pid) \
    do { \
        uint8_t *const __pointer = _pointer; \
        const uint16_t __pid = _pid; \
        __pointer[1] = 0xE0 | ((__pid >> 8) & 0x1F); \
        __pointer[2] = __pid & 0xFF; \
    } while (0)

/*
 * SDT (Service Description Table)
 */

#define SDT_GET_TSID(_psi) ((_psi->buffer[3] << 8) | _psi->buffer[4])
#define SDT_SET_TSID(_psi, _tsid) \
    do { \
        const uint16_t __tsid = _tsid; \
        _psi->buffer[3] = __tsid >> 8; \
        _psi->buffer[4] = __tsid & 0xFF; \
    } while (0)

#define SDT_GET_SECTION_NUMBER(_psi) (_psi->buffer[6])
#define SDT_SET_SECTION_NUMBER(_psi, _id) _psi->buffer[6] = _id

#define SDT_GET_LAST_SECTION_NUMBER(_psi) (_psi->buffer[7])
#define SDT_SET_LAST_SECTION_NUMBER(_psi, _id) _psi->buffer[7] = _id

#define __SDT_ITEM_DESC_SIZE(_pointer) (((_pointer[3] & 0x0F) << 8) | _pointer[4])

#define SDT_ITEMS_FIRST(_psi) (&_psi->buffer[11])
#define SDT_ITEMS_EOL(_psi, _pointer) \
    ((_pointer - _psi->buffer) >= (_psi->buffer_size - CRC32_SIZE))
#define SDT_ITEMS_NEXT(_psi, _pointer) _pointer += 5 + __SDT_ITEM_DESC_SIZE(_pointer)

#define SDT_ITEMS_FOREACH(_psi, _ptr) \
    for(_ptr = SDT_ITEMS_FIRST(_psi) \
        ; !SDT_ITEMS_EOL(_psi, _ptr) \
        ; SDT_ITEMS_NEXT(_psi, _ptr))

#define SDT_ITEM_GET_SID(_psi, _pointer) ((_pointer[0] << 8) | _pointer[1])
#define SDT_ITEM_SET_SID(_psi, _pointer, _sid) \
    do { \
        const uint16_t __sid = _sid; \
        _pointer[0] = __sid >> 8; \
        _pointer[1] = __sid & 0xFF; \
    } while (0)

#define SDT_ITEM_DESC_FIRST(_pointer) (&_pointer[5])
#define SDT_ITEM_DESC_EOL(_pointer, _desc_pointer) \
    (_desc_pointer >= SDT_ITEM_DESC_FIRST(_pointer) + __SDT_ITEM_DESC_SIZE(_pointer))
#define SDT_ITEM_DESC_NEXT(_pointer, _desc_pointer) _desc_pointer += 2 + _desc_pointer[1]

#define SDT_ITEM_DESC_FOREACH(_ptr, _desc_ptr) \
    for(_desc_ptr = SDT_ITEM_DESC_FIRST(_ptr) \
        ; !SDT_ITEM_DESC_EOL(_ptr, _desc_ptr) \
        ; SDT_ITEM_DESC_NEXT(_ptr, _desc_ptr))

/*
 * EIT (Event Information Table)
 */

#define EIT_GET_PNR(_psi) ((_psi->buffer[3] << 8) | _psi->buffer[4])
#define EIT_SET_PNR(_psi, _pnr) \
    do { \
        const uint16_t __pnr = _pnr; \
        _psi->buffer[3] = __pnr >> 8; \
        _psi->buffer[4] = __pnr & 0xFF; \
    } while (0)

#define EIT_GET_TSID(_psi) ((_psi->buffer[8] << 8) | _psi->buffer[9])
#define EIT_GET_ONID(_psi) ((_psi->buffer[10] << 8) | _psi->buffer[11])

#define EIT_ITEMS_FIRST(_psi) (&_psi->buffer[14])
#define EIT_ITEMS_EOL(_psi, _pointer) \
     ((_pointer - _psi->buffer) >= (_psi->buffer_size - CRC32_SIZE))
#define EIT_ITEMS_NEXT(_psi, _pointer) _pointer += 12 + EIT_ITEM_DESC_SIZE(_pointer)

#define EIT_ITEMS_FOREACH(_psi, _ptr) \
    for(_ptr = EIT_ITEMS_FIRST(_psi) \
        ; !EIT_ITEMS_EOL(_psi, _ptr) \
        ; EIT_ITEMS_NEXT(_psi, _ptr))

#define EIT_ITEM_GET_EID(_pointer) ((_pointer[0] << 8) | _pointer[1])
#define EIT_ITEM_START_TM_MJD(_pointer) ((_pointer[2] << 8) | _pointer[3])
#define EIT_ITEM_START_TM_UTC(_pointer) ((_pointer[4] << 16) | (_pointer[5] << 8) | _pointer[6])
#define EIT_ITEM_DURATION(_pointer) ((_pointer[7] << 16) | (_pointer[8] << 8) | _pointer[9])
#define EIT_GET_RUN_STAT(_pointer) (_pointer[10] >> 5)
#define EIT_GET_FREE_CA(_pointer) ((_pointer[10] & 0x10) >> 4)
#define EIT_ITEM_DESC_SIZE(_pointer)    (((_pointer[10] & 0x0F) << 8) | _pointer[11])

#define EIT_ITEM_DURATION_SEC(_pointer) \
    ((_pointer[7] >> 4) * 10 + (_pointer[7] & 0x0F)) * 3600 + \
    ((_pointer[8] >> 4) * 10 + (_pointer[8] & 0x0F)) * 60 + \
    ((_pointer[9] >> 4) * 10 + (_pointer[9] & 0x0F))

#define EIT_ITEM_START_UT(_pointer) \
    (EIT_ITEM_START_TM_MJD(_pointer) - 40587) * 86400 + \
    ((_pointer[4] >> 4) * 10 + (_pointer[4] & 0x0F)) * 3600 + \
    ((_pointer[5] >> 4) * 10 + (_pointer[5] & 0x0F)) * 60 + \
    ((_pointer[6] >> 4) * 10 + (_pointer[6] & 0x0F))

#define EIT_ITEM_STOP_UT(_pointer) EIT_ITEM_START_UT(_pointer) + EIT_ITEM_DURATION_SEC(_pointer)

#define EIT_ITEM_DESC_FIRST(_pointer) (&_pointer[12])
#define EIT_ITEM_DESC_EOL(_pointer, _desc_pointer) \
    (_desc_pointer >= EIT_ITEM_DESC_FIRST(_pointer) + EIT_ITEM_DESC_SIZE(_pointer))
#define EIT_ITEM_DESC_NEXT(_pointer, _desc_pointer) _desc_pointer += 2 + _desc_pointer[1]

#define EIT_ITEM_DESC_FOREACH(_ptr, _desc_ptr) \
     for(_desc_ptr = EIT_ITEM_DESC_FIRST(_ptr) \
         ; !EIT_ITEM_DESC_EOL(_ptr, _desc_ptr) \
         ; EIT_ITEM_DESC_NEXT(_ptr, _desc_ptr))

#endif /* _TS_PSI_ */
