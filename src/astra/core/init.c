/*
 * Astra Core (Initialization)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015-2016, Artem Kharitonov <artem@3phase.pw>
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

#include <astra/astra.h>
#include <astra/core/init.h>
#include <astra/core/mainloop.h>
#include <astra/core/event.h>
#include <astra/core/thread.h>
#include <astra/core/timer.h>
#include <astra/core/socket.h>
#include <astra/luaapi/state.h>

#define MSG(_msg) "[init] " _msg

int asc_exit_status = 0;

void asc_srand(void)
{
    unsigned long a = clock();
    unsigned long b = time(NULL);
    unsigned long c = getpid();

    a = a - b;  a = a - c;  a = a ^ (c >> 13);
    b = b - c;  b = b - a;  b = b ^ (a << 8);
    c = c - a;  c = c - b;  c = c ^ (b >> 13);
    a = a - b;  a = a - c;  a = a ^ (c >> 12);
    b = b - c;  b = b - a;  b = b ^ (a << 16);
    c = c - a;  c = c - b;  c = c ^ (b >> 5);
    a = a - b;  a = a - c;  a = a ^ (c >> 3);
    b = b - c;  b = b - a;  b = b ^ (a << 10);
    c = c - a;  c = c - b;  c = c ^ (b >> 15);

    srand(c);
}

void asc_lib_init(void)
{
    /* rest of the init routines may need logging functions */
    asc_log_core_init();

    /* ...and sockets */
    asc_socket_core_init();

    /* call order doesn't really matter for these */
    asc_thread_core_init();
    asc_timer_core_init();
    asc_event_core_init();
    asc_main_loop_init();

    /* Lua modules may need features init'd above */
    lua = lua_api_init();
}

void asc_lib_destroy(void)
{
    /*
     * Clean up our Lua state along with all the module instances.
     *
     * Unless there's a buggy module in there somewhere, this should
     * take care of any and all background threads, event handles,
     * timers, etc.
     */
    ASC_FREE(lua, lua_api_destroy);

    /* join any stray threads */
    asc_thread_core_destroy();

    /* main loop uses events for the wake up pipe */
    asc_main_loop_destroy();

    /* cleaning up rogue events might invoke their on_error callbacks */
    asc_event_core_destroy();

    /* no side effects for this one */
    asc_timer_core_destroy();

    /* nothing left to use sockets or logs */
    asc_socket_core_destroy();
    asc_log_core_destroy();
}

void asc_lib_exit(int status)
{
    asc_log_debug(MSG("immediate exit requested, rc=%d"), status);

    asc_lib_destroy();
    asc_exit_status = status;
    exit(status);
}

void asc_lib_abort(void)
{
    asc_exit_status = ASC_EXIT_ABORT;
    exit(ASC_EXIT_ABORT);
}
