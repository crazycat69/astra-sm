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
HRESULT set_system_type(ITuningSpace *spc, DVBSystemType type)
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

/*
 * ATSC
 */

static
const bda_network_t net_atsc =
{
    .name = { "atsc" },

    .provider = &CLSID_ATSCNetworkProvider,
    .locator = &CLSID_ATSCLocator,
    .tuning_space = &CLSID_ATSCTuningSpace,
    .network_type = &ATSC_TERRESTRIAL_TV_NETWORK_TYPE,
};

/*
 * CQAM
 */

static
const bda_network_t net_cqam =
{
    .name = { "cqam" },

    .provider = NULL, /* not supported by legacy providers */
    .locator = &CLSID_DigitalCableLocator,
    .tuning_space = &CLSID_DigitalCableTuningSpace,
    .network_type = &DIGITAL_CABLE_NETWORK_TYPE,
};

/*
 * DVB-C
 */

static
HRESULT space_dvbc(ITuningSpace *spc)
{
    return set_system_type(spc, DVB_Cable);
}

static
const bda_network_t net_dvbc =
{
    .name = { "dvbc", "c" },

    .provider = &CLSID_DVBCNetworkProvider,
    .locator = &CLSID_DVBCLocator,
    .tuning_space = &CLSID_DVBTuningSpace,
    .network_type = &DVB_CABLE_TV_NETWORK_TYPE,

    .init_tuning_space = space_dvbc,
};

/*
 * DVB-S
 */

static
HRESULT space_dvbs(ITuningSpace *spc)
{
    IDVBSTuningSpace *spc_dvbs = NULL;
    HRESULT hr = ITuningSpace_QueryInterface(spc, &IID_IDVBSTuningSpace
                                             , (void **)&spc_dvbs);
    if (FAILED(hr))
        return hr;

    /* use universal LNB settings by default */
    hr = IDVBSTuningSpace_put_LowOscillator(spc_dvbs, 9750000);
    if (FAILED(hr)) goto out;

    hr = IDVBSTuningSpace_put_HighOscillator(spc_dvbs, 10600000);
    if (FAILED(hr)) goto out;

    hr = IDVBSTuningSpace_put_LNBSwitch(spc_dvbs, 11700000);
    if (FAILED(hr)) goto out;

    hr = set_system_type(spc, DVB_Satellite);
out:
    SAFE_RELEASE(spc_dvbs);
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

    .init_tuning_space = space_dvbs,
};

/*
 * DVB-S2
 */

static
HRESULT locator_dvbs2(ILocator *loc)
{
    /* check if the OS supports DVB-S2 */
    IDVBSLocator2 *sloc2 = NULL;
    HRESULT hr = ILocator_QueryInterface(loc, &IID_IDVBSLocator2
                                         , (void **)&sloc2);
    if (FAILED(hr))
        return hr;

    SAFE_RELEASE(sloc2);
    return S_OK;
}

static
const bda_network_t net_dvbs2 =
{
    .name = { "dvbs2", "s2" },

    .provider = &CLSID_DVBSNetworkProvider,
    .locator = &CLSID_DVBSLocator,
    .tuning_space = &CLSID_DVBSTuningSpace,
    .network_type = &DVB_SATELLITE_TV_NETWORK_TYPE,

    .init_locator = locator_dvbs2,
    .init_tuning_space = space_dvbs,
};

/*
 * DVB-T
 */

static
HRESULT space_dvbt(ITuningSpace *spc)
{
    return set_system_type(spc, DVB_Terrestrial);
}

static
const bda_network_t net_dvbt =
{
    .name = { "dvbt", "t" },

    .provider = &CLSID_DVBTNetworkProvider,
    .locator = &CLSID_DVBTLocator,
    .tuning_space = &CLSID_DVBTuningSpace,
    .network_type = &DVB_TERRESTRIAL_TV_NETWORK_TYPE,

    .init_tuning_space = space_dvbt,
};

/*
 * DVB-T2
 */

static
const bda_network_t net_dvbt2 =
{
    .name = { "dvbt2", "t2" },

    .provider = &CLSID_DVBTNetworkProvider,
    .locator = &CLSID_DVBTLocator2,
    .tuning_space = &CLSID_DVBTuningSpace,
    .network_type = &DVB_TERRESTRIAL_TV_NETWORK_TYPE,

    .init_tuning_space = space_dvbt,
};

/*
 * ISDB-S
 */

static
HRESULT space_isdbs(ITuningSpace *spc)
{
    return set_system_type(spc, ISDB_Satellite);
}

static
const bda_network_t net_isdbs =
{
    .name = { "isdbs" },

    .provider = NULL, /* not supported by legacy providers */
    .locator = &CLSID_ISDBSLocator,
    .tuning_space = &CLSID_DVBSTuningSpace,
    .network_type = &ISDB_SATELLITE_TV_NETWORK_TYPE,

    .init_tuning_space = space_isdbs,
};

/*
 * ISDB-T
 */

static
HRESULT space_isdbt(ITuningSpace *spc)
{
    return set_system_type(spc, ISDB_Terrestrial);
}

static
const bda_network_t net_isdbt =
{
    .name = { "isdbt" },

    .provider = NULL, /* not supported by legacy providers */
    .locator = &CLSID_DVBTLocator,
    .tuning_space = &CLSID_DVBTuningSpace,
    .network_type = &ISDB_TERRESTRIAL_TV_NETWORK_TYPE,

    .init_tuning_space = space_isdbt,
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

/* create BDA tuning space for given network type */
HRESULT bda_tuning_space(const bda_network_t *net, ITuningSpace **out)
{
    HRESULT hr = E_FAIL;

    ILocator *locator = NULL;
    ITuningSpace *space = NULL;
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

    /* create locator */
    hr = CoCreateInstance(net->locator, NULL, CLSCTX_INPROC
                          , &IID_ILocator, (void **)&locator);
    if (FAILED(hr)) goto out;

    if (net->init_locator != NULL)
    {
        hr = net->init_locator(locator);
        if (FAILED(hr)) goto out;
    }

    /* set up tuning space */
    hr = CoCreateInstance(net->tuning_space, NULL, CLSCTX_INPROC
                          , &IID_ITuningSpace, (void **)&space);
    if (FAILED(hr)) goto out;

    hr = ITuningSpace_put__NetworkType(space, net->network_type);
    if (FAILED(hr)) goto out;

    hr = ITuningSpace_put_FriendlyName(space, name);
    if (FAILED(hr)) goto out;

    hr = ITuningSpace_put_UniqueName(space, name);
    if (FAILED(hr)) goto out;

    if (net->init_tuning_space != NULL)
    {
        hr = net->init_tuning_space(space);
        if (FAILED(hr)) goto out;
    }

    hr = ITuningSpace_put_DefaultLocator(space, locator);
    if (FAILED(hr)) goto out;

    ITuningSpace_AddRef(space);
    *out = space;

out:
    SAFE_RELEASE(space);
    SAFE_RELEASE(locator);

    ASC_FREE(name, SysFreeString);

    return hr;
}
