/*
 * Astra Utils (MD5)
 * http://cesbo.com/astra
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

/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * (This is a heavily cut-down "BSD license".)
 *
 * This differs from Colin Plumb's older public domain implementation in that
 * no exactly 32-bit integer data type is required (any 32-bit or wider
 * unsigned integer data type will do), there's no compile-time endianness
 * configuration, and the function prototypes match OpenSSL's.  No code from
 * Colin Plumb's implementation has been reused; this comment merely compares
 * the properties of the two independent implementations.
 *
 * The primary goals of this implementation are portability and ease of use.
 * It is meant to be fast, but not as fast as possible.  Some known
 * optimizations are not included to reduce source code size and avoid
 * compile-time configuration.
 */

#include <astra/astra.h>
#include <astra/utils/md5.h>

/*
 * The basic MD5 functions.
 *
 * F and G are optimized compared to their RFC 1321 definitions for
 * architectures that lack an AND-NOT instruction, just like in Colin Plumb's
 * implementation.
 */
#define F(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z) ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z) (((x) ^ (y)) ^ (z))
#define H2(x, y, z) ((x) ^ ((y) ^ (z)))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))

/*
 * The MD5 transformation for all four rounds.
 */
#define STEP(f, a, b, c, d, x, t, s) \
    do { \
        (a) += f((b), (c), (d)) + (x) + (t); \
        (a) = (((a) << (s)) | (((a) & 0xffffffff) >> (32 - (s)))); \
        (a) += (b); \
    } while (0)

/*
 * SET reads 4 input bytes in little-endian byte order and stores them in a
 * properly aligned word in host byte order.
 */
#define SET(n) (asc_get_le32(&ptr[(n) * 4]))
#define GET(n) SET(n)

/*
 * This processes one or more 64-byte data blocks, but does NOT update the bit
 * counters.  There are no alignment requirements.
 */
static
const void *md5_transform(md5_ctx_t *ctx, const void *data, size_t len)
{
    const uint8_t *ptr = (uint8_t *)data;
    len /= 64;

    uint32_t a = ctx->a;
    uint32_t b = ctx->b;
    uint32_t c = ctx->c;
    uint32_t d = ctx->d;

    do
    {
        const uint32_t saved_a = a;
        const uint32_t saved_b = b;
        const uint32_t saved_c = c;
        const uint32_t saved_d = d;

        /* Round 1 */
        STEP(F, a, b, c, d, SET(0), 0xd76aa478, 7);
        STEP(F, d, a, b, c, SET(1), 0xe8c7b756, 12);
        STEP(F, c, d, a, b, SET(2), 0x242070db, 17);
        STEP(F, b, c, d, a, SET(3), 0xc1bdceee, 22);
        STEP(F, a, b, c, d, SET(4), 0xf57c0faf, 7);
        STEP(F, d, a, b, c, SET(5), 0x4787c62a, 12);
        STEP(F, c, d, a, b, SET(6), 0xa8304613, 17);
        STEP(F, b, c, d, a, SET(7), 0xfd469501, 22);
        STEP(F, a, b, c, d, SET(8), 0x698098d8, 7);
        STEP(F, d, a, b, c, SET(9), 0x8b44f7af, 12);
        STEP(F, c, d, a, b, SET(10), 0xffff5bb1, 17);
        STEP(F, b, c, d, a, SET(11), 0x895cd7be, 22);
        STEP(F, a, b, c, d, SET(12), 0x6b901122, 7);
        STEP(F, d, a, b, c, SET(13), 0xfd987193, 12);
        STEP(F, c, d, a, b, SET(14), 0xa679438e, 17);
        STEP(F, b, c, d, a, SET(15), 0x49b40821, 22);

        /* Round 2 */
        STEP(G, a, b, c, d, GET(1), 0xf61e2562, 5);
        STEP(G, d, a, b, c, GET(6), 0xc040b340, 9);
        STEP(G, c, d, a, b, GET(11), 0x265e5a51, 14);
        STEP(G, b, c, d, a, GET(0), 0xe9b6c7aa, 20);
        STEP(G, a, b, c, d, GET(5), 0xd62f105d, 5);
        STEP(G, d, a, b, c, GET(10), 0x02441453, 9);
        STEP(G, c, d, a, b, GET(15), 0xd8a1e681, 14);
        STEP(G, b, c, d, a, GET(4), 0xe7d3fbc8, 20);
        STEP(G, a, b, c, d, GET(9), 0x21e1cde6, 5);
        STEP(G, d, a, b, c, GET(14), 0xc33707d6, 9);
        STEP(G, c, d, a, b, GET(3), 0xf4d50d87, 14);
        STEP(G, b, c, d, a, GET(8), 0x455a14ed, 20);
        STEP(G, a, b, c, d, GET(13), 0xa9e3e905, 5);
        STEP(G, d, a, b, c, GET(2), 0xfcefa3f8, 9);
        STEP(G, c, d, a, b, GET(7), 0x676f02d9, 14);
        STEP(G, b, c, d, a, GET(12), 0x8d2a4c8a, 20);

        /* Round 3 */
        STEP(H, a, b, c, d, GET(5), 0xfffa3942, 4);
        STEP(H2, d, a, b, c, GET(8), 0x8771f681, 11);
        STEP(H, c, d, a, b, GET(11), 0x6d9d6122, 16);
        STEP(H2, b, c, d, a, GET(14), 0xfde5380c, 23);
        STEP(H, a, b, c, d, GET(1), 0xa4beea44, 4);
        STEP(H2, d, a, b, c, GET(4), 0x4bdecfa9, 11);
        STEP(H, c, d, a, b, GET(7), 0xf6bb4b60, 16);
        STEP(H2, b, c, d, a, GET(10), 0xbebfbc70, 23);
        STEP(H, a, b, c, d, GET(13), 0x289b7ec6, 4);
        STEP(H2, d, a, b, c, GET(0), 0xeaa127fa, 11);
        STEP(H, c, d, a, b, GET(3), 0xd4ef3085, 16);
        STEP(H2, b, c, d, a, GET(6), 0x04881d05, 23);
        STEP(H, a, b, c, d, GET(9), 0xd9d4d039, 4);
        STEP(H2, d, a, b, c, GET(12), 0xe6db99e5, 11);
        STEP(H, c, d, a, b, GET(15), 0x1fa27cf8, 16);
        STEP(H2, b, c, d, a, GET(2), 0xc4ac5665, 23);

        /* Round 4 */
        STEP(I, a, b, c, d, GET(0), 0xf4292244, 6);
        STEP(I, d, a, b, c, GET(7), 0x432aff97, 10);
        STEP(I, c, d, a, b, GET(14), 0xab9423a7, 15);
        STEP(I, b, c, d, a, GET(5), 0xfc93a039, 21);
        STEP(I, a, b, c, d, GET(12), 0x655b59c3, 6);
        STEP(I, d, a, b, c, GET(3), 0x8f0ccc92, 10);
        STEP(I, c, d, a, b, GET(10), 0xffeff47d, 15);
        STEP(I, b, c, d, a, GET(1), 0x85845dd1, 21);
        STEP(I, a, b, c, d, GET(8), 0x6fa87e4f, 6);
        STEP(I, d, a, b, c, GET(15), 0xfe2ce6e0, 10);
        STEP(I, c, d, a, b, GET(6), 0xa3014314, 15);
        STEP(I, b, c, d, a, GET(13), 0x4e0811a1, 21);
        STEP(I, a, b, c, d, GET(4), 0xf7537e82, 6);
        STEP(I, d, a, b, c, GET(11), 0xbd3af235, 10);
        STEP(I, c, d, a, b, GET(2), 0x2ad7d2bb, 15);
        STEP(I, b, c, d, a, GET(9), 0xeb86d391, 21);

        a += saved_a;
        b += saved_b;
        c += saved_c;
        d += saved_d;

        ptr += 64;
    } while (--len > 0);

    ctx->a = a;
    ctx->b = b;
    ctx->c = c;
    ctx->d = d;

    return ptr;
}

void au_md5_init(md5_ctx_t *ctx)
{
    ctx->a = 0x67452301;
    ctx->b = 0xefcdab89;
    ctx->c = 0x98badcfe;
    ctx->d = 0x10325476;

    ctx->lo = 0;
    ctx->hi = 0;
}

void au_md5_update(md5_ctx_t *ctx, const void *data, size_t len)
{
    const uint32_t saved_lo = ctx->lo;
    const size_t used = saved_lo & 0x3f;

    if ((ctx->lo = (saved_lo + len) & 0x1fffffff) < saved_lo)
        ctx->hi++;
    ctx->hi += len >> 29;

    if (used > 0)
    {
        const size_t available = 64 - used;

        if (len < available)
        {
            memcpy(&ctx->buffer[used], data, len);
            return;
        }

        memcpy(&ctx->buffer[used], data, available);
        md5_transform(ctx, ctx->buffer, 64);

        data = (const uint8_t *)data + available;
        len -= available;
    }

    if (len >= 64)
    {
        data = md5_transform(ctx, data, len & ~(size_t)0x3f);
        len &= 0x3f;
    }

    memcpy(ctx->buffer, data, len);
}

void au_md5_final(md5_ctx_t *ctx, uint8_t digest[MD5_DIGEST_SIZE])
{
    size_t used = ctx->lo & 0x3f;
    ctx->buffer[used++] = 0x80;

    size_t available = 64 - used;
    if (available < 8)
    {
        memset(&ctx->buffer[used], 0, available);
        md5_transform(ctx, ctx->buffer, 64);
        used = 0;
        available = 64;
    }

    memset(&ctx->buffer[used], 0, available - 8);

    ctx->lo <<= 3;
    asc_put_le32(&ctx->buffer[56], ctx->lo);
    asc_put_le32(&ctx->buffer[60], ctx->hi);

    md5_transform(ctx, ctx->buffer, 64);

    asc_put_le32(&digest[0], ctx->a);
    asc_put_le32(&digest[4], ctx->b);
    asc_put_le32(&digest[8], ctx->c);
    asc_put_le32(&digest[12], ctx->d);

    memset(ctx, 0, sizeof(*ctx));
}

/*
 * MD5 crypt
 */

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this
 * notice you can do whatever you want with this stuff. If we meet some
 * day, and you think this stuff is worth it, you can buy me a beer in
 * return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

/* 0 ... 63 => ascii - 64 */
static
const char md5_itoa64[] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static
const char md5_magic[] = "$1$";

static
void md5_to64(char *s, uint32_t v, unsigned int n)
{
    while (n-- > 0)
    {
        *(s++) = md5_itoa64[v & 0x3f];
        v >>= 6;
    }
}

void au_md5_crypt(const char *pw, const char *salt, char buf[MD5_CRYPT_SIZE])
{
    /* refine the salt first */
    const char *sp = salt;

    /* if it starts with the magic string, then skip that */
    if (!strncmp(sp, md5_magic, sizeof(md5_magic) - 1))
        sp += sizeof(md5_magic) - 1;

    /* it stops at the first '$', max 8 chars */
    const char *ep;
    for (ep = sp; *ep != '$'; ep++)
    {
        if (*ep == '\0' || ep >= (sp + 8))
            break;
    }

    /* get the length of the true salt */
    const size_t sl = (size_t)(ep - sp);

    /* stash the salt */
    char salt_copy[9] = { 0 };
    memcpy(salt_copy, sp, sl);

    md5_ctx_t ctx;
    au_md5_init(&ctx);

    /* the password first, since that is what is most unknown */
    au_md5_update(&ctx, pw, strlen(pw));

    /* then our magic string */
    au_md5_update(&ctx, md5_magic, sizeof(md5_magic) - 1);

    /* then the raw salt */
    au_md5_update(&ctx, sp, sl);

    /* then just as many characters of the MD5(pw, salt, pw) */
    uint8_t final[MD5_DIGEST_SIZE + 1] = { 0 };
    md5_ctx_t ctx1;

    au_md5_init(&ctx1);
    au_md5_update(&ctx1, pw, strlen(pw));
    au_md5_update(&ctx1, sp, sl);
    au_md5_update(&ctx1, pw, strlen(pw));
    au_md5_final(&ctx1, final);

    for (ssize_t pl = (ssize_t)strlen(pw); pl > 0; pl -= 16)
        au_md5_update(&ctx, final, ((pl > 16) ? 16 : (size_t)pl));

    /* don't leave anything around in VM they could use */
    memset(final, 0, sizeof(final));

    /* then something really weird... */
    for (size_t i = strlen(pw); i != 0; i >>= 1)
    {
        const void *const d = (i & 1) ? final : (uint8_t *)pw;
        au_md5_update(&ctx, d, 1);
    }

    /* now make the output string */
    au_md5_final(&ctx, final);

    /*
     * And now, just to make sure things don't run too fast
     * On a 60 Mhz Pentium this takes 34 msec, so you would
     * need 30 seconds to build a 1000 entry dictionary...
     */
    for (unsigned int i = 0; i < 1000; i++)
    {
        au_md5_init(&ctx1);

        if (i & 1)
            au_md5_update(&ctx1, pw, strlen(pw));
        else
            au_md5_update(&ctx1, final, 16);

        if (i % 3)
            au_md5_update(&ctx1, sp, sl);

        if (i % 7)
            au_md5_update(&ctx1, pw, strlen(pw));

        if (i & 1)
            au_md5_update(&ctx1, final, 16);
        else
            au_md5_update(&ctx1, pw, strlen(pw));

        au_md5_final(&ctx1, final);
    }

    char hash[64] = { 0 };
    char *p = hash;
    uint32_t l;

    final[16] = final[5];
    for (size_t i = 0; i < 5; i++)
    {
        l = (((uint32_t)final[i] << 16)
             | ((uint32_t)final[i + 6] << 8)
             | ((uint32_t)final[i + 12]));

        md5_to64(p, l, 4);
        p += 4;
    }

    l = final[11];
    md5_to64(p, l, 2);
    p += 2;
    *p = '\0';

    snprintf(buf, MD5_CRYPT_SIZE, "%s%s$%s", md5_magic, salt_copy, hash);

    /* don't leave anything around in VM they could use */
    memset(final, 0, sizeof(final));
    memset(salt_copy, 0, sizeof(salt_copy));
    memset(&ctx, 0, sizeof(ctx));
    memset(&ctx1, 0, sizeof(ctx1));
    memset(hash, 0, sizeof(hash));
}
