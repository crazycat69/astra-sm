/*
 * Astra Utils (SHA-1)
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

#ifndef _AU_SHA1_H_
#define _AU_SHA1_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

typedef struct
{
    size_t size;
    unsigned int H[5];
    unsigned int W[16];
} sha1_ctx_t;

#define SHA1_DIGEST_SIZE 20

void au_sha1_init(sha1_ctx_t *ctx);
void au_sha1_update(sha1_ctx_t *ctx, const void *data, size_t len);
void au_sha1_final(sha1_ctx_t *ctx, uint8_t digest[SHA1_DIGEST_SIZE]);

#endif /* _AU_SHA1_H_ */
