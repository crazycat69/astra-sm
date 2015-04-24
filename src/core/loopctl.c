/*
 * Astra Core (Main loop control)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *                    2015, Artem Kharitonov <artem@sysert.ru>
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

#include <astra.h>

jmp_buf main_loop;
bool is_main_loop_idle = true;

/* FIXME: move lua stuff to separate unit in core */
lua_State *lua = NULL;

void astra_exit(void)
{
#ifndef _WIN32
    longjmp(main_loop, 1);
#else
    exit(0);
#endif
}

void astra_abort(void)
{
    asc_log_error("[main] abort execution");

    if (lua != NULL)
    {
        asc_log_error("[main] Lua backtrace:");

        lua_Debug ar;
        int level = 1;
        while(lua_getstack(lua, level, &ar))
        {
            lua_getinfo(lua, "nSl", &ar);
            asc_log_error("[main] %d: %s:%d -- %s [%s]"
                          , level, ar.short_src, ar.currentline
                          , (ar.name) ? ar.name : "<unknown>"
                          , ar.what);
            ++level;
        }
    }

    abort();
}

void astra_reload(void)
{
    longjmp(main_loop, 2);
}
