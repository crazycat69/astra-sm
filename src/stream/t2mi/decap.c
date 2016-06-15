/*
 * Astra Module: T2-MI de-encapsulator
 *
 * Copyright (C) 2016, Artem Kharitonov <artem@3phase.pw>
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
 *      t2mi_decap
 *
 * Module Options:
 *      upstream    - object, stream module instance
 *      name        - string, instance identifier for logging
 *      pnr         - number, program containing T2-MI payload
 *      pid         - number, force decapsulator to process this pid
 *      plp         - number, PLP ID (defaults to first one available)
 */

#include <astra.h>
#include <luaapi/stream.h>
#include <mpegts/t2mi.h>

struct module_data_t
{
    STREAM_MODULE_DATA();

    /* module configuration */
    const char *name;
    unsigned int pnr;
    unsigned int pid;
    unsigned int plp;

    /* decapsulator context */
    mpegts_t2mi_t *decap;
};

static void join_pid(module_data_t *mod, uint16_t pid)
{
    module_demux_join(mod, pid);
}

static void leave_pid(module_data_t *mod, uint16_t pid)
{
    module_demux_leave(mod, pid);
}

static void on_ts(module_data_t *mod, const uint8_t *ts)
{
    mpegts_t2mi_decap(mod->decap, ts);
}

static void module_init(lua_State *L, module_data_t *mod)
{
    module_stream_init(mod, on_ts);
    module_demux_set(mod, NULL, NULL);

    /* instance name */
    module_option_string(L, "name", &mod->name, NULL);
    if (mod->name == NULL)
        luaL_error(L, "[t2mi] option 'name' is required");

    /* decap settings */
    mod->plp = T2MI_PLP_AUTO;

    module_option_integer(L, "plp", (int *)&mod->plp);
    module_option_integer(L, "pnr", (int *)&mod->pnr);
    module_option_integer(L, "pid", (int *)&mod->pid);

    /* create decapsulator */
    mod->decap = mpegts_t2mi_init();
    mpegts_t2mi_set_fname(mod->decap, "%s", mod->name);

    mpegts_t2mi_set_demux(mod->decap, mod, join_pid, leave_pid);
    mpegts_t2mi_set_payload(mod->decap, mod->pnr, mod->pid);
    mpegts_t2mi_set_plp(mod->decap, mod->plp);

    mpegts_t2mi_set_callback(mod->decap, __module_stream_send
                             , &mod->__stream);
}

static void module_destroy(module_data_t *mod)
{
    ASC_FREE(mod->decap, mpegts_t2mi_destroy);
    module_stream_destroy(mod);
}

STREAM_MODULE_REGISTER(t2mi_decap)
{
    .init = module_init,
    .destroy = module_destroy,
};
