/*
 * Astra Module: Constant bitrate muxer
 *
 * Copyright (C) 2017, Artem Kharitonov <artem@3phase.pw>
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
 *      ts_cbr
 *
 * Module Role:
 *      Input or output stage, forwards pid requests
 *
 * Module Options:
 *      upstream    - object, stream module instance
 *      name        - string, instance identifier for logging
 *      bitrate     - number, target bitrate in bits per second
 *      pcr_interval
 *                  - number, maximum PCR interval to enforce in milliseconds
 *                      default is 35 ms for DVB compliance
 *      buffer_size - number, buffer size in milliseconds at target bitrate
 *                      default is 150 ms (e.g. ~187 KiB if rate is 10 Mbit)
 */

#include <astra/astra.h>
#include <astra/luaapi/stream.h>
#include <astra/mpegts/pcr.h>
#include <astra/mpegts/psi.h>

#define MSG(_msg) "[cbr %s] " _msg, mod->name

struct module_data_t
{
    STREAM_MODULE_DATA();
};

static
void on_ts(module_data_t *mod, const uint8_t *ts)
{
    ASC_UNUSED(mod);
    ASC_UNUSED(ts);
}

static
void module_init(lua_State *L, module_data_t *mod)
{
    module_stream_init(L, mod, on_ts);
}

static
void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);
}

STREAM_MODULE_REGISTER(ts_cbr)
{
    .init = module_init,
    .destroy = module_destroy,
};
