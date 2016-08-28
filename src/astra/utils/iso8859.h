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

#ifndef _AU_ISO8859_H_
#define _AU_ISO8859_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

char *au_iso8859_dec(const uint8_t *data, size_t size) __wur;

#endif /* _AU_ISO8859_H_ */
