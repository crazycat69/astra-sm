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
 * TurboSight PLP ID
 */

static
const GUID KSPROPSETID_BdaTunerExtensionProperties =
    {0xfaa8f3e5,0x31d4,0x4e41,{0x88,0xef,0xd9,0xeb,0x71,0x6f,0x6e,0xc9}};

enum
{
    KSPROPERTY_BDA_PLPINFO = 22,
};

struct tbs_plp_info
{
    uint8_t id;
    uint8_t count;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t id_list[256];
};

static
HRESULT tbs_plp_tune(void *data, const bda_tune_cmd_t *tune)
{
    HRESULT hr = S_OK;

    if (tune->stream_id != -1)
    {
        struct tbs_plp_info plp =
        {
            .id = tune->stream_id,
        };

        IKsPropertySet *const prop = (IKsPropertySet *)data;
        hr = IKsPropertySet_Set(prop
                                , &KSPROPSETID_BdaTunerExtensionProperties
                                , KSPROPERTY_BDA_PLPINFO
                                , NULL, 0, &plp, sizeof(plp));
    }

    return hr;
}

static
HRESULT tbs_plp_init(IBaseFilter *filters[], void **data)
{
    return generic_init(filters, data
                        , &KSPROPSETID_BdaTunerExtensionProperties
                        , KSPROPERTY_BDA_PLPINFO);
}

static
const bda_extension_t ext_tbs_plp =
{
    .name = "tbs_plp",
    .description = "TurboSight PLP ID",

    .init = tbs_plp_init,
    .destroy = generic_destroy,

    .tune = tbs_plp_tune,
};

/*
 * public API
 */

/* list of supported BDA extensions */
static
const bda_extension_t *const ext_list[] =
{
    &ext_tbs_plp,
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
HRESULT bda_ext_tune(module_data_t *mod, const bda_tune_cmd_t *tune)
{
    HRESULT out_hr = S_OK;

    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
            (bda_extension_t *)asc_list_data(mod->extensions);

        if (ext->tune != NULL)
        {
            const HRESULT hr = ext->tune(ext->data, tune);

            if (FAILED(hr))
            {
                BDA_DEBUG("couldn't send tuning data for %s", ext->name);
                out_hr = hr;
            }
        }
    }

    return out_hr;
}

/* send raw DiSEqC command */
HRESULT bda_ext_diseqc(module_data_t *mod, const uint8_t cmd[BDA_DISEQC_LEN])
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

    return out_hr;
}
