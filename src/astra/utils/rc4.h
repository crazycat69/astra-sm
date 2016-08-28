/*
 * Astra Utils (RC4)
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

#ifndef _AU_RC4_H_
#define _AU_RC4_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

typedef struct
{
    uint8_t perm[256];
    uint8_t index1;
    uint8_t index2;
} rc4_ctx_t;

void au_rc4_init(rc4_ctx_t *state, const uint8_t *key, int keylen);
void au_rc4_crypt(rc4_ctx_t *state, uint8_t *dst, const uint8_t *buf
                  , int buflen);

#endif /* _AU_RC4_H_ */
