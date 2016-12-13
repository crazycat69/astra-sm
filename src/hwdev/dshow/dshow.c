/*
 * Astra Module: DirectShow
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

#include <astra/astra.h>
#include "dshow.h"

/* format DirectShow error message. the result must be freed using free() */
char *dshow_error_msg(HRESULT hr)
{
    wchar_t buf[MAX_ERROR_TEXT_LEN] = { L'\0' };
    const size_t bufsiz = ASC_ARRAY_SIZE(buf);

    const DWORD ret = AMGetErrorTextW(hr, buf, bufsiz);
    if (ret == 0)
        StringCchPrintfW(buf, bufsiz, L"Unknown Error: 0x%2x", hr);

    char *const msg = cx_narrow(buf);
    if (msg != NULL)
    {
        /* remove trailing punctuation */
        for (ssize_t i = strlen(msg) - 1; i >= 0; i--)
        {
            if (msg[i] != '.' && !isspace(msg[i]))
                break;

            msg[i] = '\0';
        }
    }

    return msg;
}

/* create moniker enumerator for a specified device category */
HRESULT dshow_enum(const CLSID *category, IEnumMoniker **out)
{
    if (category == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    ICreateDevEnum *dev_enum = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_SystemDeviceEnum, NULL
                                  , CLSCTX_INPROC_SERVER, &IID_ICreateDevEnum
                                  , (void **)&dev_enum);
    DS_WANT_PTR(hr, dev_enum);
    if (FAILED(hr))
        return hr;

    hr = ICreateDevEnum_CreateClassEnumerator(dev_enum, category, out, 0);
    DS_WANT_ENUM(hr, *out);
    SAFE_RELEASE(dev_enum);

    return hr;
}

/* return filter that has a specific index in its category */
HRESULT dshow_filter_by_index(const CLSID *category, size_t index
                              , IBaseFilter **out, char **fname)
{
    HRESULT hr = E_FAIL;

    IEnumMoniker *enum_moniker = NULL;
    IMoniker *moniker = NULL;

    if (category == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    hr = dshow_enum(category, &enum_moniker);
    if (hr != S_OK)
        return hr; /* empty category */

    if (index > 0)
    {
        /* skip elements leading up to requested filter */
        hr = IEnumMoniker_Skip(enum_moniker, index);
        if (hr != S_OK) goto out;
    }

    hr = IEnumMoniker_Next(enum_moniker, 1, &moniker, NULL);
    DS_WANT_ENUM(hr, moniker);
    if (hr != S_OK) goto out;

    hr = dshow_filter_from_moniker(moniker, out, fname);

out:
    SAFE_RELEASE(moniker);
    SAFE_RELEASE(enum_moniker);

    return hr;
}

/* return filter with matching device path */
HRESULT dshow_filter_by_path(const CLSID *category, const char *devpath
                             , IBaseFilter **out, char **fname)
{
    if (category == NULL || devpath == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    IEnumMoniker *enum_moniker = NULL;
    HRESULT hr = dshow_enum(category, &enum_moniker);
    if (hr != S_OK)
        return hr; /* empty category */

    IMoniker *moniker = NULL;
    const size_t devlen = strlen(devpath);

    do
    {
        SAFE_RELEASE(moniker);

        if (*out != NULL)
            break;

        /* fetch next item */
        hr = IEnumMoniker_Next(enum_moniker, 1, &moniker, NULL);
        DS_WANT_ENUM(hr, moniker);

        if (hr != S_OK)
            break; /* no more filters */

        char *buf = NULL;
        hr = dshow_get_property(moniker, "DevicePath", &buf);
        if (SUCCEEDED(hr))
        {
            /* compare beginning of the device path */
            if (!strncmp(buf, devpath, devlen))
                hr = dshow_filter_from_moniker(moniker, out, fname);

            free(buf);
        }
    } while (true);

    SAFE_RELEASE(enum_moniker);

    return hr;
}

/* create filter object from a moniker */
HRESULT dshow_filter_from_moniker(IMoniker *moniker, IBaseFilter **out
                                  , char **fname)
{
    HRESULT hr = E_FAIL;

    IBindCtx *bind_ctx = NULL;
    IBaseFilter *filter = NULL;

    if (moniker == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    hr = CreateBindCtx(0, &bind_ctx);
    DS_WANT_PTR(hr, bind_ctx);
    if (FAILED(hr)) goto out;

    hr = IMoniker_BindToObject(moniker, bind_ctx, NULL
                               , &IID_IBaseFilter, (void **)&filter);
    DS_WANT_PTR(hr, filter);
    if (FAILED(hr)) goto out;

    if (fname != NULL)
    {
        /* NOTE: if fname is set, it must be freed by the caller */
        hr = dshow_get_property(moniker, "FriendlyName", fname);
        if (FAILED(hr)) goto out;
    }

    IBaseFilter_AddRef(filter);
    *out = filter;

out:
    SAFE_RELEASE(filter);
    SAFE_RELEASE(bind_ctx);

    return hr;
}

/* create filter graph and its associated interfaces */
HRESULT dshow_filter_graph(IFilterGraph2 **out_graph, IMediaEvent **out_event
                           , HANDLE *out_evhdl)
{
    HRESULT hr = E_FAIL;

    IFilterGraph2 *graph = NULL;
    IUnknown *dummy = NULL;
    IRegisterServiceProvider *regsvc = NULL;
    IMediaEvent *event = NULL;

    if (out_graph == NULL)
        return E_POINTER;

    *out_graph = NULL;

    /* create graph */
    hr = CoCreateInstance(&CLSID_FilterGraphNoThread, NULL
                          , CLSCTX_INPROC_SERVER, &IID_IFilterGraph2
                          , (void **)&graph);
    DS_WANT_PTR(hr, graph);
    if (FAILED(hr)) return hr;

    /*
     * Apply memory leak "fix" for universal NP. Dummy object doesn't
     * have to be a locator; in fact, anything that doesn't support
     * the IESEventService interface will do.
     */
    hr = CoCreateInstance(&CLSID_DVBTLocator, NULL, CLSCTX_INPROC_SERVER
                          , &IID_IUnknown, (void **)&dummy);
    DS_WANT_PTR(hr, dummy);
    if (FAILED(hr)) goto out;

    hr = IFilterGraph2_QueryInterface(graph, &IID_IRegisterServiceProvider
                                      , (void **)&regsvc);
    DS_WANT_PTR(hr, regsvc);
    if (FAILED(hr)) goto out;

    hr = regsvc->lpVtbl->RegisterService(regsvc, &CLSID_ESEventService
                                         , dummy);
    if (FAILED(hr)) goto out;

    /* return graph and friends */
    if (out_event != NULL)
    {
        hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaEvent
                                          , (void **)&event);
        DS_WANT_PTR(hr, event);
        if (FAILED(hr)) goto out;

        if (out_evhdl != NULL)
        {
            hr = IMediaEvent_GetEventHandle(event, (OAEVENT *)out_evhdl);
            if (FAILED(hr)) goto out;
        }

        IMediaEvent_AddRef(event);
        *out_event = event;
    }

    IFilterGraph2_AddRef(graph);
    *out_graph = graph;

out:
    SAFE_RELEASE(event);
    SAFE_RELEASE(regsvc);
    SAFE_RELEASE(dummy);
    SAFE_RELEASE(graph);

    return hr;
}

/* look for a filter pin with matching parameters */
HRESULT dshow_find_pin(IBaseFilter *filter, PIN_DIRECTION dir
                       , bool skip_busy, const char *name, IPin **out)
{
    if (filter == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* convert pin name */
    wchar_t *wname = NULL;
    if (name != NULL)
    {
        wname = cx_widen(name);
        if (wname == NULL)
            return E_OUTOFMEMORY;
    }

    /* look for requested pin */
    IEnumPins *enum_pins = NULL;
    HRESULT hr = IBaseFilter_EnumPins(filter, &enum_pins);
    DS_WANT_PTR(hr, enum_pins);

    if (FAILED(hr))
    {
        free(wname);
        return hr;
    }

    IPin *pin = NULL;
    do
    {
        SAFE_RELEASE(pin);

        /* fetch next item */
        hr = IEnumPins_Next(enum_pins, 1, &pin, NULL);
        DS_WANT_ENUM(hr, pin);

        if (hr != S_OK)
            break; /* no more pins */

        if (skip_busy && dshow_pin_connected(pin))
            continue; /* don't want busy pin */

        PIN_INFO info;
        memset(&info, 0, sizeof(info));
        hr = IPin_QueryPinInfo(pin, &info);
        if (hr != S_OK)
            continue; /* no info */

        SAFE_RELEASE(info.pFilter);
        if (info.dir != dir)
            continue; /* wrong direction */

        if (wname != NULL && wcscmp(wname, info.achName))
            continue; /* wrong name */

        /* found it */
        *out = pin;
        break;
    } while (true);

    if (SUCCEEDED(hr) && hr != S_OK)
        hr = E_NOINTERFACE; /* don't return S_FALSE */

    SAFE_RELEASE(enum_pins);
    ASC_FREE(wname, free);

    return hr;
}

/* query filter to get the graph it's currently in */
HRESULT dshow_get_graph(IBaseFilter *filter, IFilterGraph2 **out)
{
    if (filter == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* get basic interface */
    FILTER_INFO fi;
    memset(&fi, 0, sizeof(fi));

    HRESULT hr = IBaseFilter_QueryFilterInfo(filter, &fi);
    if (FAILED(hr))
        return hr;

    if (fi.pGraph == NULL)
        return VFW_E_NOT_IN_GRAPH;

    /* get extended interface */
    hr = IFilterGraph_QueryInterface(fi.pGraph, &IID_IFilterGraph2
                                     , (void **)out);
    DS_WANT_PTR(hr, *out);
    SAFE_RELEASE(fi.pGraph);

    return hr;
}

/* fetch property from a moniker */
HRESULT dshow_get_property(IMoniker *moniker, const char *prop, char **out)
{
    HRESULT hr = E_FAIL;

    wchar_t *wprop = NULL;
    VARIANT prop_var;

    IBindCtx *bind_ctx = NULL;
    IPropertyBag *bag = NULL;

    if (moniker == NULL || prop == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;
    memset(&prop_var, 0, sizeof(prop_var));
    prop_var.vt = VT_BSTR;

    /* convert property name */
    wprop = cx_widen(prop);
    if (wprop == NULL)
        return E_OUTOFMEMORY;

    /* read property from property bag */
    hr = CreateBindCtx(0, &bind_ctx);
    DS_WANT_PTR(hr, bind_ctx);
    if (FAILED(hr)) goto out;

    hr = IMoniker_BindToStorage(moniker, bind_ctx, NULL
                                , &IID_IPropertyBag, (void **)&bag);
    DS_WANT_PTR(hr, bag);
    if (FAILED(hr)) goto out;

    hr = IPropertyBag_Read(bag, wprop, &prop_var, NULL);
    if (FAILED(hr)) goto out;

    if (prop_var.bstrVal != NULL)
        *out = cx_narrow(prop_var.bstrVal);

    if (*out == NULL)
        hr = E_OUTOFMEMORY;

out:
    VariantClear(&prop_var);
    ASC_FREE(wprop, free);

    SAFE_RELEASE(bag);
    SAFE_RELEASE(bind_ctx);

    return hr;
}

/* check whether a pin is connected */
bool dshow_pin_connected(IPin *pin)
{
    IPin *remote = NULL;
    HRESULT hr = IPin_ConnectedTo(pin, &remote);

    if (SUCCEEDED(hr))
    {
        SAFE_RELEASE(remote);
        return true;
    }

    return false;
}
