/*
 * Astra Module: Hardware Device (Enumeration)
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

#ifndef _HWDEV_DRIVERS_H_
#define _HWDEV_DRIVERS_H_ 1

#ifdef _WIN32
extern const hw_driver_t hw_driver_bda;
#endif

static const hw_driver_t *hw_drivers[] =
{
#ifdef _WIN32
    &hw_driver_bda,
#endif
    NULL,
};

#endif /* _HWDEV_DRIVERS_H_ */
