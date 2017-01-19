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
 * TurboSight PCI-E
 */

static
const GUID KSPROPSETID_BdaTunerExtensionProperties =
    {0xfaa8f3e5,0x31d4,0x4e41,{0x88,0xef,0xd9,0xeb,0x71,0x6f,0x6e,0xc9}};

enum
{
    KSPROPERTY_BDA_ACCESS = 18,
    KSPROPERTY_BDA_PLPINFO = 22,
    KSPROPERTY_BDA_PLS = 23,
};

typedef enum
{
    TBSACCESS_LNBPOWER,
    TBSACCESS_DISEQC,
    TBSACCESS_22K,
} tbs_cmd_t;

typedef enum
{
    LNBPOWER_OFF,
    LNBPOWER_18V,
    LNBPOWER_13V,
    LNBPOWER_ON
} tbs_lnbpwr_t;

typedef enum
{
    BURST_OFF,
    BURST_ON,
    BURST_UNMODULATED,
    BURST_MODULATED,
} tbs_22k_mode_t;

typedef struct
{
	tbs_cmd_t		access_mode;
	tbs_22k_mode_t	tbs22k_mode;
	bool			bOnOff;
	tbs_lnbpwr_t	LNBPower_mode;
	uint8_t		diseqc_send_message[128];
	uint32_t	diseqc_send_message_length;
	uint8_t		diseqc_receive_message[128];
	uint32_t	diseqc_receive_message_length;
	uint8_t		reserved[256];//reserved for future use 
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
HRESULT tbs_pcie_pre_tune(void *data, const bda_tune_cmd_t *tune)
{
    HRESULT hr = S_OK;
    IKsPropertySet *const prop = (IKsPropertySet *)data;

    if (tune->stream_id != -1)
    {
        tbs_plp_t plp =
        {
            .id = tune->stream_id & 0xff,
        };

        hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_BdaTunerExtensionProperties
                                , KSPROPERTY_BDA_PLPINFO
                                , NULL, 0, &plp, sizeof(plp));

        asc_log_debug("tbs_pcie_pre_tune: PLP/ISI %d - result %d",
                          plp.id, (int)hr);
	}

	if (hr) return hr;

	if (tune->pls_mode || tune->pls_code)
	{
        tbs_pls_t pls =
        {
            .pls_code = tune->pls_code & 0x3FFFF,
            .pls_mode = tune->pls_mode & 0x3,
		};

			hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_BdaTunerExtensionProperties
                                , KSPROPERTY_BDA_PLS
                                , NULL, 0, &pls, sizeof(pls));

			asc_log_debug("tbs_pcie_pre_tune: PLS mode %d, code %d - result %d",
						pls.pls_mode, pls.pls_code, (int)hr);
	}

    return hr;
}

static
HRESULT tbs_pcie_diseqc(void *data, const bda_diseqc_cmd_t *cmd)
{
    HRESULT hr = S_OK;
    IKsPropertySet *const prop = (IKsPropertySet *)data;

    tbs_access_t tbscmd;
    tbscmd.access_mode = TBSACCESS_DISEQC;
    tbscmd.diseqc_send_message_length = cmd->diseqc_len;
    memcpy(tbscmd.diseqc_send_message,cmd->diseqc_cmd,cmd->diseqc_len);

    hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_BdaTunerExtensionProperties
                                , KSPROPERTY_BDA_ACCESS
                                , NULL, 0, &tbscmd, sizeof(tbscmd));

    return hr;
}

static
HRESULT tbs_pcie_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_BdaTunerExtensionProperties
                        , KSPROPERTY_BDA_PLPINFO);
}

static
const bda_extension_t ext_tbs_pcie =
{
    .name = "tbs_pcie",
    .description = "TurboSight PCI-E",

    .init = tbs_pcie_init,
    .destroy = generic_destroy,

    .pre_tune = tbs_pcie_pre_tune,
    .diseqc = tbs_pcie_diseqc,
};

/*
 * TurboSight USB
 */

static
const GUID KSPROPSETID_QBOXControlProperties =
    {0xC6EFE5EB,0x855A,0x4f1b,{0xB7,0xAA,0x87,0xB5,0xE1,0xDC,0x41,0x13}};

enum
{
    KSPROPERTY_CTRL_TUNER,
	KSPROPERTY_CTRL_IR,
	KSPROPERTY_CTRL_22K_TONE,
	KSPROPERTY_CTRL_MOTOR,
	KSPROPERTY_CTRL_LNBPW,
	KSPROPERTY_CTRL_LOCK_TUNER,
    KSPROPERTY_CTRL_ACCESS = 18,
    KSPROPERTY_CTRL_PLPINFO = 19,
    KSPROPERTY_CTRL_PLS = 20,
};

typedef struct {	
	uint32_t	freq;
	uint32_t	lof_low;
	uint32_t	lof_high;
	uint32_t	sr;
	uint8_t		pol;
	uint8_t		lnb_pwr;
	uint8_t		tone_22khz;
	uint8_t		tone_burst;
	uint8_t		lnb_source;

	uint8_t 	diseqc_cmd[5];
	uint8_t		ir_code;
	uint8_t		lock;
	uint8_t		strength;
	uint8_t 	quality;
	uint8_t 	reserved[256];
} tbs_usb_cmd_t;

static
HRESULT tbs_usb_pre_tune(void *data, const bda_tune_cmd_t *tune)
{
    HRESULT hr = S_OK;
    IKsPropertySet *const prop = (IKsPropertySet *)data;

    if (tune->stream_id != -1)
    {
        tbs_plp_t plp =
        {
            .id = tune->stream_id & 0xff,
        };

        hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_QBOXControlProperties
                                , KSPROPERTY_CTRL_PLPINFO
                                , NULL, 0, &plp, sizeof(plp));

        asc_log_debug("tbs_usb_pre_tune: PLP/ISI %d - result %d",
                          plp.id, (int)hr);
	}

	if (hr) return hr;

	if (tune->pls_mode || tune->pls_code)
	{
        tbs_pls_t pls =
        {
            .pls_code = tune->pls_code & 0x3FFFF,
            .pls_mode = tune->pls_mode & 0x3,
		};

			hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_QBOXControlProperties
                                , KSPROPERTY_CTRL_PLS
                                , NULL, 0, &pls, sizeof(pls));

			asc_log_debug("tbs_usb_pre_tune: PLS mode %d, code %d - result %d",
						pls.pls_mode, pls.pls_code, (int)hr);
	}

    return hr;
}

static
HRESULT tbs_usb_diseqc(void *data, const bda_diseqc_cmd_t *cmd)
{
    HRESULT hr = S_OK;

    tbs_access_t tbscmd;
    tbscmd.access_mode = TBSACCESS_DISEQC;
    tbscmd.diseqc_send_message_length = cmd->diseqc_len;
    memcpy(tbscmd.diseqc_send_message,cmd->diseqc_cmd,cmd->diseqc_len);

    IKsPropertySet *const prop = (IKsPropertySet *)data;
    hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_QBOXControlProperties
                                , KSPROPERTY_CTRL_ACCESS
                                , NULL, 0, &tbscmd, sizeof(tbscmd));

    return hr;
}

static
HRESULT tbs_usb_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_QBOXControlProperties
                        , KSPROPERTY_CTRL_PLPINFO);
}

static
const bda_extension_t ext_tbs_usb =
{
    .name = "tbs_usb",
    .description = "TurboSight USB",

    .init = tbs_usb_init,
    .destroy = generic_destroy,

    .pre_tune = tbs_usb_pre_tune,
    .diseqc = tbs_usb_diseqc,
};

/*
 * Omicom S2 PCI
 */

static
const GUID KSPROPSETID_OmcDiSEqCProperties =
	{0x7DB2DEEAL,0x42B4,0x423d,{0xA2, 0xF7, 0x19, 0xC3, 0x2E, 0x51, 0xCC, 0xC1}};

enum
{
	KSPROPERTY_OMC_DISEQC_WRITE,
	KSPROPERTY_OMC_DISEQC_READ,				
	KSPROPERTY_OMC_DISEQC_SET22K,
	KSPROPERTY_OMC_DISEQC_ENCABLOSSCOMP,
	KSPROPERTY_OMC_DISEQC_TONEBURST
};

struct omc_diseqc_info
{
	uint32_t	nLen;
	uint8_t		pBuffer[64];
	uint32_t	nRepeatCount;
};

static
const GUID KSPROPSETID_OmcCustomProperties =
	{0x7DB2DEE6L,0x42B4,0x423d,{0xA2, 0xF7, 0x19, 0xC3, 0x2E, 0x51, 0xCC, 0xC1}};

struct omc_pls_info
{
	uint32_t	pls_mode;
	uint32_t	pls_code;
};

enum
{
	KSPROPERTY_OMC_CUSTOM_SIGNAL_OFFSET,
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

static
HRESULT omc_pci_post_tune(void *data, const bda_tune_cmd_t *tune)
{
    HRESULT hr = S_OK;
    IKsPropertySet *const prop = (IKsPropertySet *)data;

    if (tune->stream_id != -1)
    {
		
		uint32_t isi = tune->stream_id & 0xFF;

        hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_OmcCustomProperties
                                , KSPROPERTY_OMC_CUSTOM_MIS_FILTER
                                , NULL, 0, &isi, sizeof(isi));

        asc_log_debug("omc_pci_post_tune: PLP/ISI %d - result %d",
                          isi, (int)hr);		
	}

	if (hr) return hr;
	
	if (tune->pls_mode || tune->pls_code)
	{
        struct omc_pls_info pls =
        {
            .pls_code = tune->pls_code & 0x3FFFF,
            .pls_mode = tune->pls_mode & 0x3,
        };

        hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_OmcCustomProperties
                                , KSPROPERTY_OMC_CUSTOM_PLS_SCRAM
                                , NULL, 0, &pls, sizeof(pls));

		asc_log_debug("omc_pci_post_tune: PLS mode %d, code %d - result %d",
                          pls.pls_mode, pls.pls_code, (int)hr);
    }

    return S_OK;
}

static
HRESULT omc_pci_diseqc(void *data, const bda_diseqc_cmd_t *cmd)
{
    HRESULT hr = S_OK;

    struct omc_diseqc_info omc_diseqc_cmd;
    omc_diseqc_cmd.nLen = cmd->diseqc_len;
    memcpy(omc_diseqc_cmd.pBuffer,cmd->diseqc_cmd,cmd->diseqc_len);
	omc_diseqc_cmd.nRepeatCount = 1;

    IKsPropertySet *const prop = (IKsPropertySet *)data;
    hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_OmcDiSEqCProperties
                                , KSPROPERTY_OMC_DISEQC_WRITE
                                , NULL, 0, &omc_diseqc_cmd, sizeof(omc_diseqc_cmd));

    return hr;
}

static
HRESULT omc_pci_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_OmcDiSEqCProperties
                        , KSPROPERTY_OMC_DISEQC_WRITE);
}

static
const bda_extension_t ext_omc_pci =
{
    .name = "omc_pci",
    .description = "Omicom S2 PCI",

    .init = omc_pci_init,
    .destroy = generic_destroy,

    .post_tune = omc_pci_post_tune,
    .diseqc = omc_pci_diseqc,
};

/*
 * TurboSight USB
 */

static
const GUID KSPROPERTYSET_CCTunerControl =
    {0xA3E871E9,0x1F10,0x473e,{0x99,0xBD,0xEE,0x70,0xE0,0xD2,0xF0,0x70}};

enum
{
	KSPROPERTY_CC_SET_FREQUENCY,
	KSPROPERTY_CC_SET_DISEQC,
	KSPROPERTY_CC_GET_SIGNAL_STATS,
};

typedef struct {	
	uint32_t freq;
	uint32_t lof1;
	uint32_t lof2;
	uint32_t slof;
	uint32_t sr;

	Polarisation pol;
	uint32_t std;
	ModulationType mod;
	BinaryConvolutionCodeRate fec;
	RollOff rolloff;
	Pilot pilot;
	uint32_t stream_id;
	uint32_t lnb_source;

	uint32_t diseqc_len;
	uint8_t diseqc_cmd[8];

	uint32_t strength;
	uint32_t quality;
	bool locked;

	int32_t rflevel;/* dBm */
	int32_t snr10;/* dB, snr * 10 */
	uint32_t ber10e7;
} cc_tuner_cmd_t;

static
HRESULT cc_post_tune(void *data, const bda_tune_cmd_t *tune)
{
    HRESULT hr = S_OK;
    IKsPropertySet *const prop = (IKsPropertySet *)data;

	cc_tuner_cmd_t cc_cmd;
	cc_cmd.freq = tune->frequency/1000;
	cc_cmd.lof1 = tune->lof1/1000;
	cc_cmd.lof2 = tune->lof2/1000;
	cc_cmd.slof = tune->slof/1000;
	cc_cmd.sr = tune->symbolrate/1000;
	cc_cmd.pol = tune->polarization;
	cc_cmd.std = 0;
	cc_cmd.mod = tune->modulation;
	cc_cmd.rolloff = tune->rolloff;
	cc_cmd.pilot = tune->pilot;
	cc_cmd.lnb_source = tune->lnb_source;
	cc_cmd.stream_id = (tune->pls_mode << 26) | (tune->pls_code << 8) | tune->stream_id;
	
	hr = IKsPropertySet_Set(prop
							, &KSPROPERTYSET_CCTunerControl
							, KSPROPERTY_CC_SET_FREQUENCY
							, NULL, 0, &cc_cmd, sizeof(cc_cmd));

    asc_log_debug("cc_post_tune: stream_id %d, lnb_source %d - result %d",
                          cc_cmd.stream_id, cc_cmd.lnb_source, (int)hr);		
	
    return hr;
}

static
HRESULT cc_diseqc(void *data, const bda_diseqc_cmd_t *cmd)
{
    HRESULT hr = S_OK;

    cc_tuner_cmd_t cc_cmd;
    cc_cmd.diseqc_len = cmd->diseqc_len;
    memcpy(cc_cmd.diseqc_cmd,cmd->diseqc_cmd,cmd->diseqc_len);

    IKsPropertySet *const prop = (IKsPropertySet *)data;
    hr = IKsPropertySet_Set(prop
                                , &KSPROPERTYSET_CCTunerControl
                                , KSPROPERTY_CC_SET_DISEQC
                                , NULL, 0, &cc_cmd, sizeof(cc_cmd));

	asc_log_debug("cc_diseqc - result %d", (int)hr);		

    return hr;
}

static
HRESULT cc_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPERTYSET_CCTunerControl
                        , KSPROPERTY_CC_SET_FREQUENCY);
}

static
const bda_extension_t ext_cc =
{
    .name = "cc",
    .description = "CrazyBDA",

    .init = cc_init,
    .destroy = generic_destroy,

    .post_tune = cc_post_tune,
    .diseqc = cc_diseqc,
};

/*
 * public API
 */

/* list of supported BDA extensions */
static
const bda_extension_t *const ext_list[] =
{
    &ext_tbs_pcie,
    &ext_tbs_usb,
	&ext_omc_pci,
	&ext_cc,
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
            BDA_DEBUG("probe for %s extension failed", ext->name);
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
HRESULT bda_ext_pre_tune(module_data_t *mod, const bda_tune_cmd_t *tune)
{
    HRESULT out_hr = S_OK;

    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
            (bda_extension_t *)asc_list_data(mod->extensions);

        if (ext->pre_tune != NULL)
        {
            const HRESULT hr = ext->pre_tune(ext->data, tune);

            if (FAILED(hr))
            {
                BDA_DEBUG("couldn't send pre-tuning data for %s", ext->name);
                out_hr = hr;
            }
        }
    }

    return out_hr;
}

HRESULT bda_ext_post_tune(module_data_t *mod, const bda_tune_cmd_t *tune)
{
    HRESULT out_hr = S_OK;

    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
            (bda_extension_t *)asc_list_data(mod->extensions);

        if (ext->post_tune != NULL)
        {
            const HRESULT hr = ext->post_tune(ext->data, tune);

            if (FAILED(hr))
            {
                BDA_DEBUG("couldn't send post-tuning data for %s", ext->name);
                out_hr = hr;
            }
        }
    }

    return out_hr;
}

/* send raw DiSEqC command */
HRESULT bda_ext_diseqc(module_data_t *mod, const bda_diseqc_cmd_t *cmd)
{
    HRESULT out_hr = S_OK;


    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
            (bda_extension_t *)asc_list_data(mod->extensions);

        if (ext->diseqc != NULL)
        {
            const HRESULT hr = ext->diseqc(ext->data, cmd);

            if (FAILED(hr))
            {
                BDA_DEBUG("couldn't send DiSEqC command for %s", ext->name);
                out_hr = hr;
            }
        }
    }

    asc_log_debug("bda_ext_diseqc - result %d", (int)out_hr);

    return out_hr;
}
