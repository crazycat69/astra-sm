/*
 * Astra Core (Main loop)
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
#include <core/mainloop.h>
#include <core/event.h>
#include <core/thread.h>
#include <core/timer.h>
#include <luaapi/luaapi.h>
#include <luaapi/state.h>

#define MSG(_msg) "[mainloop] " _msg

/* garbage collector interval */
#define LUA_GC_TIMEOUT (1 * 1000 * 1000)

typedef struct
{
    uint32_t flags;
    unsigned stop_cnt;
} asc_main_loop_t;

asc_main_loop_t *main_loop;

__asc_inline
void asc_main_loop_init(void)
{
    main_loop = (asc_main_loop_t *)calloc(1, sizeof(*main_loop));
    asc_assert(main_loop != NULL, MSG("calloc() failed"));
}

__asc_inline
void asc_main_loop_destroy(void)
{
    ASC_FREE(main_loop, free);
}

__asc_inline
void asc_main_loop_set(uint32_t flag)
{
    main_loop->flags |= flag;
}

bool asc_main_loop_run(void)
{
    uint64_t current_time = asc_utime();
    uint64_t gc_check_timeout = current_time;

    while (true)
    {
        asc_event_core_loop();
        asc_timer_core_loop();
        asc_thread_core_loop();

        if (main_loop->flags)
        {
            const uint32_t flags = main_loop->flags;
            main_loop->flags = 0;

            if (flags & MAIN_LOOP_SHUTDOWN)
            {
                main_loop->stop_cnt = 0;
                return false;
            }
            else if (flags & MAIN_LOOP_RELOAD)
            {
                return true;
            }
            else if (flags & MAIN_LOOP_SIGHUP)
            {
                asc_log_hup();

                lua_getglobal(lua, "on_sighup");
                if(lua_isfunction(lua, -1))
                {
                    lua_call(lua, 0, 0);
                    asc_main_loop_busy();
                }
                else
                    lua_pop(lua, 1);
            }
            else if (flags & MAIN_LOOP_NO_SLEEP)
                continue;
        }

        current_time = asc_utime();
        if ((current_time - gc_check_timeout) >= LUA_GC_TIMEOUT)
        {
            gc_check_timeout = current_time;
            lua_gc(lua, LUA_GCCOLLECT, 0);
        }

        asc_usleep(1000);
    }
}

void astra_shutdown(void)
{
    if (main_loop->flags & MAIN_LOOP_SHUTDOWN)
    {
        if (++main_loop->stop_cnt >= 3)
        {
            /*
             * NOTE: can't use regular exit() here as this is usually
             *       run by a signal handler thread. cleanup will try to
             *       join the thread on itself, possibly resulting in
             *       a deadlock.
             */
            _exit(EXIT_MAINLOOP);
        }
        else if (main_loop->stop_cnt >= 2)
        {
            asc_log_error(MSG("main thread appears to be blocked; "
                              "will abort on next shutdown request"));
        }
    }

    asc_main_loop_set(MAIN_LOOP_SHUTDOWN);
}
