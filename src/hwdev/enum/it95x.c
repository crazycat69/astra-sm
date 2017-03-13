/*
 * Astra Module: Hardware Enumerator (IT95x)
 *
 * Copyright (C) 2017, Artem Kharitonov <artem@3phase.pw>
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

#include "enum.h"
#include "../it95x/api.h"

#ifdef _WIN32

static
int it95x_enumerate(lua_State *L)
{
    lua_newtable(L);
    return 1;
}

#elif defined(__linux) /* _WIN32 */
#   error "This module it not yet available on Linux"
#endif /* __linux */

HW_ENUM_REGISTER(it95x)
{
    .name = "it95x_output",
    .description = "ITE IT9500 Series DVB-T Modulators",
    .enumerate = it95x_enumerate,
};
