/*
 * Astra Utils (PID file)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
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
 * Not standard module. As initialize option uses string - path to the pid-file
 *
 * Module Name:
 *      pidfile
 *
 * Usage:
 *      pidfile("/path/to/file.pid")
 */

#include <astra.h>
#include <luaapi/luaapi.h>

struct module_data_t
{
    MODULE_LUA_DATA();

    int idx_self;
};

static const char *filename = NULL;

#ifndef HAVE_MKOSTEMP
static inline int __mkstemp(char *tpl)
{
    int fd = mkstemp(tpl);
    if (fd == -1)
        return fd;

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
    {
        close(fd);
        fd = -1;
    }

    return fd;
}
#else /* !HAVE_MKOSTEMP */
#   define __mkstemp(__tpl) mkostemp(__tpl, O_CLOEXEC)
#endif /* !HAVE_MKOSTEMP */

static void module_init(lua_State *L, module_data_t *mod)
{
    if(filename)
    {
        asc_log_error("[pidfile] already created in %s", filename);
        astra_abort();
    }

    filename = luaL_checkstring(L, MODULE_OPTIONS_IDX);

    if(!access(filename, W_OK))
        unlink(filename);

    static char tmp_pidfile[256];
    snprintf(tmp_pidfile, sizeof(tmp_pidfile), "%s.XXXXXX", filename);
    const int fd = __mkstemp(tmp_pidfile);
    if(fd == -1)
    {
        asc_log_error("[pidfile %s] mkstemp() failed [%s]", filename, strerror(errno));
        astra_abort();
    }

    static char pid[8];
    int size = snprintf(pid, sizeof(pid), "%d\n", getpid());
    if(write(fd, pid, size) == -1)
    {
        fprintf(stderr, "[pidfile %s] write() failed [%s]\n", filename, strerror(errno));
        astra_abort();
    }

    fchmod(fd, 0644);
    close(fd);

    const int link_ret = link(tmp_pidfile, filename);
    unlink(tmp_pidfile);
    if(link_ret == -1)
    {
        asc_log_error("[pidfile %s] link() failed [%s]", filename, strerror(errno));
        astra_abort();
    }

    // store in registry to prevent the instance destroying
    lua_pushvalue(L, 3);
    mod->idx_self = luaL_ref(L, LUA_REGISTRYINDEX);
}

static void module_destroy(module_data_t *mod)
{
    if(!access(filename, W_OK))
        unlink(filename);

    filename = NULL;

    luaL_unref(MODULE_L(mod), LUA_REGISTRYINDEX, mod->idx_self);
}

MODULE_LUA_METHODS()
{
    { NULL, NULL },
};
MODULE_LUA_REGISTER(pidfile)
