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

#define MSG(_msg) "[dvb_input %s] " _msg, mod->name

/* device reopen timeout, seconds */
#define ERROR_CD_TICKS 10

/*
 * error handling (a.k.a. the joys of working with COM in plain C)
 */

/* logging voodoo */
static __fmt_printf(4, 5)
void log_hr(const module_data_t *mod, HRESULT hr
            , asc_log_type_t level, const char *msg, ...)
{
    char fmt[2048] = { '\0' };
    int ret = 0;

    /* cook up a format string */
    if (SUCCEEDED(hr))
    {
        /* success codes won't format properly, nor is there a need */
        ret = snprintf(fmt, sizeof(fmt), MSG("%s"), msg);
    }
    else
    {
        /* append formatted HRESULT to error message */
        char *const err = dshow_error_msg(hr);
        ret = snprintf(fmt, sizeof(fmt), MSG("%s: %s"), msg, err);
        free(err);
    }

    if (ret > 0)
    {
        va_list ap;
        va_start(ap, msg);
        asc_log_va(level, fmt, ap);
        va_end(ap);
    }
}

/* log formatted error message */
#define __BDA_LOG(_level, ...) \
    do { log_hr(mod, hr, _level, __VA_ARGS__); } while (0)

#define BDA_ERROR(...) __BDA_LOG(ASC_LOG_ERROR, __VA_ARGS__)
#define BDA_DEBUG(...) __BDA_LOG(ASC_LOG_DEBUG, __VA_ARGS__)

/* go to cleanup, unconditionally */
#define __BDA_THROW(_level, ...) \
    do { \
        __BDA_LOG(_level, __VA_ARGS__); \
        if (SUCCEEDED(hr)) \
            hr = E_FAIL; \
        goto out; \
    } while (0)

#define BDA_THROW(...) __BDA_THROW(ASC_LOG_ERROR, __VA_ARGS__)
#define BDA_THROW_D(...) __BDA_THROW(ASC_LOG_DEBUG, __VA_ARGS__)

/* go to cleanup if HRESULT indicates failure */
#define __BDA_CHECK_HR(_level, ...) \
    do { \
        if (FAILED(hr)) \
            __BDA_THROW(_level, __VA_ARGS__); \
    } while (0)

#define BDA_CHECK_HR(...) __BDA_CHECK_HR(ASC_LOG_ERROR, __VA_ARGS__)
#define BDA_CHECK_HR_D(...) __BDA_CHECK_HR(ASC_LOG_DEBUG, __VA_ARGS__)

/* go to cleanup if a NULL pointer is detected */
#define __BDA_CKPTR(_level, _ptr, ...) \
    do { \
        if (SUCCEEDED(hr) && _ptr == NULL) \
            hr = E_POINTER; \
        __BDA_CHECK_HR(_level, __VA_ARGS__); \
    } while (0)

#define BDA_CKPTR(_ptr, ...) __BDA_CKPTR(ASC_LOG_ERROR, _ptr, __VA_ARGS__)
#define BDA_CKPTR_D(_ptr, ...) __BDA_CKPTR(ASC_LOG_DEBUG, _ptr, __VA_ARGS__)

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

    BDA_CHECK_HR_D("couldn't add source filter to graph");

    IBaseFilter_AddRef(source);
    *out = source;

out:
    ASC_FREE(wbuf, free);
    ASC_FREE(fname, free);

    SAFE_RELEASE(source);

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
    BDA_CHECK_HR_D("couldn't get source filter's graph");

    hr = dshow_find_pin(source, PINDIR_OUTPUT, true, NULL, &source_out);
    BDA_CHECK_HR_D("couldn't find output pin on source filter");

    /* list possible candidates for attaching to source filter */
    hr = dshow_enum(&KSCATEGORY_BDA_RECEIVER_COMPONENT, &enum_moniker);
    if (FAILED(hr))
        BDA_THROW_D("couldn't enumerate BDA receiver filters");
    else if (hr != S_OK)
        goto out; /* no receivers installed */

    do
    {
        SAFE_RELEASE(rcv_in);
        SAFE_RELEASE(rcv);
        SAFE_RELEASE(moniker);

        ASC_FREE(fname, free);
        ASC_FREE(wbuf, free);

        if (*out != NULL)
            break;

        hr = IEnumMoniker_Next(enum_moniker, 1, &moniker, NULL);
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
    SAFE_RELEASE(enum_moniker);
    SAFE_RELEASE(source_out);
    SAFE_RELEASE(graph);

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
    BDA_CHECK_HR_D("couldn't get capture filter's graph");

    hr = CoCreateInstance(&CLSID_MPEG2Demultiplexer, NULL, CLSCTX_INPROC
                          , &IID_IBaseFilter, (void **)&demux);
    BDA_CKPTR_D(demux, "couldn't create demultiplexer filter");

    hr = dshow_find_pin(tail, PINDIR_OUTPUT, true, NULL, &tail_out);
    BDA_CHECK_HR_D("couldn't find output pin on capture filter");

    hr = dshow_find_pin(demux, PINDIR_INPUT, true, NULL, &demux_in);
    BDA_CHECK_HR_D("couldn't find input pin on demultiplexer filter");

    hr = IFilterGraph2_AddFilter(graph, demux, L"Demux");
    BDA_CHECK_HR_D("couldn't add demultiplexer to the graph");

    hr = IFilterGraph2_ConnectDirect(graph, tail_out, demux_in, NULL);
    BDA_CHECK_HR_D("couldn't connect capture filter to demultiplexer");

    IBaseFilter_AddRef(demux);
    *out = demux;

out:
    SAFE_RELEASE(demux_in);
    SAFE_RELEASE(tail_out);
    SAFE_RELEASE(demux);
    SAFE_RELEASE(graph);

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
    BDA_CHECK_HR_D("couldn't get demultiplexer's graph");

    /* create first filter from the TIF category */
    hr = dshow_filter_by_index(&KSCATEGORY_BDA_TRANSPORT_INFORMATION, 0
                               , &tif, NULL);
    BDA_CHECK_HR_D("couldn't instantiate transport information filter");

    /* connect TIF to demux */
    hr = dshow_find_pin(tif, PINDIR_INPUT, true, NULL, &tif_in);
    BDA_CHECK_HR_D("couldn't find input pin on TIF");

    hr = dshow_find_pin(demux, PINDIR_OUTPUT, true, NULL, &demux_out);
    BDA_CHECK_HR_D("couldn't find output pin on demultiplexer");

    hr = IFilterGraph2_AddFilter(graph, tif, L"TIF");
    BDA_CHECK_HR_D("couldn't add transport information filter to graph");

    hr = IFilterGraph2_ConnectDirect(graph, demux_out, tif_in, NULL);
    BDA_CHECK_HR_D("couldn't connect TIF to demultiplexer");

    IBaseFilter_AddRef(tif);
    *out = tif;

out:
    SAFE_RELEASE(demux_out);
    SAFE_RELEASE(tif_in);
    SAFE_RELEASE(tif);
    SAFE_RELEASE(graph);

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
    SAFE_RELEASE(mpeg_out);
    SAFE_RELEASE(mpeg);

    return hr;
}

/* create TS probe and connect it directly to the capture filter */
static
void on_sample(void *arg, const void *buf, size_t len);

static
HRESULT create_probe(module_data_t *mod, IBaseFilter *tail, IBaseFilter **out)
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
    BDA_CHECK_HR_D("couldn't get capture filter's graph");

    hr = dshow_find_pin(tail, PINDIR_OUTPUT, true, NULL, &tail_out);
    BDA_CHECK_HR_D("couldn't find output pin on capture filter");

    /* try creating and attaching probes with different media subtypes */
    for (unsigned int i = 0; ; i++)
    {
        SAFE_RELEASE(probe);
        SAFE_RELEASE(probe_in);

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
        hr = dshow_grabber(on_sample, mod, &mt, &probe);
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
    BDA_CHECK_HR_D("couldn't connect TS probe to capture filter");

out:
    SAFE_RELEASE(tail_out);
    SAFE_RELEASE(graph);

    return hr;
}

/* create TS probe and connect it to demux PID mapper */
static
HRESULT create_probe_dmx(module_data_t *mod, IMPEG2PIDMap *pidmap
                         , IBaseFilter **out)
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
    BDA_CHECK_HR_D("couldn't get demultiplexer's graph");

    /* create probe and attach it to demultiplexer pin */
    AM_MEDIA_TYPE mt;
    memset(&mt, 0, sizeof(mt));
    mt.majortype = MEDIATYPE_Stream;
    mt.subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;

    hr = dshow_grabber(on_sample, mod, &mt, &probe);
    BDA_CHECK_HR_D("couldn't instantiate TS probe filter");

    hr = dshow_find_pin(probe, PINDIR_INPUT, true, NULL, &probe_in);
    BDA_CHECK_HR_D("couldn't find input pin on TS probe");

    hr = IFilterGraph2_AddFilter(graph, probe, L"Probe");
    BDA_CHECK_HR_D("couldn't add TS probe to graph");

    hr = IFilterGraph2_ConnectDirect(graph, demux_out, probe_in, NULL);
    BDA_CHECK_HR_D("couldn't connect TS probe to demultiplexer");

    IBaseFilter_AddRef(probe);
    *out = probe;

out:
    SAFE_RELEASE(probe_in);
    SAFE_RELEASE(probe);
    SAFE_RELEASE(graph);
    SAFE_RELEASE(demux);
    SAFE_RELEASE(demux_out);

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
    BDA_CHECK_HR_D("couldn't retrieve list of topology node types");

    for (unsigned int i = 0; i < node_types_cnt; i++)
    {
        /* list interfaces for each node type */
        memset(&node_intf, 0, sizeof(node_intf));
        hr = IBDA_Topology_GetNodeInterfaces(topology, node_types[i]
                                             , &node_intf_cnt
                                             , ASC_ARRAY_SIZE(node_intf)
                                             , node_intf);
        BDA_CHECK_HR_D("couldn't retrieve list of node interfaces");

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
    SAFE_RELEASE(node);
    SAFE_RELEASE(topology);

    return hr;
}

/* request signal statistics from driver */
static
HRESULT signal_stats_load(const module_data_t *mod
                          , IBDA_SignalStatistics *signal
                          , bda_signal_stats_t *out)
{
    HRESULT hr = E_FAIL;

    if (signal == NULL || out == NULL)
        return E_POINTER;

    memset(out, 0, sizeof(*out));

    hr = signal->lpVtbl->get_SignalLocked(signal, &out->locked);
    BDA_CHECK_HR_D("couldn't retrieve signal lock status");

    hr = signal->lpVtbl->get_SignalPresent(signal, &out->present);
    BDA_CHECK_HR_D("couldn't retrieve signal presence status");

    hr = signal->lpVtbl->get_SignalQuality(signal, &out->quality);
    BDA_CHECK_HR_D("couldn't retrieve signal quality value");

    hr = signal->lpVtbl->get_SignalStrength(signal, &out->strength);
    BDA_CHECK_HR_D("couldn't retrieve signal strength value");

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

/* submit tune request to network provider */
static
HRESULT provider_tune(const module_data_t *mod, IBaseFilter *provider)
{
    HRESULT hr = E_FAIL;

    ITuneRequest *request = NULL;
    ITuningSpace *space = NULL;
    ITuner *provider_tuner = NULL;

    if (provider == NULL)
        return E_POINTER;

    /* create tune request from user data */
    hr = bda_tune_request(&mod->tune, &request);
    BDA_CHECK_HR_D("couldn't create tune request");

    if (mod->debug)
        bda_dump_request(request);

    hr = ITuneRequest_get_TuningSpace(request, &space);
    BDA_CKPTR_D(space, "couldn't retrieve tuning space");

    hr = IBaseFilter_QueryInterface(provider, &IID_ITuner
                                    , (void **)&provider_tuner);
    BDA_CKPTR_D(provider_tuner, "couldn't query ITuner interface");

    hr = ITuner_put_TuningSpace(provider_tuner, space);
    BDA_CHECK_HR_D("couldn't assign tuning space to provider");

    hr = ITuner_put_TuneRequest(provider_tuner, request);
    BDA_CHECK_HR_D("couldn't submit tune request to provider");

out:
    SAFE_RELEASE(provider_tuner);
    SAFE_RELEASE(space);
    SAFE_RELEASE(request);

    return hr;
}

/* connect network provider to the source filter */
static
HRESULT provider_setup(const module_data_t *mod, IBaseFilter *provider
                       , IBaseFilter *source)
{
    HRESULT hr = E_FAIL;
    bool retry_pins = false;

    IFilterGraph2 *graph = NULL;
    IPin *provider_out = NULL;
    IPin *source_in = NULL;

    if (provider == NULL || source == NULL)
        return E_POINTER;

    /* get provider's graph */
    hr = dshow_get_graph(provider, &graph);
    BDA_CHECK_HR_D("couldn't get network provider's graph");

    /* get filters' pins */
    hr = dshow_find_pin(provider, PINDIR_OUTPUT, true, NULL, &provider_out);
    BDA_CHECK_HR_D("couldn't find output pin on network provider filter");

    hr = dshow_find_pin(source, PINDIR_INPUT, true, NULL, &source_in);
    BDA_CHECK_HR_D("couldn't find input pin on source filter");

    /* connect pins and submit tune request to provider */
    hr = IFilterGraph2_ConnectDirect(graph, provider_out, source_in, NULL);
    if (FAILED(hr))
    {
        /*
         * NOTE: With legacy providers, we have to submit a tune request
         *       before connecting pins.
         */
        retry_pins = true;
    }

    hr = provider_tune(mod, provider);
    BDA_CHECK_HR_D("couldn't configure provider with initial tuning data");

    if (retry_pins)
    {
        hr = IFilterGraph2_ConnectDirect(graph, provider_out, source_in, NULL);
        BDA_CHECK_HR_D("couldn't connect network provider to tuner");
    }

out:
    SAFE_RELEASE(source_in);
    SAFE_RELEASE(provider_out);
    SAFE_RELEASE(graph);

    return hr;
}

/* load saved PID list into demultiplexer's PID mapper */
static
HRESULT restore_pids(const module_data_t *mod, IMPEG2PIDMap *pidmap)
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

    SAFE_RELEASE(enum_pid);

    /* create and submit PID array */
    ULONG pids[TS_MAX_PID] = { 0 };
    unsigned int cnt = 0;

    for (unsigned int i = 0; i < ASC_ARRAY_SIZE(pids); i++)
    {
        if (mod->joined_pids[i])
            pids[cnt++] = i;
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
    if (FAILED(hr))
        BDA_THROW_D("couldn't enumerate filters in graph");
    else if (hr != S_OK)
        goto out; /* empty graph */

    do
    {
        SAFE_RELEASE(filter);

        hr = IEnumFilters_Next(enum_filters, 1, &filter, NULL);
        if (hr == VFW_E_ENUM_OUT_OF_SYNC)
        {
            hr = IEnumFilters_Reset(enum_filters);
            BDA_CHECK_HR_D("couldn't reset filter enumerator");

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

    SAFE_RELEASE(enum_filters);

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
    BDA_CHECK_HR_D("couldn't submit ROT registration data");

    if (*out == 0)
        hr = E_INVALIDARG;

out:
    SAFE_RELEASE(moniker);
    SAFE_RELEASE(rot);

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
    if (FAILED(hr))
        return hr;
    else if (rot == NULL)
        return E_POINTER;

    hr = IRunningObjectTable_Revoke(rot, *reg);
    SAFE_RELEASE(rot);
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
    BDA_CHECK_HR_D("couldn't switch the graph into running state");

    do
    {
        hr = IMediaControl_GetState(control, 100, &state); /* 100ms */
        BDA_CHECK_HR_D("couldn't retrieve graph state");

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
    if (FAILED(hr))
        return hr;
    else if (control == NULL)
        return E_POINTER;

    /* asynchronously switch graph into stopped state */
    hr = IMediaControl_StopWhenReady(control);
    SAFE_RELEASE(control);

    return hr;
}

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
    BDA_CHECK_HR("CoInitializeEx() failed");
    asc_assert(hr != S_FALSE, MSG("COM initialized twice!"));

    need_uninit = true;

    /* create filter graph */
    hr = CoCreateInstance(&CLSID_FilterGraphNoThread, NULL, CLSCTX_INPROC
                          , &IID_IFilterGraph2, (void **)&graph);
    BDA_CKPTR(graph, "failed to create filter graph");

    hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaEvent
                                      , (void **)&event);
    BDA_CKPTR(event, "failed to query IMediaEvent interface");

    hr = IMediaEvent_GetEventHandle(event, (OAEVENT *)&graph_evt);
    BDA_CKPTR(graph_evt, "failed to retrieve graph's event handle");

    if (mod->debug)
    {
        /* make the graph visible in GraphEdt */
        hr = rot_register(mod, graph, &mod->rot_reg);
        if (FAILED(hr))
            BDA_DEBUG("failed to register the graph in ROT");
    }

    /* set up network provider and source filter */
    hr = bda_net_provider(mod->tune.net, &provider);
    BDA_CHECK_HR("failed to create network provider filter");

    hr = IFilterGraph2_AddFilter(graph, provider, L"Network Provider");
    BDA_CHECK_HR("failed to add network provider filter to graph");

    hr = create_source(mod, graph, &source);
    if (FAILED(hr))
        BDA_THROW("failed to create source filter");
    else if (hr != S_OK)
        BDA_THROW("failed to find the requested device");

    hr = provider_setup(mod, provider, source);
    BDA_CHECK_HR("failed to connect network provider to source filter");

    /* add demodulator and capture filters if this device has them */
    hr = create_receiver(mod, source, &demod);
    BDA_CHECK_HR("failed to create demodulator filter");

    if (demod != NULL)
    {
        hr = create_receiver(mod, demod, &capture);
        BDA_CHECK_HR("failed to create capture filter");

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
    BDA_CHECK_HR("failed to search device topology for signal stats");

    if (signal == NULL)
        asc_log_warning(MSG("couldn't find signal statistics interface"));

    if (!mod->no_dvr)
    {
        /* add TS probe (no PID filtering) */
        IBaseFilter *tail;
        if (capture != NULL)
            tail = capture;
        else
            tail = source;

        if (mod->budget)
        {
            /* insert probe between capture filter and demux */
            hr = create_probe(mod, tail, &probe);
            BDA_CHECK_HR("failed to create TS probe");

            tail = probe;
        }

        /* set up demultiplexer and TIF */
        hr = create_demux(mod, tail, &demux);
        BDA_CHECK_HR("failed to initialize demultiplexer");

        hr = create_tif(mod, demux, &tif);
        BDA_CHECK_HR("failed to initialize transport information filter");

        /* add TS probe (PID filtering enabled) */
        if (!mod->budget)
        {
            hr = create_pidmap(mod, demux, &pidmap);
            BDA_CHECK_HR("failed to initialize PID mapper");

            hr = create_probe_dmx(mod, pidmap, &probe);
            BDA_CHECK_HR("failed to create TS probe");

            hr = restore_pids(mod, pidmap);
            if (FAILED(hr))
                BDA_ERROR("failed to load joined PID list into PID mapper");
        }

        // -------
        // TODO
        // -------
        // - samplegrabber with callbacks
        //   - how to handle buffering?
        //     - buffer in callback is not necessarily aligned on
        //       TS packet boundary
        // -------

        // TODO: call bda_ext_init()
        //       probe extensions on source/demod/capture
        //       CAM, diseq, etc

        // TODO: call bda_ext_tune()

        /* start the graph */
        hr = control_run(mod, graph);
        BDA_CHECK_HR("failed to run the graph");

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
        // TODO: bda_ext_destroy()
        rot_unregister(&mod->rot_reg);
        remove_filters(mod, graph);
    }

    SAFE_RELEASE(pidmap);
    SAFE_RELEASE(tif);
    SAFE_RELEASE(demux);
    SAFE_RELEASE(probe);
    SAFE_RELEASE(signal);
    SAFE_RELEASE(capture);
    SAFE_RELEASE(demod);
    SAFE_RELEASE(source);
    SAFE_RELEASE(provider);
    SAFE_RELEASE(event);
    SAFE_RELEASE(graph);

    if (need_uninit)
        CoUninitialize();

    return hr;
}

static
void graph_teardown(module_data_t *mod)
{
    control_stop(mod->graph);

    // TODO: bda_ext_destroy()
    rot_unregister(&mod->rot_reg);
    remove_filters(mod, mod->graph);

    SAFE_RELEASE(mod->signal);
    SAFE_RELEASE(mod->pidmap);
    SAFE_RELEASE(mod->provider);
    SAFE_RELEASE(mod->event);
    SAFE_RELEASE(mod->graph);

    mod->graph_evt = NULL;
    mod->cooldown = 0;
    signal_stats_set(mod, NULL);

    CoUninitialize();
}

/*
 * TS buffering
 */

// TODO: on_pat, on_pmt for CAM
//       ! if mod->extension_flags & BDA_EXTENSION_CA
//       called from on_sample via mpegts_psi_t
//       on_pmt queues CA user cmd w/pid list to control thread
//       ctl thread talks to CAM via vendor extension

/* called by the probe filter when it has media samples */
static
void on_sample(void *arg, const void *buf, size_t len)
{
    module_data_t *const mod = (module_data_t *)arg;

    /* TODO: implement buffering */
    __uarg(mod);
    __uarg(buf);
    __uarg(len);

    /* NOTE: this callback is NOT run by the control thread! */
    // XXX: is it always the same thread?
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
    BDA_CHECK_HR_D("couldn't stop the graph");

    hr = provider_tune(mod, mod->provider);
    BDA_CHECK_HR_D("couldn't configure provider with tuning data");

    // XXX: do we need to rejoin PIDs and reissue diseqc cmds?

    // TODO: call bda_ext_tune()

    hr = control_run(mod, mod->graph);
    BDA_CHECK_HR_D("couldn't restart the graph");

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

    hr = signal_stats_load(mod, mod->signal, &s);
    BDA_CHECK_HR("failed to retrieve signal statistics from driver");

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
            BDA_CHECK_HR("failed to restart tuning process");
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
        BDA_CHECK_HR("failed to free event parameters");

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
        asc_log_info(MSG("reopening device in %u seconds"), ERROR_CD_TICKS);
        mod->cooldown = ERROR_CD_TICKS;
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
}

/*
static
void cmd_diseqc(mod, command sequence)
{
    mod->sequence = sequence

    call bda_ext_diseqc()
}
*/

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
            /*
             * TODO: implement sending diseqc commands
             *
             * cmd_diseqc(mod, &cmd->diseqc);
             */
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
