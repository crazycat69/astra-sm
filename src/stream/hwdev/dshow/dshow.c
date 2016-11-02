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

// FIXME: move these to astra.h
#define UNICODE
#define _UNICODE
// FIXME

#include <astra/astra.h>
#include "dshow.h"

/* format DirectShow error message. the result must be freed using free() */
char *dshow_error_msg(HRESULT hr)
{
    wchar_t buf[MAX_ERROR_TEXT_LEN] = { L'\0' };
    static const size_t bufsiz = ASC_ARRAY_SIZE(buf);

    const DWORD ret = AMGetErrorText(hr, buf, bufsiz);
    if (ret == 0)
        StringCchPrintf(buf, bufsiz, L"Unknown Error: 0x%2x", hr);

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
    if (out == NULL)
        return E_POINTER;

    *out = NULL;

    ICreateDevEnum *dev_enum = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC
                                  , &IID_ICreateDevEnum, (void **)&dev_enum);
    if (FAILED(hr))
        return hr;

    hr = ICreateDevEnum_CreateClassEnumerator(dev_enum, category, out, 0);
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
    do
    {
        SAFE_RELEASE(moniker);

        if (*out != NULL)
            break;

        /* fetch next item */
        hr = IEnumMoniker_Next(enum_moniker, 1, &moniker, NULL);
        if (hr != S_OK)
            break; /* no more filters */

        char *buf = NULL;
        hr = dshow_get_property(moniker, "DevicePath", &buf);
        if (SUCCEEDED(hr))
        {
            if (strstr(buf, devpath) == buf)
                hr = dshow_filter_from_moniker(moniker, out, fname);

            free(buf);
        }
    } while (true);

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
    if (FAILED(hr)) return hr;

    hr = IMoniker_BindToObject(moniker, bind_ctx, NULL
                               , &IID_IBaseFilter, (void **)&filter);
    if (FAILED(hr)) goto out;

    if (fname != NULL)
    {
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

    /* convert property name */
    wprop = cx_widen(prop);
    if (wprop == NULL)
        return E_OUTOFMEMORY;

    memset(&prop_var, 0, sizeof(prop_var));
    prop_var.vt = VT_BSTR;

    /* read property from property bag */
    hr = CreateBindCtx(0, &bind_ctx);
    if (FAILED(hr)) goto out;

    hr = IMoniker_BindToStorage(moniker, bind_ctx, NULL
                                , &IID_IPropertyBag, (void **)&bag);
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

/* look for a filter pin with matching direction */
HRESULT dshow_find_pin(IBaseFilter *filter, PIN_DIRECTION dir, bool free_only
                       , IPin **out)
{
    if (out == NULL)
        return E_POINTER;

    *out = NULL;

    IEnumPins *enum_pins = NULL;
    HRESULT hr = IBaseFilter_EnumPins(filter, &enum_pins);
    if (FAILED(hr))
        return hr;

    IPin *pin = NULL;
    do
    {
        SAFE_RELEASE(pin);

        hr = IEnumPins_Next(enum_pins, 1, &pin, NULL);
        if (hr != S_OK)
        {
            /* no more pins */
            if (SUCCEEDED(hr))
                hr = E_NOINTERFACE;

            break;
        }

        if (free_only)
        {
            /* skip busy pins */
            IPin *remote = NULL;
            hr = IPin_ConnectedTo(pin, &remote);
            if (hr == S_OK)
            {
                SAFE_RELEASE(remote);
                continue;
            }
        }

        PIN_DIRECTION pin_dir;
        hr = IPin_QueryDirection(pin, &pin_dir);
        if (hr == S_OK && pin_dir == dir)
        {
            *out = pin;
            break;
        }
    } while (true);

    SAFE_RELEASE(enum_pins);

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
    SAFE_RELEASE(fi.pGraph);

    return hr;
}
