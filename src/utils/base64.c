/*
 * Astra Utils (Base64)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
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

#include <astra.h>
#include <utils/base64.h>

static const char base64_list[] = \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define XX 0

static const uint8_t base64_index[256] =
{
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
    52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,XX,XX,XX,
    XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
    XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};

char *au_base64_enc(const void *in, size_t in_size, size_t *out_size)
{
    if(!in)
        return NULL;

    size_t size = ((in_size + 2) / 3) * 4;

    char *const out = ASC_ALLOC(size + 1, char);

    for(size_t i = 0, j = 0; i < in_size;)
    {
        uint32_t octet_a = (i < in_size) ? ((uint8_t *)in)[i++] : 0;
        uint32_t octet_b = (i < in_size) ? ((uint8_t *)in)[i++] : 0;
        uint32_t octet_c = (i < in_size) ? ((uint8_t *)in)[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        out[j++] = base64_list[(triple >> 3 * 6) & 0x3F];
        out[j++] = base64_list[(triple >> 2 * 6) & 0x3F];
        out[j++] = base64_list[(triple >> 1 * 6) & 0x3F];
        out[j++] = base64_list[(triple >> 0 * 6) & 0x3F];
    }

    switch(in_size % 3)
    {
        case 0:
            break;
        case 1:
            out[size - 2] = '=';
        case 2:
            out[size - 1] = '=';
            break;
    }
    out[size] = 0;

    if(out_size)
        *out_size = size;

    return out;
}

void *au_base64_dec(const char *in, size_t in_size, size_t *out_size)
{
    if(in_size == 0)
    {
        while(in[in_size])
            ++in_size;
    }

    if(in_size % 4 != 0)
        return NULL;

    size_t size = (in_size / 4) * 3;

    if(in[in_size - 2] == '=')
        size -= 2;
    else if(in[in_size - 1] == '=')
        size -= 1;

    uint8_t *const out = ASC_ALLOC(size, uint8_t);

    for(size_t i = 0, j = 0; i < in_size;)
    {
        uint32_t sextet_a = (in[i] == '=') ? (0 & i++) : base64_index[(uint8_t)in[i++]];
        uint32_t sextet_b = (in[i] == '=') ? (0 & i++) : base64_index[(uint8_t)in[i++]];
        uint32_t sextet_c = (in[i] == '=') ? (0 & i++) : base64_index[(uint8_t)in[i++]];
        uint32_t sextet_d = (in[i] == '=') ? (0 & i++) : base64_index[(uint8_t)in[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
                        + (sextet_b << 2 * 6)
                        + (sextet_c << 1 * 6)
                        + (sextet_d << 0 * 6);

        if (j < size)
            out[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < size)
            out[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < size)
            out[j++] = (triple >> 0 * 8) & 0xFF;
    }

    if(out_size)
        *out_size = size;

    return out;
}
