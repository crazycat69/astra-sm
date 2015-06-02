/*
 * Astra Main App
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
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
#ifndef _WIN32
#   include <signal.h>
#endif

#ifndef _WIN32
static void signal_handler(int signum)
{
    switch(signum)
    {
        case SIGHUP:
            asc_main_loop_set(MAIN_LOOP_SIGHUP);
            return;

        case SIGUSR1:
            astra_reload();
            return;

        case SIGPIPE:
            return;

        default:
            astra_exit();
    }
}
#else
static bool WINAPI signal_handler(DWORD signum)
{
    switch(signum)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            astra_exit();
            break;

        default:
            break;
    }
    return true;
}
#endif

static void asc_srand(void)
{
    unsigned long a = clock();
    unsigned long b = time(NULL);
#ifndef _WIN32
    unsigned long c = getpid();
#else
    unsigned long c = GetCurrentProcessId();
#endif

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

int main(int argc, const char **argv)
{
#ifndef _WIN32
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, signal_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGQUIT, signal_handler);
#else
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)signal_handler, true);
#endif

astra_reload_entry:

    asc_srand();
    astra_core_init();

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
            astra_exit();
        }

        int ret = -1;

        if(argv[1][0] == '-' && argv[1][1] == 0)
            ret = luaL_dofile(lua, NULL);
        else if(!access(argv[1], R_OK))
            ret = luaL_dofile(lua, argv[1]);
        else
        {
            printf("Error: initial script isn't found\n");
            astra_exit();
        }

        if(ret != 0)
            luaL_error(lua, "[main] %s", lua_tostring(lua, -1));
    }

    /* start main loop */
    const bool again = asc_main_loop_run();
    asc_log_info("[main] %s", again ? "restarting" : "shutting down");
    astra_core_destroy();

    if (again)
        goto astra_reload_entry;

    return 0;
}
