/*
 * Astra Module: Pipe (Transcode)
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

static void module_init(module_data_t *mod)
{
    mod->prefix = "pipe_transcode";

    /* pump TS through child's stdin and stdout */
    mod->config.sin.mode = CHILD_IO_MPEGTS;
    mod->config.sout.mode = CHILD_IO_MPEGTS;
    mod->config.sout.on_flush = pipe_child_ts;

    /* receive text lines from its stderr */
    mod->config.serr.mode = CHILD_IO_TEXT;
    mod->config.serr.on_flush = pipe_child_text;

    pipe_init(mod);
}

static void module_destroy(module_data_t *mod)
{
    pipe_destroy(mod);
}

MODULE_STREAM_METHODS()
MODULE_LUA_METHODS()
{
    MODULE_STREAM_METHODS_REF()
};
MODULE_LUA_REGISTER(pipe_transcode)
