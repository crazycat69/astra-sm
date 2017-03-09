/*
 * Astra Module: Hardware Enumerator (BDA)
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

#include "enum.h"
#include "../bda/bda.h"

/* put error message at the top of the stack and go to cleanup */
#define ENUM_THROW(_msg) \
    do { \
        char *const _err = dshow_error_msg(hr); \
        lua_pushfstring(L, _msg ": %s", _err); \
        free(_err); \
        goto out; \
    } while (0)

/* "throw" if HRESULT indicates failure */
#define ENUM_CKHR(_msg) \
    do { \
        if (FAILED(hr)) \
            ENUM_THROW(_msg); \
    } while (0)

/* "throw" if a NULL pointer is detected */
#define ENUM_CKPTR(_ptr, _msg) \
    do { \
        ASC_WANT_PTR(hr, _ptr); \
        ENUM_CKHR(_msg); \
    } while (0)

/* check for error condition */
#define ENUM_CATCH() \
    lua_isstring(L, -1)

/* check if the tuner supports a particular network type */
static
void probe_tuner(lua_State *L, IBaseFilter *source, const bda_network_t *net)
{
    HRESULT hr = E_FAIL;

    bool pins_connected = false;

    IBaseFilter *provider = NULL;
    IFilterGraph2 *graph = NULL;
    IPin *provider_out = NULL, *source_in = NULL;
    ITuningSpace *space = NULL;
    ITuneRequest *request = NULL;
    ITuner *provider_tuner = NULL;

    /* create network provider */
    hr = bda_net_provider(net, &provider);
    ENUM_CKHR("couldn't create network provider");

    /* create graph and add filters */
    hr = dshow_filter_graph(&graph, NULL, NULL);
    ENUM_CKHR("couldn't create filter graph");

    hr = IFilterGraph2_AddFilter(graph, provider, NULL);
    ENUM_CKHR("couldn't add network provider to graph");

    hr = IFilterGraph2_AddFilter(graph, source, NULL);
    ENUM_CKHR("couldn't add source filter to graph");

    /* try connecting the pins */
    hr = dshow_find_pin(provider, PINDIR_OUTPUT, true, NULL, &provider_out);
    ENUM_CKHR("couldn't find network provider's output pin");

    hr = dshow_find_pin(source, PINDIR_INPUT, true, NULL, &source_in);
    ENUM_CKHR("couldn't find source filter's input pin");

    hr = IFilterGraph2_ConnectDirect(graph, provider_out, source_in, NULL);
    if (SUCCEEDED(hr))
        pins_connected = true;

    /* create empty tune request */
    hr = bda_tuning_space(net, &space);
    ENUM_CKHR("couldn't initialize tuning space");

    hr = ITuningSpace_CreateTuneRequest(space, &request);
    ENUM_CKPTR(request, "couldn't create tune request");

    /* submit request to network provider */
    hr = IBaseFilter_QueryInterface(provider, &IID_ITuner
                                    , (void **)&provider_tuner);
    ENUM_CKPTR(provider_tuner, "couldn't query ITuner interface");

    hr = ITuner_put_TuningSpace(provider_tuner, space);
    ENUM_CKHR("couldn't assign tuning space to provider");

    hr = ITuner_put_TuneRequest(provider_tuner, request);
    ENUM_CKHR("couldn't submit tune request to provider");

    if (!pins_connected)
    {
        /*
         * NOTE: With legacy providers, we have to submit a tune request
         *       before connecting pins.
         */
        hr = IFilterGraph2_ConnectDirect(graph, provider_out, source_in, NULL);
        ENUM_CKHR("couldn't connect network provider to tuner");
    }

out:
    ASC_RELEASE(provider_tuner);
    ASC_RELEASE(request);
    ASC_RELEASE(space);
    ASC_RELEASE(source_in);
    ASC_RELEASE(provider_out);
    ASC_RELEASE(graph);
    ASC_RELEASE(provider);

    if (ENUM_CATCH())
    {
        if (!asc_log_is_debug())
        {
            /* set "netname" => nil */
            lua_pop(L, 1);
            lua_pushnil(L);
        }
        else
        {
            /* set "netname" => "error msg" */
        }
    }
    else
    {
        /* set "netname" => true */
        lua_pushboolean(L, 1);
    }
}

/* put device details into Lua table at the top of the stack */
static
int parse_moniker(lua_State *L, IMoniker *moniker)
{
    HRESULT hr = E_FAIL;

    int supported_nets = 0;
    char *buf = NULL;

    IBaseFilter *source = NULL;

    /* get device path */
    hr = dshow_get_property(moniker, "DevicePath", &buf);
    ENUM_CKHR("couldn't retrieve device path");

    lua_pushstring(L, buf);
    lua_setfield(L, -2, "devpath");
    free(buf);

    /* probe tuner for supported network types */
    hr = dshow_filter_from_moniker(moniker, &source, &buf);
    ENUM_CKHR("couldn't instantiate device filter");

    lua_pushstring(L, buf);
    lua_setfield(L, -2, "name");
    free(buf);

    lua_newtable(L);
    for (const bda_network_t *const *ptr = bda_network_list
         ; *ptr != NULL; ptr++)
    {
        const bda_network_t *const net = *ptr;

        probe_tuner(L, source, net);
        if (lua_isboolean(L, -1) && lua_toboolean(L, -1))
            supported_nets++;

        lua_setfield(L, -2, net->name[0]);
    }
    lua_setfield(L, -2, "type");

out:
    ASC_RELEASE(source);

    if (ENUM_CATCH())
    {
        lua_setfield(L, -2, "error");
        return -1;
    }

    return supported_nets;
}

/* return a Lua table containing a list of installed tuners */
static
int bda_enumerate(lua_State *L)
{
    HRESULT hr = E_FAIL;

    bool need_uninit = false;
    int dev_idx = 0;

    IEnumMoniker *enum_moniker = NULL;
    IMoniker *moniker = NULL;

    lua_newtable(L);

    /* initialize COM */
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    ENUM_CKHR("CoInitializeEx() failed");

    need_uninit = true;

    /* list BDA tuners */
    hr = dshow_enum(&KSCATEGORY_BDA_NETWORK_TUNER, &enum_moniker, 0);
    if (FAILED(hr))
        ENUM_THROW("couldn't create device enumerator");
    else if (hr != S_OK)
        goto out; /* no tuners; return empty table */

    do
    {
        ASC_RELEASE(moniker);

        /* fetch next item */
        hr = IEnumMoniker_Next(enum_moniker, 1, &moniker, NULL);
        ASC_WANT_ENUM(hr, moniker);

        if (FAILED(hr))
            ENUM_THROW("couldn't retrieve next device filter");
        else if (hr != S_OK)
            break; /* no more items */

        /* add tuner to list */
        lua_newtable(L);
        lua_pushinteger(L, dev_idx++);
        lua_setfield(L, -2, "adapter");

        if (parse_moniker(L, moniker) != 0)
        {
            const int pos = luaL_len(L, -2) + 1;
            lua_rawseti(L, -2, pos);
        }
        else
        {
            /* tuner doesn't support any digital networks */
            lua_pop(L, 1);
        }
    } while (true);

out:
    ASC_RELEASE(enum_moniker);

    if (need_uninit)
        CoUninitialize();

    if (ENUM_CATCH())
        lua_error(L);

    return 1;
}

HW_ENUM_REGISTER(bda)
{
    .name = "dvb_input",
    .description = "DVB Input (DirectShow BDA)",
    .enumerate = bda_enumerate,
};
