/*
 * Astra Core (Initialization)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015, Artem Kharitonov <artem@sysert.ru>
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
#include <core/init.h>
#include <core/mainloop.h>
#include <core/event.h>
#include <core/thread.h>
#include <core/timer.h>
#include <core/socket.h>

#define MSG(_msg) "[core] " _msg

int astra_exit_status = 0;

void astra_core_init(void)
{
    /* call order doesn't really matter here */
    asc_lua_core_init();

    asc_thread_core_init();
    asc_timer_core_init();
    asc_socket_core_init();
    asc_event_core_init();

    asc_main_loop_init();
}

void astra_core_destroy(void)
{
    /* this frees streaming modules */
    asc_lua_core_destroy();

    asc_event_core_destroy();
    asc_socket_core_destroy();
    asc_timer_core_destroy();
    asc_thread_core_destroy();

    asc_main_loop_destroy();
    asc_log_core_destroy();
}

void astra_exit(int status)
{
    asc_log_debug(MSG("immediate exit requested, rc=%d"), status);

    astra_core_destroy();
    astra_exit_status = status;
    exit(status);
}

void astra_abort(void)
{
    int level = 0;
    if (lua != NULL)
    {
        lua_Debug ar;
        while (lua_getstack(lua, level, &ar))
        {
            if (++level == 1)
                asc_log_error(MSG("abort execution. lua backtrace:"));

            lua_getinfo(lua, "nSl", &ar);
            asc_log_error(MSG("%d: %s:%d -- %s [%s]")
                          , level, ar.short_src, ar.currentline
                          , (ar.name) ? ar.name : "<unknown>"
                          , ar.what);
        }
    }

    if (level == 0)
        asc_log_error(MSG("abort execution"));

    astra_exit_status = EXIT_ABORT;
    exit(EXIT_ABORT);
}
