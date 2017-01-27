/*
 * Astra Module: BDA (Graph builder)
 *
 * Copyright (C) 2016-2017, Artem Kharitonov <artem@3phase.pw>
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
#include <astra/core/mainloop.h>

/* device reopen timeout, seconds */
#define BDA_REINIT_TICKS 10

/* buffer dequeue threshold, packets */
#define BDA_BUFFER_THRESH 10

/* minimum delay between DiSEqC commands, milliseconds */
#define BDA_DISEQC_DELAY 15

/*
 * helper functions for working with the graph
 */

/* create a source filter based on user settings and add it to the graph */
static
HRESULT create_source(const module_data_t *mod, IFilterGraph2 *graph
                      , IBaseFilter **out)
{
    HRESULT hr = E_FAIL;

    char *fname = NULL;
    wchar_t *wbuf = NULL;

    IBaseFilter *source = NULL;

    if (graph == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* instantiate source filter */
    if (mod->adapter != -1)
    {
        /* search by adapter number */
        hr = dshow_filter_by_index(&KSCATEGORY_BDA_NETWORK_TUNER
                                   , mod->adapter, &source, &fname);
    }
    else if (mod->devpath != NULL)
    {
        /* search by unique device path */
        hr = dshow_filter_by_path(&KSCATEGORY_BDA_NETWORK_TUNER
                                  , mod->devpath, &source, &fname);
    }

    if (hr != S_OK)
        goto out;

    /* log filter name and add it to graph */
    asc_log_info(MSG("source: %s"), fname);

    wbuf = cx_widen(fname);
    if (wbuf != NULL)
        hr = IFilterGraph2_AddFilter(graph, source, wbuf);
    else
        hr = E_OUTOFMEMORY;

    BDA_CKHR_D("couldn't add source filter to graph");

    IBaseFilter_AddRef(source);
    *out = source;

out:
    ASC_FREE(wbuf, free);
    ASC_FREE(fname, free);

    ASC_RELEASE(source);

    return hr;
}

/* find a receiver corresponding to the source and connect it to the graph */
static
HRESULT create_receiver(const module_data_t *mod, IBaseFilter *source
                        , IBaseFilter **out)
{
    HRESULT hr = E_FAIL;

    char *fname = NULL;
    wchar_t *wbuf = NULL;

    IFilterGraph2 *graph = NULL;
    IPin *source_out = NULL;
    IEnumMoniker *enum_moniker = NULL;
    IMoniker *moniker = NULL;
    IBaseFilter *rcv = NULL;
    IPin *rcv_in = NULL;

    if (source == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* get source's graph and output pin */
    hr = dshow_get_graph(source, &graph);
    BDA_CKHR_D("couldn't get source filter's graph");

    hr = dshow_find_pin(source, PINDIR_OUTPUT, true, NULL, &source_out);
    BDA_CKHR_D("couldn't find output pin on source filter");

    /* list possible candidates for attaching to source filter */
    hr = dshow_enum(&KSCATEGORY_BDA_RECEIVER_COMPONENT, &enum_moniker);
    if (FAILED(hr))
        BDA_THROW_D("couldn't enumerate BDA receiver filters");
    else if (hr != S_OK)
        goto out; /* no receivers installed */

    do
    {
        ASC_RELEASE(rcv_in);
        ASC_RELEASE(rcv);
        ASC_RELEASE(moniker);

        ASC_FREE(fname, free);
        ASC_FREE(wbuf, free);

        if (*out != NULL)
            break;

        /* fetch next item */
        hr = IEnumMoniker_Next(enum_moniker, 1, &moniker, NULL);
        ASC_WANT_ENUM(hr, moniker);

        if (FAILED(hr))
            BDA_THROW_D("couldn't retrieve next receiver filter");
        else if (hr != S_OK)
            break; /* no more filters */

        /* add filter to graph and try to connect pins */
        hr = dshow_filter_from_moniker(moniker, &rcv, &fname);
        if (FAILED(hr)) continue;

        hr = dshow_find_pin(rcv, PINDIR_INPUT, true, NULL, &rcv_in);
        if (FAILED(hr)) continue;

        wbuf = cx_widen(fname);
        if (wbuf != NULL)
            hr = IFilterGraph2_AddFilter(graph, rcv, wbuf);
        else
            hr = E_OUTOFMEMORY;

        if (FAILED(hr))
            continue;

        hr = IFilterGraph2_ConnectDirect(graph, source_out, rcv_in, NULL);
        if (hr == S_OK)
        {
            /* found it. will exit the cycle on next iteration */
            IBaseFilter_AddRef(rcv);
            *out = rcv;

            asc_log_info(MSG("capture: %s"), fname);
        }
        else
        {
            IFilterGraph2_RemoveFilter(graph, rcv);
        }
    } while (true);

out:
    ASC_RELEASE(enum_moniker);
    ASC_RELEASE(source_out);
    ASC_RELEASE(graph);

    return hr;
}

/* create demultiplexer filter and connect it to the graph */
static
HRESULT create_demux(const module_data_t *mod, IBaseFilter *tail
                     , IBaseFilter **out)
{
    HRESULT hr = E_FAIL;

    IFilterGraph2 *graph = NULL;
    IBaseFilter *demux = NULL;
    IPin *tail_out = NULL, *demux_in = NULL;

    if (tail == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    hr = dshow_get_graph(tail, &graph);
    BDA_CKHR_D("couldn't get capture filter's graph");

    hr = CoCreateInstance(&CLSID_MPEG2Demultiplexer, NULL
                          , CLSCTX_INPROC_SERVER, &IID_IBaseFilter
                          , (void **)&demux);
    BDA_CKPTR_D(demux, "couldn't create demultiplexer filter");

    hr = dshow_find_pin(tail, PINDIR_OUTPUT, true, NULL, &tail_out);
    BDA_CKHR_D("couldn't find output pin on capture filter");

    hr = dshow_find_pin(demux, PINDIR_INPUT, true, NULL, &demux_in);
    BDA_CKHR_D("couldn't find input pin on demultiplexer filter");

    hr = IFilterGraph2_AddFilter(graph, demux, L"Demux");
    BDA_CKHR_D("couldn't add demultiplexer to the graph");

    hr = IFilterGraph2_ConnectDirect(graph, tail_out, demux_in, NULL);
    if (FAILED(hr))
    {
        IFilterGraph2_RemoveFilter(graph, demux);
        BDA_THROW_D("couldn't connect capture filter to demultiplexer");
    }

    IBaseFilter_AddRef(demux);
    *out = demux;

out:
    ASC_RELEASE(demux_in);
    ASC_RELEASE(tail_out);
    ASC_RELEASE(demux);
    ASC_RELEASE(graph);

    return hr;
}

/* create TIF and connect it to the graph */
static
HRESULT create_tif(const module_data_t *mod, IBaseFilter *demux
                   , IBaseFilter **out)
{
    HRESULT hr = E_FAIL;

    IFilterGraph2 *graph = NULL;
    IBaseFilter *tif = NULL;
    IPin *tif_in = NULL, *demux_out = NULL;

    if (demux == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* get demultiplexer's graph */
    hr = dshow_get_graph(demux, &graph);
    BDA_CKHR_D("couldn't get demultiplexer's graph");

    /* create first filter from the TIF category */
    hr = dshow_filter_by_index(&KSCATEGORY_BDA_TRANSPORT_INFORMATION, 0
                               , &tif, NULL);
    BDA_CKHR_D("couldn't instantiate transport information filter");

    /* connect TIF to demux */
    hr = dshow_find_pin(tif, PINDIR_INPUT, true, NULL, &tif_in);
    BDA_CKHR_D("couldn't find input pin on TIF");

    hr = dshow_find_pin(demux, PINDIR_OUTPUT, true, NULL, &demux_out);
    BDA_CKHR_D("couldn't find output pin on demultiplexer");

    hr = IFilterGraph2_AddFilter(graph, tif, L"TIF");
    BDA_CKHR_D("couldn't add transport information filter to graph");

    /*
     * NOTE: There's a handle leak somewhere inside psisdecd.dll.
     *       No way to fix it except to throw out the standard TIF
     *       and reimplement its interfaces from scratch.
     */
    hr = IFilterGraph2_ConnectDirect(graph, demux_out, tif_in, NULL);
    if (FAILED(hr))
    {
        IFilterGraph2_RemoveFilter(graph, tif);
        BDA_THROW_D("couldn't connect TIF to demultiplexer");
    }

    IBaseFilter_AddRef(tif);
    *out = tif;

out:
    ASC_RELEASE(demux_out);
    ASC_RELEASE(tif_in);
    ASC_RELEASE(tif);
    ASC_RELEASE(graph);

    return hr;
}

/* create an output pin with PID mapping on the demux */
static
HRESULT create_pidmap(const module_data_t *mod, IBaseFilter *demux
                      , IMPEG2PIDMap **out)
{
    HRESULT hr = E_FAIL;
    wchar_t pin_name[] = L"TS Out";

    IMpeg2Demultiplexer *mpeg = NULL;
    IPin *mpeg_out = NULL;

    if (demux == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* add output pin to demultiplexer */
    hr = IBaseFilter_QueryInterface(demux, &IID_IMpeg2Demultiplexer
                                    , (void **)&mpeg);
    BDA_CKPTR_D(mpeg, "couldn't query IMpeg2Demultiplexer interface");

    AM_MEDIA_TYPE mt;
    memset(&mt, 0, sizeof(mt));
    mt.majortype = MEDIATYPE_Stream;
    mt.subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;

    hr = IMpeg2Demultiplexer_CreateOutputPin(mpeg, &mt, pin_name, &mpeg_out);
    BDA_CKPTR_D(mpeg_out, "couldn't create output pin on demultiplexer");

    /* query pid mapper */
    hr = IPin_QueryInterface(mpeg_out, &IID_IMPEG2PIDMap, (void **)out);
    BDA_CKPTR_D(*out, "couldn't query IMPEG2PIDMap interface");

out:
    ASC_RELEASE(mpeg_out);
    ASC_RELEASE(mpeg);

    return hr;
}

/* create TS probe and connect it directly to the capture filter */
static
void on_sample(void *arg, const uint8_t *buf, size_t len);

static
HRESULT create_probe(const module_data_t *mod, IBaseFilter *tail
                     , void *arg, IBaseFilter **out)
{
    HRESULT hr = E_FAIL;

    IFilterGraph2 *graph = NULL;
    IPin *tail_out = NULL, *probe_in = NULL;
    IBaseFilter *probe = NULL;

    if (tail == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* get tail filter's graph and output pin */
    hr = dshow_get_graph(tail, &graph);
    BDA_CKHR_D("couldn't get capture filter's graph");

    hr = dshow_find_pin(tail, PINDIR_OUTPUT, true, NULL, &tail_out);
    BDA_CKHR_D("couldn't find output pin on capture filter");

    /* try creating and attaching probes with different media subtypes */
    for (unsigned int i = 0; ; i++)
    {
        ASC_RELEASE(probe);
        ASC_RELEASE(probe_in);

        if (*out != NULL)
            break;

        /* set next type */
        AM_MEDIA_TYPE mt;
        memset(&mt, 0, sizeof(mt));
        mt.majortype = MEDIATYPE_Stream;

        if (i == 0)
            mt.subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;
        else if (i == 1)
            mt.subtype = KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT;
        else
            break;

        /* attach probe */
        hr = dshow_grabber(on_sample, arg, &mt, &probe);
        if (FAILED(hr)) continue;

        hr = dshow_find_pin(probe, PINDIR_INPUT, true, NULL, &probe_in);
        if (FAILED(hr)) continue;

        hr = IFilterGraph2_AddFilter(graph, probe, L"Probe");
        if (FAILED(hr)) continue;

        hr = IFilterGraph2_ConnectDirect(graph, tail_out, probe_in, NULL);
        if (SUCCEEDED(hr))
        {
            IBaseFilter_AddRef(probe);
            *out = probe;
        }
        else
        {
            IFilterGraph2_RemoveFilter(graph, probe);
        }
    }
    BDA_CKHR_D("couldn't connect TS probe to capture filter");

out:
    ASC_RELEASE(tail_out);
    ASC_RELEASE(graph);

    return hr;
}

/* create TS probe and connect it to demux PID mapper */
static
HRESULT create_probe_dmx(const module_data_t *mod, IMPEG2PIDMap *pidmap
                         , void *arg, IBaseFilter **out)
{
    HRESULT hr = E_FAIL;

    IPin *demux_out = NULL;
    IBaseFilter *demux = NULL;
    IFilterGraph2 *graph = NULL;
    IBaseFilter *probe = NULL;
    IPin *probe_in = NULL;

    if (pidmap == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* get graph object from PID mapper */
    hr = IMPEG2PIDMap_QueryInterface(pidmap, &IID_IPin, (void **)&demux_out);
    BDA_CKPTR_D(demux_out, "couldn't query IPin interface");

    PIN_INFO info;
    memset(&info, 0, sizeof(info));
    hr = IPin_QueryPinInfo(demux_out, &info);
    BDA_CKPTR_D(info.pFilter, "couldn't query PID mapper's pin information");
    demux = info.pFilter;

    hr = dshow_get_graph(demux, &graph);
    BDA_CKHR_D("couldn't get demultiplexer's graph");

    /* create probe and attach it to demultiplexer pin */
    AM_MEDIA_TYPE mt;
    memset(&mt, 0, sizeof(mt));
    mt.majortype = MEDIATYPE_Stream;
    mt.subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;

    hr = dshow_grabber(on_sample, arg, &mt, &probe);
    BDA_CKHR_D("couldn't instantiate TS probe filter");

    hr = dshow_find_pin(probe, PINDIR_INPUT, true, NULL, &probe_in);
    BDA_CKHR_D("couldn't find input pin on TS probe");

    hr = IFilterGraph2_AddFilter(graph, probe, L"Probe");
    BDA_CKHR_D("couldn't add TS probe to graph");

    hr = IFilterGraph2_ConnectDirect(graph, demux_out, probe_in, NULL);
    if (FAILED(hr))
    {
        IFilterGraph2_RemoveFilter(graph, probe);
        BDA_THROW_D("couldn't connect TS probe to demultiplexer");
    }

    IBaseFilter_AddRef(probe);
    *out = probe;

out:
    ASC_RELEASE(probe_in);
    ASC_RELEASE(probe);
    ASC_RELEASE(graph);
    ASC_RELEASE(demux);
    ASC_RELEASE(demux_out);

    return hr;
}

/* locate and query a specific control node */
static
HRESULT find_control_node(const module_data_t *mod, IBaseFilter *filter
                          , const GUID *intf, const IID *iid, void **out)
{
    HRESULT hr = E_FAIL;

    ULONG node_types_cnt = 0;
    ULONG node_types[32] = { 0 };
    ULONG node_intf_cnt = 0;
    GUID node_intf[32];

    IBDA_Topology *topology = NULL;
    IUnknown *node = NULL;

    if (filter == NULL || intf == NULL || iid == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* get topology interface */
    hr = IBaseFilter_QueryInterface(filter, &IID_IBDA_Topology
                                    , (void **)&topology);
    BDA_CKPTR_D(topology, "couldn't query IBDA_Topology interface");

    /* list node types */
    hr = IBDA_Topology_GetNodeTypes(topology, &node_types_cnt
                                    , ASC_ARRAY_SIZE(node_types)
                                    , node_types);
    BDA_CKHR_D("couldn't retrieve list of topology node types");

    for (unsigned int i = 0; i < node_types_cnt; i++)
    {
        /* list interfaces for each node type */
        memset(&node_intf, 0, sizeof(node_intf));
        hr = IBDA_Topology_GetNodeInterfaces(topology, node_types[i]
                                             , &node_intf_cnt
                                             , ASC_ARRAY_SIZE(node_intf)
                                             , node_intf);
        BDA_CKHR_D("couldn't retrieve list of node interfaces");

        for (unsigned int j = 0; j < node_intf_cnt; j++)
        {
            if (IsEqualGUID(intf, &node_intf[j]))
            {
                hr = IBDA_Topology_GetControlNode(topology, 0, 1
                                                  , node_types[i]
                                                  , &node);
                BDA_CKPTR_D(node, "couldn't retrieve control node interface");

                hr = IUnknown_QueryInterface(node, iid, out);
                BDA_CKPTR_D(*out, "couldn't query control node");

                goto out;
            }
        }
    }

    /* node not found */
    hr = S_FALSE;

out:
    ASC_RELEASE(node);
    ASC_RELEASE(topology);

    return hr;
}

/* submit user tuning data to network provider */
static
HRESULT provider_tune(const module_data_t *mod, IBaseFilter *provider
                      , const bda_tune_cmd_t *tune)
{
    HRESULT hr = E_FAIL;

    ITuneRequest *request = NULL;
    ITuningSpace *space = NULL;
    ITuner *provider_tuner = NULL;

    if (provider == NULL || tune == NULL)
        return E_POINTER;

    /* create tune request from user data */
    hr = bda_tune_request(tune, &request);
    BDA_CKHR_D("couldn't create tune request");

    if (mod->debug)
        bda_dump_request(request);

    /* load it into provider */
    hr = ITuneRequest_get_TuningSpace(request, &space);
    BDA_CKPTR_D(space, "couldn't retrieve tuning space");

    hr = IBaseFilter_QueryInterface(provider, &IID_ITuner
                                    , (void **)&provider_tuner);
    BDA_CKPTR_D(provider_tuner, "couldn't query ITuner interface");

    hr = ITuner_put_TuningSpace(provider_tuner, space);
    BDA_CKHR_D("couldn't assign tuning space to provider");

    hr = ITuner_put_TuneRequest(provider_tuner, request);
    BDA_CKHR_D("couldn't submit tune request to provider");

out:
    ASC_RELEASE(provider_tuner);
    ASC_RELEASE(space);
    ASC_RELEASE(request);

    return hr;
}

/* connect network provider to the source filter */
static
HRESULT provider_setup(const module_data_t *mod, IBaseFilter *provider
                       , IBaseFilter *source, const bda_tune_cmd_t *tune)
{
    HRESULT hr = E_FAIL;
    bool retry_pins = false;

    IFilterGraph2 *graph = NULL;
    IPin *provider_out = NULL;
    IPin *source_in = NULL;

    if (provider == NULL || source == NULL || tune == NULL)
        return E_POINTER;

    /* get provider's graph */
    hr = dshow_get_graph(provider, &graph);
    BDA_CKHR_D("couldn't get network provider's graph");

    /* get filters' pins */
    hr = dshow_find_pin(provider, PINDIR_OUTPUT, true, NULL, &provider_out);
    BDA_CKHR_D("couldn't find output pin on network provider filter");

    hr = dshow_find_pin(source, PINDIR_INPUT, true, NULL, &source_in);
    BDA_CKHR_D("couldn't find input pin on source filter");

    /* connect pins and submit initial tuning data to provider */
    hr = IFilterGraph2_ConnectDirect(graph, provider_out, source_in, NULL);
    if (FAILED(hr))
    {
        /*
         * NOTE: With legacy providers, we have to submit a tune request
         *       before connecting pins.
         */
        retry_pins = true;
    }

    hr = provider_tune(mod, provider, tune);
    BDA_CKHR_D("couldn't configure provider with initial tuning data");

    if (retry_pins)
    {
        hr = IFilterGraph2_ConnectDirect(graph, provider_out, source_in, NULL);
        BDA_CKHR_D("couldn't connect network provider to tuner");
    }

out:
    ASC_RELEASE(source_in);
    ASC_RELEASE(provider_out);
    ASC_RELEASE(graph);

    return hr;
}

/* load saved PID list into demultiplexer's PID mapper */
static
HRESULT restore_pids(IMPEG2PIDMap *pidmap, const bool join[TS_MAX_PID])
{
    if (pidmap == NULL)
        return E_POINTER;

    /* remove existing PID mappings first */
    IEnumPIDMap *enum_pid = NULL;
    HRESULT hr = IMPEG2PIDMap_EnumPIDMap(pidmap, &enum_pid);

    if (hr == S_OK && enum_pid != NULL)
    {
        PID_MAP old[TS_MAX_PID];
        ULONG old_cnt = 0;

        hr = IEnumPIDMap_Next(enum_pid, ASC_ARRAY_SIZE(old), old, &old_cnt);
        if (hr == S_OK && old_cnt > 0)
        {
            ULONG unpids[TS_MAX_PID];
            for (unsigned int i = 0; i < old_cnt; i++)
                unpids[i] = old[i].ulPID;

            IMPEG2PIDMap_UnmapPID(pidmap, old_cnt, unpids);
        }
    }

    ASC_RELEASE(enum_pid);

    /* create and submit PID array */
    ULONG pids[TS_MAX_PID] = { 0 };
    unsigned int cnt = 0;

    if (join != NULL)
    {
        for (unsigned int i = 0; i < TS_MAX_PID; i++)
        {
            if (join[i])
                pids[cnt++] = i;
        }
    }

    if (cnt > 0)
        hr = IMPEG2PIDMap_MapPID(pidmap, cnt, pids, MEDIA_TRANSPORT_PACKET);
    else
        hr = S_OK;

    return hr;
}

/* remove all filters from the graph */
static
HRESULT remove_filters(const module_data_t *mod, IFilterGraph2 *graph)
{
    HRESULT hr = E_FAIL;

    IEnumFilters *enum_filters = NULL;
    IBaseFilter *filter = NULL;

    if (graph == NULL)
        return E_POINTER;

    hr = IFilterGraph2_EnumFilters(graph, &enum_filters);
    BDA_CKPTR_D(enum_filters, "couldn't enumerate filters in graph");

    do
    {
        ASC_RELEASE(filter);

        hr = IEnumFilters_Next(enum_filters, 1, &filter, NULL);
        ASC_WANT_ENUM(hr, filter);

        if (hr == VFW_E_ENUM_OUT_OF_SYNC)
        {
            hr = IEnumFilters_Reset(enum_filters);
            BDA_CKHR_D("couldn't reset filter enumerator");

            continue;
        }

        if (FAILED(hr))
            BDA_THROW_D("couldn't retrieve next filter in graph");
        else if (hr != S_OK)
            break; /* no more filters */

        hr = IFilterGraph2_RemoveFilter(graph, filter);
        if (FAILED(hr))
            BDA_DEBUG("couldn't remove filter from graph");
    } while (true);

out:
    if (hr == S_FALSE)
        hr = S_OK;

    ASC_RELEASE(enum_filters);

    return hr;
}

/* register the graph in the running object table */
static
HRESULT rot_register(const module_data_t *mod, IFilterGraph2 *graph
                     , DWORD *out)
{
    HRESULT hr = E_FAIL;
    wchar_t wbuf[256] = { L'\0' };

    IRunningObjectTable *rot = NULL;
    IMoniker *moniker = NULL;

    if (graph == NULL || out == NULL)
        return E_POINTER;

    *out = 0;

    /* get ROT interface */
    hr = GetRunningObjectTable(0, &rot);
    BDA_CKPTR_D(rot, "couldn't retrieve the running object table interface");

    /*
     * Create a moniker identifying the graph. The moniker must follow
     * this exact naming convention, otherwise it won't show up in GraphEdt.
     */
    StringCchPrintfW(wbuf, ASC_ARRAY_SIZE(wbuf)
                     , L"FilterGraph %08x pid %08x"
                     , (void *)graph, GetCurrentProcessId());

    hr = CreateItemMoniker(L"!", wbuf, &moniker);
    BDA_CKPTR_D(moniker, "couldn't create moniker for ROT registration");

    /* register filter graph in the table */
    hr = IRunningObjectTable_Register(rot, 0, (IUnknown *)graph, moniker, out);
    BDA_CKHR_D("couldn't submit ROT registration data");

    if (*out == 0)
        hr = E_INVALIDARG;

out:
    ASC_RELEASE(moniker);
    ASC_RELEASE(rot);

    return hr;
}

/* revoke graph's ROT registration */
static
HRESULT rot_unregister(DWORD *reg)
{
    if (reg == NULL)
        return E_POINTER;

    if (*reg == 0)
        return E_INVALIDARG;

    IRunningObjectTable *rot = NULL;
    HRESULT hr = GetRunningObjectTable(0, &rot);
    ASC_WANT_PTR(hr, rot);
    if (FAILED(hr))
        return hr;

    hr = IRunningObjectTable_Revoke(rot, *reg);
    ASC_RELEASE(rot);
    *reg = 0;

    return hr;
}

/* start the graph */
static
HRESULT control_run(const module_data_t *mod, IFilterGraph2 *graph)
{
    HRESULT hr = E_FAIL;

    OAFilterState state = State_Stopped;
    unsigned int tries = 0;

    IMediaControl *control = NULL;

    if (graph == NULL)
        return E_POINTER;

    /* get media control */
    hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaControl
                                      , (void **)&control);
    BDA_CKPTR_D(control, "couldn't query IMediaControl interface");

    /* switch the graph into running state */
    hr = IMediaControl_Run(control);
    BDA_CKHR_D("couldn't switch the graph into running state");

    do
    {
        hr = IMediaControl_GetState(control, 100, &state); /* 100ms */
        BDA_CKHR_D("couldn't retrieve graph state");

        if (hr == S_OK && state == State_Running)
            break;
        else if (tries++ >= 10)
            BDA_THROW_D("timed out waiting for the graph to start");
    } while (true);

out:
    if (control != NULL)
    {
        if (hr != S_OK)
            IMediaControl_StopWhenReady(control);

        IMediaControl_Release(control);
    }

    return hr;
}

/* stop the graph */
static
HRESULT control_stop(IFilterGraph2 *graph)
{
    if (graph == NULL)
        return E_POINTER;

    /* get media control */
    IMediaControl *control = NULL;
    HRESULT hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaControl
                                              , (void **)&control);
    ASC_WANT_PTR(hr, control);
    if (FAILED(hr)) return hr;

    /* asynchronously switch graph into stopped state */
    hr = IMediaControl_StopWhenReady(control);
    ASC_RELEASE(control);

    return hr;
}

/*
 * device data exchange
 */

/* request signal statistics from driver */
static
HRESULT signal_stats_load(module_data_t *mod, bda_signal_stats_t *out)
{
    HRESULT hr = E_FAIL;

    if (out == NULL)
        return E_POINTER;

    memset(out, 0, sizeof(*out));

    hr = mod->signal->lpVtbl->get_SignalLocked(mod->signal, &out->locked);
    BDA_CKHR_D("couldn't retrieve signal lock status");

    hr = mod->signal->lpVtbl->get_SignalPresent(mod->signal, &out->present);
    BDA_CKHR_D("couldn't retrieve signal presence status");

    hr = mod->signal->lpVtbl->get_SignalQuality(mod->signal, &out->quality);
    BDA_CKHR_D("couldn't retrieve signal quality value");

    hr = mod->signal->lpVtbl->get_SignalStrength(mod->signal, &out->strength);
    BDA_CKHR_D("couldn't retrieve signal strength value");

out:
    return hr;
}

/* update last known signal statistics */
static
void signal_stats_set(module_data_t *mod, const bda_signal_stats_t *stats)
{
    asc_mutex_lock(&mod->signal_lock);

    if (stats != NULL)
        memcpy(&mod->signal_stats, stats, sizeof(mod->signal_stats));
    else
        memset(&mod->signal_stats, 0, sizeof(mod->signal_stats));

    asc_mutex_unlock(&mod->signal_lock);
}

/* run saved DiSEqC sequence through available extensions */
static
HRESULT diseqc_sequence_run(module_data_t *mod)
{
    HRESULT hr = S_OK;

    for (unsigned int i = 0; i < mod->diseqc.seq_size; i++)
    {
        const bda_diseqc_seq_t *const seq = &mod->diseqc.seq[i];

        if (seq->data_len > 0)
        {
            hr = bda_ext_diseqc(mod, seq->data, seq->data_len);
            BDA_CKHR_D("couldn't send DiSEqC command");
        }

        if (seq->lnbpower != BDA_EXT_LNBPOWER_NOT_SET)
        {
            hr = bda_ext_lnbpower(mod, seq->lnbpower);
            BDA_CKHR_D("couldn't set LNB power mode");
        }

        if (seq->t22k != BDA_EXT_22K_NOT_SET)
        {
            hr = bda_ext_22k(mod, seq->t22k);
            BDA_CKHR_D("couldn't set 22kHz tone mode");
        }

        if (seq->toneburst != BDA_EXT_TONEBURST_NOT_SET)
        {
            hr = bda_ext_toneburst(mod, seq->toneburst);
            BDA_CKHR_D("couldn't set tone burst mode");
        }

        asc_usleep(BDA_DISEQC_DELAY + seq->delay);
    }

out:
    return hr;
}

// TODO: restore_ca() / CAM reset function

/*
 * graph initialization and cleanup (uses above functions)
 */

static
HRESULT graph_setup(module_data_t *mod)
{
    HRESULT hr = E_FAIL;
    bool need_uninit = false;

    IFilterGraph2 *graph = NULL;
    IMediaEvent *event = NULL;
    HANDLE graph_evt = NULL;
    IBaseFilter *provider = NULL;
    IBaseFilter *source = NULL;
    IBaseFilter *demod = NULL;
    IBaseFilter *capture = NULL;
    IBDA_SignalStatistics *signal = NULL;
    IBaseFilter *probe = NULL;
    IBaseFilter *demux = NULL;
    IBaseFilter *tif = NULL;
    IMPEG2PIDMap *pidmap = NULL;

    /* initialize COM on this thread */
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    BDA_CKHR("CoInitializeEx() failed");
    asc_assert(hr != S_FALSE, MSG("COM initialized twice!"));

    need_uninit = true;

    /* set up graph and event interface */
    hr = dshow_filter_graph(&graph, &event, &graph_evt);
    BDA_CKHR("failed to create filter graph");

    if (mod->debug)
    {
        /* make the graph visible in GraphEdt */
        hr = rot_register(mod, graph, &mod->rot_reg);
        if (FAILED(hr))
            BDA_DEBUG("failed to register the graph in ROT");
    }

    /* set up network provider and source filter */
    bda_tune_cmd_t tune;
    memcpy(&tune, &mod->tune, sizeof(tune));

    if (mod->diseqc.port != BDA_LNB_SOURCE_NOT_DEFINED)
        tune.lnb_source = mod->diseqc.port;

    hr = bda_net_provider(tune.net, &provider);
    BDA_CKHR("failed to create network provider filter");

    hr = IFilterGraph2_AddFilter(graph, provider, L"Network Provider");
    BDA_CKHR("failed to add network provider filter to graph");

    hr = create_source(mod, graph, &source);
    if (FAILED(hr))
        BDA_THROW("failed to create source filter");
    else if (hr != S_OK)
        BDA_THROW("failed to find the requested device");

    hr = provider_setup(mod, provider, source, &tune);
    BDA_CKHR("failed to connect network provider to source filter");

    /* add demodulator and capture filters if this device has them */
    hr = create_receiver(mod, source, &demod);
    BDA_CKHR("failed to create demodulator filter");

    if (demod != NULL)
    {
        hr = create_receiver(mod, demod, &capture);
        BDA_CKHR("failed to create capture filter");

        if (capture == NULL)
        {
            /* only two filters, source and capture */
            capture = demod;
            demod = NULL;
        }
    }

    /* create signal statistics interface */
    hr = find_control_node(mod, source, &KSPROPSETID_BdaSignalStats
                           , &IID_IBDA_SignalStatistics, (void **)&signal);
    BDA_CKHR("failed to search device topology for signal stats");

    if (signal == NULL)
        asc_log_warning(MSG("couldn't find signal statistics interface"));

    if (!mod->no_dvr)
    {
        /* add TS probe */
        IBaseFilter *tail;
        if (capture != NULL)
            tail = capture;
        else
            tail = source;

        if (mod->budget)
        {
            /* insert probe after capture filter (no PID filter) */
            hr = create_probe(mod, tail, mod, &probe);
            BDA_CKHR("failed to create TS probe");

            tail = probe;
        }

        /* set up demultiplexer and TIF */
        hr = create_demux(mod, tail, &demux);
        BDA_CKHR("failed to initialize demultiplexer");

        hr = create_tif(mod, demux, &tif);
        BDA_CKHR("failed to initialize transport information filter");

        if (!mod->budget)
        {
            /* insert probe after demux (PID filter enabled) */
            hr = create_pidmap(mod, demux, &pidmap);
            BDA_CKHR("failed to initialize PID mapper");

            hr = create_probe_dmx(mod, pidmap, mod, &probe);
            BDA_CKHR("failed to create TS probe");

            hr = restore_pids(pidmap, mod->joined_pids);
            if (FAILED(hr))
                BDA_ERROR("failed to load joined PID list into PID mapper");
        }

        /* scan for vendor-specific BDA extensions */
        IBaseFilter *flt_list[4];
        flt_list[0] = source;
        flt_list[1] = capture;
        flt_list[2] = demod;
        flt_list[3] = NULL;

        hr = bda_ext_init(mod, flt_list);
        if (FAILED(hr))
            BDA_ERROR("error while probing for vendor extensions");

        /* call pre-tuning hooks */
        hr = bda_ext_tune(mod, &tune, BDA_TUNE_PRE);
        if (FAILED(hr))
            BDA_ERROR("error while sending extension pre-tuning data");

        /* start the graph */
        hr = control_run(mod, graph);
        BDA_CKHR("failed to run the graph");

        /* call post-tuning hooks */
        hr = bda_ext_tune(mod, &tune, BDA_TUNE_POST);
        if (FAILED(hr))
            BDA_ERROR("error while sending extension post-tuning data");

        /* run stored DiSEqC sequence */
        hr = diseqc_sequence_run(mod);
        if (FAILED(hr))
            BDA_ERROR("error while running DiSEqC command sequence");

        /* TODO: reset CAM state / reset_cam() */

        /* set signal lock timeout */
        mod->tunefail = 0;
        mod->cooldown = mod->timeout;
    }

    signal_stats_set(mod, NULL);

    /* increase refcount on objects of interest */
    IFilterGraph2_AddRef(graph);
    mod->graph = graph;

    IMediaEvent_AddRef(event);
    mod->event = event;
    mod->graph_evt = graph_evt;

    IBaseFilter_AddRef(provider);
    mod->provider = provider;

    if (signal != NULL)
    {
        IBDA_SignalStatistics_AddRef(signal);
        mod->signal = signal;
    }

    if (pidmap != NULL)
    {
        IMPEG2PIDMap_AddRef(pidmap);
        mod->pidmap = pidmap;
    }

    need_uninit = false;
    hr = S_OK;

out:
    if (need_uninit)
    {
        bda_ext_destroy(mod);
        remove_filters(mod, graph);
        rot_unregister(&mod->rot_reg);
    }

    ASC_RELEASE(pidmap);
    ASC_RELEASE(tif);
    ASC_RELEASE(demux);
    ASC_RELEASE(probe);
    ASC_RELEASE(signal);
    ASC_RELEASE(capture);
    ASC_RELEASE(demod);
    ASC_RELEASE(source);
    ASC_RELEASE(provider);
    ASC_RELEASE(event);
    ASC_RELEASE(graph);

    if (need_uninit)
        CoUninitialize();

    return hr;
}

static
void graph_teardown(module_data_t *mod)
{
    control_stop(mod->graph);
    bda_ext_destroy(mod);

    ASC_RELEASE(mod->signal);
    ASC_RELEASE(mod->pidmap);
    ASC_RELEASE(mod->provider);
    ASC_RELEASE(mod->event);

    remove_filters(mod, mod->graph);
    rot_unregister(&mod->rot_reg);
    ASC_RELEASE(mod->graph);

    mod->graph_evt = NULL;
    mod->cooldown = 0;
    signal_stats_set(mod, NULL);

    mod->frag_pos = 0;

    CoUninitialize();
}

/*
 * TS buffering and frame alignment
 *
 * NOTE: these are run by a "data" thread, managed internally by the OS.
 */

/* push single packet to ring buffer */
static
void buffer_push(module_data_t *mod, const uint8_t *ts)
{
    mod->buf.pending++;

    const size_t next = (mod->buf.head + 1) % mod->buf.size;
    if (next == mod->buf.tail)
    {
        mod->buf.dropped++;
        return;
    }

    memcpy(mod->buf.data[next], ts, TS_PACKET_SIZE);
    mod->buf.head = next;
}

/* called by the probe filter when it has media samples */
static
void on_sample(void *arg, const uint8_t *buf, size_t len)
{
    module_data_t *const mod = (module_data_t *)arg;

    asc_mutex_lock(&mod->buf.lock);

    /* reunite packet head and tail */
    if (mod->frag_pos > 0)
    {
        size_t more = TS_PACKET_SIZE - mod->frag_pos;
        if (len < more)
            more = len;

        memcpy(&mod->frag[mod->frag_pos], buf, more);
        mod->frag_pos += more;

        if (mod->frag_pos >= TS_PACKET_SIZE)
        {
            if (TS_IS_SYNC(mod->frag))
                buffer_push(mod, mod->frag);

            mod->frag_pos = 0;
        }

        buf += more;
        len -= more;
    }

    /* push full packets */
    while (len >= TS_PACKET_SIZE)
    {
        if (TS_IS_SYNC(buf))
        {
            buffer_push(mod, buf);

            buf += TS_PACKET_SIZE;
            len -= TS_PACKET_SIZE;
        }
        else
        {
            buf++;
            len--;
        }
    }

    asc_mutex_unlock(&mod->buf.lock);

    /* put remainder into frag storage */
    if (len > 0)
    {
        memcpy(mod->frag, buf, len);
        mod->frag_pos = len;
    }

    /* ask main thread to dequeue */
    if (mod->buf.pending >= BDA_BUFFER_THRESH)
    {
        asc_job_queue(mod, bda_buffer_pop, mod);
        asc_wake();

        mod->buf.pending = 0;
    }
}

/*
 * runtime graph control
 */

/* stop the graph, submit tuning data and start it again */
static
HRESULT restart_tuning(module_data_t *mod)
{
    HRESULT hr = E_FAIL;

    /*
     * NOTE: Depending on the driver, a graph restart might be needed
     *       for the tuning process to actually begin.
     */

    hr = control_stop(mod->graph);
    BDA_CKHR_D("couldn't stop the graph");

    /* same tuning routine as graph_setup() */
    bda_tune_cmd_t tune;
    memcpy(&tune, &mod->tune, sizeof(tune));

    if (mod->diseqc.port != BDA_LNB_SOURCE_NOT_DEFINED)
        tune.lnb_source = mod->diseqc.port;

    hr = provider_tune(mod, mod->provider, &tune);
    BDA_CKHR_D("couldn't configure provider with tuning data");

    hr = bda_ext_tune(mod, &tune, BDA_TUNE_PRE);
    if (FAILED(hr))
        BDA_ERROR("error while sending extension pre-tuning data");

    hr = control_run(mod, mod->graph);
    BDA_CKHR_D("couldn't run the graph");

    hr = bda_ext_tune(mod, &tune, BDA_TUNE_POST);
    if (FAILED(hr))
        BDA_ERROR("error while sending extension post-tuning data");

    hr = diseqc_sequence_run(mod);
    if (FAILED(hr))
        BDA_ERROR("error while running DiSEqC command sequence");

    /* TODO: reset CAM / reset_cam() */
    // XXX: rejoin pids?

    /* reset signal lock timeout */
    mod->cooldown = mod->timeout;
    signal_stats_set(mod, NULL);

out:
    return hr;
}

/* react to change in signal status */
static
HRESULT watch_signal(module_data_t *mod)
{
    HRESULT hr = E_FAIL;

    bda_signal_stats_t s;
    const char *str = NULL;

    hr = signal_stats_load(mod, &s);
    BDA_CKHR("failed to retrieve signal statistics from driver");

    if (!mod->no_dvr)
    {
        /* continuously tune the device until signal lock is acquired */
        if (s.locked && !mod->signal_stats.locked)
        {
            str = " acquired";
            mod->cooldown = 0;
        }
        else if (mod->signal_stats.locked && !s.locked)
        {
            str = " lost";
            mod->cooldown = mod->timeout;
            mod->tunefail++;
        }
        else if (!s.locked && --mod->cooldown <= 0)
        {
            /* time's up, still no lock */
            asc_log_debug(MSG("resending tuning data (%u)")
                          , ++mod->tunefail);

            if (mod->tunefail == 1)
                str = " no"; /* always report first tuning failure */

            hr = restart_tuning(mod);
            BDA_CKHR("failed to restart tuning process");
        }
    }

    /* log signal status */
    if (mod->log_signal && str == NULL)
        str = (s.locked ? "" : " no");

    if (str != NULL)
    {
        asc_log_info(MSG("tuner has%s lock. status: %c%c, "
                         "quality: %ld%%, strength: %ld%%")
                     , str, s.present ? 'S' : '_'
                     , s.locked ? 'L' : '_'
                     , s.quality, s.strength);
    }

    signal_stats_set(mod, &s);

out:
    return hr;
}

/* list of events to be treated as errors */
static inline __func_const
const char *event_text(long ec)
{
    switch (ec)
    {
        case EC_COMPLETE:
            return "all data has been rendered";

        case EC_USERABORT:
            return "user has terminated playback";

        case EC_ERRORABORT:
        case EC_ERRORABORTEX:
            return "operation aborted due to an error";

        case EC_STREAM_ERROR_STOPPED:
            return "stream stopped due to an error";

        case EC_ERROR_STILLPLAYING:
            return "command to run the graph has failed";

        case EC_PAUSED:
            return "pause request has completed";

        case EC_END_OF_SEGMENT:
            return "end of a segment was reached";

        case EC_DEVICE_LOST:
            return "device was removed";

        case EC_PLEASE_REOPEN:
            return "source file has changed";

        case EC_FILE_CLOSED:
            return "source file was closed";

        case EC_VMR_RECONNECTION_FAILED:
            return "VMR reconnection failed";
    }

    return NULL;
}

/* service graph event queue */
static
HRESULT handle_events(module_data_t *mod)
{
    // XXX: does hotplug work with no_dvr ?
    HRESULT hr = E_FAIL;

    do
    {
        const char *ev_text = NULL;
        long ec;
        LONG_PTR p1, p2;

        /* wait for an event (50ms timeout) */
        hr = IMediaEvent_GetEvent(mod->event, &ec, &p1, &p2, 50);
        if (hr == E_ABORT)
        {
            /* no more events */
            hr = S_OK;
            break;
        }
        else if (hr != S_OK)
        {
            BDA_THROW("failed to retrieve next graph event");
        }

        /* bail out if the event indicates an error */
        ev_text = event_text(ec);
        if (ev_text == NULL)
            asc_log_debug(MSG("ignoring unknown event: 0x%02lx"), ec);

        hr = IMediaEvent_FreeEventParams(mod->event, ec, p1, p2);
        BDA_CKHR("failed to free event parameters");

        if (ev_text != NULL)
            BDA_THROW("unexpected event: %s (0x%02lx)", ev_text, ec);
    } while (true);

out:
    return hr;
}

/* wait for graph event or user command */
static
void wait_events(module_data_t *mod)
{
    HANDLE ev[2] = { mod->queue_evt, NULL };
    DWORD ev_cnt = 1;

    if (mod->graph_evt != NULL)
        ev[ev_cnt++] = mod->graph_evt;

    /* wait up to 1 second */
    const DWORD ret = WaitForMultipleObjects(ev_cnt, ev, FALSE, 1000);
    asc_assert(ret != WAIT_FAILED, MSG("event wait failed: %s")
               , asc_error_msg());
}

/* list of state enum strings */
static inline __func_const
const char *state_name(bda_state_t state)
{
    switch (state)
    {
        case BDA_STATE_INIT:    return "INIT";
        case BDA_STATE_RUNNING: return "RUNNING";
        case BDA_STATE_STOPPED: return "STOPPED";
        case BDA_STATE_ERROR:   return "ERROR";

        default:
            return "UNKNOWN";
    }
}

/* set new module state */
static
void set_state(module_data_t *mod, bda_state_t state)
{
    if (mod->state == state)
        return;

    asc_log_debug(MSG("setting state to %s"), state_name(state));
    mod->state = state;

    if (state == BDA_STATE_ERROR)
    {
        /* NOTE: when in an error state, CD timer counts down to reinit */
        asc_log_info(MSG("reopening device in %u seconds"), BDA_REINIT_TICKS);
        mod->cooldown = BDA_REINIT_TICKS;
    }
}

/*
 * user commands
 */

/* set tuning data, opening the device if necessary */
static
void cmd_tune(module_data_t *mod, const bda_tune_cmd_t *tune)
{
    memcpy(&mod->tune, tune, sizeof(*tune));

    if (mod->state != BDA_STATE_RUNNING)
    {
        /* schedule device initialization */
        set_state(mod, BDA_STATE_INIT);
        SetEvent(mod->queue_evt);
    }
    else if (!mod->no_dvr)
    {
        /* apply new configuration */
        const HRESULT hr = restart_tuning(mod);

        if (FAILED(hr))
        {
            BDA_ERROR("failed to reconfigure device with new tuning data");

            graph_teardown(mod);
            set_state(mod, BDA_STATE_ERROR);
        }
        else
        {
            /* reset tuning failure counter */
            mod->tunefail = 0;
        }
    }
}

/* request demultiplexer to join or leave PID */
static
void cmd_pid(module_data_t *mod, bool join, uint16_t pid)
{
    mod->joined_pids[pid] = join;

    if (mod->pidmap != NULL)
    {
        ULONG val = pid;
        HRESULT hr;

        if (join)
        {
            hr = IMPEG2PIDMap_MapPID(mod->pidmap, 1, &val
                                     , MEDIA_TRANSPORT_PACKET);
        }
        else
        {
            hr = IMPEG2PIDMap_UnmapPID(mod->pidmap, 1, &val);
        }

        if (FAILED(hr))
        {
            BDA_ERROR("failed to %s pid %hu"
                      , (join ? "join" : "leave"), pid);
        }
    }
}

/* enable or disable CAM descrambling for a specific program */
static
void cmd_ca(module_data_t *mod, bool enable, uint16_t pnr)
{
    mod->ca_pmts[pnr] = enable;

    /* TODO: implement CAM support */
    asc_log_error(MSG("STUB: %s CAM for PNR %hu")
                  , (enable ? "enable" : "disable"), pnr);

    // TODO: call bda_ext_ca()
    // check for state & !no_dvr
}

/* apply user DiSEqC setting */
static
void cmd_diseqc(module_data_t *mod, const bda_diseqc_cmd_t *diseqc)
{
    memcpy(&mod->diseqc, diseqc, sizeof(*diseqc));

    if (mod->state != BDA_STATE_RUNNING || mod->no_dvr)
        return;

    if (mod->diseqc.port != BDA_LNB_SOURCE_NOT_DEFINED)
    {
        /* DiSEqC 1.0 port number; have to send it as part of tuning data */
        const HRESULT hr = restart_tuning(mod);
        if (FAILED(hr))
        {
            BDA_ERROR("failed to change DiSEqC port");

            graph_teardown(mod);
            set_state(mod, BDA_STATE_ERROR);
        }
    }
    else
    {
        /* array of DiSEqC commands */
        const HRESULT hr = diseqc_sequence_run(mod);
        if (FAILED(hr))
            BDA_ERROR("error while running DiSEqC command sequence");
    }
}

/* execute user command */
static
void execute_cmd(module_data_t *mod, const bda_user_cmd_t *cmd)
{
    switch (cmd->cmd)
    {
        case BDA_COMMAND_TUNE:
            cmd_tune(mod, &cmd->tune);
            break;

        case BDA_COMMAND_DEMUX:
            cmd_pid(mod, cmd->demux.join, cmd->demux.pid);
            break;

        case BDA_COMMAND_CA:
            cmd_ca(mod, cmd->ca.enable, cmd->ca.pnr);
            break;

        case BDA_COMMAND_DISEQC:
            cmd_diseqc(mod, &cmd->diseqc);
            break;

        case BDA_COMMAND_QUIT:
        case BDA_COMMAND_CLOSE:
        default:
            graph_teardown(mod);
            set_state(mod, BDA_STATE_STOPPED);
            break;
    }
}

/*
 * thread loop
 */

void bda_graph_loop(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;
    bool quit = false;

    asc_log_debug(MSG("control thread started"));
    mod->state = BDA_STATE_STOPPED;

    do
    {
        /* run queued user commands */
        asc_mutex_lock(&mod->queue_lock);
        asc_list_clear(mod->queue)
        {
            bda_user_cmd_t *const item =
                (bda_user_cmd_t *)asc_list_data(mod->queue);

            /* execute command with mutex unlocked */
            asc_mutex_unlock(&mod->queue_lock);

            if (item->cmd == BDA_COMMAND_QUIT)
                quit = true;

            execute_cmd(mod, item);
            free(item);

            asc_mutex_lock(&mod->queue_lock);
        }
        asc_mutex_unlock(&mod->queue_lock);

        if (quit)
            break;

        /* handle state change */
        switch (mod->state)
        {
            case BDA_STATE_INIT:
            {
                const HRESULT hr = graph_setup(mod);
                if (SUCCEEDED(hr))
                    set_state(mod, BDA_STATE_RUNNING);
                else
                    set_state(mod, BDA_STATE_ERROR);

                break;
            }

            case BDA_STATE_RUNNING:
            {
                HRESULT hr = handle_events(mod);

                if (SUCCEEDED(hr) && mod->signal != NULL)
                    hr = watch_signal(mod);

                if (FAILED(hr))
                {
                    graph_teardown(mod);
                    set_state(mod, BDA_STATE_ERROR);
                }

                break;
            }

            case BDA_STATE_ERROR:
            {
                if (--mod->cooldown <= 0)
                    set_state(mod, BDA_STATE_INIT);

                break;
            }

            case BDA_STATE_STOPPED:
            default:
            {
                /* do nothing */
                break;
            }
        }

        /* sleep until next event */
        wait_events(mod);
    } while (true);

    asc_log_debug(MSG("control thread exiting"));
}
