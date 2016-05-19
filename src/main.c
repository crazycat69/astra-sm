/*
 * Astra Main App
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
#include <luaapi/luaapi.h>
#include <luaapi/state.h>

#ifdef _WIN32
#   include <mmsystem.h>
#endif

#include "sighandler.h"

int main(int argc, const char **argv)
{
#ifdef _WIN32
    /* line buffering is not supported on win32 */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* increase timer resolution */
    TIMECAPS timecaps = { 0, 0 };
    if (timeGetDevCaps(&timecaps, sizeof(timecaps)) == TIMERR_NOERROR)
        timeBeginPeriod(timecaps.wPeriodMin);
#endif /* _WIN32 */

    signal_setup();

astra_reload_entry:
    asc_srand();
    asc_lib_init();
    signal_enable(true);

    /* pass command line to lua */
    lua_newtable(lua);
    for(int i = 1; i < argc; ++i)
    {
        lua_pushinteger(lua, i);
        lua_pushstring(lua, argv[i]);
        lua_settable(lua, -3);
    }
    lua_setglobal(lua, "argv");

    /* run built-in script */
    lua_getglobal(lua, "inscript");
    if(lua_isfunction(lua, -1))
    {
        lua_call(lua, 0, 0);
    }
    else
    {
        lua_pop(lua, 1);

        if(argc < 2)
        {
            printf(PACKAGE_STRING "\n");
            printf("Usage: %s script.lua [OPTIONS]\n", argv[0]);
            asc_lib_exit(EXIT_FAILURE);
        }

        int ret = -1;

        if(argv[1][0] == '-' && argv[1][1] == 0)
            ret = luaL_dofile(lua, NULL);
        else if(!access(argv[1], R_OK))
            ret = luaL_dofile(lua, argv[1]);
        else
        {
            printf("Error: initial script isn't found\n");
            asc_lib_exit(EXIT_FAILURE);
        }

        if(ret != 0)
            luaL_error(lua, "[main] %s", lua_tostring(lua, -1));
    }

    /* start main loop */
    const bool again = asc_main_loop_run();
    asc_log_info("[main] %s", again ? "restarting" : "shutting down");

    signal_enable(false);
    asc_lib_destroy();

    if (again)
        goto astra_reload_entry;

    return 0;
}
