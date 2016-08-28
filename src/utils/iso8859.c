/*
 * Astra Utils (ISO-8859)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2013-2015, Andrey Dyldin <and@cesbo.com>
 *               2014, Vitaliy Batin <fyrerx@gmail.com>
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
#include <utils/iso8859.h>

static
uint8_t *iso8859_1_decode(const uint8_t *data, size_t size)
{
    uint8_t *const text = ASC_ALLOC(size * 2 + 1, uint8_t);
    uint8_t c;
    size_t i = 0, j = 0;

    while(i < size)
    {
        c = data[i++];
        if(c < 0x80)
        {
            if(!c) break;
            text[j++] = c;
        }
        else
        {
            text[j++] = 0xC0 | (c >> 6);
            text[j++] = 0x80 | (c & 0x3F);
        }
    }

    text[j] = '\0';
    return text;
}

static
uint8_t *iso8859_2_decode(const uint8_t *data, size_t size)
{
    uint8_t *const text = ASC_ALLOC(size * 2 + 1, uint8_t);
    uint8_t c;
    size_t i = 0, j = 0;

    static const uint8_t map[][2] =
    {
        { 0xC2, 0xA0 }, { 0xC4, 0x84 }, { 0xCB, 0x98 }, { 0xC5, 0x81 },
        { 0xC2, 0xA4 }, { 0xC4, 0xBD }, { 0xC5, 0x9A }, { 0xC2, 0xA7 },
        { 0xC2, 0xA8 }, { 0xC5, 0xA0 }, { 0xC5, 0x9E }, { 0xC5, 0xA4 },
        { 0xC5, 0xB9 }, { 0xC2, 0xAD }, { 0xC5, 0xBD }, { 0xC5, 0xBB },
        { 0xC2, 0xB0 }, { 0xC4, 0x85 }, { 0xCB, 0x9B }, { 0xC5, 0x82 },
        { 0xC2, 0xB4 }, { 0xC4, 0xBE }, { 0xC5, 0x9B }, { 0xCB, 0x87 },
        { 0xC2, 0xB8 }, { 0xC5, 0xA1 }, { 0xC5, 0x9F }, { 0xC5, 0xA5 },
        { 0xC5, 0xBA }, { 0xCB, 0x9D }, { 0xC5, 0xBE }, { 0xC5, 0xBC },
        { 0xC5, 0x94 }, { 0xC3, 0x81 }, { 0xC3, 0x82 }, { 0xC4, 0x82 },
        { 0xC3, 0x84 }, { 0xC4, 0xB9 }, { 0xC4, 0x86 }, { 0xC3, 0x87 },
        { 0xC4, 0x8C }, { 0xC3, 0x89 }, { 0xC4, 0x98 }, { 0xC3, 0x8B },
        { 0xC4, 0x9A }, { 0xC3, 0x8D }, { 0xC3, 0x8E }, { 0xC4, 0x8E },
        { 0xC4, 0x90 }, { 0xC5, 0x83 }, { 0xC5, 0x87 }, { 0xC3, 0x93 },
        { 0xC3, 0x94 }, { 0xC5, 0x90 }, { 0xC3, 0x96 }, { 0xC3, 0x97 },
        { 0xC5, 0x98 }, { 0xC5, 0xAE }, { 0xC3, 0x9A }, { 0xC5, 0xB0 },
        { 0xC3, 0x9C }, { 0xC3, 0x9D }, { 0xC5, 0xA2 }, { 0xC3, 0x9F },
        { 0xC5, 0x95 }, { 0xC3, 0xA1 }, { 0xC3, 0xA2 }, { 0xC4, 0x83 },
        { 0xC3, 0xA4 }, { 0xC4, 0xBA }, { 0xC4, 0x87 }, { 0xC3, 0xA7 },
        { 0xC4, 0x8D }, { 0xC3, 0xA9 }, { 0xC4, 0x99 }, { 0xC3, 0xAB },
        { 0xC4, 0x9B }, { 0xC3, 0xAD }, { 0xC3, 0xAE }, { 0xC4, 0x8F },
        { 0xC4, 0x91 }, { 0xC5, 0x84 }, { 0xC5, 0x88 }, { 0xC3, 0xB3 },
        { 0xC3, 0xB4 }, { 0xC5, 0x91 }, { 0xC3, 0xB6 }, { 0xC3, 0xB7 },
        { 0xC5, 0x99 }, { 0xC5, 0xAF }, { 0xC3, 0xBA }, { 0xC5, 0xB1 },
        { 0xC3, 0xBC }, { 0xC3, 0xBD }, { 0xC5, 0xA3 }, { 0xCB, 0x99 }
    };

    while(i < size)
    {
        c = data[i++];

        if(c < 0x80)
        {
            if(!c) break;
            text[j++] = c;
        }
        else if(c < 0xA0) {}
        else
        {
            const uint8_t *item = map[c - 0xA0];
            text[j++] = item[0];
            text[j++] = item[1];
        }
    }

    text[j] = '\0';
    return text;
}

static
uint8_t *iso8859_4_decode(const uint8_t *data, size_t size)
{
    uint8_t *const text = ASC_ALLOC(size * 2 + 1, uint8_t);
    uint8_t c;
    size_t i = 0, j = 0;

    static const uint8_t map[][2] =
    {
        { 0xC2, 0xA0 }, { 0xC4, 0x84 }, { 0xC4, 0xB8 }, { 0xC5, 0x96 },
        { 0xC2, 0xA4 }, { 0xC4, 0xA8 }, { 0xC4, 0xBB }, { 0xC2, 0xA7 },
        { 0xC2, 0xA8 }, { 0xC5, 0xA0 }, { 0xC4, 0x92 }, { 0xC4, 0xA2 },
        { 0xC5, 0xA6 }, { 0xC2, 0xAD }, { 0xC5, 0xBD }, { 0xC2, 0xAF },
        { 0xC2, 0xB0 }, { 0xC4, 0x85 }, { 0xCB, 0x9B }, { 0xC5, 0x97 },
        { 0xC2, 0xB4 }, { 0xC4, 0xA9 }, { 0xC4, 0xBC }, { 0xCB, 0x87 },
        { 0xC2, 0xB8 }, { 0xC5, 0xA1 }, { 0xC4, 0x93 }, { 0xC4, 0xA3 },
        { 0xC5, 0xA7 }, { 0xC5, 0x8A }, { 0xC5, 0xBE }, { 0xC5, 0x8B },
        { 0xC4, 0x80 }, { 0xC3, 0x81 }, { 0xC3, 0x82 }, { 0xC3, 0x83 },
        { 0xC3, 0x84 }, { 0xC3, 0x85 }, { 0xC3, 0x86 }, { 0xC4, 0xAE },
        { 0xC4, 0x8C }, { 0xC3, 0x89 }, { 0xC4, 0x98 }, { 0xC3, 0x8B },
        { 0xC4, 0x96 }, { 0xC3, 0x8D }, { 0xC3, 0x8E }, { 0xC4, 0xAA },
        { 0xC4, 0x90 }, { 0xC5, 0x85 }, { 0xC5, 0x8C }, { 0xC4, 0xB6 },
        { 0xC3, 0x94 }, { 0xC3, 0x95 }, { 0xC3, 0x96 }, { 0xC3, 0x97 },
        { 0xC3, 0x98 }, { 0xC5, 0xB2 }, { 0xC3, 0x9A }, { 0xC3, 0x9B },
        { 0xC3, 0x9C }, { 0xC5, 0xA8 }, { 0xC5, 0xAA }, { 0xC3, 0x9F },
        { 0xC4, 0x81 }, { 0xC3, 0xA1 }, { 0xC3, 0xA2 }, { 0xC3, 0xA3 },
        { 0xC3, 0xA4 }, { 0xC3, 0xA5 }, { 0xC3, 0xA6 }, { 0xC4, 0xAF },
        { 0xC4, 0x8D }, { 0xC3, 0xA9 }, { 0xC4, 0x99 }, { 0xC3, 0xAB },
        { 0xC4, 0x97 }, { 0xC3, 0xAD }, { 0xC3, 0xAE }, { 0xC4, 0xAB },
        { 0xC4, 0x91 }, { 0xC5, 0x86 }, { 0xC5, 0x8D }, { 0xC4, 0xB7 },
        { 0xC3, 0xB4 }, { 0xC3, 0xB5 }, { 0xC3, 0xB6 }, { 0xC3, 0xB7 },
        { 0xC3, 0xB8 }, { 0xC5, 0xB3 }, { 0xC3, 0xBA }, { 0xC3, 0xBB },
        { 0xC3, 0xBC }, { 0xC5, 0xA9 }, { 0xC5, 0xAB }, { 0xCB, 0x99 }
    };

    while(i < size)
    {
        c = data[i++];

        if(c < 0x80)
        {
            if(!c) break;
            text[j++] = c;
        }
        else if(c < 0xA0) {}
        else
        {
            const uint8_t *item = map[c - 0xA0];
            text[j++] = item[0];
            text[j++] = item[1];
        }
    }

    text[j] = '\0';
    return text;
}

static
uint8_t *iso8859_5_decode(const uint8_t *data, size_t size)
{
    uint8_t *const text = ASC_ALLOC(size * 2 + 1, uint8_t);
    uint8_t c, u1, u2;
    size_t i = 0, j = 0;

    while(i < size)
    {
        c = data[i++];

        if(c < 0x80)
        {
            if(!c) break;
            text[j++] = c;
        }
        else if(c < 0xA0) {}
        else
        {
            u1 = 0xD0;
            u2 = 0x80 | (c & 0x1F);

            if(c >= 0xE0) u1 |= 0x01;
            else if(c >= 0xC0) u2 |= 0x20;

            text[j++] = u1;
            text[j++] = u2;
        }
    }

    text[j] = '\0';
    return text;
}

static
uint8_t *iso8859_7_decode(const uint8_t *data, size_t size)
{
    uint8_t *const text = ASC_ALLOC(size * 2 + 1, uint8_t);
    uint8_t c, u1, u2;
    size_t i = 0, j = 0;

    while(i < size)
    {
        c = data[i++];

        if(c < 0x80)
        {
            if(!c) break;
            text[j++] = c;
        }
        else
        {
            u1 = 0xCE;
            u2 = c - 0x30;

            if(c >= 0xF0)
            {
                u1 |= 0x01;
                u2 -= 0x40;
            }

            text[j++] = u1;
            text[j++] = u2;
        }
    }

    text[j] = '\0';
    return text;
}

static
uint8_t *iso8859_8_decode(const uint8_t *data, size_t size)
{
    uint8_t *const text = ASC_ALLOC(size * 2 + 1, uint8_t);
    uint8_t c;
    size_t i = 0, j = 0;

    while(i < size)
    {
        c = data[i++];

        if(c < 0x80)
        {
            if(!c) break;
            text[j++] = c;
        }
        else if(c < 0xA0) {}
        else if(c < 0xBF)
        {
            text[j++] = c;
        }
        else if(c < 0xE0) {}
        else if(c < 0xFB)
        {
            text[j++] = 0xD7;
            text[j++] = c - 0x50;
        }
    }

    text[j] = '\0';
    return text;
}

static
uint8_t *iso8859_9_decode(const uint8_t *data, size_t size)
{
    uint8_t *const text = ASC_ALLOC(size * 2 + 1, uint8_t);
    uint8_t c;
    size_t i = 0, j = 0;

    while(i < size)
    {
        c = data[i++];

        if(c <= 0xCF)
        {
            if(!c) break;
            text[j++] = c;
        }
        else if(c == 0xD0)
        {
            text[j++] = 0x01;
            text[j++] = 0x1E;
        }
        else if(c <= 0xDC)
        {
            text[j++] = c;
        }
        else if(c == 0xDD)
        {
            text[j++] = 0x01;
            text[j++] = 0x30;
        }
        else if(c == 0xDE)
        {
            text[j++] = 0x01;
            text[j++] = 0x5E;
        }
        else if(c <= 0xEF)
        {
            text[j++] = c;
        }
        else if(c == 0xF0)
        {
            text[j++] = 0x01;
            text[j++] = 0x1F;
        }
        else if(c <= 0xFC)
        {
            text[j++] = c;
        }
        else if(c == 0xFD)
        {
            text[j++] = 0x01;
            text[j++] = 0x31;
        }
        else if(c == 0xFE)
        {
            text[j++] = 0x01;
            text[j++] = 0x5F;
        }
        else if(c == 0xFF)
        {
            text[j++] = 0xFF;
        }
    }

    text[j] = '\0';
    return text;
}

char *au_iso8859_dec(const uint8_t *data, size_t size)
{
    if(size == 0)
    {
        while(data[size])
            ++size;
    }

    const uint8_t charset_id = data[0];

    if(charset_id == 0x10)
    {
        switch((data[1] << 8) | (data[2]))
        {
            case 0x02: return (char *)iso8859_2_decode(&data[3], size - 3); // Central European
            case 0x04: return (char *)iso8859_4_decode(&data[3], size - 3); // North European
            case 0x05: return (char *)iso8859_5_decode(&data[3], size - 3); // Cyrillic
            case 0x07: return (char *)iso8859_7_decode(&data[3], size - 3); // Greek
            case 0x08: return (char *)iso8859_8_decode(&data[3], size - 3); // Hebrew
            case 0x09: return (char *)iso8859_9_decode(&data[3], size - 3); // Turkish
            default: break;
        }
    }
    else if(charset_id < 0x10)
    {
        switch(charset_id)
        {
            case 0x01: return (char *)iso8859_5_decode(&data[1], size - 1); // Cyrillic
            case 0x03: return (char *)iso8859_7_decode(&data[1], size - 1); // Greek
            case 0x04: return (char *)iso8859_8_decode(&data[1], size - 1); // Hebrew
            case 0x05: return (char *)iso8859_9_decode(&data[1], size - 1); // Turkish
            default: break;
        }
    }
    else if(charset_id >= 0x20)
    {
        return (char *)iso8859_1_decode(data, size); // Western European
    }

    /* dump raw data */
    static const char unknown_charset[] = "unknown charset: 0x";
    const size_t buf_size = sizeof(unknown_charset) + (size * 2);

    char *const text = ASC_ALLOC(buf_size, char);
    size_t skip = snprintf(text, buf_size, "%s", unknown_charset);
    for(size_t i = 0; i < size; i++)
        skip += snprintf(&text[skip], buf_size - skip, "%02X", data[i]);

    return text;
}
