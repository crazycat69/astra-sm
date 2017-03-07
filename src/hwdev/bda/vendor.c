/*
 * Astra Module: BDA (Vendor extensions)
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

/*
 * NOTE: Most proprietary BDA extensions can be accessed using
 *       the IKsPropertySet interface implemented on one of the pins.
 */

static
HRESULT generic_init(IBaseFilter *filters[], void **data
                     , const GUID *prop_set, DWORD prop_id)
{
    HRESULT hr = E_NOTIMPL;

    for (; *filters != NULL; filters++)
    {
        IKsPropertySet *prop = NULL;
        hr = dshow_find_ksprop(*filters, prop_set, prop_id, &prop);

        if (SUCCEEDED(hr))
        {
            *data = prop;
            break;
        }
    }

    return hr;
}

static
void generic_destroy(void *data)
{
    IKsPropertySet *prop = (IKsPropertySet *)data;
    ASC_RELEASE(prop);
}

/*
 * TurboSight
 */

/* property set for PCIe devices */
static
const GUID KSPROPSETID_BdaTunerExtensionProperties =
    {0xfaa8f3e5,0x31d4,0x4e41,{0x88,0xef,0xd9,0xeb,0x71,0x6f,0x6e,0xc9}};

enum
{
    KSPROPERTY_BDA_ACCESS = 18,
    KSPROPERTY_BDA_PLPINFO = 22,
    KSPROPERTY_BDA_PLS = 23,
};

/* property set for USB devices */
static
const GUID KSPROPSETID_QBOXControlProperties =
    {0xc6efe5eb,0x855a,0x4f1b,{0xb7,0xaa,0x87,0xb5,0xe1,0xdc,0x41,0x13}};

enum
{
    KSPROPERTY_CTRL_TUNER = 0,
    KSPROPERTY_CTRL_IR,
    KSPROPERTY_CTRL_22K_TONE,
    KSPROPERTY_CTRL_MOTOR,
    KSPROPERTY_CTRL_LNBPW,
    KSPROPERTY_CTRL_LOCK_TUNER,
    KSPROPERTY_CTRL_ACCESS = 18,
    KSPROPERTY_CTRL_PLPINFO = 19,
    KSPROPERTY_CTRL_PLS = 20,
};

typedef struct
{
    uint32_t freq;
    uint32_t lof_low;
    uint32_t lof_high;
    uint32_t sr;
    uint8_t pol;
    uint8_t lnb_pwr;
    uint8_t tone_22khz;
    uint8_t tone_burst;
    uint8_t lnb_source;
    uint8_t diseqc_cmd[5];
    uint8_t ir_code;
    uint8_t lock;
    uint8_t strength;
    uint8_t quality;
    uint8_t reserved[256];
} tbs_usb_cmd_t;

enum
{
    TBS_ACCESS_LNBPOWER = 0,
    TBS_ACCESS_DISEQC,
    TBS_ACCESS_22K,
};

enum
{
    TBS_LNBPOWER_OFF = 0,
    TBS_LNBPOWER_18V,
    TBS_LNBPOWER_13V,
    TBS_LNBPOWER_ON,
};

enum
{
    TBS_BURST_OFF = 0,
    TBS_BURST_ON,
    TBS_BURST_UNMODULATED,
    TBS_BURST_MODULATED,
};

typedef struct
{
    uint32_t access_mode;
    uint32_t tone_mode;
    uint8_t on_off;
    uint32_t lnbpower_mode;
    uint8_t diseqc_send[128];
    uint32_t diseqc_send_len;
    uint8_t diseqc_rcv[128];
    uint32_t diseqc_rcv_len;
    uint8_t reserved[256];
} tbs_access_t;

typedef struct
{
    uint8_t id;
    uint8_t count;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t id_list[256];
} tbs_plp_t;

typedef struct
{
    uint32_t pls_code;
    uint32_t pls_mode;
    uint8_t id_list[256];
} tbs_pls_t;

static
HRESULT tbs_plp_set(void *data, const bda_tune_cmd_t *tune
                    , const GUID *prop_set, DWORD prop_id)
{
    HRESULT hr = S_OK;

    if (tune->stream_id != -1)
    {
        tbs_plp_t plp =
        {
            .id = tune->stream_id,
        };

        IKsPropertySet *const prop = (IKsPropertySet *)data;
        hr = IKsPropertySet_Set(prop, prop_set, prop_id
                                , NULL, 0, &plp, sizeof(plp));
    }

    return hr;
}

static
HRESULT tbs_pls_set(void *data, const bda_tune_cmd_t *tune
                    , const GUID *prop_set, DWORD prop_id)
{
    HRESULT hr = S_OK;

    if (tune->pls_mode != -1 || tune->pls_code != -1)
    {
        tbs_pls_t pls;
        memset(&pls, 0, sizeof(pls));

        if (tune->pls_code != -1)
            pls.pls_code = tune->pls_code;

        if (tune->pls_mode != -1)
            pls.pls_mode = tune->pls_mode;

        IKsPropertySet *const prop = (IKsPropertySet *)data;
        hr = IKsPropertySet_Set(prop, prop_set, prop_id
                                , NULL, 0, &pls, sizeof(pls));
    }

    return hr;
}

static
HRESULT tbs_diseqc_send(void *data, const uint8_t *cmd, unsigned int len
                        , const GUID *prop_set, DWORD prop_id)
{
    tbs_access_t access =
    {
        .access_mode = TBS_ACCESS_DISEQC,
        .diseqc_send_len = len,
    };
    memcpy(access.diseqc_send, cmd, len);

    IKsPropertySet *const prop = (IKsPropertySet *)data;
    return IKsPropertySet_Set(prop, prop_set, prop_id
                              , NULL, 0, &access, sizeof(access));
}

/*
 * TurboSight PCIe PLP ID
 */

static
HRESULT tbs_pcie_plp_set(void *data, const bda_tune_cmd_t *tune)
{
    return tbs_plp_set(data, tune
                       , &KSPROPSETID_BdaTunerExtensionProperties
                       , KSPROPERTY_BDA_PLPINFO);
}

static
HRESULT tbs_pcie_plp_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_BdaTunerExtensionProperties
                        , KSPROPERTY_BDA_PLPINFO);
}

static
const bda_extension_t tbs_pcie_plp =
{
    .name = "tbs_pcie_plp",
    .description = "TurboSight PCIe PLP ID",

    .init = tbs_pcie_plp_init,
    .destroy = generic_destroy,

    .tune_pre = tbs_pcie_plp_set,
};

/*
 * TurboSight PCIe PLS
 */

static
HRESULT tbs_pcie_pls_set(void *data, const bda_tune_cmd_t *tune)
{
    return tbs_pls_set(data, tune
                       , &KSPROPSETID_BdaTunerExtensionProperties
                       , KSPROPERTY_BDA_PLS);
}

static
HRESULT tbs_pcie_pls_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_BdaTunerExtensionProperties
                        , KSPROPERTY_BDA_PLS);
}

static
const bda_extension_t tbs_pcie_pls =
{
    .name = "tbs_pcie_pls",
    .description = "TurboSight PCIe PLS",

    .init = tbs_pcie_pls_init,
    .destroy = generic_destroy,

    .tune_pre = tbs_pcie_pls_set,
};

/*
 * TurboSight PCIe DiSEqC
 */

static
HRESULT tbs_pcie_diseqc_send(void *data, const uint8_t *cmd, unsigned int len)
{
    return tbs_diseqc_send(data, cmd, len
                           , &KSPROPSETID_BdaTunerExtensionProperties
                           , KSPROPERTY_BDA_ACCESS);
}

static
HRESULT tbs_pcie_diseqc_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_BdaTunerExtensionProperties
                        , KSPROPERTY_BDA_ACCESS);
}

static
const bda_extension_t tbs_pcie_diseqc =
{
    .name = "tbs_pcie_diseqc",
    .description = "TurboSight PCIe DiSEqC",
    .flags = BDA_EXT_DISEQC,

    //
    // TODO: add lnb power, tone burst, 22k
    //

    .init = tbs_pcie_diseqc_init,
    .destroy = generic_destroy,

    .diseqc = tbs_pcie_diseqc_send,
};

/*
 * TurboSight USB PLP ID
 */

static
HRESULT tbs_usb_plp_set(void *data, const bda_tune_cmd_t *tune)
{
    return tbs_plp_set(data, tune
                       , &KSPROPSETID_QBOXControlProperties
                       , KSPROPERTY_CTRL_PLPINFO);
}

static
HRESULT tbs_usb_plp_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_QBOXControlProperties
                        , KSPROPERTY_CTRL_PLPINFO);
}

static
const bda_extension_t tbs_usb_plp =
{
    .name = "tbs_usb_plp",
    .description = "TurboSight USB PLP ID",

    .init = tbs_usb_plp_init,
    .destroy = generic_destroy,

    .tune_pre = tbs_usb_plp_set,
};

/*
 * TurboSight USB PLS
 */

static
HRESULT tbs_usb_pls_set(void *data, const bda_tune_cmd_t *tune)
{
    return tbs_pls_set(data, tune
                       , &KSPROPSETID_QBOXControlProperties
                       , KSPROPERTY_CTRL_PLS);
}

static
HRESULT tbs_usb_pls_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_QBOXControlProperties
                        , KSPROPERTY_CTRL_PLS);
}

static
const bda_extension_t tbs_usb_pls =
{
    .name = "tbs_usb_pls",
    .description = "TurboSight USB PLS",

    .init = tbs_usb_pls_init,
    .destroy = generic_destroy,

    .tune_pre = tbs_usb_pls_set,
};

/*
 * TurboSight USB DiSEqC
 */

static
HRESULT tbs_usb_diseqc_send(void *data, const uint8_t *cmd, unsigned int len)
{
    return tbs_diseqc_send(data, cmd, len
                           , &KSPROPSETID_QBOXControlProperties
                           , KSPROPERTY_CTRL_ACCESS);
}

static
HRESULT tbs_usb_diseqc_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_QBOXControlProperties
                        , KSPROPERTY_CTRL_ACCESS);
}

static
const bda_extension_t tbs_usb_diseqc =
{
    .name = "tbs_usb_diseqc",
    .description = "TurboSight USB DiSEqC",
    .flags = BDA_EXT_DISEQC,

    // TODO: see tbs_pcie_diseqc

    .init = tbs_usb_diseqc_init,
    .destroy = generic_destroy,

    .diseqc = tbs_usb_diseqc_send,
};

/*
 * Omicom S2 PCI
 */

/* DiSEqC */
static
const GUID KSPROPSETID_OmcDiSEqCProperties =
    {0x7db2deeal,0x42b4,0x423d,{0xa2,0xf7,0x19,0xc3,0x2e,0x51,0xcc,0xc1}};

enum
{
    KSPROPERTY_OMC_DISEQC_WRITE = 0,
    KSPROPERTY_OMC_DISEQC_READ,
    KSPROPERTY_OMC_DISEQC_SET22K,
    KSPROPERTY_OMC_DISEQC_ENCABLOSSCOMP,
    KSPROPERTY_OMC_DISEQC_TONEBURST,
};

typedef struct
{
    uint32_t len;
    uint8_t buf[64];
    uint32_t repeat;
} omc_diseqc_t;

/* custom properties */
static
const GUID KSPROPSETID_OmcCustomProperties =
    {0x7db2dee6l,0x42b4,0x423d,{0xa2,0xf7,0x19,0xc3,0x2e,0x51,0xcc,0xc1}};

enum
{
    KSPROPERTY_OMC_CUSTOM_SIGNAL_OFFSET = 0,
    KSPROPERTY_OMC_CUSTOM_SEARCH_MODE,
    KSPROPERTY_OMC_CUSTOM_SEARCH_RANGE,
    KSPROPERTY_OMC_CUSTOM_SEARCH,
    KSPROPERTY_OMC_CUSTOM_SIGNAL_INFO,
    KSPROPERTY_OMC_CUSTOM_STREAM_INFO,
    KSPROPERTY_OMC_CUSTOM_MIS_FILTER,
    KSPROPERTY_OMC_CUSTOM_RFSCAN,
    KSPROPERTY_OMC_CUSTOM_IQSCAN,
    KSPROPERTY_OMC_CUSTOM_PLS_SCRAM,
};

typedef struct
{
    uint32_t pls_mode;
    uint32_t pls_code;
} omc_pls_t;

/*
 * Omicom S2 PCI ISI
 */

static
HRESULT omc_pci_isi_set(void *data, const bda_tune_cmd_t *tune)
{
    HRESULT hr = S_OK;

    if (tune->stream_id != -1)
    {
        uint32_t isi = tune->stream_id & 0xff;

        IKsPropertySet *const prop = (IKsPropertySet *)data;
        hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_OmcCustomProperties
                                , KSPROPERTY_OMC_CUSTOM_MIS_FILTER
                                , NULL, 0, &isi, sizeof(isi));
    }

    return hr;
}

static
HRESULT omc_pci_isi_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_OmcCustomProperties
                        , KSPROPERTY_OMC_CUSTOM_MIS_FILTER);
}

static
const bda_extension_t omc_pci_isi =
{
    .name = "omc_pci_isi",
    .description = "Omicom S2 PCI ISI",

    .init = omc_pci_isi_init,
    .destroy = generic_destroy,

    .tune_post = omc_pci_isi_set,
};

/*
 * Omicom S2 PCI PLS
 */

static
HRESULT omc_pci_pls_set(void *data, const bda_tune_cmd_t *tune)
{
    HRESULT hr = S_OK;

    if (tune->pls_mode != -1 || tune->pls_code != -1)
    {
        omc_pls_t pls;
        memset(&pls, 0, sizeof(pls));

        if (tune->pls_code != -1)
            pls.pls_code = tune->pls_code;

        if (tune->pls_mode != -1)
            pls.pls_mode = tune->pls_mode;

        IKsPropertySet *const prop = (IKsPropertySet *)data;
        hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_OmcCustomProperties
                                , KSPROPERTY_OMC_CUSTOM_PLS_SCRAM
                                , NULL, 0, &pls, sizeof(pls));
    }

    return hr;
}

static
HRESULT omc_pci_pls_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_OmcCustomProperties
                        , KSPROPERTY_OMC_CUSTOM_PLS_SCRAM);
}

static
const bda_extension_t omc_pci_pls =
{
    .name = "omc_pci_pls",
    .description = "Omicom S2 PCI PLS",

    .init = omc_pci_pls_init,
    .destroy = generic_destroy,

    .tune_post = omc_pci_pls_set,
};

/*
 * Omicom S2 PCI DiSEqC
 */

static
HRESULT omc_pci_diseqc_send(void *data, const uint8_t *cmd, unsigned int len)
{
    omc_diseqc_t diseqc =
    {
        .len = len,
        .repeat = 1,
    };
    memcpy(&diseqc.buf, cmd, len);

    IKsPropertySet *const prop = (IKsPropertySet *)data;
    return IKsPropertySet_Set(prop, &KSPROPSETID_OmcDiSEqCProperties
                              , KSPROPERTY_OMC_DISEQC_WRITE, NULL, 0
                              , &diseqc, sizeof(diseqc));
}

static
HRESULT omc_pci_diseqc_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_OmcDiSEqCProperties
                        , KSPROPERTY_OMC_DISEQC_WRITE);
}

static
const bda_extension_t omc_pci_diseqc =
{
    .name = "omc_pci_diseqc",
    .description = "Omicom S2 PCI DiSEqC",
    .flags = BDA_EXT_DISEQC,

    // TODO: add 22kHz, tone burst

    .init = omc_pci_diseqc_init,
    .destroy = generic_destroy,

    .diseqc = omc_pci_diseqc_send,
};

/*
 * Microsoft PID Filter
 */

static
HRESULT ms_pidmap_set(void *data, uint16_t pid, bool join)
{
    IMPEG2PIDMap *const pidmap = (IMPEG2PIDMap *)data;

    // TODO
    ASC_UNUSED(pidmap);
    ASC_UNUSED(pid);
    ASC_UNUSED(join);

    return E_NOTIMPL;
}

static
HRESULT ms_pidmap_bulk(void *data, const bool pids[TS_MAX_PID])
{
    IMPEG2PIDMap *const pidmap = (IMPEG2PIDMap *)data;

    // TODO
    ASC_UNUSED(pidmap);
    ASC_UNUSED(pids);

    return E_NOTIMPL;
}

static
HRESULT ms_pidmap_init(IBaseFilter *filters[], void **data)
{
    // TODO
    ASC_UNUSED(filters);
    ASC_UNUSED(data);

    return E_NOTIMPL;
}

static
void ms_pidmap_destroy(void *data)
{
    IMPEG2PIDMap *pidmap = (IMPEG2PIDMap *)data;
    ASC_RELEASE(pidmap);
}

static
const bda_extension_t ms_pidmap =
{
    .name = "ms_pidmap",
    .description = "Microsoft PID Filter",
    .flags = BDA_EXT_PIDMAP,

    .init = ms_pidmap_init,
    .destroy = ms_pidmap_destroy,

    .pid_set = ms_pidmap_set,
    .pid_bulk = ms_pidmap_bulk,
};

/*
 * public API
 */

/* list of supported BDA extensions */
static
const bda_extension_t *const ext_list[] =
{
    /* TurboSight */
    &tbs_pcie_plp,
    &tbs_pcie_pls,
    &tbs_pcie_diseqc,
    &tbs_usb_plp,
    &tbs_usb_pls,
    &tbs_usb_diseqc,

    /* Omicom */
    &omc_pci_isi,
    &omc_pci_pls,
    &omc_pci_diseqc,

    /* Microsoft */
    &ms_pidmap,

    NULL,
};

/* probe device filters for known extensions */
HRESULT bda_ext_init(module_data_t *mod, IBaseFilter *filters[])
{
    HRESULT out_hr = S_OK;

    for (size_t i = 0; ext_list[i] != NULL; i++)
    {
        const bda_extension_t *const ext = ext_list[i];

        if (mod->ext_flags & ext->flags)
        {
            asc_log_debug(MSG("skipping extension: %s"), ext->name);
            continue;
        }

        void *data = NULL;
        const HRESULT hr = ext->init(filters, &data);

        if (SUCCEEDED(hr))
        {
            bda_extension_t *const item = ASC_ALLOC(1, bda_extension_t);

            memcpy(item, ext, sizeof(*item));
            item->data = data;

            asc_list_insert_tail(mod->extensions, item);
            mod->ext_flags |= ext->flags;

            asc_log_debug(MSG("added vendor extension: %s (%s)")
                          , ext->name, ext->description);
        }
        else if (hr != E_NOTIMPL)
        {
            BDA_ERROR_D(hr, "probe for %s extension failed", ext->name);
            out_hr = hr;
        }
    }

    return out_hr;
}

/* clean up extension private data */
void bda_ext_destroy(module_data_t *mod)
{
    asc_list_clear(mod->extensions)
    {
        bda_extension_t *const ext =
            (bda_extension_t *)asc_list_data(mod->extensions);

        ext->destroy(ext->data);
        free(ext);
    }

    mod->ext_flags = 0;
}

/* send additional tuning data */
HRESULT bda_ext_tune(module_data_t *mod, const bda_tune_cmd_t *tune
                     , bda_tune_hook_t when)
{
    /* don't report error if no extensions provide tuning hooks */
    HRESULT out_hr = S_OK;

    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
            (bda_extension_t *)asc_list_data(mod->extensions);

        HRESULT hr = S_OK;

        if (when == BDA_TUNE_PRE && ext->tune_pre != NULL)
            hr = ext->tune_pre(ext->data, tune);

        if (when == BDA_TUNE_POST && ext->tune_post != NULL)
            hr = ext->tune_post(ext->data, tune);

        if (FAILED(hr))
        {
            BDA_ERROR_D(hr, "couldn't send tuning data for %s", ext->name);
            out_hr = hr;
        }
    }

    return out_hr;
}

/* send raw DiSEqC command */
HRESULT bda_ext_diseqc(module_data_t *mod, const uint8_t *cmd
                       , unsigned int len)
{
    HRESULT hr = E_NOTIMPL;

    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
            (bda_extension_t *)asc_list_data(mod->extensions);

        if (ext->diseqc != NULL)
        {
            hr = ext->diseqc(ext->data, cmd, len);
            if (FAILED(hr))
            {
                BDA_ERROR_D(hr, "couldn't send DiSEqC command via '%s'"
                            , ext->name);
            }
        }
    }

    return hr;
}

/* switch LNB power and voltage */
HRESULT bda_ext_lnbpower(module_data_t *mod, bda_lnbpower_mode_t mode)
{
    HRESULT hr = E_NOTIMPL;

    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
                (bda_extension_t *)asc_list_data(mod->extensions);

        if (ext->lnbpower != NULL)
        {
            hr = ext->lnbpower(ext->data, mode);
            if (FAILED(hr))
            {
                BDA_ERROR_D(hr, "couldn't set LNB power mode via '%s'"
                            , ext->name);
            }
        }
    }

    return hr;
}

/* toggle 22kHz tone */
HRESULT bda_ext_22k(module_data_t *mod, bda_22k_mode_t mode)
{
    HRESULT hr = E_NOTIMPL;

    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
                (bda_extension_t *)asc_list_data(mod->extensions);

        if (ext->t22k != NULL)
        {
            hr = ext->t22k(ext->data, mode);
            if (FAILED(hr))
            {
                BDA_ERROR_D(hr, "couldn't set 22kHz tone mode via '%s'"
                            , ext->name);
            }
        }
    }

    return hr;
}

/* switch mini-DiSEqC input */
HRESULT bda_ext_toneburst(module_data_t *mod, bda_toneburst_mode_t mode)
{
    HRESULT hr = E_NOTIMPL;

    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
                (bda_extension_t *)asc_list_data(mod->extensions);

        if (ext->toneburst != NULL)
        {
            hr = ext->toneburst(ext->data, mode);
            if (FAILED(hr))
            {
                BDA_ERROR_D(hr, "couldn't set tone burst mode via '%s'"
                            , ext->name);
            }
        }
    }

    return hr;
}

/* map or unmap a single PID */
HRESULT bda_ext_pid_set(module_data_t *mod, uint16_t pid, bool join)
{
    HRESULT hr = E_NOTIMPL;

    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
                (bda_extension_t *)asc_list_data(mod->extensions);

        if (ext->pid_set != NULL)
        {
            hr = ext->pid_set(ext->data, pid, join);
            if (FAILED(hr))
            {
                BDA_ERROR_D(hr, "couldn't add or remove PID via '%s'"
                            , ext->name);
            }
        }
    }

    return hr;
}

/* load a complete PID list into filter */
HRESULT bda_ext_pid_bulk(module_data_t *mod, const bool pids[TS_MAX_PID])
{
    HRESULT hr = E_NOTIMPL;

    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
                (bda_extension_t *)asc_list_data(mod->extensions);

        if (ext->pid_bulk != NULL)
        {
            hr = ext->pid_bulk(ext->data, pids);
            if (FAILED(hr))
            {
                BDA_ERROR_D(hr, "couldn't load PID whitelist via '%s'"
                            , ext->name);
            }
        }
    }

    return hr;
}
