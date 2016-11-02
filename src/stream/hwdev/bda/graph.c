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
#define COOLDOWN_TICKS 10

/*
 * error handling (a.k.a. the joys of working with COM in plain C)
 */

/* logging voodoo */
static __fmt_printf(4, 5)
void log_hr(const module_data_t *mod, HRESULT hr
            , asc_log_type_t level, const char *msg, ...)
{
    char fmt[2048] = { 0 };
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

/* go to cleanup, unconditionally */
#define BDA_THROW(...) \
    do { \
        log_hr(mod, hr, ASC_LOG_ERROR, __VA_ARGS__); \
        if (SUCCEEDED(hr)) \
            hr = E_FAIL; \
        goto out; \
    } while (0)

#define BDA_THROW_D(...) \
    do { \
        log_hr(mod, hr, ASC_LOG_DEBUG, __VA_ARGS__); \
        if (SUCCEEDED(hr)) \
            hr = E_FAIL; \
        goto out; \
    } while (0)

/* go to cleanup if HRESULT indicates failure */
#define BDA_CHECK_HR(...) \
    do { \
        if (FAILED(hr)) \
            BDA_THROW(__VA_ARGS__); \
    } while (0)

#define BDA_CHECK_HR_D(...) \
    do { \
        if (FAILED(hr)) \
            BDA_THROW_D(__VA_ARGS__); \
    } while (0)

/* log formatted error message */
#define BDA_ERROR(...) \
    do { \
        log_hr(mod, hr, ASC_LOG_ERROR, __VA_ARGS__); \
    } while (0)

#define BDA_ERROR_D(...) \
    do { \
        log_hr(mod, hr, ASC_LOG_DEBUG, __VA_ARGS__); \
    } while (0)

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

    wbuf = cx_widen(fname); /* name is non-essential here */
    hr = IFilterGraph2_AddFilter(graph, source, wbuf);
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
    IEnumMoniker *enum_moniker = NULL;
    IPin *source_out = NULL;
    IMoniker *moniker = NULL;
    IBaseFilter *rcv = NULL;
    IPin *rcv_in = NULL;

    if (source == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* get source's graph */
    hr = dshow_get_graph(source, &graph);
    BDA_CHECK_HR_D("couldn't get source filter's graph");

    /* list possible candidates for attaching to source filter */
    hr = dshow_enum(&KSCATEGORY_BDA_RECEIVER_COMPONENT, &enum_moniker);
    if (FAILED(hr))
        BDA_THROW_D("couldn't enumerate BDA receiver filters");
    else if (hr != S_OK)
        goto out; /* no receivers installed */

    hr = dshow_find_pin(source, PINDIR_OUTPUT, true, &source_out);
    BDA_CHECK_HR_D("couldn't find output pin on source filter");

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

        hr = dshow_find_pin(rcv, PINDIR_INPUT, true, &rcv_in);
        if (FAILED(hr)) continue;

        wbuf = cx_widen(fname);
        hr = IFilterGraph2_AddFilter(graph, rcv, wbuf);
        if (FAILED(hr)) continue;

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
            hr = IFilterGraph2_RemoveFilter(graph, rcv);
            BDA_CHECK_HR_D("couldn't remove receiver filter from graph");
        }
    } while (true);

out:
    SAFE_RELEASE(source_out);
    SAFE_RELEASE(enum_moniker);
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
    IPin *tail_out, *demux_in = NULL;

    if (tail == NULL || out == NULL)
        return E_POINTER;

    hr = dshow_get_graph(tail, &graph);
    BDA_CHECK_HR_D("couldn't get capture filter's graph");

    hr = CoCreateInstance(&CLSID_MPEG2Demultiplexer, NULL, CLSCTX_INPROC
                          , &IID_IBaseFilter, (void **)&demux);
    BDA_CHECK_HR_D("couldn't create demultiplexer filter");

    hr = dshow_find_pin(tail, PINDIR_OUTPUT, true, &tail_out);
    BDA_CHECK_HR_D("couldn't find output pin on capture filter");

    hr = dshow_find_pin(demux, PINDIR_INPUT, true, &demux_in);
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
    IEnumMoniker *enum_moniker = NULL;
    IMoniker *moniker = NULL;
    IBaseFilter *tif = NULL;
    IPin *tif_in = NULL;
    IPin *demux_out = NULL;

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
    hr = dshow_find_pin(tif, PINDIR_INPUT, true, &tif_in);
    BDA_CHECK_HR_D("couldn't find input pin on TIF");

    hr = dshow_find_pin(demux, PINDIR_OUTPUT, true, &demux_out);
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
    SAFE_RELEASE(moniker);
    SAFE_RELEASE(enum_moniker);
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
    BDA_CHECK_HR_D("couldn't query IMpeg2Demultiplexer interface");

    AM_MEDIA_TYPE mt;
    memset(&mt, 0, sizeof(mt));
    mt.majortype = MEDIATYPE_Stream;
    mt.subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;

    hr = IMpeg2Demultiplexer_CreateOutputPin(mpeg, &mt, pin_name, &mpeg_out);
    BDA_CHECK_HR_D("couldn't create output pin on demultiplexer");

    /* query pid mapper */
    hr = IPin_QueryInterface(mpeg_out, &IID_IMPEG2PIDMap, (void **)out);

out:
    SAFE_RELEASE(mpeg_out);
    SAFE_RELEASE(mpeg);

    return hr;
}

/* create TS probe and connect it to the graph */
static
void on_sample(void *arg, const void *buf, size_t len);

static
HRESULT create_probe(module_data_t *mod, IBaseFilter *tail, IBaseFilter **out)
{
    HRESULT hr = E_FAIL;

    IFilterGraph2 *graph = NULL;
    IPin *tail_out, *probe_in = NULL;
    IBaseFilter *probe = NULL;

    if (tail == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* get tail filter's graph and output pin */
    hr = dshow_get_graph(tail, &graph);
    BDA_CHECK_HR_D("couldn't get capture filter's graph");

    hr = dshow_find_pin(tail, PINDIR_OUTPUT, true, &tail_out);
    BDA_CHECK_HR_D("couldn't find output pin on capture filter");

    /* try creating and attaching probes with different media subtypes */
    for (unsigned int i = 0; i < 3; i++)
    {
        SAFE_RELEASE(probe);
        SAFE_RELEASE(probe_in);

        AM_MEDIA_TYPE mt;
        memset(&mt, 0, sizeof(mt));
        mt.majortype = MEDIATYPE_Stream;

        if (i == 0)
            mt.subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;
        else if (i == 1)
            mt.subtype = KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT;
        else
            break;

        hr = dshow_grabber(on_sample, mod, &mt, &probe);
        BDA_CHECK_HR_D("couldn't instantiate TS probe filter");

        hr = dshow_find_pin(probe, PINDIR_INPUT, true, &probe_in);
        BDA_CHECK_HR_D("couldn't find input pin on TS probe");

        hr = IFilterGraph2_AddFilter(graph, probe, L"Probe");
        BDA_CHECK_HR_D("couldn't add TS probe to graph");

        hr = IFilterGraph2_ConnectDirect(graph, tail_out, probe_in, NULL);
        if (SUCCEEDED(hr))
            break;

        IFilterGraph2_RemoveFilter(graph, probe);
    }
    BDA_CHECK_HR_D("couldn't connect TS probe to capture filter");

    IBaseFilter_AddRef(probe);
    *out = probe;

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
    BDA_CHECK_HR_D("couldn't query IPin interface");

    PIN_INFO info;
    memset(&info, 0, sizeof(info));
    hr = IPin_QueryPinInfo(demux_out, &info);
    BDA_CHECK_HR_D("couldn't query PID mapper's pin information");

    demux = info.pFilter;
    if (demux == NULL)
        BDA_THROW_D("couldn't retrieve PID mapper's owning filter");

    hr = dshow_get_graph(demux, &graph);
    BDA_CHECK_HR_D("couldn't get demultiplexer's graph");

    /* create probe and attach it to demultiplexer pin */
    AM_MEDIA_TYPE mt;
    memset(&mt, 0, sizeof(mt));
    mt.majortype = MEDIATYPE_Stream;
    mt.subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;

    hr = dshow_grabber(on_sample, mod, &mt, &probe);
    BDA_CHECK_HR_D("couldn't instantiate TS probe filter");

    hr = dshow_find_pin(probe, PINDIR_INPUT, true, &probe_in);
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

/* callback: create signal statistics interface */
static
bool node_signal_stats(IBDA_Topology *topology, ULONG type, const GUID *intf
                       , void *arg)
{
    if (!IsEqualGUID(intf, &KSPROPSETID_BdaSignalStats))
        return false;

    bool found = false;
    IUnknown *node = NULL;

    HRESULT hr = IBDA_Topology_GetControlNode(topology, 0, 1, type, &node);
    if (SUCCEEDED(hr))
    {
        void **out = (void **)arg;
        hr = IUnknown_QueryInterface(node, &IID_IBDA_SignalStatistics, out);
        if (SUCCEEDED(hr))
            found = true;
    }

    SAFE_RELEASE(node);
    return found;
}

/* invoke callback for every node in device topology */
typedef bool (*node_callback_t)(IBDA_Topology *, ULONG, const GUID *, void *);

static
HRESULT enumerate_topology(const module_data_t *mod, IBaseFilter *filter
                           , node_callback_t callback, void *arg)
{
    HRESULT hr = E_FAIL;

    ULONG node_types_cnt = 0;
    ULONG node_types[32] = { 0 };

    IBDA_Topology *topology = NULL;

    if (filter == NULL)
        return E_POINTER;

    /* get topology interface */
    hr = IBaseFilter_QueryInterface(filter, &IID_IBDA_Topology
                                    , (void **)&topology);
    BDA_CHECK_HR_D("couldn't query IBDA_Topology interface");

    /* list node types */
    hr = IBDA_Topology_GetNodeTypes(topology, &node_types_cnt
                                    , ASC_ARRAY_SIZE(node_types)
                                    , node_types);
    BDA_CHECK_HR_D("couldn't retrieve list of topology node types");

    for (unsigned int i = 0; i < node_types_cnt; i++)
    {
        /* list interfaces for each node type */
        ULONG node_intf_cnt = 0;
        GUID node_intf[32];

        memset(&node_intf, 0, sizeof(node_intf));
        hr = IBDA_Topology_GetNodeInterfaces(topology, node_types[i]
                                             , &node_intf_cnt
                                             , ASC_ARRAY_SIZE(node_intf)
                                             , node_intf);
        BDA_CHECK_HR_D("couldn't retrieve list of node interfaces");

        for (unsigned int j = 0; j < node_intf_cnt; j++)
        {
            if (callback(topology, node_types[i], &node_intf[j], arg))
                goto out;
        }
    }

out:
    SAFE_RELEASE(topology);

    return hr;
}

/* submit tune request to network provider */
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
    BDA_CHECK_HR_D("couldn't create tune request");

    if (mod->debug)
        bda_dump_request(request);

    hr = ITuneRequest_get_TuningSpace(request, &space);
    BDA_CHECK_HR_D("couldn't retrieve tuning space");

    hr = IBaseFilter_QueryInterface(provider, &IID_ITuner
                                    , (void **)&provider_tuner);
    BDA_CHECK_HR_D("couldn't query ITuner interface");

    hr = ITuner_put_TuningSpace(provider_tuner, space);
    BDA_CHECK_HR_D("couldn't assign tuning space to provider");

    hr = ITuner_put_TuneRequest(provider_tuner, request);

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
    hr = dshow_find_pin(provider, PINDIR_OUTPUT, true, &provider_out);
    BDA_CHECK_HR_D("couldn't find output pin on network provider filter");

    hr = dshow_find_pin(source, PINDIR_INPUT, true, &source_in);
    BDA_CHECK_HR_D("couldn't find input pin on source filter");

    /* connect pins and submit tune request to provider */
    hr = IFilterGraph2_ConnectDirect(graph, provider_out, source_in, NULL);
    if (FAILED(hr))
    {
        /*
         * With legacy providers, we have to submit a tune request
         * before connecting pins.
         */
        retry_pins = true;
    }

    hr = provider_tune(mod, provider, &mod->tune);
    BDA_CHECK_HR_D("couldn't submit initial tune request to provider");

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

    if (hr == S_OK)
    {
        PID_MAP old[TS_MAX_PID];
        ULONG old_cnt;

        hr = IEnumPIDMap_Next(enum_pid, ASC_ARRAY_SIZE(old), old, &old_cnt);
        if (hr == S_OK)
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
            BDA_ERROR_D("couldn't remove filter from graph");
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
    BDA_CHECK_HR_D("couldn't retrieve the running object table interface");

    /*
     * Create a moniker identifying the graph. The moniker must follow
     * this exact naming convention, otherwise it won't show up in GraphEdt.
     */
    StringCchPrintf(wbuf, ASC_ARRAY_SIZE(wbuf)
                    , L"FilterGraph %08x pid %08x"
                    , (void *)graph, GetCurrentProcessId());

    hr = CreateItemMoniker(L"!", wbuf, &moniker);
    BDA_CHECK_HR_D("couldn't create an item moniker for ROT registration");

    /* register filter graph in the table */
    hr = IRunningObjectTable_Register(rot, 0, (IUnknown *)graph, moniker, out);

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
    BDA_CHECK_HR_D("couldn't query IMediaControl interface");

    /* switch the graph into running state */
    hr = IMediaControl_Run(control);
    BDA_CHECK_HR_D("couldn't switch the graph into running state");

    do
    {
        hr = IMediaControl_GetState(control, 100, &state);
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
    IBaseFilter *probe = NULL;
    IBaseFilter *demux = NULL;
    IBaseFilter *tif = NULL;

    IMPEG2PIDMap *pidmap = NULL;
    IBDA_SignalStatistics *signal = NULL;

    /* initialize COM on this thread */
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    BDA_CHECK_HR("CoInitializeEx() failed");
    asc_assert(hr != S_FALSE, MSG("COM initialized twice!"));

    need_uninit = true;

    /* create filter graph */
    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC
                          , &IID_IFilterGraph2, (void **)&graph);
    BDA_CHECK_HR("failed to create filter graph");

    hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaEvent
                                      , (void **)&event);
    BDA_CHECK_HR("failed to query IMediaEvent interface");

    hr = IMediaEvent_GetEventHandle(event, (OAEVENT *)&graph_evt);
    BDA_CHECK_HR("failed to retrieve graph's event handle");

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

    /* create signal statistics interface */
    hr = enumerate_topology(mod, source, node_signal_stats, &signal);
    BDA_CHECK_HR("failed to search device topology for signal stats");
    if (signal == NULL)
        asc_log_warning(MSG("couldn't find signal statistics interface"));

    // -------
    // TODO
    // -------
    // - samplegrabber with callbacks
    //   - how to handle buffering?
    //     - buffer in callback is not necessarily aligned on TS packet bdry
    //   - which thread executes the callbacks?
    //     - worker thread, not related to control thread
    // - scan for vendor extensions (CAM, Diseqc, etc)
    // -------

    if (mod->debug)
    {
        hr = rot_register(mod, graph, &mod->rot_reg);
        if (FAILED(hr))
            BDA_ERROR_D("failed to register the graph in ROT");
    }

    /* start the graph */
    hr = control_run(mod, graph);
    BDA_CHECK_HR("failed to run the graph");

    /* increase refcount on objects of interest */
    IFilterGraph2_AddRef(graph);
    mod->graph = graph;

    IMediaEvent_AddRef(event);
    mod->event = event;
    mod->graph_evt = graph_evt;

    IBaseFilter_AddRef(provider);
    mod->provider = provider;

    if (pidmap != NULL)
    {
        IMPEG2PIDMap_AddRef(pidmap);
        mod->pidmap = pidmap;
    }

    if (signal != NULL)
    {
        IBDA_SignalStatistics_AddRef(signal);
        mod->signal = signal;
    }

    need_uninit = false;
    hr = S_OK;

out:
    if (need_uninit)
    {
        rot_unregister(&mod->rot_reg);
        remove_filters(mod, graph);
    }

    SAFE_RELEASE(signal);
    SAFE_RELEASE(pidmap);
    SAFE_RELEASE(tif);
    SAFE_RELEASE(demux);
    SAFE_RELEASE(probe);
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
    rot_unregister(&mod->rot_reg);
    remove_filters(mod, mod->graph);

    SAFE_RELEASE(mod->signal);
    SAFE_RELEASE(mod->pidmap);
    SAFE_RELEASE(mod->provider);
    SAFE_RELEASE(mod->event);
    SAFE_RELEASE(mod->graph);

    mod->graph_evt = NULL;

    CoUninitialize();
}

/*
 * graph runtime control
 */

/* set new graph state */
static
void graph_set_state(module_data_t *mod, bda_state_t state)
{
    if (mod->state == state)
        return;

    const char *str = NULL;
    switch (state)
    {
        case BDA_STATE_INIT:    str = "INIT";    break;
        case BDA_STATE_RUNNING: str = "RUNNING"; break;
        case BDA_STATE_STOPPED: str = "STOPPED"; break;
        case BDA_STATE_ERROR:   str = "ERROR";   break;
        default:
            str = "UNKNOWN";
            break;
    }

    asc_log_debug(MSG("setting state to %s"), str);
    mod->state = state;

    if (state == BDA_STATE_ERROR)
    {
        asc_log_info(MSG("reopening device in %u seconds"), COOLDOWN_TICKS);
        mod->cooldown = COOLDOWN_TICKS;
    }
}

/* set tuning data, opening the device if necessary */
static
void graph_set_tune(module_data_t *mod, const bda_tune_cmd_t *tune)
{
    memcpy(&mod->tune, tune, sizeof(*tune));

    if (mod->state == BDA_STATE_RUNNING)
    {
        const HRESULT hr = provider_tune(mod, mod->provider, &mod->tune);
        if (FAILED(hr))
        {
            BDA_ERROR("failed to submit tune request");

            graph_teardown(mod);
            graph_set_state(mod, BDA_STATE_ERROR);
        }
    }
    else
    {
        graph_set_state(mod, BDA_STATE_INIT);
    }
}

/* request demultiplexer to join or leave PID */
static
void graph_set_pid(module_data_t *mod, bool join, uint16_t pid)
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
void graph_set_ca(module_data_t *mod, bool enable, uint16_t pnr)
{
    mod->ca_pmts[pnr] = enable;

    /* TODO: implement CAM support */
    asc_log_error(MSG("STUB: %s CAM for PNR %hu")
                  , (enable ? "enable" : "disable"), pnr);
}

/*
static
void graph_set_diseqc(mod, command sequence)
{
}
*/

/* dispatch graph events */
static
HRESULT graph_do_events(module_data_t *mod)
{
    HRESULT hr = E_FAIL;

    /* empty event queue */
    do
    {
        long ec;
        LONG_PTR p1, p2;

        /* wait up to 50ms */
        hr = IMediaEvent_GetEvent(mod->event, &ec, &p1, &p2, 50);
        if (hr == E_ABORT)
        {
            /* no more events */
            hr = S_OK;
            break;
        }
        else if (FAILED(hr))
        {
            BDA_THROW("failed to retrieve next graph event");
        }

        // TODO
        switch (ec)
        {
            default:
                asc_log_info(MSG("ec = %ld"), ec);
                break;
        }

        hr = IMediaEvent_FreeEventParams(mod->event, ec, p1, p2);
        BDA_CHECK_HR("failed to free event parameters");
    } while (true);

    /* update signal statistics */
    if (mod->signal != NULL)
    {
        bda_signal_stats_t s;
        memset(&s, 0, sizeof(s));

        hr = mod->signal->lpVtbl->get_SignalLocked(mod->signal, &s.locked);
        BDA_CHECK_HR("failed to retrieve signal lock status");

        hr = mod->signal->lpVtbl->get_SignalPresent(mod->signal, &s.present);
        BDA_CHECK_HR("failed to retrieve signal presence status");

        hr = mod->signal->lpVtbl->get_SignalQuality(mod->signal, &s.quality);
        BDA_CHECK_HR("failed to retrieve signal quality value");

        hr = mod->signal->lpVtbl->get_SignalStrength(mod->signal, &s.strength);
        BDA_CHECK_HR("failed to retrieve signal strength value");

        /* resend tuning request on lock loss */
        // TODO

        /* notify main thread */
        asc_mutex_lock(&mod->signal_lock);
        memcpy(&mod->signal_stats, &s, sizeof(s));
        asc_mutex_unlock(&mod->signal_lock);

        asc_job_queue(mod, bda_on_stats, mod);
    }

out:
    return hr;
}

/* wait for graph event or user command */
static
void graph_wait_events(module_data_t *mod)
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

/* execute user command */
static
void graph_execute(module_data_t *mod, const bda_user_cmd_t *cmd)
{
    switch (cmd->cmd)
    {
        case BDA_COMMAND_TUNE:
            graph_set_tune(mod, &cmd->tune);
            break;

        case BDA_COMMAND_DEMUX:
            graph_set_pid(mod, cmd->demux.join, cmd->demux.pid);
            break;

        case BDA_COMMAND_CA:
            graph_set_ca(mod, cmd->ca.enable, cmd->ca.pnr);
            break;

        case BDA_COMMAND_DISEQC:
            /*
             * TODO: implement sending diseqc commands
             *
             * graph_set_diseqc(mod, &cmd->diseqc);
             */
            break;

        case BDA_COMMAND_QUIT:
        case BDA_COMMAND_CLOSE:
        default:
            graph_teardown(mod);
            graph_set_state(mod, BDA_STATE_STOPPED);
            break;
    }
}

/*
 * TS buffering
 */

/* called by the probe filter when it has media samples */
static
void on_sample(void *arg, const void *buf, size_t len)
{
    module_data_t *const mod = (module_data_t *)arg;

    /* TODO: implement buffering */
    __uarg(mod);
    __uarg(buf);
    __uarg(len);
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

            graph_execute(mod, item);
            free(item);

            asc_mutex_lock(&mod->queue_lock);
        }
        asc_mutex_unlock(&mod->queue_lock);

        if (quit)
            break;

        /* handle state changes */
        switch (mod->state)
        {
            case BDA_STATE_INIT:
            {
                const HRESULT hr = graph_setup(mod);
                if (SUCCEEDED(hr))
                    graph_set_state(mod, BDA_STATE_RUNNING);
                else
                    graph_set_state(mod, BDA_STATE_ERROR);

                break;
            }

            case BDA_STATE_RUNNING:
            {
                const HRESULT hr = graph_do_events(mod);
                if (FAILED(hr))
                {
                    graph_teardown(mod);
                    graph_set_state(mod, BDA_STATE_ERROR);
                }

                break;
            }

            case BDA_STATE_ERROR:
            {
                if (--mod->cooldown <= 0)
                    graph_set_state(mod, BDA_STATE_INIT);

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
        graph_wait_events(mod);
    } while (true);

    asc_log_debug(MSG("control thread exiting"));
}
