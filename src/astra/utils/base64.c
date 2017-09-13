/*
 * Astra Utils (Base64)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *                    2017, Artem Kharitonov <artem@3phase.pw>
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
#include <astra/utils/base64.h>

static
const char b64_list[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static
const uint8_t b64_table[256] =
{
    /* ASCII table */
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
};

char *au_base64_enc(const void *data, size_t data_len, size_t *b64_len)
{
    const size_t out_len = ((data_len + 2) / 3) * 4;
    char *const out = ASC_ALLOC(out_len + 1, char);

    const uint8_t *pi = (uint8_t *)data;
    const uint8_t *const end = pi + data_len;
    char *po = out;

    for (; pi + 2 < end; pi += 3)
    {
        *(po++) = b64_list[(pi[0] >> 2) & 0x3f];
        *(po++) = b64_list[((pi[0] << 4) & 0x30) | ((pi[1] >> 4) & 0xf)];
        *(po++) = b64_list[((pi[1] << 2) & 0x3c) | ((pi[2] >> 6) & 0x3)];
        *(po++) = b64_list[pi[2] & 0x3f];
    }

    if (pi < end)
    {
        /* add padding */
        *(po++) = b64_list[(pi[0] >> 2) & 0x3f];
        if (pi + 1 == end)
        {
            *(po++) = b64_list[(pi[0] << 4) & 0x30];
            *(po++) = '=';
        }
        else
        {
            *(po++) = b64_list[((pi[0] << 4) & 0x30) | ((pi[1] >> 4) & 0xf)];
            *(po++) = b64_list[(pi[1] << 2) & 0x3c];
        }
        *(po++) = '=';
    }

    *(po++) = '\0';

    if (b64_len != NULL)
        *b64_len = out_len; /* not including terminating null byte */

    return out;
}

void *au_base64_dec(const char *data, size_t data_len, size_t *plain_len)
{
    size_t left;
    for (left = 0; left < data_len; left++)
    {
        if (b64_table[(uint8_t)data[left]] > 63)
            break;
    }

    size_t out_len = ((left + 3) / 4) * 3;
    uint8_t *const out = ASC_ALLOC(out_len + 1, uint8_t);

    const uint8_t *pi = (uint8_t *)data;
    uint8_t *po = out;

    while (left > 4)
    {
        *(po++) = (uint8_t)(b64_table[pi[0]] << 2) | (b64_table[pi[1]] >> 4);
        *(po++) = (uint8_t)(b64_table[pi[1]] << 4) | (b64_table[pi[2]] >> 2);
        *(po++) = (uint8_t)(b64_table[pi[2]] << 6) | (b64_table[pi[3]]);

        pi += 4;
        left -= 4;
    }

    if (left > 1)
        *(po++) = (uint8_t)(b64_table[pi[0]] << 2) | (b64_table[pi[1]] >> 4);

    if (left > 2)
        *(po++) = (uint8_t)(b64_table[pi[1]] << 4) | (b64_table[pi[2]] >> 2);

    if (left > 3)
        *(po++) = (uint8_t)(b64_table[pi[2]] << 6) | (b64_table[pi[3]]);

    /* add terminator in case the return value is used as a string */
    *(po++) = '\0';

    if (plain_len != NULL)
    {
        out_len -= (4 - left) & 3;
        *plain_len = out_len; /* does not include null byte */
    }

    return out;
}
