/*
 * Astra Utils (Str2Hex)
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
#include <astra/utils/strhex.h>

static
const char char_str[] = "0123456789ABCDEF";

char *au_hex2str(char *dst, const void *src, size_t srclen)
{
    uint8_t *const srcp = (uint8_t *)src;

    for (size_t i = 0; i < srclen; i++)
    {
        const size_t j = i * 2;
        dst[j + 0] = char_str[srcp[i] >> 4];
        dst[j + 1] = char_str[srcp[i] & 0x0f];
    }
    dst[srclen * 2] = '\0';

    return dst;
}

static inline
uint8_t nibble_to_bin(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    return 0;
}

static inline
uint8_t octet_to_bin(const char *c)
{
    return (nibble_to_bin(c[0]) << 4) | nibble_to_bin(c[1]);
}

void *au_str2hex(const char *src, void *dst, size_t dstlen)
{
    uint8_t *const dstp = (uint8_t *)dst;

    if (dstlen == 0)
        dstlen = ~0;

    for (size_t i = 0; src[0] && src[1] && i < dstlen; src += 2, ++i)
        dstp[i] = octet_to_bin(src);

    return dst;
}
