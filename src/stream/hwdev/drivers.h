/*
 * Astra Module: Hardware Device (Driver list)
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

#include "hwdev.h"

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

static inline
const hw_driver_t *hw_find_driver(const char *drvname)
{
    const hw_driver_t *drv = NULL;
    for (size_t i = 0; hw_drivers[i] != NULL; i++)
    {
        if (!strcmp(hw_drivers[i]->name, drvname))
        {
            drv = hw_drivers[i];
            break;
        }
    }

    return drv;
}

#endif /* _HWDEV_DRIVERS_H_ */
