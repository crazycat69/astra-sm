/*
 * Astra Module: Hardware Enumerator
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

#ifndef _HWDEV_ENUM_H_
#define _HWDEV_ENUM_H_ 1

#include <astra/astra.h>
#include <astra/luaapi/module.h>

typedef struct
{
    const char *name;
    const char *description;
    int (*enumerate)(lua_State *);
} hw_enum_t;

/* macros for auto-generated enumerator list */
#define HW_ENUM_LIST() \
    static const hw_enum_t *enum_list[] =

#define HW_ENUM_SYMBOL(_name) \
    __hw_enum_##_name

#define HW_ENUM_DEF(_name) \
    const hw_enum_t HW_ENUM_SYMBOL(_name)

#define HW_ENUM_DECL(_name) \
    extern HW_ENUM_DEF(_name)

#define HW_ENUM_REGISTER(_name) \
    HW_ENUM_DECL(_name); \
    HW_ENUM_DEF(_name) =

#endif /* _HWDEV_ENUM_H_ */
