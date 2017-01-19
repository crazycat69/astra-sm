/*
 * Astra Module: DigitalDevices standalone CI
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
 *      ddci
 *
 * Module Role:
 *      Input stage, forwards pid requests
 */

#include "dvb.h"
#include <astra/core/mainloop.h>
#include <astra/core/thread.h>

#define MSG(_msg) "[ddci %d:%d] " _msg, mod->adapter, mod->frontend

#define BUFFER_SIZE (1022 * TS_PACKET_SIZE)

struct module_data_t
{
    STREAM_MODULE_DATA();

    int adapter;
    int frontend;

    /* Base */
    char dev_name[32];
    // bool is_thread;

    /* */
    dvb_ca_t *ca;

    /* */
    int enc_sec_fd;

    /* */
    int dec_sec_fd;

    asc_thread_t *sec_thread;
    asc_thread_buffer_t *sec_thread_output;

    bool is_ca_thread_started;
    asc_thread_t *ca_thread;
};

#define THREAD_DELAY_CA (1 * 1000 * 1000)

/*
 *  oooooooo8 ooooooooooo  oooooooo8
 * 888         888    88 o888     88
 *  888oooooo  888ooo8   888
 *         888 888    oo 888o     oo
 * o88oooo888 o888ooo8888 888oooo88
 *
 */

static void on_thread_close(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    if(mod->dec_sec_fd > 0)
    {
        close(mod->dec_sec_fd);
        mod->dec_sec_fd = 0;
    }

    if(mod->sec_thread)
    {
        ASC_FREE(mod->sec_thread, asc_thread_join);
        asc_wake_close();
    }

    if(mod->sec_thread_output)
    {
        asc_job_prune(mod->sec_thread_output);
        ASC_FREE(mod->sec_thread_output, asc_thread_buffer_destroy);
    }
}

static void on_thread_read(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    uint8_t ts[TS_PACKET_SIZE];
    while (true)
    {
        const ssize_t r = asc_thread_buffer_read(mod->sec_thread_output, ts, sizeof(ts));
        if (r != sizeof(ts))
            return;

        module_stream_send(mod, ts);
    }
}

static void thread_loop(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;
    uint8_t ts[TS_PACKET_SIZE];
    uint64_t system_time, system_time_buffer = 0;

    mod->dec_sec_fd = open(mod->dev_name, O_RDONLY);

    while(1)
    {
        const ssize_t len = read(mod->dec_sec_fd, ts, sizeof(ts));
        if(len == -1)
            break;

        if(len == sizeof(ts) && ts[0] == 0x47)
        {
            const ssize_t r = asc_thread_buffer_write(mod->sec_thread_output, ts, sizeof(ts));
            if(r != TS_PACKET_SIZE)
            {
                // overflow
            }
            else
            {
                /*
                 * TODO: add proper buffering with sync byte alignment checks
                 */
                system_time = asc_utime();
                if (system_time > system_time_buffer + 5000)
                {
                    system_time_buffer = system_time;
                    asc_job_queue(mod->sec_thread_output, on_thread_read, mod);
                    asc_wake();
                }
            }
        }
    }
}

static void sec_open(module_data_t *mod)
{
    mod->enc_sec_fd = open(mod->dev_name, O_WRONLY | O_NONBLOCK);
    if(mod->enc_sec_fd <= 0)
    {
        asc_log_error(MSG("failed to open sec [%s]"), strerror(errno));
        asc_lib_abort();
    }

    mod->sec_thread = asc_thread_init();
    mod->sec_thread_output = asc_thread_buffer_init(BUFFER_SIZE);

    asc_wake_open();
    asc_thread_start(mod->sec_thread, mod, thread_loop, on_thread_close);
}

static void sec_close(module_data_t *mod)
{
    if(mod->enc_sec_fd > 0)
    {
        close(mod->enc_sec_fd);
        mod->enc_sec_fd = 0;
    }

    if(mod->sec_thread)
        on_thread_close(mod);
}

/*
 * ooooooooooo ooooo ooooo oooooooooo  ooooooooooo      o      ooooooooo
 * 88  888  88  888   888   888    888  888    88      888      888    88o
 *     888      888ooo888   888oooo88   888ooo8       8  88     888    888
 *     888      888   888   888  88o    888    oo    8oooo88    888    888
 *    o888o    o888o o888o o888o  88o8 o888ooo8888 o88o  o888o o888ooo88
 *
 */

static void on_ca_thread_close(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    mod->is_ca_thread_started = false;
    if(mod->ca_thread)
        ASC_FREE(mod->ca_thread, asc_thread_join);
}

static void ca_thread_loop(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    ca_open(mod->ca);

    nfds_t nfds = 0;

    struct pollfd fds[1];
    memset(fds, 0, sizeof(fds));

    fds[nfds].fd = mod->ca->ca_fd;
    fds[nfds].events = POLLIN;
    ++nfds;

    mod->is_ca_thread_started = true;

    uint64_t current_time = asc_utime();
    uint64_t ca_check_timeout = current_time;

    while(mod->is_ca_thread_started)
    {
        const int ret = poll(fds, nfds, 100);

        if(ret < 0)
        {
            asc_log_error(MSG("poll() failed [%s]"), strerror(errno));
            asc_lib_abort();
        }

        if(ret > 0)
        {
            if(fds[0].revents)
                ca_loop(mod->ca, fds[0].revents & (POLLPRI | POLLIN));
        }

        current_time = asc_utime();

        if(current_time >= ca_check_timeout + THREAD_DELAY_CA)
        {
            ca_check_timeout = current_time;
            ca_loop(mod->ca, 0);
        }
    }

    ca_close(mod->ca);
}

/*
 * oooo     oooo  ooooooo  ooooooooo  ooooo  oooo ooooo       ooooooooooo
 *  8888o   888 o888   888o 888    88o 888    88   888         888    88
 *  88 888o8 88 888     888 888    888 888    88   888         888ooo8
 *  88  888  88 888o   o888 888    888 888    88   888      o  888    oo
 * o88o  8  o88o  88ooo88  o888ooo88    888oo88   o888ooooo88 o888ooo8888
 *
 */

static void on_ts(module_data_t *mod, const uint8_t *ts)
{
    if(mod->ca->ca_fd > 0)
        ca_on_ts(mod->ca, ts);

    if(write(mod->enc_sec_fd, ts, TS_PACKET_SIZE) != TS_PACKET_SIZE)
        asc_log_error(MSG("sec write failed"));
}

static int method_ca_set_pnr(lua_State *L, module_data_t *mod)
{
    if(!mod->ca || !mod->ca->ca_fd)
        return 0;

    const uint16_t pnr = lua_tointeger(L, 2);
    const bool is_set = lua_toboolean(L, 3);
    ((is_set) ? ca_append_pnr : ca_remove_pnr)(mod->ca, pnr);
    return 0;
}

static void module_init(lua_State *L, module_data_t *mod)
{
    module_stream_init(L, mod, on_ts);

    mod->ca = ASC_ALLOC(1, dvb_ca_t);

    static const char __adapter[] = "adapter";
    if(!module_option_integer(L, __adapter, &mod->adapter))
    {
        asc_log_error(MSG("option '%s' is required"), __adapter);
        asc_lib_abort();
    }
    module_option_integer(L, "frontend", &mod->frontend);
    mod->ca->adapter = mod->adapter;
    mod->ca->frontend = mod->frontend;
    const size_t path_size = sprintf(mod->dev_name, "/dev/dvb/adapter%d/", mod->adapter);

    while(1)
    {
        // check ci%d
        sprintf(&mod->dev_name[path_size], "ci%d", mod->frontend);
        if(!access(mod->dev_name, W_OK))
            break;

        // check sec%d
        sprintf(&mod->dev_name[path_size], "sec%d", mod->frontend);
        if(!access(mod->dev_name, W_OK))
            break;

        asc_log_error(MSG("ci-device is not found"));
        asc_lib_abort();
    }

    mod->ca_thread = asc_thread_init();
    asc_thread_start(mod->ca_thread, mod, ca_thread_loop, on_ca_thread_close);

    sec_open(mod);

    while(!mod->is_ca_thread_started)
        asc_usleep(500);
}

static void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);

    sec_close(mod);

    if(mod->ca_thread)
        on_ca_thread_close(mod);

    free(mod->ca);
}

static const module_method_t module_methods[] =
{
    { "ca_set_pnr", method_ca_set_pnr },
    { NULL, NULL },
};

STREAM_MODULE_REGISTER(ddci)
{
    .init = module_init,
    .destroy = module_destroy,
    .methods = module_methods,
};
