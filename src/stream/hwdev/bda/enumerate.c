/*
 * Astra Module: BDA
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

#include "bda.h"

#define BDA_ENUM_THROW(_func) \
    do { \
        char *const _msg = dshow_error_msg(hr); \
        lua_pushfstring(L, _func ": %s", _msg); \
        free(_msg); \
        goto out; \
    } while (0)

#define BDA_ENUM_CATCH() \
    lua_isstring(L, -1)

/* check if the tuner supports a particular network type */
static
bool probe_tuner(lua_State *L, IBaseFilter *tuner_dev
                 , const bda_network_t *net)
{
    HRESULT hr = E_FAIL;
    bool pins_connected = false;

    IBaseFilter *net_prov = NULL;
    IGraphBuilder *graph = NULL;
    IPin *prov_out = NULL, *tuner_in = NULL;
    ITuningSpace *spc = NULL;
    ITuneRequest *req = NULL;
    ITuner *prov_tuner = NULL;

    /* create network provider */
    hr = bda_net_provider(net, &net_prov);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't create network provider");

    /* create graph and add filters */
    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC
                          , &IID_IGraphBuilder, (void **)&graph);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't create filter graph");

    hr = IGraphBuilder_AddFilter(graph, net_prov, NULL);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't add network provider to graph");

    hr = IGraphBuilder_AddFilter(graph, tuner_dev, NULL);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't add source filter to graph");

    /* try connecting the pins */
    hr = dshow_find_pin(net_prov, PINDIR_OUTPUT, true, &prov_out);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't find network provider's output pin");

    hr = dshow_find_pin(tuner_dev, PINDIR_INPUT, true, &tuner_in);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't find source filter's input pin");

    hr = IGraphBuilder_ConnectDirect(graph, prov_out, tuner_in, NULL);
    if (SUCCEEDED(hr))
        pins_connected = true;

    /* create empty tune request */
    hr = bda_tuning_space(net, &spc);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't initialize tuning space");

    hr = ITuningSpace_CreateTuneRequest(spc, &req);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't create tune request");

    /* submit request to network provider */
    hr = IBaseFilter_QueryInterface(net_prov, &IID_ITuner
                                    , (void **)&prov_tuner);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't query ITuner interface");

    hr = ITuner_put_TuningSpace(prov_tuner, spc);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't assign tuning space to provider");

    hr = ITuner_put_TuneRequest(prov_tuner, req);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't submit tune request to provider");

    if (!pins_connected)
    {
        /*
         * With legacy providers, we have to submit a tune request
         * before connecting pins.
         */
        hr = IGraphBuilder_ConnectDirect(graph, prov_out, tuner_in, NULL);
        if (FAILED(hr))
            BDA_ENUM_THROW("couldn't connect network provider to tuner");
    }

out:
    SAFE_RELEASE(prov_tuner);
    SAFE_RELEASE(req);
    SAFE_RELEASE(spc);
    SAFE_RELEASE(tuner_in);
    SAFE_RELEASE(prov_out);
    SAFE_RELEASE(graph);
    SAFE_RELEASE(net_prov);

    if (BDA_ENUM_CATCH())
    {
        asc_log_debug("%s", lua_tostring(L, -1));
        lua_pop(L, 1);

        return false;
    }

    return true;
}

/* put device details into Lua table at the top of the stack */
static
int parse_moniker(lua_State *L, IMoniker *moniker)
{
    HRESULT hr = E_FAIL;
    int supported_nets = 0;

    IMalloc *alloc = NULL;
    IBindCtx *bind_ctx = NULL;
    IPropertyBag *prop_bag = NULL;
    IBaseFilter *tuner_dev = NULL;

    char *buf = NULL;
    wchar_t *wbuf = NULL;

    hr = CoGetMalloc(1, &alloc);
    if (FAILED(hr))
        BDA_ENUM_THROW("CoGetMalloc()");

    hr = CreateBindCtx(0, &bind_ctx);
    if (FAILED(hr))
        BDA_ENUM_THROW("CreateBindCtx()");

    /* get friendly name */
    hr = IMoniker_BindToStorage(moniker, bind_ctx, NULL, &IID_IPropertyBag
                                , (void **)&prop_bag);
    if (FAILED(hr))
        BDA_ENUM_THROW("IMoniker::BindToStorage()");

    VARIANT var_fname;
    memset(&var_fname, 0, sizeof(var_fname));

    hr = IPropertyBag_Read(prop_bag, L"FriendlyName", &var_fname, NULL);
    if (FAILED(hr))
        BDA_ENUM_THROW("IPropertyBag::Read()");

    if (var_fname.bstrVal != NULL)
    {
        buf = cx_narrow(var_fname.bstrVal);

        /* checking for NULL is not necessary here */
        asc_log_debug("probing '%s'", buf);
        lua_pushstring(L, buf);
        lua_setfield(L, -2, "name");

        free(buf);
    }

    /* get display name */
    hr = IMoniker_GetDisplayName(moniker, bind_ctx, NULL, &wbuf);
    if (FAILED(hr))
        BDA_ENUM_THROW("IMoniker::GetDisplayName()");

    if (wbuf != NULL)
    {
        buf = cx_narrow(wbuf);
        IMalloc_Free(alloc, wbuf);

        lua_pushstring(L, buf);
        lua_setfield(L, -2, "displayname");
        free(buf);
    }

    /* probe tuner for supported network types */
    hr = IMoniker_BindToObject(moniker, bind_ctx, NULL, &IID_IBaseFilter
                               , (void **)&tuner_dev);
    if (FAILED(hr))
        BDA_ENUM_THROW("IMoniker::BindToObject()");

    lua_newtable(L);
    for (const bda_network_t *const *ptr = bda_network_list
         ; *ptr != NULL; ptr++)
    {
        const bda_network_t *const net = *ptr;
        asc_log_debug("checking if %s is supported", net->name[0]);

        if (probe_tuner(L, tuner_dev, net))
        {
            lua_pushboolean(L, 1);
            lua_setfield(L, -2, net->name[0]);
            supported_nets++;
        }
    }
    lua_setfield(L, -2, "type");

out:
    SAFE_RELEASE(tuner_dev);
    SAFE_RELEASE(prop_bag);
    SAFE_RELEASE(bind_ctx);
    SAFE_RELEASE(alloc);

    if (BDA_ENUM_CATCH())
    {
        lua_setfield(L, -2, "error");
        return -1;
    }

    return supported_nets;
}

int bda_enumerate(lua_State *L)
{
    HRESULT hr = E_FAIL;
    bool co_init = false;

    IEnumMoniker *enum_moniker = NULL;
    IMoniker *moniker = NULL;

    int dev_idx = 0;

    lua_newtable(L);

    /* initialize COM */
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        BDA_ENUM_THROW("CoInitializeEx()");

    co_init = true;

    /* list BDA tuners */
    hr = dshow_enum(&KSCATEGORY_BDA_NETWORK_TUNER, &enum_moniker);
    if (FAILED(hr))
        BDA_ENUM_THROW("couldn't create device enumerator");
    else if (hr != S_OK)
        goto out; /* no tuners; return empty table */

    do
    {
        SAFE_RELEASE(moniker);

        /* fetch next item */
        hr = IEnumMoniker_Next(enum_moniker, 1, &moniker, NULL);
        if (FAILED(hr))
            BDA_ENUM_THROW("IEnumMoniker::Next()");
        else if (hr != S_OK)
            break; /* no more items */

        /* add tuner to list */
        lua_newtable(L);
        lua_pushinteger(L, dev_idx++);
        lua_setfield(L, -2, "device");

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
    SAFE_RELEASE(enum_moniker);

    if (co_init)
        CoUninitialize();

    if (BDA_ENUM_CATCH())
        lua_error(L);

    return 1;
}

const hw_driver_t hw_driver_bda =
{
    .name = "dvb_input",
    .description = "DVB Input (DirectShow BDA)",

    .enumerate = bda_enumerate,
};
