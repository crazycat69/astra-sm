/*
 * Astra Module: HTTP Module: WebSocket
 * http://cesbo.com/astra
 *
 * Copyright (C) 2014, Andrey Dyldin <and@cesbo.com>
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
#include <astra/core/list.h>
#include <astra/luaapi/module.h>
#include <astra/utils/base64.h>
#include <astra/utils/sha1.h>

#include "../http.h"

/* WebSocket Frame */
#define FRAME_HEADER_SIZE 2
#define FRAME_KEY_SIZE 4
#define FRAME_SIZE8_SIZE 0
#define FRAME_SIZE16_SIZE 2
#define FRAME_SIZE64_SIZE 8

struct module_data_t
{
    MODULE_DATA();

    int idx_callback;
};

typedef struct
{
    uint8_t *buffer;
    size_t size;
    size_t skip;
} frame_t;

struct http_response_t
{
    module_data_t *mod;

    uint32_t header_size;
    uint32_t data_size;

    uint8_t frame_key[FRAME_KEY_SIZE];
    uint8_t frame_key_i;

    asc_list_t *frame_queue;
};

/*
 * client->mod - http_server module
 * client->response->mod - http_websocket module
 */

static const char __websocket_magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static void on_websocket_ready(void *arg)
{
    http_client_t *const client = (http_client_t *)arg;
    http_response_t *const response = client->response;

    asc_list_first(response->frame_queue);
    frame_t *frame = (frame_t *)asc_list_data(response->frame_queue);

    ssize_t size = asc_socket_send(  client->sock
                                   , &frame->buffer[frame->skip]
                                   , frame->size - frame->skip);
    if(size <= 0)
    {
        http_client_error(client, "failed to send data: %s", asc_error_msg());
        http_client_close(client);
        return;
    }

    frame->skip += size;

    if(frame->size == frame->skip)
    {
        free(frame->buffer);
        free(frame);
        asc_list_remove_current(response->frame_queue);
        if(asc_list_eol(response->frame_queue))
            asc_socket_set_on_ready(client->sock, NULL);
    }
}

/* Stack: 1 - server, 2 - client, 3 - response */
static void on_websocket_send(void *arg)
{
    http_client_t *const client = (http_client_t *)arg;
    http_response_t *const response = client->response;
    lua_State *const L = module_lua(client->mod);

    const char *str = lua_tostring(L, 3);
    const int str_size = luaL_len(L, 3);

    frame_t *const frame = ASC_ALLOC(1, frame_t);

    if(str_size <= 125)
    {
        frame->size = FRAME_HEADER_SIZE;
        frame->buffer = ASC_ALLOC(frame->size + str_size, uint8_t);

        frame->buffer[1] = str_size & 0xFF;
    }
    else if(str_size <= 0xFFFF)
    {
        frame->size = FRAME_HEADER_SIZE + FRAME_SIZE16_SIZE;
        frame->buffer = ASC_ALLOC(frame->size + str_size, uint8_t);

        frame->buffer[1] = 126;
        frame->buffer[2] = (str_size >> 8) & 0xFF;
        frame->buffer[3] = (str_size     ) & 0xFF;
    }
    else
    {
        frame->size = FRAME_HEADER_SIZE + FRAME_SIZE64_SIZE;
        frame->buffer = ASC_ALLOC(frame->size + str_size, uint8_t);

        frame->buffer[1] = 127;
        frame->buffer[2] = 0;
        frame->buffer[3] = 0;
        frame->buffer[4] = 0;
        frame->buffer[5] = 0;
        frame->buffer[6] = (str_size >> 24) & 0xFF;
        frame->buffer[7] = (str_size >> 16) & 0xFF;
        frame->buffer[8] = (str_size >> 8 ) & 0xFF;
        frame->buffer[9] = (str_size      ) & 0xFF;
    }

    frame->buffer[0] = 0x81;
    memcpy(&frame->buffer[frame->size], str, str_size);
    frame->size += str_size;
    frame->skip = 0;

    asc_list_insert_tail(response->frame_queue, frame);
    if(asc_list_count(response->frame_queue) == 1)
        asc_socket_set_on_ready(client->sock, on_websocket_ready);
}

static void on_websocket_read(void *arg)
{
    http_client_t *const client = (http_client_t *)arg;
    http_response_t *const response = client->response;
    lua_State *const L = module_lua(client->mod);

    ssize_t size;
    uint8_t *data = (uint8_t *)client->buffer;

    if(response->header_size == 0)
    {
        size = asc_socket_recv(client->sock, data, FRAME_HEADER_SIZE);
        if(size <= 0)
        {
            http_client_close(client);
            return;
        }

        // TODO: check FIN, OPCODE
        // const bool fin = ((data[0] & 0x80) == 0x80);

        const uint8_t opcode = data[0] & 0x0F;
        if(opcode == 0x08)
        {
            http_client_close(client);
            return;
        }
        else if(opcode != 0x01)
        {
            http_client_error(client, "wrong opcode type");
            http_client_close(client);
            return;
        }

        const uint8_t data_size = data[1] & 0x7F;
        if(data_size < 126)
            response->header_size = FRAME_HEADER_SIZE + FRAME_SIZE8_SIZE + FRAME_KEY_SIZE;
        else if(data_size == 126)
            response->header_size = FRAME_HEADER_SIZE + FRAME_SIZE16_SIZE + FRAME_KEY_SIZE;
        else if(data_size == 127)
            response->header_size = FRAME_HEADER_SIZE + FRAME_SIZE64_SIZE + FRAME_KEY_SIZE;
        else
        {
            http_client_error(client, "wrong websocket frame format");
            http_client_close(client);
            return;
        }

        return;
    }

    if(response->data_size == 0)
    {
        size = asc_socket_recv(  client->sock
                                       , &data[FRAME_HEADER_SIZE]
                                       , response->header_size - FRAME_HEADER_SIZE);
        if(size <= 0)
        {
            http_client_close(client);
            return;
        }

        const uint8_t data_size = data[1] & 0x7F;
        if(data_size < 126)
        {
            response->data_size = data_size;
        }
        else if(data_size == 126)
        {
            response->data_size = (data[2] << 8) | data[3];
        }
        else if(data_size == 127)
        {
            if(data[2] || data[3] || data[4] || data[5])
            {
                http_client_error(client, "wrong frame size");
                http_client_close(client);
                return;
            }
            response->data_size = (  (data[6] << 24)
                                   | (data[7] << 16)
                                   | (data[8] << 8 )
                                   | (data[9]      ));
        }

        response->frame_key_i = 0;
        memcpy(  response->frame_key
               , &data[response->header_size - FRAME_KEY_SIZE]
               , FRAME_KEY_SIZE);
        return;
    }

    const uint32_t data_size = (response->data_size <= HTTP_BUFFER_SIZE)
                             ? response->data_size
                             : HTTP_BUFFER_SIZE;

    size = asc_socket_recv(client->sock, data, data_size);
    if(size <= 0)
    {
        http_client_close(client);
        return;
    }

    // TODO: SSE
    uint32_t skip = 0;
    while(skip < (uint32_t)size)
    {
        data[skip] ^= response->frame_key[response->frame_key_i];
        ++skip;
        response->frame_key_i = (response->frame_key_i + 1) % 4;
    }

    if(response->data_size == (uint32_t)size)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, response->mod->idx_callback);
        lua_rawgeti(L, LUA_REGISTRYINDEX, client->idx_server);
        lua_pushlightuserdata(L, client);

        if(client->content)
        {
            string_buffer_addlstring(client->content, (const char *)data, response->data_size);
            string_buffer_push(L, client->content);
            client->content = NULL;
        }
        else
        {
            lua_pushlstring(L, (const char *)data, response->data_size);
        }

        if (lua_tr_call(L, 3, 0) != 0)
            lua_err_log(L);

        response->header_size = 0;
        response->data_size = 0;
    }
    else
    {
        if(!client->content)
            client->content = string_buffer_alloc();

        string_buffer_addlstring(client->content, (const char *)data, size);

        response->data_size -= size;

        if(response->data_size == 0)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, response->mod->idx_callback);
            lua_rawgeti(L, LUA_REGISTRYINDEX, client->idx_server);
            lua_pushlightuserdata(L, client);
            string_buffer_push(L, client->content);
            client->content = NULL;
            if (lua_tr_call(L, 3, 0) != 0)
                lua_err_log(L);

            response->header_size = 0;
            response->data_size = 0;
        }
    }
}

static int module_call(lua_State *L, module_data_t *mod)
{
    http_client_t *const client = (http_client_t *)lua_touserdata(L, 3);

    if(lua_isnil(L, 4))
    {
        if(client->response)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, client->response->mod->idx_callback);
            lua_rawgeti(L, LUA_REGISTRYINDEX, client->idx_server);
            lua_pushlightuserdata(L, client);
            lua_pushnil(L);
            if (lua_tr_call(L, 3, 0) != 0)
                lua_err_log(L);

            if(client->content)
            {
                string_buffer_free(client->content);
                client->content = NULL;
            }
            if(client->response->frame_queue)
            {
                asc_list_for(client->response->frame_queue)
                {
                    frame_t *frame = (frame_t *)asc_list_data(client->response->frame_queue);
                    free(frame->buffer);
                    free(frame);
                }
                asc_list_destroy(client->response->frame_queue);
                client->response->frame_queue = NULL;
            }
            free(client->response);
            client->response = NULL;
        }
        return 0;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, client->idx_request);
    lua_getfield(L, -1, "headers");

    lua_getfield(L, -1, "upgrade");
    if(lua_isnil(L, -1))
    {
        lua_pop(L, 3);
        http_client_abort(client, 400, NULL);
        return 0;
    }
    const char *upgrade = lua_tostring(L, -1);
    if(strcmp(upgrade, "websocket") != 0)
    {
        lua_pop(L, 3);
        http_client_abort(client, 400, NULL);
        return 0;
    }
    lua_pop(L, 1); // upgrade

    char *accept_key = NULL;

    lua_getfield(L, -1, "sec-websocket-key");
    if(lua_isstring(L, -1))
    {
        const char *key = lua_tostring(L, -1);
        const int key_size = luaL_len(L, -1);
        sha1_ctx_t ctx;
        memset(&ctx, 0, sizeof(sha1_ctx_t));
        au_sha1_init(&ctx);
        au_sha1_update(&ctx, (const uint8_t *)key, key_size);
        au_sha1_update(&ctx, __websocket_magic, sizeof(__websocket_magic) - 1);
        uint8_t digest[SHA1_DIGEST_SIZE];
        au_sha1_final(&ctx, digest);
        accept_key = au_base64_enc(digest, sizeof(digest), NULL);
    }
    lua_pop(L, 1); // sec-websocket-key

    lua_pop(L, 2); // request + headers

    client->response = ASC_ALLOC(1, http_response_t);
    client->response->mod = mod;
    client->response->frame_queue = asc_list_init();
    client->on_send = on_websocket_send;
    client->on_read = on_websocket_read;
    client->on_ready = NULL;

    http_response_code(client, 101, "Switching Protocols");
    http_response_header(client, "Upgrade: websocket");
    http_response_header(client, "Connection: Upgrade");
    if(accept_key)
    {
        http_response_header(client, "Sec-WebSocket-Accept: %s", accept_key);
        free(accept_key);
    }
    http_response_send(client);

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
    ASC_ASSERT(lua_isfunction(L, -1), "[http_websocket] option 'callback' is required");
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
        luaL_unref(module_lua(mod), LUA_REGISTRYINDEX, mod->idx_callback);
        mod->idx_callback = 0;
    }
}

MODULE_REGISTER(http_websocket)
{
    .init = module_init,
    .destroy = module_destroy,
};
