/*
 * Astra Module: UDP Output
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
 * Module Name:
 *      udp_output
 *
 * Module Options:
 *      upstream    - object, stream instance returned by module_instance:stream()
 *      addr        - string, source IP address
 *      port        - number, source UDP port
 *      ttl         - number, time to live
 *      localaddr   - string, IP address of the local interface
 *      socket_size - number, socket buffer size
 *      rtp         - boolean, use RTP instead of RAW UDP
 *      sync        - boolean, use MPEG-TS syncing
 *      sync_opts   - string, sync buffer options
 */

#include <astra.h>
#include <core/socket.h>
#include <core/timer.h>
#include <luaapi/stream.h>
#include <mpegts/sync.h>

#define MSG(_msg) "[udp_output %s:%d] " _msg, mod->addr, mod->port

#define UDP_BUFFER_SIZE 1460

struct module_data_t
{
    MODULE_STREAM_DATA();

    const char *addr;
    int port;

    bool is_rtp;
    uint16_t rtpseq;

    asc_socket_t *sock;
    bool can_send;
    size_t dropped;

    struct
    {
        uint32_t skip;
        uint8_t buffer[UDP_BUFFER_SIZE];
    } packet;

    mpegts_sync_t *sync;
    asc_timer_t *sync_loop;
};

static void on_ready(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    if(mod->dropped > 0)
    {
        asc_log_error(MSG("socket buffer full, dropped %zu packets"), mod->dropped);
        mod->dropped = 0;
    }

    mod->can_send = true;
    asc_socket_set_on_ready(mod->sock, NULL);
}

static void on_sync_ts(module_data_t *mod, const uint8_t *ts)
{
    const bool ret = mpegts_sync_push(mod->sync, ts, 1);

    if (!ret)
    {
        asc_log_error(MSG("sync push failed, resetting buffer"));
        mpegts_sync_reset(mod->sync, SYNC_RESET_ALL);
    }
}

static void on_output_ts(module_data_t *mod, const uint8_t *ts)
{
    if(!mod->can_send)
    {
        mod->dropped++;
        return;
    }

    if(mod->is_rtp && mod->packet.skip == 0)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        const uint64_t msec = ((tv.tv_sec % 1000000) * 1000) + (tv.tv_usec / 1000);

        mod->packet.buffer[2] = (mod->rtpseq >> 8) & 0xFF;
        mod->packet.buffer[3] = (mod->rtpseq     ) & 0xFF;

        mod->packet.buffer[4] = (msec >> 24) & 0xFF;
        mod->packet.buffer[5] = (msec >> 16) & 0xFF;
        mod->packet.buffer[6] = (msec >>  8) & 0xFF;
        mod->packet.buffer[7] = (msec      ) & 0xFF;

        ++mod->rtpseq;

        mod->packet.skip += 12;
    }

    memcpy(&mod->packet.buffer[mod->packet.skip], ts, TS_PACKET_SIZE);
    mod->packet.skip += TS_PACKET_SIZE;

    if(mod->packet.skip > UDP_BUFFER_SIZE - TS_PACKET_SIZE)
    {
        const ssize_t ret = asc_socket_sendto(mod->sock
                                              , mod->packet.buffer
                                              , mod->packet.skip);

        if(ret == -1)
        {
            if(asc_socket_would_block())
            {
                mod->can_send = false;
                asc_socket_set_on_ready(mod->sock, on_ready);
            }
            else
                asc_log_warning(MSG("sendto(): %s"), asc_error_msg());
        }

        mod->packet.skip = 0;
    }
}

static void module_init(lua_State *L, module_data_t *mod)
{
    module_option_string(L, "addr", &mod->addr, NULL);
    if(mod->addr == NULL)
        luaL_error(L, "[udp_output] option 'addr' is required");

    mod->port = 1234;
    module_option_integer(L, "port", &mod->port);

    module_option_boolean(L, "rtp", &mod->is_rtp);
    if(mod->is_rtp)
    {
        const uint32_t rtpssrc = (uint32_t)rand();

#define RTP_PT_H261     31      /* RFC2032 */
#define RTP_PT_MP2T     33      /* RFC2250 */

        mod->packet.buffer[0 ] = 0x80; // RTP version
        mod->packet.buffer[1 ] = RTP_PT_MP2T;
        mod->packet.buffer[8 ] = (rtpssrc >> 24) & 0xFF;
        mod->packet.buffer[9 ] = (rtpssrc >> 16) & 0xFF;
        mod->packet.buffer[10] = (rtpssrc >>  8) & 0xFF;
        mod->packet.buffer[11] = (rtpssrc      ) & 0xFF;
    }

    mod->sock = asc_socket_open_udp4(mod);
    asc_socket_set_reuseaddr(mod->sock, 1);
    if(!asc_socket_bind(mod->sock, NULL, 0))
        luaL_error(L, MSG("couldn't bind socket"));

    int value;
    if(module_option_integer(L, "socket_size", &value))
        asc_socket_set_buffer(mod->sock, 0, value);

    const char *localaddr = NULL;
    module_option_string(L, "localaddr", &localaddr, NULL);
    if(localaddr)
        asc_socket_set_multicast_if(mod->sock, localaddr);

    value = 32;
    module_option_integer(L, "ttl", &value);
    asc_socket_set_multicast_ttl(mod->sock, value);

    asc_socket_multicast_join(mod->sock, mod->addr, NULL);
    asc_socket_set_sockaddr(mod->sock, mod->addr, mod->port);

    mod->can_send = false;
    asc_socket_set_on_ready(mod->sock, on_ready);

    stream_callback_t on_ts = on_output_ts;
    bool sync_on = false;
    module_option_boolean(L, "sync", &sync_on);

    if(sync_on)
    {
        mod->sync = mpegts_sync_init();

        mpegts_sync_set_on_write(mod->sync, (ts_callback_t)on_ts);
        mpegts_sync_set_arg(mod->sync, (void *)mod);
        mpegts_sync_set_fname(mod->sync, "udp/sync %s:%d"
                              , mod->addr, mod->port);

        const char *optstr = NULL;
        module_option_string(L, "sync_opts", &optstr, NULL);
        if (optstr != NULL && !mpegts_sync_parse_opts(mod->sync, optstr))
            luaL_error(L, MSG("invalid value for option 'sync_opts'"));

        mod->sync_loop = asc_timer_init(1, mpegts_sync_loop, mod->sync);
        on_ts = on_sync_ts;
    }

    module_stream_init(mod, on_ts);
}

static void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);

    ASC_FREE(mod->sync_loop, asc_timer_destroy);
    ASC_FREE(mod->sync, mpegts_sync_destroy);
    ASC_FREE(mod->sock, asc_socket_close);
}

MODULE_STREAM_METHODS()
MODULE_LUA_METHODS()
{
    MODULE_STREAM_METHODS_REF(),
};
MODULE_LUA_REGISTER(udp_output)
