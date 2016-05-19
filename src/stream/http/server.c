/*
 * Astra Module: HTTP Server
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
 *      http_server
 *
 * Module Options:
 *      addr         - string, server IP address
 *      port         - number, server port
 *      server_name  - string, default value: "Astra"
 *      http_version - string, default value: "HTTP/1.1"
 *      sctp         - boolean, use sctp instead of tcp
 *      route        - list, format: { { "/path", callback }, ... }
 *
 * Module Methods:
 *      port()      - return number, server port
 *      close()     - close server
 *      close(client)
 *                  - close client connection
 *      send(client, response)
 *                  - response - table, possible values:
 *                    * code - number, response code. required
 *                    * message - string, response code description. default: see http_code()
 *                    * headers - table (list of strings), response headers
 *                    * content - string, response body from the string
 *      data(client)
 *                  - return table, client data
 */

#include <astra.h>
#include <core/timer.h>
#include <luaapi/stream.h>

#include "http.h"

#define MSG(_msg) "[http_server %s:%d] " _msg, mod->addr, mod->port

struct module_data_t
{
    MODULE_LUA_DATA();

    int idx_self;

    const char *addr;
    int port;
    const char *server_name;
    const char *http_version;

    asc_list_t *routes;

    asc_socket_t *sock;
    asc_list_t *clients;
};

typedef struct
{
    const char *path;
    int idx_callback;
} route_t;

static const char __method[] = "method";
static const char __version[] = "version";
static const char __path[] = "path";
static const char __query[] = "query";
static const char __headers[] = "headers";
static const char __content[] = "content";
static const char __code[] = "code";
static const char __message[] = "message";

static const char __content_length[] = "Content-Length: ";
static const char __connection_close[] = "Connection: close";

/*
 *   oooooooo8 ooooo       ooooo ooooooooooo oooo   oooo ooooooooooo
 * o888     88  888         888   888    88   8888o  88  88  888  88
 * 888          888         888   888ooo8     88 888o88      888
 * 888o     oo  888      o  888   888    oo   88   8888      888
 *  888oooo88  o888ooooo88 o888o o888ooo8888 o88o    88     o888o
 *
 */

static void callback(lua_State *L, http_client_t *client)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, client->idx_callback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, client->mod->idx_self);
    lua_pushlightuserdata(L, client);
    if(client->status == 3)
        lua_rawgeti(L, LUA_REGISTRYINDEX, client->idx_request);
    else
        lua_pushnil(L);
    lua_call(L, 3, 0);
}

static void on_client_close(void *arg)
{
    http_client_t *const client = (http_client_t *)arg;
    module_data_t *const mod = client->mod;
    lua_State *const L = MODULE_L(mod);

    if(!client->sock)
        return;

    asc_socket_close(client->sock);
    client->sock = NULL;

    if(client->status == 3)
    {
        client->status = 0;
        callback(L, client);
    }

    if(client->response)
        asc_log_error(MSG("client instance is not released"));

    if(client->idx_content)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, client->idx_content);
        client->idx_content = 0;
    }

    if(client->idx_request)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, client->idx_request);
        client->idx_request = 0;
    }

    if(client->idx_data)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, client->idx_data);
        client->idx_data = 0;
    }

    if(client->content)
    {
        string_buffer_free(client->content);
        client->content = NULL;
    }

    asc_list_remove_item(mod->clients, client);
    free(client);
}

static bool routecmp(const char *path, const char *route)
{
    size_t skip = 0;
    while(1)
    {
        const char cp = path[skip];
        const char cr = route[skip];

        if(cp == cr)
        {
            if(cp == '\0')
                return true;

            ++skip;
        }
        else
        {
            if(cr == '*')
                return true;

            break;
        }
    }

    return false;
}

/*
 * oooooooooo  ooooooooooo      o      ooooooooo
 *  888    888  888    88      888      888    88o
 *  888oooo88   888ooo8       8  88     888    888
 *  888  88o    888    oo    8oooo88    888    888
 * o888o  88o8 o888ooo8888 o88o  o888o o888ooo88
 *
 */

static void on_client_read(void *arg)
{
    http_client_t *const client = (http_client_t *)arg;
    module_data_t *const mod = client->mod;
    lua_State *const L = MODULE_L(mod);

    ssize_t size = asc_socket_recv(  client->sock
                                   , &client->buffer[client->buffer_skip]
                                   , HTTP_BUFFER_SIZE - client->buffer_skip);
    if(size <= 0)
    {
        on_client_close(client);
        return;
    }

    if(client->status == 3)
    {
        asc_log_warning(MSG("received data after request"));
        return;
    }

    char *uri_host = NULL;
    size_t uri_host_size = 0;

    size_t eoh = 0; // end of headers
    size_t skip = 0;
    client->buffer_skip += size;

    if(client->status == 0)
    {
        // check empty line
        while(skip < client->buffer_skip)
        {
            if(   client->buffer[skip + 0] == '\r'
               && client->buffer[skip + 1] == '\n'
               && client->buffer[skip + 2] == '\r'
               && client->buffer[skip + 3] == '\n')
            {
                eoh = skip + 4;
                client->status = 1; // empty line is found
                break;
            }
            ++skip;
        }

        if(client->status != 1)
            return;
    }

    if(client->status == 1)
    {
        parse_match_t m[4];

        skip = 0;

/*
 *     oooooooooo  ooooooooooo  ooooooo  ooooo  oooo ooooooooooo  oooooooo8 ooooooooooo
 *      888    888  888    88 o888   888o 888    88   888    88  888        88  888  88
 *      888oooo88   888ooo8   888     888 888    88   888ooo8     888oooooo     888
 * ooo  888  88o    888    oo 888o  8o888 888    88   888    oo          888    888
 * 888 o888o  88o8 o888ooo8888  88ooo88    888oo88   o888ooo8888 o88oooo888    o888o
 *                                   88o8
 */

        if(!http_parse_request(client->buffer, eoh, m))
        {
            asc_log_error(MSG("failed to parse request line"));
            on_client_close(client);
            return;
        }

        lua_newtable(L);
        const int request = lua_gettop(L);

        lua_pushvalue(L, -1);
        if(client->idx_request)
            luaL_unref(L, LUA_REGISTRYINDEX, client->idx_request);
        client->idx_request = luaL_ref(L, LUA_REGISTRYINDEX);

        lua_pushstring(L, asc_socket_addr(client->sock));
        lua_setfield(L, request, "addr");
        lua_pushinteger(L, asc_socket_port(client->sock));
        lua_setfield(L, request, "port");

        lua_pushlstring(L, &client->buffer[m[1].so], m[1].eo - m[1].so);
        const char *method = lua_tostring(L, -1);
        lua_setfield(L, request, __method);

        client->is_head = (strcmp(method, "HEAD") == 0);

        lua_pushlstring(L, &client->buffer[m[2].so], m[2].eo - m[2].so);
        lua_setfield(L, request, "request_uri");

        size_t path_skip = m[2].so;
        if(client->buffer[path_skip] != '/' && client->buffer[path_skip] != '*')
        {
            while(path_skip < m[2].eo && client->buffer[path_skip] != ':')
                ++path_skip;
            if(client->buffer[path_skip + 1] != '/' || client->buffer[path_skip + 2] != '/')
            {
                asc_log_error(MSG("failed to parse request URI"));
                lua_pop(L, 1); // request
                on_client_close(client);
                return;
            }
            path_skip += 3;

            skip = path_skip;
            while(path_skip < m[2].eo && client->buffer[path_skip] != '/')
                ++path_skip;

            uri_host = &client->buffer[skip];
            uri_host_size = path_skip - skip;
        }

        skip = path_skip;
        while(path_skip < m[2].eo && client->buffer[path_skip] != '?')
            ++path_skip;

        const bool is_safe = lua_safe_path(L, &client->buffer[skip], path_skip - skip);
        const char *path = lua_tostring(L, -1);
        lua_setfield(L, request, __path);

        if(!is_safe)
        {
            lua_pop(L, 1); // request
            http_client_redirect(client, 302, path);
            return;
        }

        if(path_skip < m[2].eo)
        {
            ++path_skip; // skip '?'
            if(!lua_parse_query(L, &client->buffer[path_skip], m[2].eo - path_skip))
            {
                asc_log_error(MSG("failed to parse query line"));
                lua_pop(L, 2); // query + request
                on_client_close(client);
                return;
            }
            lua_setfield(L, request, __query);
        }

        lua_pushlstring(L, &client->buffer[m[3].so], m[3].eo - m[3].so);
        lua_setfield(L, request, __version);

        skip = m[0].eo;

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
        lua_setfield(L, request, __headers);
        const int headers = lua_gettop(L);

        while(skip < eoh)
        {
            if(!http_parse_header(&client->buffer[skip], eoh - skip, m))
            {
                asc_log_error(MSG("failed to parse request headers"));
                on_client_close(client);
                return;
            }

            if(m[1].eo == 0)
            { /* empty line */
                skip += m[0].eo;
                client->status = 2;
                break;
            }

            lua_string_to_lower(L, &client->buffer[skip], m[1].eo);
            lua_pushlstring(L, &client->buffer[skip + m[2].so], m[2].eo - m[2].so);
            lua_settable(L, headers);

            skip += m[0].eo;
        }

        if(uri_host)
        {
            lua_pushlstring(L, uri_host, uri_host_size);
            lua_setfield(L, headers, "host");
        }

        lua_getfield(L, headers, "content-length");
        if(lua_isnumber(L, -1))
        {
            client->chunk_left = lua_tointeger(L, -1);
            if(client->chunk_left > 0)
            {
                if(client->content)
                    string_buffer_free(client->content);
                client->content = string_buffer_alloc();
                client->is_content_length = true;
            }
        }
        lua_pop(L, 1); // content-length

        lua_pop(L, 2); // headers + request

        client->idx_callback = 0;
        asc_list_for(mod->routes)
        {
            route_t *route = (route_t *)asc_list_data(mod->routes);
            if(routecmp(path, route->path))
            {
                client->idx_callback = route->idx_callback;
                break;
            }
        }

        if(!client->idx_callback)
        {
            http_client_warning(client, "route not found %s", path);
            http_client_abort(client, 404, NULL);
            return;
        }

        if(!client->content)
        {
            client->status = 3;
            callback(L, client);
            return;
        }

        if(skip >= client->buffer_skip)
        {
            client->buffer_skip = 0;
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

    // Content-Length: *
    if(client->is_content_length)
    {
        const size_t tail = client->buffer_skip - skip;

        if(client->chunk_left > tail)
        {
            string_buffer_addlstring(client->content,
                &client->buffer[skip], client->buffer_skip - skip);
            client->chunk_left -= tail;
        }
        else
        {
            string_buffer_addlstring(client->content,
                &client->buffer[skip], client->chunk_left);
            client->chunk_left = 0;

            lua_rawgeti(L, LUA_REGISTRYINDEX, client->idx_request);
            string_buffer_push(L, client->content);
            client->content = NULL;
            lua_setfield(L, -2, __content);
            lua_pop(L, 1); // request

            client->status = 3;
            callback(L, client);
        }

        client->buffer_skip = 0;
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
    http_client_t *const client = (http_client_t *)arg;
    module_data_t *const mod = client->mod;
    lua_State *const L = MODULE_L(mod);

    lua_rawgeti(L, LUA_REGISTRYINDEX, client->idx_content);
    const char *content = lua_tostring(L, -1);
    if(client->chunk_left == 0)
    {
        client->buffer_skip = 0;
        client->chunk_left = luaL_len(L, -1);
    }
    lua_pop(L, 1); // content

    const size_t content_send = (client->chunk_left > HTTP_BUFFER_SIZE)
                              ? HTTP_BUFFER_SIZE
                              : client->chunk_left;

    const ssize_t send_size = asc_socket_send(  client->sock
                                              , (void *)&content[client->buffer_skip]
                                              , content_send);
    if(send_size == -1)
    {
        asc_log_error(MSG("failed to send content: %s"), asc_error_msg());
        on_client_close(client);
        return;
    }
    client->buffer_skip += send_size;
    client->chunk_left -= send_size;

    if(client->chunk_left == 0)
        on_client_close(client);
}

/* Stack: 1 - server, 2 - client, 3 - response */
static int method_send(lua_State *L, module_data_t *mod)
{
    asc_assert(lua_islightuserdata(L, 2), MSG(":send() client instance required"));
    http_client_t *client = (http_client_t *)lua_touserdata(L, 2);

    if(client->on_send)
    {
        client->on_send(client);
        return 0;
    }

    if(client->response)
    {
        asc_log_error(MSG(":send() failed, instance is busy"));
        return 0;
    }

    const int idx_response = 3;

    lua_getfield(L, idx_response, __code);
    const int code = lua_tointeger(L, -1);
    lua_pop(L, 1); // code

    lua_getfield(L, idx_response, __message);
    const char *message = lua_isstring(L, -1) ? lua_tostring(L, -1) : NULL;
    lua_pop(L, 1); // message

    http_response_code(client, code, message);

    lua_getfield(L, idx_response, __content);
    if(lua_isstring(L, -1))
    {
        const int content_length = luaL_len(L, -1);
        http_response_header(client, "%s%d", __content_length, content_length);

        if(client->idx_content)
            luaL_unref(L, LUA_REGISTRYINDEX, client->idx_content);
        client->idx_content = luaL_ref(L, LUA_REGISTRYINDEX);

        client->on_send = NULL;
        client->on_read = NULL;
        client->on_ready = on_ready_send_content;
    }
    else
        lua_pop(L, 1); // content

    lua_getfield(L, idx_response, __headers);
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

    return 0;
}

/*
 *      o      oooooooooo   ooooo
 *     888      888    888   888
 *    8  88     888oooo88    888
 *   8oooo88    888          888
 * o88o  o888o o888o        o888o
 *
 */

static void on_ready_send_response(void *arg)
{
    http_client_t *client = (http_client_t *)arg;
    module_data_t *mod = client->mod;

    const size_t content_send = (client->chunk_left > HTTP_BUFFER_SIZE)
                              ? HTTP_BUFFER_SIZE
                              : client->chunk_left;

    const ssize_t send_size = asc_socket_send(  client->sock
                                              , &client->buffer[client->buffer_skip]
                                              , content_send);
    if(send_size == -1)
    {
        asc_log_error(MSG("failed to send response: %s"), asc_error_msg());
        on_client_close(client);
        return;
    }
    client->buffer_skip += send_size;
    client->chunk_left -= send_size;

    if(client->chunk_left == 0)
    {
        if(   (client->idx_content || client->response)
           && (client->is_head == false))
        {
            client->buffer_skip = 0;

            asc_socket_set_on_read(client->sock, client->on_read);
            asc_socket_set_on_ready(client->sock, client->on_ready);
            return;
        }

        on_client_close(client);
    }
}

static const char * http_code(int code)
{
    switch(code)
    {
        case 200: return "Ok";

        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";

        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 429: return "Too Many Requests";

        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";

        default:  return "Status Code Undefined";
    }
}

void http_response_code(http_client_t *client, int code, const char *message)
{
    if(!message)
        message = http_code(code);

    client->chunk_left  = snprintf(client->buffer, HTTP_BUFFER_SIZE
                                   , "%s %d %s\r\n"
                                   , client->mod->http_version, code, message);

    client->chunk_left += snprintf(&client->buffer[client->chunk_left]
                                   , HTTP_BUFFER_SIZE - client->chunk_left
                                   , "Server: %s\r\n"
                                   , client->mod->server_name);
}

void http_response_header(http_client_t *client, const char *header, ...)
{
    va_list ap;
    va_start(ap, header);

    client->chunk_left += vsnprintf(&client->buffer[client->chunk_left]
                                    , HTTP_BUFFER_SIZE - client->chunk_left
                                    , header, ap);
    client->buffer[client->chunk_left + 0] = '\r';
    client->buffer[client->chunk_left + 1] = '\n';
    client->chunk_left += 2;

    va_end(ap);
}

void http_response_send(http_client_t *client)
{
    client->buffer[client->chunk_left + 0] = '\r';
    client->buffer[client->chunk_left + 1] = '\n';
    client->chunk_left += 2;

    client->buffer_skip = 0;

    asc_socket_set_on_read(client->sock, NULL);
    asc_socket_set_on_ready(client->sock, on_ready_send_response);
}

void http_client_warning(http_client_t *client, const char *message, ...)
{
    module_data_t *mod = client->mod;

    va_list ap;
    va_start(ap, message);

    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), message, ap);
    asc_log_warning(MSG("%s"), buffer);

    va_end(ap);
}

void http_client_error(http_client_t *client, const char *message, ...)
{
    module_data_t *mod = client->mod;

    va_list ap;
    va_start(ap, message);

    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), message, ap);
    asc_log_error(MSG("%s"), buffer);

    va_end(ap);
}

void http_client_close(http_client_t *client)
{
    on_client_close(client);
}

void http_client_abort(http_client_t *client, int code, const char *text)
{
    module_data_t *const mod = client->mod;
    lua_State *const L = MODULE_L(mod);

    const char *message = http_code(code);

    static const char abort_template[] =
        "<html><head>"
            "<title>%d %s</title>"
        "</head><body>"
            "<h1>%s</h1>"
            "<p>%s</p>"
            "<hr />"
            "<i>%s</i>"
        "</body></html>\r\n";

    lua_pushfstring(  L
                    , abort_template
                    , code, message
                    , message, (text) ? text : "&nbsp;"
                    , mod->server_name);

    const int content_length = luaL_len(L, -1);

    if(client->idx_content)
        luaL_unref(L, LUA_REGISTRYINDEX, client->idx_content);
    client->idx_content = luaL_ref(L, LUA_REGISTRYINDEX);

    client->on_read = NULL;
    client->on_ready = on_ready_send_content;

    http_response_code(client, code, message);
    http_response_header(client, "Content-Type: text/html");
    http_response_header(client, "%s%d", __content_length, content_length);
    http_response_header(client, __connection_close);
    http_response_send(client);
}

void http_client_redirect(http_client_t *client, int code, const char *location)
{
    if(!code)
        code = 302;

    if(client->idx_content)
    {
        lua_State *const L = MODULE_L(client->mod);
        luaL_unref(L, LUA_REGISTRYINDEX, client->idx_content);
        client->idx_content = 0;
    }

    client->on_read = NULL;
    client->on_ready = NULL;

    client->is_head = true; // hack to close connection after response

    http_response_code(client, code, NULL);
    http_response_header(client, "Location: %s", location);
    http_response_header(client, __connection_close);
    http_response_send(client);
}

/*
 *  oooooooo8 ooooooooooo oooooooooo ooooo  oooo ooooooooooo oooooooooo
 * 888         888    88   888    888 888    88   888    88   888    888
 *  888oooooo  888ooo8     888oooo88   888  88    888ooo8     888oooo88
 *         888 888    oo   888  88o     88888     888    oo   888  88o
 * o88oooo888 o888ooo8888 o888o  88o8    888     o888ooo8888 o888o  88o8
 *
 */

static void on_server_close(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;
    lua_State *const L = MODULE_L(mod);

    if(!mod->sock)
        return;

    asc_socket_close(mod->sock);
    mod->sock = NULL;

    if(mod->clients)
    {
        http_client_t *prev_client = NULL;
        for(  asc_list_first(mod->clients)
            ; !asc_list_eol(mod->clients)
            ; asc_list_first(mod->clients))
        {
            http_client_t *client = (http_client_t *)asc_list_data(mod->clients);
            asc_assert(client != prev_client
                       , MSG("loop on on_server_close() client:%p")
                       , (void *)client);
            on_client_close(client);
            prev_client = client;
        }

        asc_list_destroy(mod->clients);
        mod->clients = NULL;
    }

    if(mod->routes)
    {
        for(  asc_list_first(mod->routes)
            ; !asc_list_eol(mod->routes)
            ; asc_list_first(mod->routes))
        {
            route_t *route = (route_t *)asc_list_data(mod->routes);
            luaL_unref(L, LUA_REGISTRYINDEX, route->idx_callback);
            free(route);
            asc_list_remove_current(mod->routes);
        }

        asc_list_destroy(mod->routes);
        mod->routes = NULL;
    }

    if(mod->idx_self)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, mod->idx_self);
        mod->idx_self = 0;
    }
}

static void on_server_accept(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    http_client_t *client = (http_client_t *)calloc(1, sizeof(http_client_t));
    client->mod = mod;
    client->idx_server = mod->idx_self;

    if(!asc_socket_accept(mod->sock, &client->sock, client))
    {
        free(client);
        on_server_close(mod);
        asc_lib_abort(); // TODO: try to restart server
    }

    asc_list_insert_tail(mod->clients, client);

    asc_log_debug(MSG("client connected %s:%d (%zu clients)")
                      , asc_socket_addr(client->sock)
                      , asc_socket_port(client->sock)
                      , asc_list_size(mod->clients));

    asc_socket_set_on_read(client->sock, on_client_read);
    asc_socket_set_on_close(client->sock, on_client_close);
}

/*
 * oooo     oooo  ooooooo  ooooooooo  ooooo  oooo ooooo       ooooooooooo
 *  8888o   888 o888   888o 888    88o 888    88   888         888    88
 *  88 888o8 88 888     888 888    888 888    88   888         888ooo8
 *  88  888  88 888o   o888 888    888 888    88   888      o  888    oo
 * o88o  8  o88o  88ooo88  o888ooo88    888oo88   o888ooooo88 o888ooo8888
 *
 */

static int method_data(lua_State *L, module_data_t *mod)
{
    asc_assert(lua_islightuserdata(L, 2), MSG(":data() client instance required"));
    http_client_t *client = (http_client_t *)lua_touserdata(L, 2);

    if(!client->idx_data)
    {
        lua_newtable(L);
        client->idx_data = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, client->idx_data);
    return 1;
}

static int method_close(lua_State *L, module_data_t *mod)
{
    if(lua_gettop(L) == 1)
    {
        on_server_close(mod);
    }
    else
    {
        asc_assert(lua_islightuserdata(L, 2), MSG(":close() client instance required"));
        http_client_t *client = (http_client_t *)lua_touserdata(L, 2);
        on_client_close(client);
    }

    return 0;
}

static int method_redirect(lua_State *L, module_data_t *mod)
{
    asc_assert(lua_islightuserdata(L, 2), MSG(":redirect() client instance required"));
    asc_assert(lua_isstring(L, 3), MSG(":redirect() location required"));
    http_client_t *client = (http_client_t *)lua_touserdata(L, 2);
    const char *location = lua_tostring(L, 3);
    http_client_redirect(client, 302, location);
    return 0;
}

static int method_abort(lua_State *L, module_data_t *mod)
{
    asc_assert(lua_islightuserdata(L, 2), MSG(":abort() client instance required"));
    asc_assert(lua_isnumber(L, 3), MSG(":abort() code required"));
    http_client_t *client = (http_client_t *)lua_touserdata(L, 2);
    const int code = lua_tointeger(L, 3);
    const char *text = lua_isstring(L, 4) ? lua_tostring(L, 4) : NULL;
    http_client_abort(client, code, text);
    return 0;
}

static bool lua_is_call(lua_State *L, int idx)
{
    bool is_call = false;

    if(lua_isfunction(L, idx))
        is_call = true;
    else if(lua_istable(L, idx))
    {
        lua_getmetatable(L, idx);
        lua_getfield(L, -1, "__call");
        is_call = lua_isfunction(L, -1);
        lua_pop(L, 2);
    }

    return is_call;
}

static void module_init(lua_State *L, module_data_t *mod)
{
    module_option_string(L, "addr", &mod->addr, NULL);
    if(!mod->addr || !mod->addr[0])
        mod->addr = "0.0.0.0";

    mod->port = 80;
    module_option_integer(L, "port", &mod->port);

    mod->server_name = PACKAGE_NAME;
    module_option_string(L, "server_name", &mod->server_name, NULL);

    mod->http_version = "HTTP/1.1";
    module_option_string(L, "http_version", &mod->http_version, NULL);

    // store routes in registry
    mod->routes = asc_list_init();
    lua_getfield(L, MODULE_OPTIONS_IDX, "route");
    asc_assert(lua_istable(L, -1), MSG("option 'route' is required"));
    for(lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
    {
        bool is_ok = false;
        do
        {
            const int item = lua_gettop(L);
            if(!lua_istable(L, item))
                break;

            lua_rawgeti(L, item, 1); // path
            if(!lua_isstring(L, -1))
                break;

            lua_rawgeti(L, item, 2); // callback
            if(!lua_is_call(L, -1))
                break;

            is_ok = true;
        } while(0);
        asc_assert(is_ok, MSG("route format: { { \"/path\", callback }, ... }"));

        route_t *route = (route_t *)malloc(sizeof(route_t));
        route->idx_callback = luaL_ref(L, LUA_REGISTRYINDEX);
        route->path = lua_tostring(L, -1);
        lua_pop(L, 1); // path

        asc_list_insert_tail(mod->routes, route);
    }
    lua_pop(L, 1); // route

    // store self in registry
    lua_pushvalue(L, 3);
    mod->idx_self = luaL_ref(L, LUA_REGISTRYINDEX);

    mod->clients = asc_list_init();

    bool sctp = false;
    module_option_boolean(L, "sctp", &sctp);
    if(sctp == true)
        mod->sock = asc_socket_open_sctp4(mod);
    else
        mod->sock = asc_socket_open_tcp4(mod);

    asc_socket_set_reuseaddr(mod->sock, 1);
    if(!asc_socket_bind(mod->sock, mod->addr, mod->port))
    {
        on_server_close(mod);
        asc_lib_abort(); // TODO: try to restart server
    }
    asc_socket_listen(mod->sock, on_server_accept, on_server_close);
}

static void module_destroy(module_data_t *mod)
{
    if(mod->idx_self == 0)
        return;

    on_server_close(mod);
}

MODULE_LUA_METHODS()
{
    { "send", method_send },
    { "close", method_close },
    { "data", method_data },
    { "redirect", method_redirect },
    { "abort", method_abort },
};
MODULE_LUA_REGISTER(http_server)
