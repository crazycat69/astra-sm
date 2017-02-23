/*
 * Astra Module: Hardware Enumerator (DVB API)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *                    2017, Artem Kharitonov <artem@3phase.pw>
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

#include "enum.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <dirent.h>

#include <linux/dvb/frontend.h>
#ifdef HAVE_LINUX_DVB_NET_H
#   include <linux/dvb/net.h>
#endif

/* directory containing DVB device nodes */
#define DVB_ROOT "/dev/dvb"

/* buffer size for MAC address' text form */
#define MAC_BUFSIZ 18

struct dvb_enum
{
    lua_State *lua;
    const char *path;
    int adapter;
    int frontend;
};

/* run callback for every entry in `path' matching `filter' */
static
int iterate_dir(const char *path, const char *filter, void *arg
                , void (*callback)(void *, const char *))
{
    DIR *const dirp = opendir(path);
    if (dirp == NULL)
        return -1;

    const size_t filter_len = strlen(filter);
    const struct dirent *entry = NULL;
    size_t count = 0;

    while ((entry = readdir(dirp)) != NULL)
    {
        if (strncmp(entry->d_name, filter, filter_len))
            continue;

        char item[PATH_MAX] = { '\0' };
        snprintf(item, sizeof(item), "%s/%s", path, entry->d_name);

        callback(arg, item);
        count++;
    }

    closedir(dirp);

    if (count == 0)
    {
        errno = ENOENT;
        return -1;
    }

    return 0;
}

/* convert number at the end of `str' to int */
static
int get_last_int(const char *str)
{
    const char *p = strrchr(str, '\0');

    for (; p > str; p--)
    {
        const int c = *(p - 1);
        if (!(c >= '0' && c <= '9'))
            break;
    }

    return atoi(p);
}

#ifdef HAVE_LINUX_DVB_NET_H
/* read network interface's MAC address in text form */
static
int get_mac(const char *ifname, char mac[MAC_BUFSIZ])
{
    const int fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
        return -1;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);

    const int ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
    close(fd);

    if (ret != 0)
        return -1;

    const uint8_t *const p = (uint8_t *)ifr.ifr_hwaddr.sa_data;
    snprintf(mac, MAC_BUFSIZ, "%02X:%02X:%02X:%02X:%02X:%02X"
             , p[0], p[1], p[2], p[3], p[4], p[5]);

    return 0;
}
#endif /* HAVE_LINUX_DVB_NET_H */

/* retrieve DVB card's MAC address */
static
void get_net_info(struct dvb_enum *ctx)
{
    lua_State *const L = ctx->lua;
    char mac[MAC_BUFSIZ] = { '\0' };

#ifdef HAVE_LINUX_DVB_NET_H
    char dev[PATH_MAX] = { '\0' };
    snprintf(dev, sizeof(dev), "%s/net%d", ctx->path, ctx->frontend);

    const int fd = open(dev, O_RDWR | O_NONBLOCK);
    if (fd != -1)
    {
        struct dvb_net_if net;
        memset(&net, 0, sizeof(net));

        if (ioctl(fd, NET_ADD_IF, &net) == 0)
        {
            char ifname[IFNAMSIZ] = { '\0' };
            snprintf(ifname, sizeof(ifname), "dvb%d_%d"
                     , ctx->adapter, ctx->frontend);

            if (get_mac(ifname, mac) != 0)
            {
                lua_pushfstring(L, "get_mac(): %s: %s"
                                , ifname, strerror(errno));
                lua_setfield(L, -2, "net_error");
            }

            ioctl(fd, NET_REMOVE_IF, net.if_num);
        }
        else
        {
            lua_pushfstring(L, "ioctl(): NET_ADD_IF: %s", strerror(errno));
            lua_setfield(L, -2, "net_error");
        }

        close(fd);
    }
    else
    {
        lua_pushfstring(L, "open(): %s: %s", dev, strerror(errno));
        lua_setfield(L, -2, "net_error");
    }
#else /* HAVE_LINUX_DVB_NET_H */
    lua_pushstring(L, "DVB networking is not supported by the OS");
    lua_setfield(L, -2, "net_error");
#endif /* !HAVE_LINUX_DVB_NET_H */

    if (strlen(mac) > 0)
    {
        lua_pushstring(L, mac);
        lua_setfield(L, -2, "mac");
    }
}

/* retrieve frontend name and type */
static
bool get_frontend_info(struct dvb_enum *ctx)
{
    lua_State *const L = ctx->lua;

    /* open device node */
    char dev[PATH_MAX] = { '\0' };
    snprintf(dev, sizeof(dev), "%s/frontend%d", ctx->path, ctx->frontend);

    bool is_busy = false;
    int fd = open(dev, O_RDWR | O_NONBLOCK);

    if (fd == -1 && errno == EBUSY)
    {
        is_busy = true;
        fd = open(dev, O_RDONLY | O_NONBLOCK);
    }

    if (fd == -1)
    {
        lua_pushfstring(L, "open(): %s: %s", dev, strerror(errno));
        lua_setfield(L, -2, "error");

        return false;
    }

    if (is_busy)
    {
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "busy");
    }

    /* query basic parameters */
    struct dvb_frontend_info feinfo;
    const int ret = ioctl(fd, FE_GET_INFO, &feinfo);
    close(fd);

    if (ret != 0)
    {
        lua_pushfstring(L, "ioctl(): FE_GET_INFO: %s", strerror(errno));
        lua_setfield(L, -2, "error");

        return false;
    }

    /* description string */
    lua_pushstring(L, feinfo.name);
    lua_setfield(L, -2, "name");

    /* delivery system */
    const char *type = NULL;
    switch (feinfo.type)
    {
        case FE_QPSK: type = "S";    break;
        case FE_OFDM: type = "T";    break;
        case FE_QAM:  type = "C";    break;
        case FE_ATSC: type = "ATSC"; break;

        default:
            break;
    }

    if (type == NULL)
    {
        lua_pushfstring(L, "unknown frontend type: %d", feinfo.type);
        lua_setfield(L, -2, "error");

        return false;
    }

    lua_pushstring(L, type);
    lua_setfield(L, -2, "type");

    return true;
}

/* probe frontend for parameters */
static
void probe_frontend(void *arg, const char *path)
{
    struct dvb_enum *const ctx = (struct dvb_enum *)arg;
    lua_State *const L = ctx->lua;

    ctx->frontend = get_last_int(path);

    lua_newtable(L);
    lua_pushinteger(L, ctx->adapter);
    lua_setfield(L, -2, "adapter");
    lua_pushinteger(L, ctx->frontend);
    lua_setfield(L, -2, "frontend");

    if (get_frontend_info(ctx))
    {
        get_net_info(ctx);
    }

    const int pos = luaL_len(L, -2) + 1;
    lua_rawseti(L, -2, pos);
}

/* list adapter's frontends */
static
void probe_adapter(void *arg, const char *path)
{
    lua_State *const L = (lua_State *)arg;

    struct dvb_enum ctx =
    {
        .lua = L,
        .path = path,
        .adapter = get_last_int(path),
    };

    /* list /dev/dvb/adapterX/frontendY */
    const int ret = iterate_dir(path, "frontend", &ctx, probe_frontend);
    if (ret != 0)
    {
        /* add fake adapter entry */
        lua_newtable(L);
        lua_pushinteger(L, ctx.adapter);
        lua_setfield(L, -2, "adapter");
        lua_pushfstring(L, "iterate_dir(): %s/frontend*: %s"
                        , path, strerror(errno));
        lua_setfield(L, -2, "error");

        const int pos = luaL_len(L, -2) + 1;
        lua_rawseti(L, -2, pos);
    }
}

/* return list of detected DVB adapters and frontends */
static
int dvbapi_enumerate(lua_State *L)
{
    lua_newtable(L);

    /* list /dev/dvb/adapterX */
    const int ret = iterate_dir(DVB_ROOT, "adapter", L, probe_adapter);
    if (ret != 0 && errno != ENOENT)
    {
        luaL_error(L, "iterate_dir(): %s/adapter*: %s"
                   , DVB_ROOT, strerror(errno));
    }

    return 1;
}

HW_ENUM_REGISTER(dvbapi)
{
    .name = "dvb_input",
    .description = "DVB Input (Linux DVB API)",
    .enumerate = dvbapi_enumerate,
};
