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

/*
 * TODO: implement own probe/injector filter instead of using SampleGrabber.
 */
#include <qedit.h>

typedef struct
{
    ISampleGrabberCBVtbl *vtbl;

    LONG ref;
    sample_callback_t callback;
    void *arg;
} MySampleGrabberCB;

/* IUnknown::QueryInterface */
static STDMETHODCALLTYPE
HRESULT QueryInterface(ISampleGrabberCB *obj, const IID *iid, void **out)
{
    if (out == NULL)
        return E_POINTER;

    if (IsEqualIID(iid, &IID_ISampleGrabberCB)
        || IsEqualIID(iid, &IID_IUnknown))
    {
        obj->lpVtbl->AddRef(obj);
        *out = obj;

        return S_OK;
    }

    *out = NULL;

    return E_NOINTERFACE;
}

/* IUnknown::AddRef */
static STDMETHODCALLTYPE
ULONG AddRef(ISampleGrabberCB *obj)
{
    MySampleGrabberCB *const cb = (MySampleGrabberCB *)obj;
    return InterlockedIncrement(&cb->ref);
}

/* IUnknown::Release */
static STDMETHODCALLTYPE
ULONG Release(ISampleGrabberCB *obj)
{
    MySampleGrabberCB *const cb = (MySampleGrabberCB *)obj;
    const ULONG ref = InterlockedDecrement(&cb->ref);

    if (ref == 0)
    {
        CoTaskMemFree(cb->vtbl);
        CoTaskMemFree(cb);
    }

    return ref;
}

/* ISampleGrabberCB::SampleCB */
static STDMETHODCALLTYPE
HRESULT SampleCB(ISampleGrabberCB *obj, double time, IMediaSample *sample)
{
    __uarg(obj);
    __uarg(time);
    __uarg(sample);

    return E_NOTIMPL;
}

/* ISampleGrabberCB::BufferCB */
static STDMETHODCALLTYPE
HRESULT BufferCB(ISampleGrabberCB *obj, double time, BYTE *buf, LONG len)
{
    __uarg(time);

    MySampleGrabberCB *const cb = (MySampleGrabberCB *)obj;
    cb->callback(cb->arg, buf, len);

    return S_OK;
}

/* callback virtual method table */
static const
ISampleGrabberCBVtbl cb_vtbl =
{
    .QueryInterface = QueryInterface,
    .AddRef = AddRef,
    .Release = Release,
    .SampleCB = SampleCB,
    .BufferCB = BufferCB,
};

/* callback object constructor */
static
HRESULT grabber_cb(sample_callback_t callback, void *arg
                   , ISampleGrabberCB **out)
{
    if (callback == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* allocate memory for object and its vtbl */
    MySampleGrabberCB *const cb =
        (MySampleGrabberCB *)CoTaskMemAlloc(sizeof(*cb));
    ISampleGrabberCBVtbl *const vtbl =
        (ISampleGrabberCBVtbl *)CoTaskMemAlloc(sizeof(*vtbl));

    if (cb == NULL || vtbl == NULL)
    {
        CoTaskMemFree(cb);
        CoTaskMemFree(vtbl);

        return E_OUTOFMEMORY;
    }

    /* set things up */
    memset(cb, 0, sizeof(*cb));
    memcpy(vtbl, &cb_vtbl, sizeof(*vtbl));

    cb->vtbl = vtbl;
    cb->ref = 1;
    cb->callback = callback;
    cb->arg = arg;

    *out = (ISampleGrabberCB *)cb;

    return S_OK;
}

/* create probe filter */
HRESULT dshow_grabber(sample_callback_t callback, void *arg
                      , const AM_MEDIA_TYPE *media_type, IBaseFilter **out)
{
    HRESULT hr = E_FAIL;

    ISampleGrabberCB *cb = NULL;
    ISampleGrabber *grabber = NULL;

    if (out == NULL)
        return E_POINTER;

    *out = NULL;

    /* create callback */
    hr = grabber_cb(callback, arg, &cb);
    if (FAILED(hr)) goto out;

    /* create grabber */
    hr = CoCreateInstance(&CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER
                          , &IID_ISampleGrabber, (void **)&grabber);
    DS_WANT_PTR(hr, grabber);
    if (FAILED(hr)) goto out;

    hr = ISampleGrabber_SetBufferSamples(grabber, FALSE);
    if (FAILED(hr)) goto out;

    hr = ISampleGrabber_SetCallback(grabber, cb, 1);
    if (FAILED(hr)) goto out;

    if (media_type != NULL)
    {
        hr = ISampleGrabber_SetMediaType(grabber, media_type);
        if (FAILED(hr)) goto out;
    }

    hr = ISampleGrabber_SetOneShot(grabber, FALSE);
    if (FAILED(hr)) goto out;

    /* query filter interface */
    hr = ISampleGrabber_QueryInterface(grabber, &IID_IBaseFilter
                                       , (void **)out);
    DS_WANT_PTR(hr, *out);

out:
    SAFE_RELEASE(grabber);
    SAFE_RELEASE(cb);

    return hr;
}
