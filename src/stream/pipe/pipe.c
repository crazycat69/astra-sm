/*
 * Astra Module: Pipe
 *
 * Copyright (C) 2015-2016, Artem Kharitonov <artem@3phase.pw>
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
 *      pipe_generic
 *
 * Module Options:
 *      upstream    - object, stream module instance
 *      name        - string, instance identifier for logging
 *      command     - string, command line
 *      restart     - number, seconds before auto restart (0 to disable)
 *      stream      - boolean, read TS data from child
 *      bypass      - boolean, pass through TS when child is unavailable
 *      sync        - boolean, buffer incoming TS
 *      sync_opts   - string, sync buffer options
 *
 * Module Methods:
 *      pid         - return process' pid (-1 if not running)
 *      send(text)  - send string to child's standard input
 */

#include <astra.h>
#include <core/child.h>
#include <core/timer.h>
#include <luaapi/stream.h>
#include <mpegts/sync.h>

#define MSG(_msg) "[%s] " _msg, mod->config.name

struct module_data_t
{
    MODULE_STREAM_DATA();

    unsigned delay;

    mpegts_sync_t *sync;
    asc_timer_t *sync_loop;
    ssize_t sync_feed;

    bool bypass;
    bool can_send;
    size_t dropped;

    asc_child_cfg_t config;
    asc_child_t *child;

    asc_timer_t *restart;
};

/*
 * process launch and termination
 */

static void on_sync_ready(void *arg);

static
void on_child_restart(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    if (mod->restart != NULL)
    {
        asc_log_debug(MSG("attempting restart..."));
        mod->restart = NULL;
    }

    if (mod->sync != NULL && mod->sync_feed <= 0)
    {
        /* don't read from pipe until sync requests data */
        mod->config.sout.ignore_read = true;
        mpegts_sync_set_on_ready(mod->sync, on_sync_ready);
    }
    else
        mod->config.sout.ignore_read = false;

    mod->child = asc_child_init(&mod->config);
    if (mod->child == NULL)
    {
        asc_log_error(MSG("failed to create process: %s")
                      , asc_error_msg());

        if (mod->delay > 0)
        {
            const unsigned ms = mod->delay * 1000;

            asc_log_info(MSG("retry in %u seconds"), mod->delay);
            mod->restart = asc_timer_one_shot(ms, on_child_restart, mod);
        }
        else
            asc_log_info(MSG("auto restart disabled, giving up"));

        return;
    }

    asc_log_info(MSG("process started (pid = %lld)")
                 , (long long)asc_child_pid(mod->child));
}

static
void on_child_close(void *arg, int exit_code)
{
    module_data_t *const mod = (module_data_t *)arg;

    char buf[64] = "restart disabled";
    if (mod->delay > 0)
    {
        const unsigned ms = mod->delay * 1000;

        snprintf(buf, sizeof(buf), "restarting in %u seconds", mod->delay);
        mod->restart = asc_timer_one_shot(ms, on_child_restart, mod);
    }

    if (exit_code == -1)
        asc_log_error(MSG("failed to terminate process; %s"), buf);
    else if (exit_code == 0)
        asc_log_info(MSG("process exited successfully; %s"), buf);
    else
        asc_log_error(MSG("process exited with code %d; %s"), exit_code, buf);

    if (mod->sync != NULL)
        mpegts_sync_set_on_ready(mod->sync, NULL);

    mod->can_send = false;
    mod->child = NULL;
}

/*
 * reading from pipe
 */

static
void on_sync_ready(void *arg)
{
    module_data_t *const mod = ((module_stream_t *)arg)->self;

    mpegts_sync_set_on_ready(mod->sync, NULL);
    asc_child_toggle_input(mod->child, STDOUT_FILENO, true);

    mpegts_sync_stat_t data;
    mpegts_sync_query(mod->sync, &data);
    mod->sync_feed = data.want;
}

static
void on_child_ts_sync(void *arg, const void *buf, size_t packets)
{
    module_data_t *const mod = (module_data_t *)arg;
    const uint8_t *const ts = (uint8_t *)buf;

    if (!mpegts_sync_push(mod->sync, ts, packets))
    {
        asc_log_error(MSG("sync push failed, resetting buffer"));
        mpegts_sync_reset(mod->sync, SYNC_RESET_ALL);

        return;
    }

    if (mod->sync_feed > 0)
    {
        mod->sync_feed -= packets;
        if (mod->sync_feed <= 0)
        {
            asc_child_toggle_input(mod->child, STDOUT_FILENO, false);
            mpegts_sync_set_on_ready(mod->sync, on_sync_ready);
        }
    }
}

static
void on_child_ts(void *arg, const void *buf, size_t packets)
{
    module_data_t *const mod = (module_data_t *)arg;
    const uint8_t *const ts = (uint8_t *)buf;

    for (size_t i = 0; i < packets; i++)
        module_stream_send(mod, &ts[i * TS_PACKET_SIZE]);
}

static
void on_child_text(void *arg, const void *buf, size_t len)
{
    __uarg(len);

    module_data_t *const mod = (module_data_t *)arg;
    const char *const text = (char *)buf;

    asc_log_warning(MSG("%s"), text);
}

/*
 * writing to pipe
 */

static
void on_child_ready(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    if (mod->dropped)
    {
        asc_log_error(MSG("%s %zu packets while waiting for child")
                      , (mod->bypass ? "bypassed" : "dropped"), mod->dropped);

        mod->dropped = 0;
    }

    mod->can_send = true;
    asc_child_set_on_ready(mod->child, NULL);
}

static
void on_upstream_ts(module_data_t *mod, const uint8_t *ts)
{
    if (!mod->can_send)
    {
        mod->dropped++;
        if (mod->bypass)
            module_stream_send(mod, ts);

        return;
    }

    const ssize_t ret = asc_child_send(mod->child, ts, 1);
    if (ret == -1)
    {
        mod->can_send = false;

        if (asc_socket_would_block())
        {
            asc_child_set_on_ready(mod->child, on_child_ready);
        }
        else
        {
            asc_log_error(MSG("send(): %s"), asc_error_msg());
            asc_child_close(mod->child);
        }
    }
}

/*
 * lua methods
 */

static
int method_pid(lua_State *L, module_data_t *mod)
{
    if (mod->child != NULL)
        lua_pushinteger(L, asc_child_pid(mod->child));
    else
        lua_pushinteger(L, -1);

    return 1;
}

static
int method_send(lua_State *L, module_data_t *mod)
{
    const char *str = luaL_checkstring(L, 1);
    const int len = luaL_len(L, 1);

    if (mod->child == NULL)
        luaL_error(L, MSG("process is not running"));

    if (mod->config.sin.mode == CHILD_IO_MPEGTS)
        luaL_error(L, MSG("can't send text while in TS mode"));

    if (len > 0)
    {
        const ssize_t ret = asc_child_send(mod->child, str, len);
        if (ret == -1)
            luaL_error(L, MSG("send(): %s"), asc_error_msg());
    }

    return 1;
}

/*
 * module init/deinit
 */

static
void module_init(lua_State *L, module_data_t *mod)
{
    /* identifier */
    const char *name = NULL;
    module_option_string(L, "name", &name, NULL);
    if (name == NULL || !strlen(name))
        luaL_error(L, "[pipe] name is required");

    mod->config.name = name;

    /* command line */
    const char *command = NULL;
    module_option_string(L, "command", &command, NULL);
    if (command == NULL || !strlen(command))
        luaL_error(L, MSG("command line is required"));

    mod->config.command = command;

    /* restart delay */
    int delay = 5;
    module_option_integer(L, "restart", &delay);
    if (delay < 0 || delay > 86400)
        luaL_error(L, MSG("restart delay out of range"));

    mod->delay = delay;

    /* write mode */
    stream_callback_t on_ts = NULL;
    mod->config.sin.mode = CHILD_IO_RAW;

    lua_getfield(L, MODULE_OPTIONS_IDX, "upstream");
    if (lua_islightuserdata(L, -1))
    {
        /* output or transcode; relay TS from upstream module */
        mod->config.sin.mode = CHILD_IO_MPEGTS;
        on_ts = on_upstream_ts;
    }
    lua_pop(L, 1);

    /* read mode */
    bool is_stream = false;
    module_option_boolean(L, "stream", &is_stream);
    if (is_stream)
    {
        /* input or transcode; expect TS data */
        mod->config.sout.mode = CHILD_IO_MPEGTS;
        mod->config.sout.on_flush = on_child_ts;
    }
    else
    {
        /* output; treat child's stdout as another stderr */
        mod->config.sout.mode = CHILD_IO_TEXT;
        mod->config.sout.on_flush = on_child_text;
    }

    mod->config.serr.mode = CHILD_IO_TEXT;
    mod->config.serr.on_flush = on_child_text;

    /* transcode mode bypass */
    module_option_boolean(L, "bypass", &mod->bypass);

    /* optional input buffering */
    bool sync_on = false;
    module_option_boolean(L, "sync", &sync_on);

    if (sync_on)
    {
        if (!is_stream)
            luaL_error(L, MSG("buffering is only supported with TS input"));

        mod->sync = mpegts_sync_init();

        mpegts_sync_set_on_write(mod->sync, __module_stream_send);
        mpegts_sync_set_arg(mod->sync, &mod->__stream);
        mpegts_sync_set_fname(mod->sync, "sync/%s", mod->config.name);

        const char *optstr = NULL;
        module_option_string(L, "sync_opts", &optstr, NULL);
        if (optstr != NULL && !mpegts_sync_parse_opts(mod->sync, optstr))
            luaL_error(L, MSG("invalid value for option 'sync_opts'"));

        mpegts_sync_stat_t data;
        mpegts_sync_query(mod->sync, &data);
        mod->sync_feed = data.want;

        mod->sync_loop = asc_timer_init(SYNC_INTERVAL_MSEC, mpegts_sync_loop
                                        , mod->sync);

        mod->config.sout.on_flush = on_child_ts_sync;
    }

    /* callbacks and arguments */
    mod->config.on_close = on_child_close;
    mod->config.on_ready = on_child_ready;
    mod->config.arg = mod;

    module_stream_init(mod, on_ts);
    on_child_restart(mod);
}

static
void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);

    ASC_FREE(mod->restart, asc_timer_destroy);
    ASC_FREE(mod->child, asc_child_destroy);
    ASC_FREE(mod->sync_loop, asc_timer_destroy);
    ASC_FREE(mod->sync, mpegts_sync_destroy);
}

MODULE_STREAM_METHODS()
MODULE_LUA_METHODS()
{
    MODULE_STREAM_METHODS_REF(),
    { "pid", method_pid },
    { "send", method_send },
};
MODULE_LUA_REGISTER(pipe_generic)
