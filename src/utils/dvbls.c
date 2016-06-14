/*
 * Astra Utils (DVB enumeration)
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
#include <luaapi/module.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <dirent.h>

#include <linux/dvb/frontend.h>
#ifdef HAVE_LINUX_DVB_NET_H
#   include <linux/dvb/net.h>
#endif

#define MSG(_msg) "[dvbls] " _msg

static int count;
static char dev_name[512];

static int adapter;
static int device;

static const char __adapter[] = "adapter";
static const char __device[] = "device";

static void iterate_dir(lua_State *L, const char *dir, const char *filter
                        , void (*callback)(lua_State *, const char *))
{
    DIR *dirp = opendir(dir);
    if(!dirp)
    {
        if(errno != ENOENT)
        {
            asc_log_error(MSG("opendir() failed: %s: %s"),
                          dir, strerror(errno));
        }
        return;
    }

    char item[64];
    const int item_len = sprintf(item, "%s/", dir);
    const int filter_len = strlen(filter);
    do
    {
        struct dirent *entry = readdir(dirp);
        if(!entry)
            break;
        if(strncmp(entry->d_name, filter, filter_len) != 0)
            continue;
        sprintf(&item[item_len], "%s", entry->d_name);
        callback(L, item);
    } while(1);

    closedir(dirp);
}

static int get_last_int(const char *str)
{
    int i = 0;
    int i_pos = -1;
    for(; str[i]; ++i)
    {
        const char c = str[i];
        if(c >= '0' && c <= '9')
        {
            if(i_pos == -1)
                i_pos = i;
        }
        else if(i_pos >= 0)
            i_pos = -1;
    }

    if(i_pos == -1)
        return 0;

    return atoi(&str[i_pos]);
}

static void check_device_net(lua_State *L)
{
#ifdef HAVE_LINUX_DVB_NET_H
    sprintf(dev_name, "/dev/dvb/adapter%d/net%d", adapter, device);

    const int fd = open(dev_name, O_RDWR | O_NONBLOCK);
    int success = 0;

    do
    {
        if(fd == -1)
        {
            lua_pushfstring(L, "failed to open [%s]", strerror(errno));
            break;
        }

        struct dvb_net_if net;
        memset(&net, 0, sizeof(net));
        if(ioctl(fd, NET_ADD_IF, &net) != 0)
        {
            lua_pushfstring(L, "NET_ADD_IF failed [%s]", strerror(errno));
            break;
        }

        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        sprintf(ifr.ifr_name, "dvb%d_%d", adapter, device);

        int sock = socket(PF_INET, SOCK_DGRAM, 0);
        if(ioctl(sock, SIOCGIFHWADDR, &ifr) != 0)
            lua_pushfstring(L, "SIOCGIFHWADDR failed [%s]", strerror(errno));
        else
        {
            const uint8_t *const p = (uint8_t *)ifr.ifr_hwaddr.sa_data;

            char mac[32];
            snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X"
                     , p[0], p[1], p[2], p[3], p[4], p[5]);

            lua_pushstring(L, mac);
        }
        close(sock);

        if(ioctl(fd, NET_REMOVE_IF, net.if_num) != 0)
        {
            lua_pop(L, 1);
            lua_pushfstring(L, "NET_REMOVE_IF failed [%s]", strerror(errno));
            break;
        }

        success = 1;
    } while(0);

    if(fd != -1)
        close(fd);

    if (!success)
    {
        lua_setfield(L, -2, "net_error");
        lua_pushstring(L, "ERROR");
    }
#else
    static const char dummy_mac[] = "DE:AD:00:00:BE:EF";
    lua_pushstring(L, dummy_mac);
#endif /* HAVE_LINUX_DVB_NET_H */

    lua_setfield(L, -2, "mac");
}

static void check_device_fe(lua_State *L)
{
    sprintf(dev_name, "/dev/dvb/adapter%d/frontend%d", adapter, device);

    bool is_busy = false;

    int fd = open(dev_name, O_RDWR | O_NONBLOCK);
    if(fd == -1)
    {
        is_busy = true;
        fd = open(dev_name, O_RDONLY | O_NONBLOCK);
    }

    static const char _error[] = "error";

    if(fd == -1)
    {
        lua_pushfstring(L, "failed to open [%s]", strerror(errno));
        lua_setfield(L, -2, _error);
        return;
    }

    lua_pushboolean(L, is_busy);
    lua_setfield(L, -2, "busy");

    struct dvb_frontend_info feinfo;
    if(ioctl(fd, FE_GET_INFO, &feinfo) != 0)
    {
        lua_pushstring(L, "failed to get frontend type");
        lua_setfield(L, -2, _error);
        close(fd);
        return;
    }
    close(fd);

    switch(feinfo.type)
    {
        case FE_QPSK:
            lua_pushstring(L, "S");
            break;
        case FE_OFDM:
            lua_pushstring(L, "T");
            break;
        case FE_QAM:
            lua_pushstring(L, "C");
            break;
        case FE_ATSC:
            lua_pushstring(L, "ATSC");
            break;
        default:
            lua_pushfstring(L, "unknown frontend type [%d]", feinfo.type);
            lua_setfield(L, -2, _error);
            return;
    }
    lua_setfield(L, -2, "type");

    lua_pushstring(L, feinfo.name);
    lua_setfield(L, -2, "frontend");

    check_device_net(L);
}

static void check_device(lua_State *L, const char *item)
{
    device = get_last_int(&item[(sizeof("/dev/dvb/adapter") - 1) + (sizeof("/net") - 1)]);

    lua_newtable(L);
    lua_pushinteger(L, adapter);
    lua_setfield(L, -2, __adapter);
    lua_pushinteger(L, device);
    lua_setfield(L, -2, __device);
    check_device_fe(L);

    ++count;
    lua_rawseti(L, -2, count);
}

static void check_adapter(lua_State *L, const char *item)
{
    adapter = get_last_int(&item[sizeof("/dev/dvb/adapter") - 1]);
    iterate_dir(L, item, "net", check_device);
}

static int dvbls_scan(lua_State *L)
{
    count = 0;
    lua_newtable(L);
    iterate_dir(L, "/dev/dvb", __adapter, check_adapter);

    if(count == 0)
        asc_log_debug(MSG("no DVB adapters found"));

    return 1;
}

MODULE_LUA_BINDING(dvbls)
{
    lua_register(L, "dvbls", dvbls_scan);
    return 1;
}
