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

#define MSG(_msg) "[main] " _msg

/* garbage collector interval */
#define LUA_GC_TIMEOUT (1 * 1000 * 1000)

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
    if (main_loop != NULL)
        main_loop->flags |= flag;
}

__asc_inline
void asc_main_loop_busy(void)
{
    asc_main_loop_set(MAIN_LOOP_NO_SLEEP);
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

    //return false;
}

void astra_exit(void)
{
    astra_core_destroy();
    exit(EXIT_SUCCESS);
}

void astra_abort(void)
{
    asc_log_error(MSG("abort execution"));

    if (lua != NULL)
    {
        asc_log_error(MSG("Lua backtrace:"));

        lua_Debug ar;
        int level = 1;
        while(lua_getstack(lua, level, &ar))
        {
            lua_getinfo(lua, "nSl", &ar);
            asc_log_error(MSG("%d: %s:%d -- %s [%s]")
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
    asc_main_loop_set(MAIN_LOOP_RELOAD);
}

void astra_shutdown(void)
{
    asc_main_loop_set(MAIN_LOOP_SHUTDOWN);
}
