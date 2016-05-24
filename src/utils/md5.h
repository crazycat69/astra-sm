/*
 * Astra Utils (MD5)
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

#ifndef _AU_MD5_H_
#define _AU_MD5_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

typedef struct
{
    uint32_t state[4];  /* state (ABCD) */
    uint32_t count[2];  /* number of bits, modulo 2^64 (lsb first) */
    uint8_t buffer[64]; /* input buffer */
} md5_ctx_t;

#define MD5_DIGEST_SIZE 16

void au_md5_init(md5_ctx_t *context);
void au_md5_update(md5_ctx_t *context, const uint8_t *data, size_t len);
void au_md5_final(md5_ctx_t *context, uint8_t digest[MD5_DIGEST_SIZE]);

void au_md5_crypt(const char *pw, const char *salt, char passwd[36]);

#endif /* _AU_MD5_H_ */
