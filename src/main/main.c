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

#include <astra/astra.h>
#include <astra/core/mainloop.h>
#include <astra/luaapi/luaapi.h>
#include <astra/luaapi/module.h>
#include <astra/luaapi/state.h>

#include "stream_list.h"

#ifdef HAVE_INSCRIPT
#   include "inscript.h"
#endif

#include "sig.h"

static
void bootstrap(lua_State *L, int argc, const char *argv[])
{
    bool dumb = false;

    /* pass command line to Lua */
    lua_pushstring(L, argv[0]);
    lua_setglobal(L, "argv0");

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
    for (size_t i = 0; manifest_list[i] != NULL; i++)
        module_register(L, manifest_list[i]);

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
    /* disable line ending translation */
    setmode(STDOUT_FILENO, _O_BINARY);
    setmode(STDERR_FILENO, _O_BINARY);

    /* line buffering is not supported on win32 */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
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

#ifdef _WIN32
/* NOTE: prototype is here to suppress missing prototype warnings */
int wmain(int argc, wchar_t *wargv[], wchar_t *wenvp[]);

int wmain(int argc, wchar_t *wargv[], wchar_t *wenvp[])
{
    __uarg(wenvp);

    /* convert wide arguments into UTF-8 */
    const char *argv[argc + 1];
    argv[argc] = NULL;

    for (int i = 0; i < argc; i++)
    {
        argv[i] = cx_narrow(wargv[i]);

        /* silently truncate argument list on error */
        if (argv[i] == NULL)
            argc = i;
    }

    /* call real main() */
    const int ret = main(argc, argv);

    /* clean up converted strings */
    while (argc-- > 0)
        free((char *)argv[argc]);

    return ret;
}
#endif /* _WIN32 */
