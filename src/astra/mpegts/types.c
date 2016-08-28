/*
 * Astra Module: MPEG-TS (extended functions)
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

#include <astra/astra.h>
#include <astra/mpegts/types.h>

static const stream_type_t stream_types[256] = {
    /* 0x00 */ { MPEGTS_PACKET_UNKNOWN, NULL },
    /* 0x01 */ { MPEGTS_PACKET_VIDEO,   "MPEG-1 video, ISO/IEC 11172-2" },
    /* 0x02 */ { MPEGTS_PACKET_VIDEO,   "MPEG-2 video, ISO/IEC 13818-2" },
    /* 0x03 */ { MPEGTS_PACKET_AUDIO,   "MPEG-1 audio, ISO/IEC 11172-3" },
    /* 0x04 */ { MPEGTS_PACKET_AUDIO,   "MPEG-2 audio, ISO/IEC 13818-3" },
    /* 0x05 */ { MPEGTS_PACKET_DATA,    "Private sections, ISO/IEC 13818-1" },
    /* 0x06 */ { MPEGTS_PACKET_DATA,    "Private PES, ISO/IEC 13818-1" },
    /* 0x07 */ { MPEGTS_PACKET_DATA,    "MHEG, ISO/IEC 13522-5" },
    /* 0x08 */ { MPEGTS_PACKET_DATA,    "DSM-CC, ISO/IEC 13818-1" },
    /* 0x09 */ { MPEGTS_PACKET_DATA,    "Auxiliary data, ISO/IEC 13818-1" },
    /* 0x0a */ { MPEGTS_PACKET_DATA,    "DSM-CC multiprotocol encapsulation, ISO/IEC 13818-6" },
    /* 0x0b */ { MPEGTS_PACKET_DATA,    "DSM-CC U-N messages, ISO/IEC 13818-6" },
    /* 0x0c */ { MPEGTS_PACKET_DATA,    "DSM-CC stream descriptors, ISO/IEC 13818-6" },
    /* 0x0d */ { MPEGTS_PACKET_DATA,    "DSM-CC sections, ISO/IEC 13818-6" },
    /* 0x0e */ { MPEGTS_PACKET_DATA,    "Auxiliary data, ISO/IEC 13818-1" },
    /* 0x0f */ { MPEGTS_PACKET_AUDIO,   "ADTS AAC, ISO/IEC 13818-7" },
    /* 0x10 */ { MPEGTS_PACKET_VIDEO,   "MPEG-4 Part 2, ISO/IEC 14496-2" },
    /* 0x11 */ { MPEGTS_PACKET_AUDIO,   "LATM AAC, ISO/IEC 14496-3" },
    /* 0x12 */ { MPEGTS_PACKET_DATA,    "MPEG-4 FlexMux PES, ISO/IEC 14496-1" },
    /* 0x13 */ { MPEGTS_PACKET_DATA,    "MPEG-4 FlexMux sections, ISO/IEC 14496-1" },
    /* 0x14 */ { MPEGTS_PACKET_DATA,    "DSM-CC Synchronized Download Protocol, ISO/IEC 13818-6" },
    /* 0x15 */ { MPEGTS_PACKET_DATA,    "Metadata in PES" },
    /* 0x16 */ { MPEGTS_PACKET_DATA,    "Metadata in sections" },
    /* 0x17 */ { MPEGTS_PACKET_DATA,    "DSM-CC Data Carousel metadata, ISO/IEC 13818-6" },
    /* 0x18 */ { MPEGTS_PACKET_DATA,    "DSM-CC Object Carousel metadata, ISO/IEC 13818-6" },
    /* 0x19 */ { MPEGTS_PACKET_DATA,    "DSM-CC Synchronized Download Protocol metadata, ISO/IEC 13818-6" },
    /* 0x1a */ { MPEGTS_PACKET_DATA,    "MPEG-2 IPMP stream, ISO/IEC 13818-11" },
    /* 0x1b */ { MPEGTS_PACKET_VIDEO,   "MPEG-4 AVC/H.264, ISO/IEC 14496-10" },
    /* 0x1c */ { MPEGTS_PACKET_UNKNOWN, NULL },
    /* 0x1d */ { MPEGTS_PACKET_UNKNOWN, NULL },
    /* 0x1e */ { MPEGTS_PACKET_UNKNOWN, NULL },
    /* 0x1f */ { MPEGTS_PACKET_UNKNOWN, NULL },
    /* 0x20 */ { MPEGTS_PACKET_UNKNOWN, NULL },
    /* 0x21 */ { MPEGTS_PACKET_UNKNOWN, NULL },
    /* 0x22 */ { MPEGTS_PACKET_UNKNOWN, NULL },
    /* 0x23 */ { MPEGTS_PACKET_UNKNOWN, NULL },
    /* 0x24 */ { MPEGTS_PACKET_VIDEO,   "HEVC/H.265, ISO/IEC 23008-2" },
};

static const stream_type_t reserved_stream[] = {
    /* 0x00 - 0x7F */
    { MPEGTS_PACKET_DATA, "Reserved" },

    /* 0x80 - 0xFF */
    { MPEGTS_PACKET_DATA, "User private" },
};

const stream_type_t *mpegts_stream_type(uint8_t type_id)
{
    const stream_type_t *st = &stream_types[type_id];

    if (st->pkt_type == 0)
    {
        const bool user_private = type_id & 0x80;
        return &reserved_stream[user_private];
    }

    return st;
}

mpegts_packet_type_t mpegts_priv_type(uint8_t desc_type)
{
    switch (desc_type)
    {
        case 0x46: /* VBI teletext */
        case 0x56: /* EBU teletext */
        case 0x59: /* DVB subtitles */
            return MPEGTS_PACKET_SUB;

        case 0x6A: /* AC3 audio */
            return MPEGTS_PACKET_AUDIO;

        default:
            return MPEGTS_PACKET_DATA;
    }
}

const char *mpegts_type_name(mpegts_packet_type_t type)
{
    switch(type)
    {
        case MPEGTS_PACKET_PAT:
            return "PAT";
        case MPEGTS_PACKET_CAT:
            return "CAT";
        case MPEGTS_PACKET_PMT:
            return "PMT";
        case MPEGTS_PACKET_VIDEO:
            return "VIDEO";
        case MPEGTS_PACKET_AUDIO:
            return "AUDIO";
        case MPEGTS_PACKET_SUB:
            return "SUB";
        case MPEGTS_PACKET_DATA:
            return "DATA";
        case MPEGTS_PACKET_ECM:
            return "ECM";
        case MPEGTS_PACKET_EMM:
            return "EMM";
        default:
            return "UNKN";
    }
}
