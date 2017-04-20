/*
 * Astra TS Library (Type definitions)
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

#include <astra/astra.h>
#include <astra/mpegts/types.h>

static
const ts_stream_type_t stream_types[256] =
{
    { 0x00, TS_TYPE_UNKNOWN, NULL },
    { 0x01, TS_TYPE_VIDEO,
      "MPEG-1 video, ISO/IEC 11172-2" },
    { 0x02, TS_TYPE_VIDEO,
      "MPEG-2 video, ISO/IEC 13818-2" },
    { 0x03, TS_TYPE_AUDIO,
      "MPEG-1 audio, ISO/IEC 11172-3" },
    { 0x04, TS_TYPE_AUDIO,
      "MPEG-2 audio, ISO/IEC 13818-3" },
    { 0x05, TS_TYPE_DATA,
      "Private sections, ISO/IEC 13818-1" },
    { 0x06, TS_TYPE_DATA,
      "Private PES, ISO/IEC 13818-1" },
    { 0x07, TS_TYPE_DATA,
      "MHEG, ISO/IEC 13522-5" },
    { 0x08, TS_TYPE_DATA,
      "DSM-CC, ISO/IEC 13818-1" },
    { 0x09, TS_TYPE_DATA,
      "Auxiliary data, ISO/IEC 13818-1" },
    { 0x0a, TS_TYPE_DATA,
      "DSM-CC multiprotocol encapsulation, ISO/IEC 13818-6" },
    { 0x0b, TS_TYPE_DATA,
      "DSM-CC U-N messages, ISO/IEC 13818-6" },
    { 0x0c, TS_TYPE_DATA,
      "DSM-CC stream descriptors, ISO/IEC 13818-6" },
    { 0x0d, TS_TYPE_DATA,
      "DSM-CC sections, ISO/IEC 13818-6" },
    { 0x0e, TS_TYPE_DATA,
      "Auxiliary data, ISO/IEC 13818-1" },
    { 0x0f, TS_TYPE_AUDIO,
      "ADTS AAC, ISO/IEC 13818-7" },
    { 0x10, TS_TYPE_VIDEO,
      "MPEG-4 Part 2, ISO/IEC 14496-2" },
    { 0x11, TS_TYPE_AUDIO,
      "LATM AAC, ISO/IEC 14496-3" },
    { 0x12, TS_TYPE_DATA,
      "MPEG-4 FlexMux PES, ISO/IEC 14496-1" },
    { 0x13, TS_TYPE_DATA,
      "MPEG-4 FlexMux sections, ISO/IEC 14496-1" },
    { 0x14, TS_TYPE_DATA,
      "DSM-CC Synchronized Download Protocol, ISO/IEC 13818-6" },
    { 0x15, TS_TYPE_DATA,
      "Metadata in PES" },
    { 0x16, TS_TYPE_DATA,
      "Metadata in sections" },
    { 0x17, TS_TYPE_DATA,
      "DSM-CC Data Carousel metadata, ISO/IEC 13818-6" },
    { 0x18, TS_TYPE_DATA,
      "DSM-CC Object Carousel metadata, ISO/IEC 13818-6" },
    { 0x19, TS_TYPE_DATA,
      "DSM-CC Synchronized Download Protocol metadata, ISO/IEC 13818-6" },
    { 0x1a, TS_TYPE_DATA,
      "MPEG-2 IPMP stream, ISO/IEC 13818-11" },
    { 0x1b, TS_TYPE_VIDEO,
      "MPEG-4 AVC/H.264, ISO/IEC 14496-10" },
    { 0x1c, TS_TYPE_UNKNOWN, NULL },
    { 0x1d, TS_TYPE_UNKNOWN, NULL },
    { 0x1e, TS_TYPE_UNKNOWN, NULL },
    { 0x1f, TS_TYPE_UNKNOWN, NULL },
    { 0x20, TS_TYPE_UNKNOWN, NULL },
    { 0x21, TS_TYPE_UNKNOWN, NULL },
    { 0x22, TS_TYPE_UNKNOWN, NULL },
    { 0x23, TS_TYPE_UNKNOWN, NULL },
    { 0x24, TS_TYPE_VIDEO,
      "HEVC/H.265, ISO/IEC 23008-2" },
};

static
const ts_stream_type_t reserved_stream[] =
{
    /* 0x00 - 0x7F */
    { 0x00, TS_TYPE_DATA, "Reserved" },

    /* 0x80 - 0xFF */
    { 0x80, TS_TYPE_DATA, "User private" },
};

const ts_stream_type_t *ts_stream_type(uint8_t type_id)
{
    const ts_stream_type_t *const st = &stream_types[type_id];

    if (st->pkt_type == 0)
    {
        const bool user_private = type_id & 0x80;
        return &reserved_stream[user_private];
    }

    return st;
}

ts_type_t ts_priv_type(uint8_t desc_type)
{
    switch (desc_type)
    {
        case 0x46: /* VBI teletext */
        case 0x56: /* EBU teletext */
        case 0x59: /* DVB subtitles */
            return TS_TYPE_SUB;

        case 0x6A: /* AC3 audio */
            return TS_TYPE_AUDIO;

        default:
            return TS_TYPE_DATA;
    }
}

const char *ts_type_name(ts_type_t type)
{
    switch (type)
    {
        case TS_TYPE_PAT:   return "PAT";
        case TS_TYPE_CAT:   return "CAT";
        case TS_TYPE_PMT:   return "PMT";
        case TS_TYPE_VIDEO: return "VIDEO";
        case TS_TYPE_AUDIO: return "AUDIO";
        case TS_TYPE_SUB:   return "SUB";
        case TS_TYPE_DATA:  return "DATA";
        case TS_TYPE_ECM:   return "ECM";
        case TS_TYPE_EMM:   return "EMM";
        default:
            return "UNKN";
    }
}
