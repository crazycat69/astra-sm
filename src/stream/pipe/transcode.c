/*
 * Astra Module: Pipe (Output)
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

#include <astra.h>

struct module_data_t
{
    MODULE_STREAM_DATA();
};

static void on_ts(module_data_t *mod, const uint8_t *ts)
{
    __uarg(mod);
    __uarg(ts);
}

static void module_init(module_data_t *mod)
{
    module_stream_init(mod, on_ts);
}

static void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);
}

MODULE_STREAM_METHODS()
MODULE_LUA_METHODS()
{
    MODULE_STREAM_METHODS_REF()
};
MODULE_LUA_REGISTER(pipe_transcode)
