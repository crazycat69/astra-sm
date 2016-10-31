/*
 * Astra Module: Hardware Device
 *
 * Copyright (C) 2016, Artem Kharitonov <artem@3phase.pw>
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

#ifndef _HWDEV_H_
#define _HWDEV_H_ 1

#include <astra/astra.h>
#include <astra/luaapi/stream.h>

typedef struct
{
    const char *name;
    const char *description;

    int (*enumerate)(lua_State *);
} hw_driver_t;

#endif /* _HWDEV_H_ */
