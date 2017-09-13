/*
 * Astra Lua Library (Utilities)
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

/*
 * Additional methods for Lua
 *
 * Methods:
 *      utils.hostname()
 *                  - get hostname of the local machine
 *      utils.ifaddrs()
 *                  - get network interfaces list (except Win32)
 *      utils.stat(path)
 *                  - file/folder information
 *      utils.readdir(path)
 *                  - iterator to scan directory located by path
 */

#include <astra/astra.h>
#include <astra/luaapi/module.h>

#include <dirent.h>

#ifndef _WIN32
#   include <netinet/in.h>
#   ifdef HAVE_IFADDRS_H
#       include <ifaddrs.h>
#   endif /* HAVE_IFADDRS_H */
#   include <netdb.h>
#endif /* !_WIN32 */

#define MSG(_msg) "[utils] " _msg

/*
 * hostname
 */

static
int method_hostname(lua_State *L)
{
    char hostname[64];
    if (gethostname(hostname, sizeof(hostname)) != 0)
        luaL_error(L, MSG("failed to get hostname"));

    lua_pushstring(L, hostname);
    return 1;
}

/*
 * ifaddrs
 */

#ifdef HAVE_GETIFADDRS
static
int method_ifaddrs(lua_State *L)
{
    struct ifaddrs *ifaddr = NULL;
    char host[NI_MAXHOST];

    const int ret = getifaddrs(&ifaddr);
    if (ret != 0)
        luaL_error(L, MSG("getifaddrs() failed"));

    lua_newtable(L);

    for (struct ifaddrs *i = ifaddr; i; i = i->ifa_next)
    {
        if (!i->ifa_addr)
            continue;

        lua_getfield(L, -1, i->ifa_name);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushstring(L, i->ifa_name);
            lua_pushvalue(L, -2);
            lua_settable(L, -4);
        }

        const int s = getnameinfo(i->ifa_addr, sizeof(struct sockaddr_in)
                                  , host, sizeof(host), NULL, 0
                                  , NI_NUMERICHOST);
        if (s == 0 && *host != '\0')
        {
            const char *ip_family = NULL;

            switch (i->ifa_addr->sa_family)
            {
                case AF_INET:
                    ip_family = "ipv4";
                    break;
                case AF_INET6:
                    ip_family = "ipv6";
                    break;
#ifdef AF_LINK
                case AF_LINK:
                    ip_family = "link";
                    break;
#endif /* AF_LINK */
                default:
                    break;
            }

            if (ip_family)
            {
                int count = 0;
                lua_getfield(L, -1, ip_family);
                if (lua_isnil(L, -1))
                {
                    lua_pop(L, 1);
                    lua_newtable(L);
                    lua_pushstring(L, ip_family);
                    lua_pushvalue(L, -2);
                    lua_settable(L, -4);
                    count = 0;
                }
                else
                    count = luaL_len(L, -1);

                lua_pushinteger(L, count + 1);
                lua_pushstring(L, host);
                lua_settable(L, -3);
                lua_pop(L, 1);
            }
        }

        lua_pop(L, 1);
    }
    freeifaddrs(ifaddr);

    return 1;
}
#endif /* HAVE_GETIFADDRS */

/*
 * stat
 */

static inline
const char *mode_to_str(unsigned int mode)
{
    switch (mode & S_IFMT)
    {
        case S_IFBLK:   return "block";
        case S_IFCHR:   return "character";
        case S_IFDIR:   return "directory";
        case S_IFIFO:   return "pipe";
        case S_IFREG:   return "file";
#ifndef _WIN32
        case S_IFLNK:   return "symlink";
        case S_IFSOCK:  return "socket";
#endif /* _WIN32 */
        default:        return "unknown";
    }
}

static
int method_stat(lua_State *L)
{
    const char *const path = luaL_checkstring(L, 1);

    struct stat sb;
    if (stat(path, &sb) != 0)
    {
        lua_pushboolean(L, 0);
        lua_pushfstring(L, "stat(): %s: %s", path, strerror(errno));

        return 2;
    }

    lua_newtable(L);

    lua_pushstring(L, mode_to_str(sb.st_mode));
    lua_setfield(L, -2, "type");

    lua_pushinteger(L, sb.st_uid);
    lua_setfield(L, -2, "uid");

    lua_pushinteger(L, sb.st_gid);
    lua_setfield(L, -2, "gid");

    lua_pushnumber(L, sb.st_size);
    lua_setfield(L, -2, "size");

    return 1;
}

/*
 * readdir
 */

static
int utils_readdir_iter(lua_State *L)
{
    DIR *const dirp = *(DIR **)lua_touserdata(L, lua_upvalueindex(1));
    const struct dirent *entry = NULL;

    do
    {
        entry = readdir(dirp);
    } while (entry != NULL && entry->d_name[0] == '.');

    if (entry == NULL)
        return 0;

    lua_pushstring(L, entry->d_name);
    return 1;
}

static
int utils_readdir_init(lua_State *L)
{
    const char *const path = luaL_checkstring(L, 1);
    DIR *const dirp = opendir(path);
    if (dirp == NULL)
        luaL_error(L, MSG("opendir(): %s: %s"), path, strerror(errno));

    DIR **const d = (DIR **)lua_newuserdata(L, sizeof(DIR *));
    *d = dirp;

    /* close directory when metadata is GC'd */
    luaL_getmetatable(L, "__utils_readdir");
    lua_setmetatable(L, -2);

    lua_pushcclosure(L, utils_readdir_iter, 1);
    return 1;
}

static
int utils_readdir_gc(lua_State *L)
{
    DIR **const dirpp = (DIR **)lua_touserdata(L, 1);

    if (*dirpp != NULL)
    {
        closedir(*dirpp);
        *dirpp = NULL;
    }

    return 0;
}

static
void module_load(lua_State *L)
{
    static const luaL_Reg api[] =
    {
        { "hostname", method_hostname },
#ifdef HAVE_GETIFADDRS
        { "ifaddrs", method_ifaddrs },
#endif /* HAVE_GETIFADDRS */
        { "stat", method_stat },
        { NULL, NULL },
    };

    luaL_newlib(L, api);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "utils");

    /* readdir */
    const int table = lua_gettop(L);
    luaL_newmetatable(L, "__utils_readdir");
    lua_pushcfunction(L, utils_readdir_gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1); /* metatable */
    lua_pushcfunction(L, utils_readdir_init);
    lua_setfield(L, table, "readdir");
    lua_pop(L, 1); /* utils */
}

BINDING_REGISTER(utils)
{
    .load = module_load,
};
