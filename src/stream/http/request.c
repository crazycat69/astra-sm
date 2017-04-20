/*
 * Astra Module: HTTP Request
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
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
 *      http_request
 *
 * Module Role (when streaming):
 *      Source or sink, no demux
 *
 * Module Options:
 *      host        - string, server hostname or IP address
 *      port        - number, server port (default: 80)
 *      path        - string, request path
 *      method      - string, method (default: "GET")
 *      version     - string, HTTP version (default: "HTTP/1.1")
 *      headers     - table, list of the request headers
 *      content     - string, request content
 *      stream      - boolean, true to read MPEG-TS stream
 *      sync        - boolean or number, enable stream synchronization
 *      sctp        - boolean, use sctp instead of tcp
 *      timeout     - number, request timeout
 *      callback    - function,
 *      upstream    - object, stream instance returned by module_instance:stream()
 */

#include <astra/astra.h>
#include <astra/core/timer.h>
#include <astra/luaapi/stream.h>
#include <astra/mpegts/sync.h>

#include "http.h"

#define MSG(_msg)                                       \
    "[http_request %s:%d%s] " _msg, mod->config.host    \
                                  , mod->config.port    \
                                  , mod->config.path

struct module_data_t
{
    STREAM_MODULE_DATA();

    struct
    {
        const char *host;
        int port;
        const char *path;
        bool sync;
        const char *sync_opts;
    } config;

    int timeout_ms;
    bool is_stream;
    bool stream_inited;

    int idx_self;

    asc_socket_t *sock;
    asc_timer_t *timeout;

    bool is_socket_busy;

    // request
    struct
    {
        int status; // 1 - connected, 2 - request done

        const char *buffer;
        size_t skip;
        size_t size;

        int idx_body;
    } request;

    bool is_head;
    bool is_connection_close;
    bool is_connection_keep_alive;

    // response
    char buffer[HTTP_BUFFER_SIZE];
    size_t buffer_skip;
    size_t chunk_left;

    int idx_response;
    int status_code;

    int status;         // 1 - empty line is found, 2 - request ready, 3 - release

    int idx_content;
    bool is_chunked;
    bool is_content_length;
    string_buffer_t *content;

    bool is_active;

    // receiver
    struct
    {
        void *arg;
        union
        {
            void (*fn)(void *, void *, size_t);
            void *ptr;
        } callback;
    } receiver;

    // stream
    struct
    {
        uint8_t *buf;
        size_t buf_size;
        size_t buf_count;
        size_t buf_read;
        size_t buf_write;
        size_t buf_fill;

        ts_sync_t *sync;
        asc_timer_t *sync_loop;
        size_t sync_ration_size;
        ssize_t sync_feed;
    } ts;
};

static const char __path[] = "path";
static const char __method[] = "method";
static const char __version[] = "version";
static const char __headers[] = "headers";
static const char __content[] = "content";
static const char __callback[] = "callback";
static const char __code[] = "code";
static const char __message[] = "message";

static const char __default_method[] = "GET";
static const char __default_path[] = "/";
static const char __default_version[] = "HTTP/1.1";

static const char __connection[] = "Connection: ";
static const char __close[] = "close";
static const char __keep_alive[] = "keep-alive";

static void on_close(void *);

static void callback(lua_State *L, module_data_t *mod)
{
    const int response = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, mod->idx_self);
    lua_getfield(L, -1, "__options");
    lua_getfield(L, -1, "callback");
    lua_pushvalue(L, -3);
    lua_pushvalue(L, response);
    if (lua_tr_call(L, 2, 0) != 0)
        lua_err_log(L);
    lua_pop(L, 3); // self + options + response
}

static void call_error(module_data_t *mod, const char *msg)
{
    lua_State *const L = module_lua(mod);

    lua_newtable(L);
    lua_pushinteger(L, 0);
    lua_setfield(L, -2, __code);
    lua_pushstring(L, msg);
    lua_setfield(L, -2, __message);
    callback(L, mod);
}

static void timeout_callback(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    ASC_FREE(mod->timeout, asc_timer_destroy);

    if(mod->request.status == 0)
    {
        mod->status = -1;
        mod->request.status = -1;
        call_error(mod, "connection timeout");
    }
    else
    {
        mod->status = -1;
        mod->request.status = -1;
        call_error(mod, "response timeout");
    }

    on_close(mod);
}

static void on_close(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;
    lua_State *const L = module_lua(mod);

    if(!mod->sock)
        return;

    if(mod->receiver.callback.ptr)
    {
        mod->receiver.callback.fn(mod->receiver.arg, NULL, 0);

        mod->receiver.arg = NULL;
        mod->receiver.callback.ptr = NULL;
    }

    asc_socket_close(mod->sock);
    mod->sock = NULL;

    ASC_FREE(mod->timeout, asc_timer_destroy);

    if(mod->request.buffer)
    {
        if(mod->request.status == 1)
            free((void *)mod->request.buffer);
        mod->request.buffer = NULL;
    }

    if(mod->request.idx_body)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, mod->request.idx_body);
        mod->request.idx_body = 0;
    }

    if(mod->request.status == 0)
    {
        mod->request.status = -1;
        call_error(mod, "connection failed");
    }
    else if(mod->status == 0)
    {
        mod->request.status = -1;
        call_error(mod, "failed to parse response");
    }

    if(mod->status == 2)
    {
        mod->status = 3;

        lua_rawgeti(L, LUA_REGISTRYINDEX, mod->idx_response);
        callback(L, mod);
    }

    if(mod->stream_inited)
    {
        module_stream_destroy(mod);

        if(mod->status == 3)
        {
            /* stream on_close */
            mod->status = -1;
            mod->request.status = -1;

            lua_pushnil(L);
            callback(L, mod);
        }
    }

    ASC_FREE(mod->ts.buf, free);
    ASC_FREE(mod->ts.sync_loop, asc_timer_destroy);
    ASC_FREE(mod->ts.sync, ts_sync_destroy);

    if(mod->idx_response)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, mod->idx_response);
        mod->idx_response = 0;
    }

    if(mod->idx_content)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, mod->idx_content);
        mod->idx_content = 0;
    }

    if(mod->idx_self)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, mod->idx_self);
        mod->idx_self = 0;
    }

    if(mod->content)
    {
        string_buffer_free(mod->content);
        mod->content = NULL;
    }
}

/*
 *  oooooooo8 ooooooooooo oooooooooo  ooooooooooo      o      oooo     oooo
 * 888        88  888  88  888    888  888    88      888      8888o   888
 *  888oooooo     888      888oooo88   888ooo8       8  88     88 888o8 88
 *         888    888      888  88o    888    oo    8oooo88    88  888  88
 * o88oooo888    o888o    o888o  88o8 o888ooo8888 o88o  o888o o88o  8  o88o
 *
 */

static void check_is_active(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    if(mod->is_active)
    {
        mod->is_active = false;
        return;
    }

    asc_log_error(MSG("receiving timeout"));
    on_close(mod);
}

static void on_sync_ready(void *arg);

static void on_ts_read(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    ssize_t size = asc_socket_recv(mod->sock
                                   , &mod->ts.buf[mod->ts.buf_write]
                                   , mod->ts.buf_size - mod->ts.buf_write);
    if(size <= 0)
    {
        on_close(mod);
        return;
    }

    mod->is_active = true;
    mod->ts.buf_write += size;
    mod->ts.buf_read = 0;

    while(1)
    {
        while(mod->ts.buf[mod->ts.buf_read] != 0x47)
        {
            ++mod->ts.buf_read;
            if(mod->ts.buf_read >= mod->ts.buf_write)
            {
                mod->ts.buf_write = 0;
                return;
            }
        }

        const size_t next = mod->ts.buf_read + TS_PACKET_SIZE;
        if(next > mod->ts.buf_write)
        {
            const size_t tail = mod->ts.buf_write - mod->ts.buf_read;
            if(tail > 0)
                memmove(mod->ts.buf, &mod->ts.buf[mod->ts.buf_read], tail);
            mod->ts.buf_write = tail;
            return;
        }

        if(mod->ts.sync != NULL)
        {
            if (!ts_sync_push(mod->ts.sync, &mod->ts.buf[mod->ts.buf_read], 1))
            {
                asc_log_error(MSG("sync push failed, resetting buffer"));
                ts_sync_reset(mod->ts.sync);

                return;
            }

            if (mod->ts.sync_feed > 0 && --mod->ts.sync_feed <= 0)
            {
                asc_socket_set_on_read(mod->sock, NULL);
                ts_sync_set_on_ready(mod->ts.sync, on_sync_ready);
            }
        }
        else
        {
            module_stream_send(mod, &mod->ts.buf[mod->ts.buf_read]);
        }

        mod->ts.buf_read += TS_PACKET_SIZE;
    }
}

static void on_sync_ready(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    ts_sync_set_on_ready(mod->ts.sync, NULL);
    asc_socket_set_on_read(mod->sock, on_ts_read);

    mod->ts.sync_feed = mod->ts.sync_ration_size;
}

/*
 * oooooooooo  ooooooooooo      o      ooooooooo
 *  888    888  888    88      888      888    88o
 *  888oooo88   888ooo8       8  88     888    888
 *  888  88o    888    oo    8oooo88    888    888
 * o888o  88o8 o888ooo8888 o88o  o888o o888ooo88
 *
 */

static void on_read(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;
    lua_State *const L = module_lua(mod);

    ASC_FREE(mod->timeout, asc_timer_destroy);

    ssize_t size = asc_socket_recv(  mod->sock
                                   , &mod->buffer[mod->buffer_skip]
                                   , HTTP_BUFFER_SIZE - mod->buffer_skip);
    if(size <= 0)
    {
        on_close(mod);
        return;
    }

    if(mod->receiver.callback.ptr)
    {
        mod->receiver.callback.fn(mod->receiver.arg, &mod->buffer[mod->buffer_skip], size);
        return;
    }

    if(mod->status == 3)
    {
        asc_log_warning(MSG("received data after response"));
        return;
    }

    size_t eoh = 0; // end of headers
    size_t skip = 0;
    mod->buffer_skip += size;

    if(mod->status == 0)
    {
        // check empty line
        while(skip < mod->buffer_skip)
        {
            if(   skip + 1 < mod->buffer_skip
               && mod->buffer[skip + 0] == '\n' && mod->buffer[skip + 1] == '\n')
            {
                eoh = skip + 2;
                mod->status = 1;
                break;
            }
            else if(   skip + 3 < mod->buffer_skip
                    && mod->buffer[skip + 0] == '\r' && mod->buffer[skip + 1] == '\n'
                    && mod->buffer[skip + 2] == '\r' && mod->buffer[skip + 3] == '\n')
            {
                eoh = skip + 4;
                mod->status = 1;
                break;
            }
            ++skip;
        }

        if(mod->status != 1)
            return;
    }

    if(mod->status == 1)
    {
        parse_match_t m[4];

        skip = 0;

/*
 *     oooooooooo  ooooooooooo  oooooooo8 oooooooooo
 *      888    888  888    88  888         888    888
 *      888oooo88   888ooo8     888oooooo  888oooo88
 * ooo  888  88o    888    oo          888 888
 * 888 o888o  88o8 o888ooo8888 o88oooo888 o888o
 *
 */

        if(!http_parse_response(mod->buffer, eoh, m))
        {
            call_error(mod, "failed to parse response line");
            on_close(mod);
            return;
        }

        lua_newtable(L);
        const int response = lua_gettop(L);

        lua_pushvalue(L, -1);
        if(mod->idx_response)
            luaL_unref(L, LUA_REGISTRYINDEX, mod->idx_response);
        mod->idx_response = luaL_ref(L, LUA_REGISTRYINDEX);

        lua_pushlstring(L, &mod->buffer[m[1].so], m[1].eo - m[1].so);
        lua_setfield(L, response, __version);

        mod->status_code = atoi(&mod->buffer[m[2].so]);
        lua_pushinteger(L, mod->status_code);
        lua_setfield(L, response, __code);

        lua_pushlstring(L, &mod->buffer[m[3].so], m[3].eo - m[3].so);
        lua_setfield(L, response, __message);

        skip += m[0].eo;

/*
 *     ooooo ooooo ooooooooooo      o      ooooooooo  ooooooooooo oooooooooo   oooooooo8
 *      888   888   888    88      888      888    88o 888    88   888    888 888
 *      888ooo888   888ooo8       8  88     888    888 888ooo8     888oooo88   888oooooo
 * ooo  888   888   888    oo    8oooo88    888    888 888    oo   888  88o           888
 * 888 o888o o888o o888ooo8888 o88o  o888o o888ooo88  o888ooo8888 o888o  88o8 o88oooo888
 *
 */

        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, response, __headers);
        const int headers = lua_gettop(L);

        while(skip < eoh)
        {
            if(!http_parse_header(&mod->buffer[skip], eoh - skip, m))
            {
                call_error(mod, "failed to parse response headers");
                on_close(mod);
                return;
            }

            if(m[1].eo == 0)
            { /* empty line */
                skip += m[0].eo;
                mod->status = 2;
                break;
            }

            lua_string_to_lower(L, &mod->buffer[skip], m[1].eo);
            lua_pushlstring(L, &mod->buffer[skip + m[2].so], m[2].eo - m[2].so);
            lua_settable(L, headers);

            skip += m[0].eo;
        }

        mod->chunk_left = 0;
        mod->is_content_length = false;

        if(mod->content)
        {
            free(mod->content);
            mod->content = NULL;
        }

        lua_getfield(L, headers, "content-length");
        if(lua_isnumber(L, -1))
        {
            mod->chunk_left = lua_tointeger(L, -1);
            if(mod->chunk_left > 0)
            {
                mod->is_content_length = true;
            }
        }
        lua_pop(L, 1); // content-length

        lua_getfield(L, headers, "transfer-encoding");
        if(lua_isstring(L, -1))
        {
            const char *encoding = lua_tostring(L, -1);
            mod->is_chunked = (strcmp(encoding, "chunked") == 0);
        }
        lua_pop(L, 1); // transfer-encoding

        if(mod->is_content_length || mod->is_chunked)
            mod->content = string_buffer_alloc();

        lua_pop(L, 2); // headers + response

        if(   (mod->is_head)
           || (mod->status_code >= 100 && mod->status_code < 200)
           || (mod->status_code == 204)
           || (mod->status_code == 304))
        {
            mod->status = 3;

            lua_rawgeti(L, LUA_REGISTRYINDEX, mod->idx_response);
            callback(L, mod);

            if(mod->is_connection_close)
                on_close(mod);

            mod->buffer_skip = 0;
            return;
        }

        if(mod->is_stream && mod->status_code == 200)
        {
            mod->status = 3;

            lua_rawgeti(L, LUA_REGISTRYINDEX, mod->idx_response);
            lua_pushboolean(L, mod->is_stream);
            lua_setfield(L, -2, "stream");
            callback(L, mod);

            mod->ts.buf = ASC_ALLOC(mod->ts.buf_size, uint8_t);
            mod->timeout = asc_timer_init(mod->timeout_ms, check_is_active, mod);

            asc_socket_set_on_read(mod->sock, on_ts_read);
            asc_socket_set_on_ready(mod->sock, NULL);

            if (mod->config.sync)
            {
                mod->ts.sync = ts_sync_init(module_stream_send, mod);

                ts_sync_set_fname(mod->ts.sync, "http_request %s:%d%s"
                                  , mod->config.host, mod->config.port
                                  , mod->config.path);

                if (mod->config.sync_opts != NULL
                    && !ts_sync_set_opts(mod->ts.sync, mod->config.sync_opts))
                {
                    asc_log_error(MSG("invalid value for option 'sync_opts'"));
                }

                mod->ts.sync_ration_size = HTTP_BUFFER_SIZE / TS_PACKET_SIZE;
                mod->ts.sync_feed = mod->ts.sync_ration_size;

                mod->ts.sync_loop = asc_timer_init(SYNC_INTERVAL_MSEC
                                                   , ts_sync_loop
                                                   , mod->ts.sync);
            }

            mod->buffer_skip = 0;
            return;
        }

        if(!mod->content)
        {
            mod->status = 3;

            lua_rawgeti(L, LUA_REGISTRYINDEX, mod->idx_response);
            callback(L, mod);

            if(mod->is_connection_close)
                on_close(mod);

            mod->buffer_skip = 0;
            return;
        }
    }

/*
 *       oooooooo8   ooooooo  oooo   oooo ooooooooooo ooooooooooo oooo   oooo ooooooooooo
 *     o888     88 o888   888o 8888o  88  88  888  88  888    88   8888o  88  88  888  88
 *     888         888     888 88 888o88      888      888ooo8     88 888o88      888
 * ooo 888o     oo 888o   o888 88   8888      888      888    oo   88   8888      888
 * 888  888oooo88    88ooo88  o88o    88     o888o    o888ooo8888 o88o    88     o888o
 *
 */

    // Transfer-Encoding: chunked
    if(mod->is_chunked)
    {
        parse_match_t m[2];

        while(skip < mod->buffer_skip)
        {
            if(!mod->chunk_left)
            {
                if(!http_parse_chunk(&mod->buffer[skip], mod->buffer_skip - skip, m))
                {
                    call_error(mod, "invalid chunk");
                    on_close(mod);
                    return;
                }

                mod->chunk_left = 0;
                for(size_t i = m[1].so; i < m[1].eo; ++i)
                {
                    char c = mod->buffer[skip + i];
                    if(c >= '0' && c <= '9')
                        mod->chunk_left = (mod->chunk_left << 4) | (c - '0');
                    else if(c >= 'a' && c <= 'f')
                        mod->chunk_left = (mod->chunk_left << 4) | (c - 'a' + 0x0A);
                    else if(c >= 'A' && c <= 'F')
                        mod->chunk_left = (mod->chunk_left << 4) | (c - 'A' + 0x0A);
                }
                skip += m[0].eo;

                if(!mod->chunk_left)
                {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, mod->idx_response);
                    string_buffer_push(L, mod->content);
                    mod->content = NULL;
                    lua_setfield(L, -2, __content);
                    mod->status = 3;
                    callback(L, mod);

                    if(mod->is_connection_close)
                    {
                        on_close(mod);
                        return;
                    }

                    break;
                }

                mod->chunk_left += 2;
            }

            const size_t tail = size - skip;
            if(mod->chunk_left <= tail)
            {
                string_buffer_addlstring(mod->content, &mod->buffer[skip], mod->chunk_left - 2);

                skip += mod->chunk_left;
                mod->chunk_left = 0;
            }
            else
            {
                string_buffer_addlstring(mod->content, &mod->buffer[skip], tail);
                mod->chunk_left -= tail;
                break;
            }
        }

        mod->buffer_skip = 0;
        return;
    }

    // Content-Length: *
    if(mod->is_content_length)
    {
        const size_t tail = mod->buffer_skip - skip;

        if(mod->chunk_left > tail)
        {
            string_buffer_addlstring(mod->content, &mod->buffer[skip], tail);
            mod->chunk_left -= tail;
        }
        else
        {
            string_buffer_addlstring(mod->content, &mod->buffer[skip], mod->chunk_left);
            mod->chunk_left = 0;

            lua_rawgeti(L, LUA_REGISTRYINDEX, mod->idx_response);
            string_buffer_push(L, mod->content);
            mod->content = NULL;
            lua_setfield(L, -2, __content);
            mod->status = 3;
            callback(L, mod);

            if(mod->is_connection_close)
            {
                on_close(mod);
                return;
            }
        }

        mod->buffer_skip = 0;
        return;
    }
}

/*
 *  oooooooo8 ooooooooooo oooo   oooo ooooooooo
 * 888         888    88   8888o  88   888    88o
 *  888oooooo  888ooo8     88 888o88   888    888
 *         888 888    oo   88   8888   888    888
 * o88oooo888 o888ooo8888 o88o    88  o888ooo88
 *
 */

static void on_ready_send_content(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    ASC_ASSERT(mod->request.size > 0, MSG("invalid content size"));

    const size_t rem = mod->request.size - mod->request.skip;
    const size_t cap = (rem > HTTP_BUFFER_SIZE) ? HTTP_BUFFER_SIZE : rem;

    const ssize_t send_size = asc_socket_send(  mod->sock
                                              , &mod->request.buffer[mod->request.skip]
                                              , cap);
    if(send_size == -1)
    {
        asc_log_error(MSG("failed to send content: %s"), asc_error_msg());
        on_close(mod);
        return;
    }
    mod->request.skip += send_size;

    if(mod->request.skip >= mod->request.size)
    {
        mod->request.buffer = NULL;

        luaL_unref(module_lua(mod), LUA_REGISTRYINDEX, mod->request.idx_body);
        mod->request.idx_body = 0;

        mod->request.status = 3;

        asc_socket_set_on_ready(mod->sock, NULL);
    }
}

static void on_ready_send_request(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    ASC_ASSERT(mod->request.size > 0, MSG("invalid request size"));

    const size_t rem = mod->request.size - mod->request.skip;
    const size_t cap = (rem > HTTP_BUFFER_SIZE) ? HTTP_BUFFER_SIZE : rem;

    const ssize_t send_size = asc_socket_send(  mod->sock
                                              , &mod->request.buffer[mod->request.skip]
                                              , cap);
    if(send_size == -1)
    {
        asc_log_error(MSG("failed to send response: %s"), asc_error_msg());
        on_close(mod);
        return;
    }
    mod->request.skip += send_size;

    if(mod->request.skip >= mod->request.size)
    {
        free((void *)mod->request.buffer);
        mod->request.buffer = NULL;

        if(mod->request.idx_body)
        {
            lua_State *const L = module_lua(mod);

            lua_rawgeti(L, LUA_REGISTRYINDEX, mod->request.idx_body);
            mod->request.buffer = lua_tostring(L, -1);
            mod->request.size = luaL_len(L, -1);
            mod->request.skip = 0;
            lua_pop(L, 1);

            mod->request.status = 2;

            asc_socket_set_on_ready(mod->sock, on_ready_send_content);
        }
        else
        {
            mod->request.status = 3;

            asc_socket_set_on_ready(mod->sock, NULL);
        }
    }
}

static void lua_make_request(lua_State *L, module_data_t *mod)
{
    ASC_ASSERT(lua_istable(L, -1),
        MSG("%s() requires table on top of the stack"), __func__);

    lua_getfield(L, -1, __method);
    const char *method = lua_isstring(L, -1) ? lua_tostring(L, -1) : __default_method;
    lua_pop(L, 1);

    mod->is_head = (strcmp(method, "HEAD") == 0);

    lua_getfield(L, -1, __path);
    mod->config.path = lua_isstring(L, -1) ? lua_tostring(L, -1) : __default_path;
    lua_pop(L, 1);

    lua_getfield(L, -1, __version);
    const char *version = lua_isstring(L, -1) ? lua_tostring(L, -1) : __default_version;
    lua_pop(L, 1);

    string_buffer_t *buffer = string_buffer_alloc();

    string_buffer_addfstring(buffer, "%s %s %s\r\n", method, mod->config.path, version);

    lua_getfield(L, -1, __headers);
    if(lua_istable(L, -1))
    {
        for(lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
        {
            const char *h = lua_tostring(L, -1);

            if(!strncasecmp(h, __connection, sizeof(__connection) - 1))
            {
                const char *hp = &h[sizeof(__connection) - 1];
                if(!strncasecmp(hp, __close, sizeof(__close) - 1))
                    mod->is_connection_close = true;
                else if(!strncasecmp(hp, __keep_alive, sizeof(__keep_alive) - 1))
                    mod->is_connection_keep_alive = true;
            }

            string_buffer_addfstring(buffer, "%s\r\n", h);
        }
    }
    lua_pop(L, 1); // headers

    string_buffer_addlstring(buffer, "\r\n", 2);

    mod->request.buffer = string_buffer_release(buffer, &mod->request.size);
    mod->request.skip = 0;

    if(mod->request.idx_body)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, mod->request.idx_body);
        mod->request.idx_body = 0;
    }

    lua_getfield(L, -1, __content);
    if(lua_isstring(L, -1))
        mod->request.idx_body = luaL_ref(L, LUA_REGISTRYINDEX);
    else
        lua_pop(L, 1);
}

static void on_connect(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;
    lua_State *const L = module_lua(mod);

    mod->request.status = 1;

    ASC_FREE(mod->timeout, asc_timer_destroy);
    mod->timeout = asc_timer_init(mod->timeout_ms, timeout_callback, mod);

    lua_rawgeti(L, LUA_REGISTRYINDEX, mod->idx_self);
    lua_getfield(L, -1, "__options");
    lua_make_request(L, mod);
    lua_pop(L, 2); // self + __options

    asc_socket_set_on_read(mod->sock, on_read);
    asc_socket_set_on_ready(mod->sock, on_ready_send_request);
}

static void on_upstream_ready(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    if(mod->ts.buf_count > 0)
    {
        size_t block_size = (mod->ts.buf_write > mod->ts.buf_read)
                          ? (mod->ts.buf_write - mod->ts.buf_read)
                          : (mod->ts.buf_size - mod->ts.buf_read);

        if(block_size > mod->ts.buf_count)
            block_size = mod->ts.buf_count;

        const ssize_t send_size = asc_socket_send(  mod->sock
                                                  , &mod->ts.buf[mod->ts.buf_read]
                                                  , block_size);

        if(send_size > 0)
        {
            mod->ts.buf_count -= send_size;
            mod->ts.buf_read += send_size;
            if(mod->ts.buf_read >= mod->ts.buf_size)
                mod->ts.buf_read = 0;
        }
        else if(send_size == -1)
        {
            asc_log_error(MSG("failed to send ts (%zu bytes): %s")
                          , block_size, asc_error_msg());
            on_close(mod);
            return;
        }
    }

    if(mod->ts.buf_count == 0)
    {
        asc_socket_set_on_ready(mod->sock, NULL);
        mod->is_socket_busy = false;
    }
}

static void on_ts(module_data_t *mod, const uint8_t *ts)
{
    if(mod->status != 3 || mod->status_code != 200)
        return;

    if(mod->ts.buf_count + TS_PACKET_SIZE >= mod->ts.buf_size)
    {
        // overflow
        mod->ts.buf_count = 0;
        mod->ts.buf_read = 0;
        mod->ts.buf_write = 0;
        if(mod->is_socket_busy)
        {
            asc_socket_set_on_ready(mod->sock, NULL);
            mod->is_socket_busy = false;
        }
        return;
    }

    const size_t buffer_write = mod->ts.buf_write + TS_PACKET_SIZE;
    if(buffer_write < mod->ts.buf_size)
    {
        memcpy(&mod->ts.buf[mod->ts.buf_write], ts, TS_PACKET_SIZE);
        mod->ts.buf_write = buffer_write;
    }
    else if(buffer_write > mod->ts.buf_size)
    {
        const size_t ts_head = mod->ts.buf_size - mod->ts.buf_write;
        memcpy(&mod->ts.buf[mod->ts.buf_write], ts, ts_head);
        mod->ts.buf_write = TS_PACKET_SIZE - ts_head;
        memcpy(mod->ts.buf, &ts[ts_head], mod->ts.buf_write);
    }
    else
    {
        memcpy(&mod->ts.buf[mod->ts.buf_write], ts, TS_PACKET_SIZE);
        mod->ts.buf_write = 0;
    }
    mod->ts.buf_count += TS_PACKET_SIZE;

    if(   mod->is_socket_busy == false
       && mod->ts.buf_count >= mod->ts.buf_fill)
    {
        asc_socket_set_on_ready(mod->sock, on_upstream_ready);
        mod->is_socket_busy = true;
    }
}

/*
 * oooo     oooo  ooooooo  ooooooooo  ooooo  oooo ooooo       ooooooooooo
 *  8888o   888 o888   888o 888    88o 888    88   888         888    88
 *  88 888o8 88 888     888 888    888 888    88   888         888ooo8
 *  88  888  88 888o   o888 888    888 888    88   888      o  888    oo
 * o88o  8  o88o  88ooo88  o888ooo88    888oo88   o888ooooo88 o888ooo8888
 *
 */

static int method_set_receiver(lua_State *L, module_data_t *mod)
{
    if(lua_isnil(L, -1))
    {
        mod->receiver.arg = NULL;
        mod->receiver.callback.ptr = NULL;
    }
    else
    {
        mod->receiver.arg = lua_touserdata(L, -2);
        mod->receiver.callback.ptr = lua_touserdata(L, -1);
    }
    return 0;
}

static int method_send(lua_State *L, module_data_t *mod)
{
    mod->status = 0;

    ASC_FREE(mod->timeout, asc_timer_destroy);
    mod->timeout = asc_timer_init(mod->timeout_ms, timeout_callback, mod);

    ASC_ASSERT(lua_istable(L, 2), MSG(":send() table required"));
    lua_pushvalue(L, 2);
    lua_make_request(L, mod);
    lua_pop(L, 2); // :send() options

    asc_socket_set_on_read(mod->sock, on_read);
    asc_socket_set_on_ready(mod->sock, on_ready_send_request);

    return 0;
}

static int method_close(lua_State *L, module_data_t *mod)
{
    ASC_UNUSED(L);

    mod->status = -1;
    mod->request.status = -1;
    on_close(mod);

    return 0;
}

static void module_init(lua_State *L, module_data_t *mod)
{
    module_option_string(L, "host", &mod->config.host, NULL);
    ASC_ASSERT(mod->config.host != NULL, MSG("option 'host' is required"));

    mod->config.port = 80;
    module_option_integer(L, "port", &mod->config.port);

    mod->config.path = __default_path;
    module_option_string(L, __path, &mod->config.path, NULL);

    lua_getfield(L, MODULE_OPTIONS_IDX, __callback);
    ASC_ASSERT(lua_isfunction(L, -1), MSG("option 'callback' is required"));
    lua_pop(L, 1); // callback

    // store self in registry
    lua_pushvalue(L, -1);
    mod->idx_self = luaL_ref(L, LUA_REGISTRYINDEX);

    module_option_boolean(L, "stream", &mod->is_stream);
    if(mod->is_stream)
    {
        module_stream_init(L, mod, NULL);
        module_demux_set(mod, NULL, NULL);
        mod->stream_inited = true;

        module_option_boolean(L, "sync", &mod->config.sync);
        module_option_string(L, "sync_opts", &mod->config.sync_opts, NULL);

        mod->ts.buf_size = HTTP_BUFFER_SIZE;
    }

    lua_getfield(L, MODULE_OPTIONS_IDX, "upstream");
    if(!lua_isnil(L, -1))
    {
        ASC_ASSERT(mod->is_stream != true, MSG("option 'upstream' is not allowed in stream mode"));

        module_stream_init(L, mod, on_ts);
        module_demux_set(mod, NULL, NULL);
        mod->stream_inited = true;

        int value = 1024;
        module_option_integer(L, "buffer_size", &value);
        mod->ts.buf_size = value * 1024;
        mod->ts.buf = ASC_ALLOC(mod->ts.buf_size, uint8_t);

        value = 128;
        module_option_integer(L, "buffer_fill", &value);
        mod->ts.buf_fill = value * 1024;
    }
    lua_pop(L, 1);

    mod->timeout_ms = 10;
    module_option_integer(L, "timeout", &mod->timeout_ms);
    mod->timeout_ms *= 1000;
    mod->timeout = asc_timer_init(mod->timeout_ms, timeout_callback, mod);

    bool sctp = false;
    module_option_boolean(L, "sctp", &sctp);
    if(sctp == true)
        mod->sock = asc_socket_open_sctp4(mod);
    else
        mod->sock = asc_socket_open_tcp4(mod);

    asc_socket_connect(mod->sock, mod->config.host, mod->config.port, on_connect, on_close);
}

static void module_destroy(module_data_t *mod)
{
    mod->status = -1;
    mod->request.status = -1;

    on_close(mod);
}

static const module_method_t module_methods[] =
{
    { "send", method_send },
    { "close", method_close },
    { "set_receiver", method_set_receiver },
    { NULL, NULL },
};

STREAM_MODULE_REGISTER(http_request)
{
    .init = module_init,
    .destroy = module_destroy,
    .methods = module_methods,
};
