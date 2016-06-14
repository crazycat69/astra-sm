/*
 * Astra Module: HTTP Module: MPEG-TS Streaming
 * http://cesbo.com/astra
 *
 * Copyright (C) 2014-2015, Andrey Dyldin <and@cesbo.com>
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
#include <luaapi/stream.h>

#include "../http.h"

struct module_data_t
{
    MODULE_DATA();

    int idx_callback;
};

struct http_response_t
{
    STREAM_MODULE_DATA();

    module_data_t *mod;

    uint8_t buffer[TS_PACKET_SIZE];
    size_t buffer_skip;
};

/*
 * client->mod - http_server module
 * client->response->mod - http_upstream module
 */

static void on_downstream_read(void *arg)
{
    http_client_t *const client = (http_client_t *)arg;

    const ssize_t size = asc_socket_recv(client->sock, client->buffer, HTTP_BUFFER_SIZE);
    if(size <= 0)
    {
        if (asc_socket_would_block())
            return;

        http_client_close(client);
        return;
    }

    ssize_t skip = 0;

    if(client->response->buffer_skip > 0)
    {
        skip = TS_PACKET_SIZE - client->response->buffer_skip;
        uint8_t *dst = &client->response->buffer[client->response->buffer_skip];
        if(size < skip)
        {
            memcpy(dst, client->buffer, size);
            client->response->buffer_skip += size;
        }
        else
        {
            memcpy(dst, client->buffer, skip);
            module_stream_send(client->response, client->response->buffer);
            client->response->buffer_skip = 0;
        }
    }

    while(skip < size)
    {
        const uint8_t *ts = (const uint8_t *)&client->buffer[skip];

        const size_t remain = size - skip;
        if(remain < TS_PACKET_SIZE)
        {
            memcpy(client->response->buffer, ts, remain);
            client->response->buffer_skip = remain;
            break;
        }
        else
        {
            module_stream_send(client->response, ts);
            skip += TS_PACKET_SIZE;
        }
    }
}

static void on_downstream_send(void *arg)
{
    http_client_t *const client = (http_client_t *)arg;
    lua_State *const L = MODULE_L(client->mod);

    if(!lua_islightuserdata(L, 2))
    {
        http_client_abort(client, 500, ":send() client instance required");
        return;
    }

    client->on_read = on_downstream_read;
    client->on_ready = NULL;
    client->on_send = NULL;

    const int idx_response = 3;

    lua_getfield(L, idx_response, "code");
    const int code = lua_tointeger(L, -1);
    lua_pop(L, 1); // code

    lua_getfield(L, idx_response, "message");
    const char *message = lua_isstring(L, -1) ? lua_tostring(L, -1) : NULL;
    lua_pop(L, 1); // message

    http_response_code(client, code, message);

    lua_getfield(L, idx_response, "headers");
    if(lua_istable(L, -1))
    {
        lua_foreach(L, -2)
        {
            const char *header = lua_tostring(L, -1);
            http_response_header(client, "%s", header);
        }
    }
    lua_pop(L, 1); // headers

    http_response_send(client);
}

static int module_call(lua_State *L, module_data_t *mod)
{
    http_client_t *const client = (http_client_t *)lua_touserdata(L, 3);

    if(lua_isnil(L, 4))
    {
        if(client->response)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, client->response->mod->idx_callback);
            lua_pushvalue(L, 2);
            lua_pushvalue(L, 3);
            lua_pushvalue(L, 4);
            lua_call(L, 3, 0);

            module_stream_destroy(client->response);

            free(client->response);
            client->response = NULL;
        }
        return 0;
    }

    client->response = ASC_ALLOC(1, http_response_t);
    client->response->mod = mod;

    client->on_send = on_downstream_send;

    // like module_stream_init()
    client->response->__stream.self = (module_data_t *)client;
    client->response->__stream.on_ts = NULL;
    __module_stream_init(&client->response->__stream);

    lua_rawgeti(L, LUA_REGISTRYINDEX, client->idx_request);
    lua_pushlightuserdata(L, &client->response->__stream);
    lua_setfield(L, -2, "stream");
    lua_pop(L, 1); // request

    lua_rawgeti(L, LUA_REGISTRYINDEX, client->response->mod->idx_callback);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_pushvalue(L, 4);
    lua_call(L, 3, 0);

    return 0;
}

static int __module_call(lua_State *L)
{
    module_data_t *const mod =
        (module_data_t *)lua_touserdata(L, lua_upvalueindex(1));

    return module_call(L, mod);
}

static void module_init(lua_State *L, module_data_t *mod)
{
    lua_getfield(L, MODULE_OPTIONS_IDX, "callback");
    asc_assert(lua_isfunction(L, -1), "[http_downstream] option 'callback' is required");
    mod->idx_callback = luaL_ref(L, LUA_REGISTRYINDEX);

    // Set callback for http route
    lua_getmetatable(L, 3);
    lua_pushlightuserdata(L, (void *)mod);
    lua_pushcclosure(L, __module_call, 1);
    lua_setfield(L, -2, "__call");
    lua_pop(L, 1);
}

static void module_destroy(module_data_t *mod)
{
    if(mod->idx_callback)
    {
        luaL_unref(MODULE_L(mod), LUA_REGISTRYINDEX, mod->idx_callback);
        mod->idx_callback = 0;
    }
}

MODULE_REGISTER(http_downstream)
{
    .init = module_init,
    .destroy = module_destroy,
};
