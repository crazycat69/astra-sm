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

/* set SystemType on DVB tuning spaces */
static
HRESULT init_space_dvbx(ITuningSpace *spc, DVBSystemType type)
{
    IDVBTuningSpace2 *spc_dvb = NULL;
    HRESULT hr = ITuningSpace_QueryInterface(spc, &IID_IDVBTuningSpace2
                                             , (void **)&spc_dvb);
    if (FAILED(hr))
        return hr;

    hr = IDVBTuningSpace2_put_SystemType(spc_dvb, type);
    SAFE_RELEASE(spc_dvb);

    return hr;
}

/* fill in basic locator properties from user tuning command */
static
HRESULT set_locator_generic(const bda_tune_cmd_t *tune, ILocator *loc)
{
    HRESULT hr = ILocator_put_CarrierFrequency(loc, tune->frequency);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_InnerFEC(loc, tune->fec_mode);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_InnerFECRate(loc, tune->fec);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_Modulation(loc, tune->modulation);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_OuterFEC(loc, tune->outer_fec_mode);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_OuterFECRate(loc, tune->outer_fec);
    if (FAILED(hr)) return hr;

    hr = ILocator_put_SymbolRate(loc, tune->symbolrate);
    return hr;
}

/*
 * ATSC
 */

static
HRESULT init_space_atsc(ITuningSpace *spc)
{
    IATSCTuningSpace *spc_atsc = NULL;
    HRESULT hr = ITuningSpace_QueryInterface(spc, &IID_IATSCTuningSpace
                                             , (void **)&spc_atsc);
    if (FAILED(hr))
        return hr;

    hr = IATSCTuningSpace_put_MaxChannel(spc_atsc, 9999);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_MinChannel(spc_atsc, 0);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_MaxMinorChannel(spc_atsc, 9999);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_MinMinorChannel(spc_atsc, 0);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_MaxPhysicalChannel(spc_atsc, 9999);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_MinPhysicalChannel(spc_atsc, 0);
out:
    SAFE_RELEASE(spc_atsc);
    return hr;
}

static
HRESULT set_space_atsc(const bda_tune_cmd_t *tune, ITuningSpace *spc)
{
    IATSCTuningSpace *spc_atsc = NULL;
    HRESULT hr = ITuningSpace_QueryInterface(spc, &IID_IATSCTuningSpace
                                             , (void **)&spc_atsc);
    if (FAILED(hr))
        return hr;

    hr = IATSCTuningSpace_put_CountryCode(spc_atsc, tune->country_code);
    if (FAILED(hr)) goto out;

    hr = IATSCTuningSpace_put_InputType(spc_atsc, tune->input_type);
out:
    SAFE_RELEASE(spc_atsc);
    return hr;
}

static
HRESULT set_request_atsc(const bda_tune_cmd_t *tune, ITuneRequest *req)
{
    IATSCChannelTuneRequest *req_atsc = NULL;
    HRESULT hr = ITuneRequest_QueryInterface(req, &IID_IATSCChannelTuneRequest
                                             , (void **)&req_atsc);
    if (FAILED(hr))
        return hr;

    hr = req_atsc->lpVtbl->put_Channel(req_atsc, tune->major_channel);
    if (FAILED(hr)) goto out;

    hr = req_atsc->lpVtbl->put_MinorChannel(req_atsc, tune->minor_channel);
out:
    SAFE_RELEASE(req_atsc);
    return hr;
}

static
HRESULT set_locator_atsc(const bda_tune_cmd_t *tune, ILocator *loc)
{
    IATSCLocator *loc_atsc = NULL;
    HRESULT hr = ILocator_QueryInterface(loc, &IID_IATSCLocator
                                         , (void **)&loc_atsc);
    if (FAILED(hr))
        return hr;

    hr = IATSCLocator_put_PhysicalChannel(loc_atsc, tune->stream_id);

    SAFE_RELEASE(loc_atsc);
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
HRESULT init_space_cqam(ITuningSpace *spc)
{
    IDigitalCableTuningSpace *spc_cqam = NULL;
    HRESULT hr = ITuningSpace_QueryInterface(spc, &IID_IDigitalCableTuningSpace
                                             , (void **)&spc_cqam);
    if (FAILED(hr))
        return hr;

    hr = spc_cqam->lpVtbl->put_MaxMajorChannel(spc_cqam, 9999);
    if (FAILED(hr)) goto out;

    hr = spc_cqam->lpVtbl->put_MinMajorChannel(spc_cqam, 0);
    if (FAILED(hr)) goto out;

    hr = spc_cqam->lpVtbl->put_MaxSourceID(spc_cqam, INT32_MAX);
    if (FAILED(hr)) goto out;

    hr = spc_cqam->lpVtbl->put_MinSourceID(spc_cqam, 0);
    if (FAILED(hr)) goto out;

    hr = init_space_atsc(spc); /* delegate to ATSC */
out:
    SAFE_RELEASE(spc_cqam);
    return hr;
}

static
HRESULT set_request_cqam(const bda_tune_cmd_t *tune, ITuneRequest *req)
{
    IDigitalCableTuneRequest *req_cqam = NULL;
    HRESULT hr = ITuneRequest_QueryInterface(req, &IID_IDigitalCableTuneRequest
                                             , (void **)&req_cqam);
    if (FAILED(hr))
        return hr;

    hr = req_cqam->lpVtbl->put_MajorChannel(req_cqam, tune->major_channel);
    if (FAILED(hr)) goto out;

    hr = req_cqam->lpVtbl->put_MinorChannel(req_cqam, tune->minor_channel);
    if (FAILED(hr)) goto out;

    hr = req_cqam->lpVtbl->put_Channel(req_cqam, tune->virtual_channel);
out:
    SAFE_RELEASE(req_cqam);
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
HRESULT init_space_dvbc(ITuningSpace *spc)
{
    return init_space_dvbx(spc, DVB_Cable);
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
HRESULT init_space_dvbs(ITuningSpace *spc)
{
    return init_space_dvbx(spc, DVB_Satellite);
}

static
HRESULT set_space_dvbs(const bda_tune_cmd_t *tune, ITuningSpace *spc)
{
    IDVBSTuningSpace *spc_s = NULL;
    HRESULT hr = ITuningSpace_QueryInterface(spc, &IID_IDVBSTuningSpace
                                             , (void **)&spc_s);
    if (FAILED(hr))
        return hr;

    hr = IDVBSTuningSpace_put_LowOscillator(spc_s, tune->lof1);
    if (FAILED(hr)) goto out;

    hr = IDVBSTuningSpace_put_HighOscillator(spc_s, tune->lof2);
    if (FAILED(hr)) goto out;

    hr = IDVBSTuningSpace_put_LNBSwitch(spc_s, tune->slof);
    if (FAILED(hr)) goto out;

    hr = IDVBSTuningSpace_put_SpectralInversion(spc_s, tune->inversion);
out:
    SAFE_RELEASE(spc_s);
    return hr;
}

static
HRESULT set_locator_dvbs(const bda_tune_cmd_t *tune, ILocator *loc)
{
    IDVBSLocator *loc_s = NULL;
    HRESULT hr = ILocator_QueryInterface(loc, &IID_IDVBSLocator
                                         , (void **)&loc_s);
    if (FAILED(hr))
        return hr;

    hr = IDVBSLocator_put_SignalPolarisation(loc_s, tune->polarization);

    SAFE_RELEASE(loc_s);
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
HRESULT init_locator_dvbs2(ILocator *loc)
{
    /* check if the OS supports DVB-S2 */
    IDVBSLocator2 *loc_s2 = NULL;
    HRESULT hr = ILocator_QueryInterface(loc, &IID_IDVBSLocator2
                                         , (void **)&loc_s2);
    if (FAILED(hr))
        return hr;

    SAFE_RELEASE(loc_s2);
    return S_OK;
}

static
HRESULT set_locator_dvbs2(const bda_tune_cmd_t *tune, ILocator *loc)
{
    IDVBSLocator2 *loc_s2 = NULL;
    HRESULT hr = ILocator_QueryInterface(loc, &IID_IDVBSLocator2
                                         , (void **)&loc_s2);
    if (FAILED(hr))
        return hr;

    hr = IDVBSLocator2_put_SignalPilot(loc_s2, tune->pilot);
    if (FAILED(hr)) goto out;

    hr = IDVBSLocator2_put_SignalRollOff(loc_s2, tune->rolloff);
    if (FAILED(hr)) goto out;

    hr = set_locator_dvbs(tune, loc); /* delegate to DVB-S */
out:
    SAFE_RELEASE(loc_s2);
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
HRESULT init_space_dvbt(ITuningSpace *spc)
{
    return init_space_dvbx(spc, DVB_Terrestrial);
}

static
HRESULT set_locator_dvbt(const bda_tune_cmd_t *tune, ILocator *loc)
{
    IDVBTLocator *loc_t = NULL;
    HRESULT hr = ILocator_QueryInterface(loc, &IID_IDVBTLocator
                                         , (void **)&loc_t);
    if (FAILED(hr))
        return hr;

    hr = IDVBTLocator_put_Bandwidth(loc_t, tune->bandwidth);
    if (FAILED(hr)) goto out;

    hr = IDVBTLocator_put_Guard(loc_t, tune->guardinterval);
    if (FAILED(hr)) goto out;

    hr = IDVBTLocator_put_HAlpha(loc_t, tune->hierarchy);
    if (FAILED(hr)) goto out;

    hr = IDVBTLocator_put_LPInnerFEC(loc_t, tune->lp_fec_mode);
    if (FAILED(hr)) goto out;

    hr = IDVBTLocator_put_LPInnerFECRate(loc_t, tune->lp_fec);
    if (FAILED(hr)) goto out;

    hr = IDVBTLocator_put_Mode(loc_t, tune->transmitmode);
out:
    SAFE_RELEASE(loc_t);
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
HRESULT set_locator_dvbt2(const bda_tune_cmd_t *tune, ILocator *loc)
{
    IDVBTLocator2 *loc_t2 = NULL;
    HRESULT hr = ILocator_QueryInterface(loc, &IID_IDVBTLocator2
                                         , (void **)&loc_t2);
    if (FAILED(hr))
        return hr;

    hr = IDVBTLocator2_put_PhysicalLayerPipeId(loc_t2, tune->stream_id);
    if (FAILED(hr)) goto out;

    hr = set_locator_dvbt(tune, loc); /* delegate to DVB-T */
out:
    SAFE_RELEASE(loc_t2);
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
HRESULT init_space_isdbs(ITuningSpace *spc)
{
    return init_space_dvbx(spc, ISDB_Satellite);
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
HRESULT init_space_isdbt(ITuningSpace *spc)
{
    return init_space_dvbx(spc, ISDB_Terrestrial);
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
    *out = NULL;

    /* try the universal provider first (available since Windows 7) */
    HRESULT hr = CoCreateInstance(&CLSID_NetworkProvider, NULL, CLSCTX_INPROC
                                  , &IID_IBaseFilter, (void **)out);

    if (FAILED(hr) && net->provider != NULL)
    {
        /* fall back to legacy provider if the network type supports it */
        hr = CoCreateInstance(net->provider, NULL, CLSCTX_INPROC
                              , &IID_IBaseFilter, (void **)out);
    }

    return hr;
}

/* create tuning space for given network type */
HRESULT bda_tuning_space(const bda_network_t *net, ITuningSpace **out)
{
    HRESULT hr = E_FAIL;

    ILocator *loc = NULL;
    ITuningSpace *spc = NULL;
    BSTR name = NULL;

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
    hr = CoCreateInstance(net->locator, NULL, CLSCTX_INPROC
                          , &IID_ILocator, (void **)&loc);
    if (FAILED(hr)) goto out;

    if (net->init_default_locator != NULL)
    {
        hr = net->init_default_locator(loc);
        if (FAILED(hr)) goto out;
    }

    /* set up tuning space */
    hr = CoCreateInstance(net->tuning_space, NULL, CLSCTX_INPROC
                          , &IID_ITuningSpace, (void **)&spc);
    if (FAILED(hr)) goto out;

    hr = ITuningSpace_put__NetworkType(spc, net->network_type);
    if (FAILED(hr)) goto out;

    hr = ITuningSpace_put_FriendlyName(spc, name);
    if (FAILED(hr)) goto out;

    hr = ITuningSpace_put_UniqueName(spc, name);
    if (FAILED(hr)) goto out;

    if (net->init_space != NULL)
    {
        hr = net->init_space(spc);
        if (FAILED(hr)) goto out;
    }

    hr = ITuningSpace_put_DefaultLocator(spc, loc);
    if (FAILED(hr)) goto out;

    ITuningSpace_AddRef(spc);
    *out = spc;

out:
    SAFE_RELEASE(spc);
    SAFE_RELEASE(loc);

    ASC_FREE(name, SysFreeString);

    return hr;
}

/* create tune request based on a user tuning command */
HRESULT bda_tune_request(const bda_tune_cmd_t *tune, ITuneRequest **out)
{
    HRESULT hr = E_FAIL;

    ITuningSpace *spc = NULL;
    ITuneRequest *req = NULL;
    ILocator *loc = NULL;

    *out = NULL;

    /* create tuning space */
    hr = bda_tuning_space(tune->net, &spc);
    if (FAILED(hr)) goto out;

    if (tune->net->set_space != NULL)
    {
        hr = tune->net->set_space(tune, spc);
        if (FAILED(hr)) goto out;
    }

    /* create tune request */
    hr = ITuningSpace_CreateTuneRequest(spc, &req);
    if (FAILED(hr)) goto out;

    if (tune->net->set_request != NULL)
    {
        hr = tune->net->set_request(tune, req);
        if (FAILED(hr)) goto out;
    }

    /* set up locator */
    hr = ITuningSpace_get_DefaultLocator(spc, &loc);
    if (FAILED(hr)) goto out;

    hr = set_locator_generic(tune, loc);
    if (FAILED(hr)) goto out;

    if (tune->net->set_locator != NULL)
    {
        hr = tune->net->set_locator(tune, loc);
        if (FAILED(hr)) goto out;
    }

    hr = ITuneRequest_put_Locator(req, loc);
    if (FAILED(hr)) goto out;

    /* return tune request */
    ITuneRequest_AddRef(req);
    *out = req;

out:
    SAFE_RELEASE(loc);
    SAFE_RELEASE(req);
    SAFE_RELEASE(spc);

    return hr;
}
