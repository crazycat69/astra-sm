/*
 * Astra Utils (MD5)
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

#ifndef _AU_MD5_H_
#define _AU_MD5_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

typedef struct
{
    uint32_t lo, hi;
    uint32_t a, b, c, d;
    uint8_t buffer[64];
    uint32_t block[16];
} md5_ctx_t;

#define MD5_DIGEST_SIZE 16
#define MD5_CRYPT_SIZE 36

void au_md5_init(md5_ctx_t *ctx);
void au_md5_update(md5_ctx_t *ctx, const void *data, size_t len);
void au_md5_final(md5_ctx_t *ctx, uint8_t digest[MD5_DIGEST_SIZE]);

void au_md5_crypt(const char *pw, const char *salt, char buf[MD5_CRYPT_SIZE]);

#endif /* _AU_MD5_H_ */
