/*
 * Astra (Main binary)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *               2015-2016, Artem Kharitonov <artem@3phase.pw>
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
#include <luaapi/module.h>
#include <luaapi/state.h>

#ifdef _WIN32
#   include <mmsystem.h>
#endif

#include "sighandler.h"
#include "stream/list.h"

#ifdef HAVE_INSCRIPT
#   include "inscript.h"
#endif

#define MSG(_msg) "[main] " _msg

static
void bootstrap(lua_State *L, int argc, const char *argv[])
{
    bool dumb = false;

    /* pass command line to Lua */
    lua_newtable(L);
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--dumb"))
            dumb = true;

        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i);
    }
    lua_setglobal(L, "argv");

    /* load built-in streaming modules */
    static const module_manifest_t *stream_list[] = {
        LUA_STREAM_BINDINGS
        NULL
    };

    for (size_t i = 0; stream_list[i] != NULL; i++)
        module_register(L, stream_list[i]);

#ifdef HAVE_INSCRIPT
    /* add package searcher so that require() works on built-in scripts */
    inscript_init(L);
#endif

    if (dumb)
    {
        /* run scripts without loading Lua libraries */
        unsigned int cnt = 0;
        for (int i = 1; i < argc; i++)
        {
            const char *filename = argv[i];
            if (!strcmp(filename, "-")) /* stdin */
                filename = NULL;
            else if (strstr(filename, "-") == filename) /* option */
                continue;

            if (luaL_loadfile(L, filename) != 0
                || lua_tr_call(L, 0, 0) != 0)
            {
                lua_err_log(L);
                asc_lib_exit(EXIT_ABORT);
            }

            cnt++;
        }

        if (cnt == 0)
        {
            printf("%s (interpreter mode)\n\n"
                   "Usage: %s --dumb [OPTIONS] FILE...\n"
                   , PACKAGE_STRING, argv[0]);

            asc_lib_exit(EXIT_FAILURE);
        }

        return;
    }

    /* normal startup */
    lua_getglobal(L, "require");
    lua_pushstring(L, "autoexec");

    if (lua_tr_call(L, 1, 0) != 0)
    {
        lua_err_log(L);
        asc_lib_exit(EXIT_ABORT);
    }
}

int main(int argc, const char *argv[])
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

    asc_srand();
    signal_setup();

    bool again = false;
    do
    {
        asc_lib_init();
        signal_enable(true);

        /* initialize and run astra instance */
        bootstrap(lua, argc, argv);

        again = asc_main_loop_run();
        asc_log_info("[main] %s", again ? "restarting" : "shutting down");

        signal_enable(false);
        asc_lib_destroy();
    } while (again);

    return 0;
}
