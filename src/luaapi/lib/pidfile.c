/*
 * Astra Lua Library (PID file)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
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

/*
 * Create pid file, remove it on instance shutdown.
 *
 * Module Name:
 *      pidfile
 *
 * Usage:
 *      pidfile("/path/to/file.pid")
 */

#include <astra.h>
#include <luaapi/module.h>

#define MSG(_msg) "[pidfile] " _msg

struct module_data_t
{
    MODULE_DATA();

    int idx_self;
};

static
const char *get_pidfile(lua_State *L)
{
    const char *filename = NULL;

    lua_getglobal(L, "pidfile");
    lua_getfield(L, -1, "path");
    if (lua_isstring(L, -1))
        filename = lua_tostring(L, -1);

    lua_pop(L, 2); /* pidfile */

    return filename;
}

static
void set_pidfile(lua_State *L, const char *filename)
{
    lua_getglobal(L, "pidfile");
    if (filename != NULL)
        lua_pushstring(L, filename);
    else
        lua_pushnil(L);

    lua_setfield(L, -2, "path");
    lua_pop(L, 1); /* pidfile */
}

static
void module_init(lua_State *L, module_data_t *mod)
{
    /* check if we've already been called */
    const char *filename = get_pidfile(L);
    if (filename != NULL)
        luaL_error(L, MSG("already created in %s"), filename);

    /* remove stale pidfile if it exists */
    filename = luaL_checkstring(L, MODULE_OPTIONS_IDX);
    if (access(filename, F_OK) == 0 && unlink(filename) != 0)
        asc_log_error(MSG("unlink(): %s: %s"), filename, strerror(errno));

    /* write pid to temporary file */
    char tmp[PATH_MAX] = { 0 };
    int size = snprintf(tmp, sizeof(tmp), "%s.XXXXXX", filename);
    if (size <= 0)
        luaL_error(L, MSG("snprintf() failed"));

    char pid[32] = { 0 };
    size = snprintf(pid, sizeof(pid), "%lld\n", (long long)getpid());
    if (size <= 0)
        luaL_error(L, MSG("snprintf() failed"));

    const int fd = mkstemp(tmp);
    if (fd == -1)
        luaL_error(L, MSG("mkstemp(): %s: %s"), tmp, strerror(errno));

    if (write(fd, pid, size) == -1)
    {
        close(fd);
        unlink(tmp);

        luaL_error(L, MSG("write(): %s: %s"), tmp, strerror(errno));
    }

    if (chmod(tmp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) != 0)
        asc_log_error(MSG("chmod(): %s: %s"), tmp, strerror(errno));

    close(fd);

    /* move pidfile into place */
#ifdef _WIN32
    if (!MoveFileEx(tmp, filename, MOVEFILE_REPLACE_EXISTING))
    {
        unlink(tmp);
        luaL_error(L, MSG("MoveFileEx(): %s to %s: %s"), tmp, filename
                   , asc_error_msg());
    }
#else /* _WIN32 */
    if (link(tmp, filename) != 0)
    {
        unlink(tmp);
        luaL_error(L, MSG("link(): %s to %s: %s"), tmp, filename
                   , strerror(errno));
    }

    if (unlink(tmp) != 0)
        asc_log_error(MSG("unlink(): %s: %s"), tmp, strerror(errno));
#endif /* !_WIN32 */

    /* store in registry to prevent garbage collection */
    set_pidfile(L, filename);

    lua_pushvalue(L, 3);
    mod->idx_self = luaL_ref(L, LUA_REGISTRYINDEX);
}

static
void module_destroy(module_data_t *mod)
{
    lua_State *const L = MODULE_L(mod);
    const char *const filename = get_pidfile(L);

    if (filename != NULL)
    {
        if (access(filename, F_OK) == 0 && unlink(filename) != 0)
            asc_log_error(MSG("unlink(): %s: %s"), filename, strerror(errno));
    }

    luaL_unref(L, LUA_REGISTRYINDEX, mod->idx_self);
}

MODULE_REGISTER(pidfile)
{
    .init = module_init,
    .destroy = module_destroy,
};
