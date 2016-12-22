/*
 * Astra Module: BDA (Request generator)
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

/* set SystemType on DVB tuning spaces */
static
HRESULT init_space_dvbx(ITuningSpace *space, DVBSystemType type)
{
    IDVBTuningSpace2 *space_dvb = NULL;
    HRESULT hr = ITuningSpace_QueryInterface(space, &IID_IDVBTuningSpace2
                                             , (void **)&space_dvb);
    ASC_WANT_PTR(hr, space_dvb);
    if (FAILED(hr)) return hr;

    hr = IDVBTuningSpace2_put_SystemType(space_dvb, type);
    ASC_RELEASE(space_dvb);

    return hr;
}

/* fill in basic locator properties from user tuning command */
static
HRESULT set_locator_generic(const bda_tune_cmd_t *tune, ILocator *locator)
{
    HRESULT hr = ILocator_put_CarrierFrequency(locator, tune->frequency);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_InnerFEC(locator, tune->fec_mode);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_InnerFECRate(locator, tune->fec);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_Modulation(locator, tune->modulation);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_OuterFEC(locator, tune->outer_fec_mode);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_OuterFECRate(locator, tune->outer_fec);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_SymbolRate(locator, tune->symbolrate);
    return hr;
}

/*
 * ATSC
 */

static
HRESULT init_space_atsc(ITuningSpace *space)
{
    IATSCTuningSpace *space_atsc = NULL;
    HRESULT hr = ITuningSpace_QueryInterface(space, &IID_IATSCTuningSpace
                                             , (void **)&space_atsc);
    ASC_WANT_PTR(hr, space_atsc);
    if (FAILED(hr)) return hr;

    hr = IATSCTuningSpace_put_MaxChannel(space_atsc, 9999);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_MinChannel(space_atsc, 0);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_MaxMinorChannel(space_atsc, 9999);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_MinMinorChannel(space_atsc, 0);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_MaxPhysicalChannel(space_atsc, 9999);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_MinPhysicalChannel(space_atsc, 0);
out:
    ASC_RELEASE(space_atsc);
    return hr;
}

static
HRESULT set_space_atsc(const bda_tune_cmd_t *tune, ITuningSpace *space)
{
    IATSCTuningSpace *space_atsc = NULL;
    HRESULT hr = ITuningSpace_QueryInterface(space, &IID_IATSCTuningSpace
                                             , (void **)&space_atsc);
    ASC_WANT_PTR(hr, space_atsc);
    if (FAILED(hr)) return hr;

    hr = IATSCTuningSpace_put_CountryCode(space_atsc, tune->country_code);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_InputType(space_atsc, tune->input_type);
out:
    ASC_RELEASE(space_atsc);
    return hr;
}

static
HRESULT set_request_atsc(const bda_tune_cmd_t *tune, ITuneRequest *request)
{
    IATSCChannelTuneRequest *request_atsc = NULL;
    HRESULT hr = ITuneRequest_QueryInterface(request
                                             , &IID_IATSCChannelTuneRequest
                                             , (void **)&request_atsc);
    ASC_WANT_PTR(hr, request_atsc);
    if (FAILED(hr)) return hr;

    hr = request_atsc->lpVtbl->put_Channel(request_atsc
                                           , tune->major_channel);
    if (FAILED(hr)) goto out;

    hr = request_atsc->lpVtbl->put_MinorChannel(request_atsc
                                                , tune->minor_channel);
out:
    ASC_RELEASE(request_atsc);
    return hr;
}

static
HRESULT set_locator_atsc(const bda_tune_cmd_t *tune, ILocator *locator)
{
    IATSCLocator *locator_atsc = NULL;
    HRESULT hr = ILocator_QueryInterface(locator, &IID_IATSCLocator
                                         , (void **)&locator_atsc);
    ASC_WANT_PTR(hr, locator_atsc);
    if (FAILED(hr)) return hr;

    hr = IATSCLocator_put_PhysicalChannel(locator_atsc, tune->stream_id);

    ASC_RELEASE(locator_atsc);
    return hr;
}

static
const bda_network_t net_atsc =
{
    .name = { "atsc" },

    .provider = &CLSID_ATSCNetworkProvider,
    .locator = &CLSID_ATSCLocator,
    .tuning_space = &CLSID_ATSCTuningSpace,
    .network_type = &ATSC_TERRESTRIAL_TV_NETWORK_TYPE,

    .init_space = init_space_atsc,

    .set_space = set_space_atsc,
    .set_request = set_request_atsc,
    .set_locator = set_locator_atsc,
};

/*
 * CQAM
 */

static
HRESULT init_space_cqam(ITuningSpace *space)
{
    IDigitalCableTuningSpace *space_cqam = NULL;
    HRESULT hr = ITuningSpace_QueryInterface(space
                                             , &IID_IDigitalCableTuningSpace
                                             , (void **)&space_cqam);
    ASC_WANT_PTR(hr, space_cqam);
    if (FAILED(hr)) return hr;

    hr = space_cqam->lpVtbl->put_MaxMajorChannel(space_cqam, 9999);
    if (FAILED(hr)) goto out;

    hr = space_cqam->lpVtbl->put_MinMajorChannel(space_cqam, 0);
    if (FAILED(hr)) goto out;

    hr = space_cqam->lpVtbl->put_MaxSourceID(space_cqam, INT32_MAX);
    if (FAILED(hr)) goto out;

    hr = space_cqam->lpVtbl->put_MinSourceID(space_cqam, 0);
    if (FAILED(hr)) goto out;

    hr = init_space_atsc(space); /* delegate to ATSC */
out:
    ASC_RELEASE(space_cqam);
    return hr;
}

static
HRESULT set_request_cqam(const bda_tune_cmd_t *tune, ITuneRequest *request)
{
    IDigitalCableTuneRequest *request_cqam = NULL;
    HRESULT hr = ITuneRequest_QueryInterface(request
                                             , &IID_IDigitalCableTuneRequest
                                             , (void **)&request_cqam);
    ASC_WANT_PTR(hr, request_cqam);
    if (FAILED(hr)) return hr;

    hr = request_cqam->lpVtbl->put_MajorChannel(request_cqam
                                                , tune->major_channel);
    if (FAILED(hr)) goto out;

    hr = request_cqam->lpVtbl->put_MinorChannel(request_cqam
                                                , tune->minor_channel);
    if (FAILED(hr)) goto out;

    hr = request_cqam->lpVtbl->put_Channel(request_cqam
                                           , tune->virtual_channel);
out:
    ASC_RELEASE(request_cqam);
    return hr;
}

static
const bda_network_t net_cqam =
{
    .name = { "cqam" },

    .provider = NULL, /* not supported by legacy providers */
    .locator = &CLSID_DigitalCableLocator,
    .tuning_space = &CLSID_DigitalCableTuningSpace,
    .network_type = &DIGITAL_CABLE_NETWORK_TYPE,

    .init_space = init_space_cqam,

    .set_space = set_space_atsc, /* same as ATSC */
    .set_request = set_request_cqam,
    .set_locator = set_locator_atsc, /* same as ATSC */
};

/*
 * DVB-C
 */

static
HRESULT init_space_dvbc(ITuningSpace *space)
{
    return init_space_dvbx(space, DVB_Cable);
}

static
const bda_network_t net_dvbc =
{
    .name = { "dvbc", "c" },

    .provider = &CLSID_DVBCNetworkProvider,
    .locator = &CLSID_DVBCLocator,
    .tuning_space = &CLSID_DVBTuningSpace,
    .network_type = &DVB_CABLE_TV_NETWORK_TYPE,

    .init_space = init_space_dvbc,
};

/*
 * DVB-S
 */

static
HRESULT init_space_dvbs(ITuningSpace *space)
{
    return init_space_dvbx(space, DVB_Satellite);
}

static
HRESULT set_space_dvbs(const bda_tune_cmd_t *tune, ITuningSpace *space)
{
    IDVBSTuningSpace *space_s = NULL;
    HRESULT hr = ITuningSpace_QueryInterface(space, &IID_IDVBSTuningSpace
                                             , (void **)&space_s);
    ASC_WANT_PTR(hr, space_s);
    if (FAILED(hr)) return hr;

    hr = IDVBSTuningSpace_put_LowOscillator(space_s, tune->lof1);
    if (FAILED(hr)) goto out;

    hr = IDVBSTuningSpace_put_HighOscillator(space_s, tune->lof2);
    if (FAILED(hr)) goto out;

    hr = IDVBSTuningSpace_put_LNBSwitch(space_s, tune->slof);
    if (FAILED(hr)) goto out;

    hr = IDVBSTuningSpace_put_SpectralInversion(space_s, tune->inversion);
out:
    ASC_RELEASE(space_s);
    return hr;
}

static
HRESULT set_locator_dvbs(const bda_tune_cmd_t *tune, ILocator *locator)
{
    IDVBSLocator *locator_s = NULL;
    HRESULT hr = ILocator_QueryInterface(locator, &IID_IDVBSLocator
                                         , (void **)&locator_s);
    ASC_WANT_PTR(hr, locator_s);
    if (FAILED(hr)) return hr;

    hr = IDVBSLocator_put_SignalPolarisation(locator_s, tune->polarization);

    ASC_RELEASE(locator_s);
    return hr;
}

static
const bda_network_t net_dvbs =
{
    .name = { "dvbs", "s" },

    .provider = &CLSID_DVBSNetworkProvider,
    .locator = &CLSID_DVBSLocator,
    .tuning_space = &CLSID_DVBSTuningSpace,
    .network_type = &DVB_SATELLITE_TV_NETWORK_TYPE,

    .init_space = init_space_dvbs,

    .set_space = set_space_dvbs,
    .set_locator = set_locator_dvbs,
};

/*
 * DVB-S2
 */

static
HRESULT init_locator_dvbs2(ILocator *locator)
{
    /* check if the OS supports DVB-S2 */
    IDVBSLocator2 *locator_s2 = NULL;
    HRESULT hr = ILocator_QueryInterface(locator, &IID_IDVBSLocator2
                                         , (void **)&locator_s2);
    ASC_WANT_PTR(hr, locator_s2);
    if (FAILED(hr)) return hr;

    ASC_RELEASE(locator_s2);
    return S_OK;
}

static
HRESULT set_locator_dvbs2(const bda_tune_cmd_t *tune, ILocator *locator)
{
    IDVBSLocator2 *locator_s2 = NULL;
    HRESULT hr = ILocator_QueryInterface(locator, &IID_IDVBSLocator2
                                         , (void **)&locator_s2);
    ASC_WANT_PTR(hr, locator_s2);
    if (FAILED(hr)) return hr;

    hr = IDVBSLocator2_put_SignalPilot(locator_s2, tune->pilot);
    if (FAILED(hr)) goto out;

    hr = IDVBSLocator2_put_SignalRollOff(locator_s2, tune->rolloff);
    if (FAILED(hr)) goto out;

    hr = set_locator_dvbs(tune, locator); /* delegate to DVB-S */
out:
    ASC_RELEASE(locator_s2);
    return hr;
}

static
const bda_network_t net_dvbs2 =
{
    .name = { "dvbs2", "s2" },

    .provider = &CLSID_DVBSNetworkProvider,
    .locator = &CLSID_DVBSLocator,
    .tuning_space = &CLSID_DVBSTuningSpace,
    .network_type = &DVB_SATELLITE_TV_NETWORK_TYPE,

    .init_default_locator = init_locator_dvbs2,
    .init_space = init_space_dvbs, /* same as DVB-S */

    .set_space = set_space_dvbs, /* same as DVB-S */
    .set_locator = set_locator_dvbs2,
};

/*
 * DVB-T
 */

static
HRESULT init_space_dvbt(ITuningSpace *space)
{
    return init_space_dvbx(space, DVB_Terrestrial);
}

static
HRESULT set_locator_dvbt(const bda_tune_cmd_t *tune, ILocator *locator)
{
    IDVBTLocator *locator_t = NULL;
    HRESULT hr = ILocator_QueryInterface(locator, &IID_IDVBTLocator
                                         , (void **)&locator_t);
    ASC_WANT_PTR(hr, locator_t);
    if (FAILED(hr)) return hr;

    hr = IDVBTLocator_put_Bandwidth(locator_t, tune->bandwidth);
    if (FAILED(hr)) goto out;

    hr = IDVBTLocator_put_Guard(locator_t, tune->guardinterval);
    if (FAILED(hr)) goto out;

    hr = IDVBTLocator_put_HAlpha(locator_t, tune->hierarchy);
    if (FAILED(hr)) goto out;

    hr = IDVBTLocator_put_LPInnerFEC(locator_t, tune->lp_fec_mode);
    if (FAILED(hr)) goto out;

    hr = IDVBTLocator_put_LPInnerFECRate(locator_t, tune->lp_fec);
    if (FAILED(hr)) goto out;

    hr = IDVBTLocator_put_Mode(locator_t, tune->transmitmode);
out:
    ASC_RELEASE(locator_t);
    return hr;
}

static
const bda_network_t net_dvbt =
{
    .name = { "dvbt", "t" },

    .provider = &CLSID_DVBTNetworkProvider,
    .locator = &CLSID_DVBTLocator,
    .tuning_space = &CLSID_DVBTuningSpace,
    .network_type = &DVB_TERRESTRIAL_TV_NETWORK_TYPE,

    .init_space = init_space_dvbt,

    .set_locator = set_locator_dvbt,
};

/*
 * DVB-T2
 */

static
HRESULT set_locator_dvbt2(const bda_tune_cmd_t *tune, ILocator *locator)
{
    IDVBTLocator2 *locator_t2 = NULL;
    HRESULT hr = ILocator_QueryInterface(locator, &IID_IDVBTLocator2
                                         , (void **)&locator_t2);
    ASC_WANT_PTR(hr, locator_t2);
    if (FAILED(hr)) return hr;

    hr = IDVBTLocator2_put_PhysicalLayerPipeId(locator_t2, tune->stream_id);
    if (FAILED(hr)) goto out;

    hr = set_locator_dvbt(tune, locator); /* delegate to DVB-T */
out:
    ASC_RELEASE(locator_t2);
    return hr;
}

static
const bda_network_t net_dvbt2 =
{
    .name = { "dvbt2", "t2" },

    .provider = &CLSID_DVBTNetworkProvider,
    .locator = &CLSID_DVBTLocator2,
    .tuning_space = &CLSID_DVBTuningSpace,
    .network_type = &DVB_TERRESTRIAL_TV_NETWORK_TYPE,

    .init_space = init_space_dvbt, /* same as DVB-T */

    .set_locator = set_locator_dvbt2,
};

/*
 * ISDB-S
 */

static
HRESULT init_space_isdbs(ITuningSpace *space)
{
    return init_space_dvbx(space, ISDB_Satellite);
}

static
const bda_network_t net_isdbs =
{
    .name = { "isdbs" },

    .provider = NULL, /* not supported by legacy providers */
    .locator = &CLSID_ISDBSLocator,
    .tuning_space = &CLSID_DVBSTuningSpace,
    .network_type = &ISDB_SATELLITE_TV_NETWORK_TYPE,

    .init_space = init_space_isdbs,

    .set_space = set_space_dvbs,
    .set_locator = set_locator_dvbs, /* same as DVB-S */
};

/*
 * ISDB-T
 */

static
HRESULT init_space_isdbt(ITuningSpace *space)
{
    return init_space_dvbx(space, ISDB_Terrestrial);
}

static
const bda_network_t net_isdbt =
{
    .name = { "isdbt" },

    .provider = NULL, /* not supported by legacy providers */
    .locator = &CLSID_DVBTLocator,
    .tuning_space = &CLSID_DVBTuningSpace,
    .network_type = &ISDB_TERRESTRIAL_TV_NETWORK_TYPE,

    .init_space = init_space_isdbt,

    .set_locator = set_locator_dvbt, /* same as DVB-T */
};

/*
 * public API
 */

/* list of supported network types */
const bda_network_t *const bda_network_list[] =
{
    &net_atsc,
    &net_cqam,
    &net_dvbc,
    &net_dvbs,
    &net_dvbs2,
    &net_dvbt,
    &net_dvbt2,
    &net_isdbs,
    &net_isdbt,
    NULL,
};

/* create network provider for given network type */
HRESULT bda_net_provider(const bda_network_t *net, IBaseFilter **out)
{
    HRESULT hr = E_FAIL;

    if (net == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* try the universal provider first (available since Windows 7) */
    hr = CoCreateInstance(&CLSID_NetworkProvider, NULL, CLSCTX_INPROC_SERVER
                          , &IID_IBaseFilter, (void **)out);
    ASC_WANT_PTR(hr, *out);

    if (FAILED(hr) && net->provider != NULL)
    {
        /* fall back to legacy provider if the network type supports it */
        hr = CoCreateInstance(net->provider, NULL, CLSCTX_INPROC_SERVER
                              , &IID_IBaseFilter, (void **)out);
        ASC_WANT_PTR(hr, *out);
    }

    return hr;
}

/* create tuning space for given network type */
HRESULT bda_tuning_space(const bda_network_t *net, ITuningSpace **out)
{
    HRESULT hr = E_FAIL;

    ILocator *locator = NULL;
    ITuningSpace *space = NULL;
    BSTR name = NULL;

    if (net == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* convert name to BSTR */
    wchar_t *const wbuf = cx_widen(net->name[0]);
    name = SysAllocString(wbuf);
    free(wbuf);

    if (name == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto out;
    }

    /* create default locator */
    hr = CoCreateInstance(net->locator, NULL, CLSCTX_INPROC_SERVER
                          , &IID_ILocator, (void **)&locator);
    ASC_WANT_PTR(hr, locator);
    if (FAILED(hr)) goto out;

    if (net->init_default_locator != NULL)
    {
        hr = net->init_default_locator(locator);
        if (FAILED(hr)) goto out;
    }

    /* set up tuning space */
    hr = CoCreateInstance(net->tuning_space, NULL, CLSCTX_INPROC_SERVER
                          , &IID_ITuningSpace, (void **)&space);
    ASC_WANT_PTR(hr, space);
    if (FAILED(hr)) goto out;

    hr = ITuningSpace_put__NetworkType(space, net->network_type);
    if (FAILED(hr)) goto out;

    hr = ITuningSpace_put_FriendlyName(space, name);
    if (FAILED(hr)) goto out;

    hr = ITuningSpace_put_UniqueName(space, name);
    if (FAILED(hr)) goto out;

    if (net->init_space != NULL)
    {
        hr = net->init_space(space);
        if (FAILED(hr)) goto out;
    }

    hr = ITuningSpace_put_DefaultLocator(space, locator);
    if (FAILED(hr)) goto out;

    ITuningSpace_AddRef(space);
    *out = space;

out:
    ASC_RELEASE(space);
    ASC_RELEASE(locator);

    ASC_FREE(name, SysFreeString);

    return hr;
}

/* create tune request based on a user tuning command */
HRESULT bda_tune_request(const bda_tune_cmd_t *tune, ITuneRequest **out)
{
    HRESULT hr = E_FAIL;

    ITuningSpace *space = NULL;
    ITuneRequest *request = NULL;
    ILocator *locator = NULL;

    if (tune == NULL || out == NULL)
        return E_POINTER;

    *out = NULL;

    /* create tuning space */
    hr = bda_tuning_space(tune->net, &space);
    if (FAILED(hr)) goto out;

    if (tune->net->set_space != NULL)
    {
        hr = tune->net->set_space(tune, space);
        if (FAILED(hr)) goto out;
    }

    /* create tune request */
    hr = ITuningSpace_CreateTuneRequest(space, &request);
    ASC_WANT_PTR(hr, request);
    if (FAILED(hr)) goto out;

    if (tune->net->set_request != NULL)
    {
        hr = tune->net->set_request(tune, request);
        if (FAILED(hr)) goto out;
    }

    /* set up locator */
    hr = ITuningSpace_get_DefaultLocator(space, &locator);
    ASC_WANT_PTR(hr, locator);
    if (FAILED(hr)) goto out;

    hr = set_locator_generic(tune, locator);
    if (FAILED(hr)) goto out;

    if (tune->net->set_locator != NULL)
    {
        hr = tune->net->set_locator(tune, locator);
        if (FAILED(hr)) goto out;
    }

    hr = ITuneRequest_put_Locator(request, locator);
    if (FAILED(hr)) goto out;

    /* return tune request */
    ITuneRequest_AddRef(request);
    *out = request;

out:
    ASC_RELEASE(locator);
    ASC_RELEASE(request);
    ASC_RELEASE(space);

    return hr;
}
