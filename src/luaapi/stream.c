/*
 * Astra Lua API (Stream Module)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
 *               2015-2016, Artem Kharitonov <artem@3phase.pw>
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

struct module_data_t
{
    /*
     * NOTE: data structs in all stream modules MUST begin with the
     *       following two members. Use STREAM_MODULE_DATA() macro when
     *       defining stream module structs as the exact definition
     *       might change in the future.
     */
    lua_State *lua;
    module_stream_t stream;
};

/*
 * init and cleanup
 */

void module_stream_init(lua_State *L, module_data_t *mod
                        , stream_callback_t on_ts)
{
    mod->stream.self = mod;
    mod->stream.on_ts = on_ts;
    mod->stream.children = asc_list_init();

    if (on_ts != NULL)
    {
        lua_getfield(L, MODULE_OPTIONS_IDX, "upstream");
        if (lua_type(L, -1) == LUA_TLIGHTUSERDATA)
        {
            module_stream_t *const parent_st =
                (module_stream_t *)lua_touserdata(L, -1);

            module_stream_attach(parent_st->self, mod);
        }
        lua_pop(L, 1);
    }
}

void module_stream_destroy(module_data_t *mod)
{
    module_stream_t *const st = &mod->stream;
    if (st->self == NULL)
        /* not initialized */
        return;

    /* leave all joined pids */
    if (st->pid_list != NULL)
    {
        for (unsigned int i = 0; i < MAX_PID; i++)
        {
            if (st->pid_list[i] > 0)
                module_demux_leave(mod, i);
        }

        ASC_FREE(st->pid_list, free);
    }

    /* detach from upstream */
    module_stream_attach(NULL, mod);

    /* detach children */
    asc_list_clear(st->children)
    {
        module_stream_t *const i =
            (module_stream_t *)asc_list_data(st->children);

        i->parent = NULL;
    }

    ASC_FREE(st->children, asc_list_destroy);

    /* reset state */
    memset(st, 0, sizeof(*st));
}

/*
 * streaming module tree
 */

static
int method_stream(lua_State *L, module_data_t *mod)
{
    lua_pushlightuserdata(L, &mod->stream);
    return 1;
}

const module_method_t module_stream_methods[] =
{
    { "stream", method_stream },
    { NULL, NULL },
};

void module_stream_attach(module_data_t *mod, module_data_t *child)
{
    module_stream_t *const cs = &child->stream;

    if (cs->parent != NULL)
    {
        asc_list_remove_item(cs->parent->children, cs);
        cs->parent = NULL;
    }

    if (mod != NULL)
    {
        module_stream_t *const ps = &mod->stream;
        asc_assert(ps->self != NULL, "attaching to uninitialized module");

        cs->parent = ps;
        asc_list_insert_tail(ps->children, cs);
    }
}

void module_stream_send(void *arg, const uint8_t *ts)
{
    module_data_t *const mod = (module_data_t *)arg;
    module_stream_t *const stream = &mod->stream;

    asc_list_for(stream->children)
    {
        module_stream_t *const i =
            (module_stream_t *)asc_list_data(stream->children);

        if (i->on_ts != NULL)
            i->on_ts(i->self, ts);
    }
}

/*
 * pid membership
 */

void module_demux_set(module_data_t *mod, demux_callback_t join_pid
                      , demux_callback_t leave_pid)
{
    module_stream_t *const st = &mod->stream;
    if (st->pid_list == NULL)
        st->pid_list = ASC_ALLOC(MAX_PID, uint8_t);

    st->join_pid = join_pid;
    st->leave_pid = leave_pid;
}

void module_demux_join(module_data_t *mod, uint16_t pid)
{
    module_stream_t *const st = &mod->stream;
    asc_assert(st->pid_list != NULL, "module_demux_set() is required");

    ++st->pid_list[pid];
    if (st->pid_list[pid] == 1 && st->parent != NULL
        && st->parent->join_pid != NULL)
    {
        st->parent->join_pid(st->parent->self, pid);
    }
}

void module_demux_leave(module_data_t *mod, uint16_t pid)
{
    module_stream_t *const st = &mod->stream;
    asc_assert(st->pid_list != NULL, "module_demux_set() is required");

    if (st->pid_list[pid] > 0)
    {
        --st->pid_list[pid];
        if (st->pid_list[pid] == 0 && st->parent != NULL
            && st->parent->leave_pid != NULL)
        {
            st->parent->leave_pid(st->parent->self, pid);
        }
    }
    else
    {
        asc_log_error("module_demux_leave() double call pid: %u", pid);
    }
}

bool module_demux_check(const module_data_t *mod, uint16_t pid)
{
    return (mod->stream.pid_list[pid] > 0);
}
