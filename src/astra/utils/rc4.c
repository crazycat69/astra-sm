/*
 * Astra Utils (RC4)
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
 * RC4
 * Copyright (c) 1996-2000 Whistle Communications, Inc.
 */

#include <astra/astra.h>
#include <astra/utils/rc4.h>

static inline
void swap_bytes(uint8_t *a, uint8_t *b)
{
    const uint8_t temp = *a;
    *a = *b;
    *b = temp;
}

void au_rc4_init(rc4_ctx_t *ctx, const uint8_t *key, size_t keylen)
{
    for (unsigned int i = 0; i < 256; i++)
        ctx->perm[i] = i;

    ctx->index1 = 0;
    ctx->index2 = 0;

    uint8_t j = 0;
    for (unsigned int i = 0; i < 256; i++)
    {
        j += ctx->perm[i] + key[i % keylen];
        swap_bytes(&ctx->perm[i], &ctx->perm[j]);
    }
}

void au_rc4_crypt(rc4_ctx_t *ctx, void *dst, const void *src, size_t len)
{
    const uint8_t *const srcp = (uint8_t *)src;
    uint8_t *const dstp = (uint8_t *)dst;

    for (size_t i = 0; i < len; i++)
    {
        ctx->index1++;
        ctx->index2 += ctx->perm[ctx->index1];

        swap_bytes(&ctx->perm[ctx->index1], &ctx->perm[ctx->index2]);

        const uint8_t j = ctx->perm[ctx->index1] + ctx->perm[ctx->index2];
        dstp[i] = srcp[i] ^ ctx->perm[j];
    }
}
