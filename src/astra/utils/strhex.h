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

#ifndef _AU_STRHEX_H_
#define _AU_STRHEX_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

char *au_hex2str(char *dst, const void *src, size_t srclen);
void *au_str2hex(const char *src, void *dst, size_t dstlen);

#endif /* _AU_STRHEX_H_ */
