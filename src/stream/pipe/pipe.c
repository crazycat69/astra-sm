/*
 * Astra Module: Pipe
 *
 * Copyright (C) 2015, Artem Kharitonov <artem@sysert.ru>
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

#include "pipe.h"

/* initialization routines common to all pipe modules */
void pipe_init(module_data_t *mod)
{
    /* channel name */
    const char *name = NULL;
    module_option_string("name", &name, NULL);
    if (name == NULL || !strlen(name))
        luaL_error(lua, "[%s] name is required", mod->prefix);

    snprintf(mod->name, sizeof(mod->name), "%s %s", mod->prefix, name);
    mod->config.name = mod->name;

    /* command to run */
    const char *command = NULL;
    module_option_string("command", &command, NULL);
    if (command == NULL || !strlen(command))
        luaL_error(lua, MSG("command line is required"));

    mod->config.command = command;

    /* restart delay */
    int delay = 5;
    module_option_number("restart", &delay);
    if (delay < 0 || delay > 86400)
        luaL_error(lua, MSG("restart delay out of range"));

    mod->delay = delay * 1000;

    /* callbacks and arguments */
    mod->config.on_close = pipe_on_close;
    mod->config.on_ready = pipe_on_ready;
    mod->config.arg = mod;

    module_stream_init(mod, pipe_upstream_ts);
    pipe_on_retry(mod);
}

void pipe_destroy(module_data_t *mod)
{
    ASC_FREE(mod->restart, asc_timer_destroy);
    ASC_FREE(mod->child, asc_child_destroy);

    module_stream_destroy(mod);
}

/* child ready to receive data */
void pipe_on_ready(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    if (mod->dropped)
    {
        asc_log_error(MSG("dropped %zu packets while waiting for child")
                      , mod->dropped);

        mod->dropped = 0;
    }

    mod->can_send = true;
    asc_child_set_on_ready(mod->child, NULL);
}

/* incoming TS packet from upstream module */
void pipe_upstream_ts(module_data_t *mod, const uint8_t *ts)
{
    if (!mod->can_send)
    {
        mod->dropped++;
        return;
    }

    // TODO: asc_child_write()
    __uarg(ts);
}

/* incoming TS packets from child */
void pipe_child_ts(void *arg, const uint8_t *ts, size_t len)
{
    __uarg(len);

    module_data_t *const mod = (module_data_t *)arg;
    module_stream_send(mod, ts);
    // TODO: send (len / 188) packets
    //       increment ts accordingly
}

/* incoming text line from child */
void pipe_child_text(void *arg, const uint8_t *text, size_t len)
{
    __uarg(len);

    module_data_t *const mod = (module_data_t *)arg;
    asc_log_warning(MSG("%s"), text);
}

/* post-termination callback */
void pipe_on_close(void *arg, int exit_code)
{
    module_data_t *const mod = (module_data_t *)arg;

    // TEST cases:
    // - status OK, no restart
    // - status OK, restart in X secs
    // - kill fail, no restart
    // - kill fail, restart in X secs

    char buf[64] = "restart disabled";
    if (mod->delay > 0)
    {
        snprintf(buf, sizeof(buf), "restarting in %u seconds"
                 , mod->delay / 1000);

        mod->restart = asc_timer_one_shot(mod->delay, pipe_on_retry, mod);
    }

    if (exit_code == -1)
        asc_log_error(MSG("failed to terminate process; %s"), buf);
    else if (exit_code == 0)
        asc_log_info(MSG("process exited successfully; %s"), buf);
    else
        asc_log_error(MSG("process exited with code %d; %s"), exit_code, buf);

    mod->can_send = false;
    mod->child = NULL;
}

/* restart timer callback */
void pipe_on_retry(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;

    if (mod->restart != NULL)
    {
        asc_log_debug(MSG("attempting restart..."));
        mod->restart = NULL;
    }

    mod->child = asc_child_init(&mod->config);
    if (mod->child == NULL)
    {
        asc_log_error(MSG("failed to create process: %s")
                      , asc_error_msg());

        if (mod->delay > 0)
        {
            asc_log_info(MSG("retry in %u seconds"), mod->delay / 1000);
            mod->restart = asc_timer_one_shot(mod->delay, pipe_on_retry, mod);
        }
        else
            asc_log_info(MSG("auto restart disabled, giving up"));

        return;
    }

    asc_log_info(MSG("process started (pid = %lld)")
                 , (long long)asc_child_pid(mod->child));
}
