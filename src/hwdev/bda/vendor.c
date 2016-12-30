/*
 * Astra Module: BDA (Vendor extensions)
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

/*
 * public API
 */

/* list of supported BDA extensions */
static
const bda_extension_t *const ext_list[] =
{
    NULL,
};

/* probe device filters for known extensions */
HRESULT bda_ext_init(module_data_t *mod, IBaseFilter *filters[])
{
    HRESULT out_hr = S_OK;

    for (const bda_extension_t *const *ptr = ext_list; *ptr != NULL; ptr++)
    {
        if (mod->ext_flags & (*ptr)->type)
        {
            asc_log_debug(MSG("skipping extension: %s"), (*ptr)->name);
            continue;
        }

        void *data = NULL;
        const HRESULT hr = (*ptr)->init(filters, &data);

        if (SUCCEEDED(hr))
        {
            bda_extension_t *const ext = ASC_ALLOC(1, bda_extension_t);

            memcpy(ext, *ptr, sizeof(bda_extension_t));
            ext->data = data;

            asc_list_insert_tail(mod->extensions, ext);
            mod->ext_flags |= ext->type;

            asc_log_debug(MSG("added vendor extension: %s (%s)")
                          , ext->name, ext->description);
        }
        else if (hr != E_NOTIMPL)
        {
            BDA_DEBUG("probe for %s extension failed", (*ptr)->name);
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
            const HRESULT hr = ext->tune(ext, tune);

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
HRESULT bda_ext_diseqc(module_data_t *mod, const uint8_t cmd[6])
{
    HRESULT out_hr = S_OK;

    asc_list_for(mod->extensions)
    {
        bda_extension_t *const ext =
            (bda_extension_t *)asc_list_data(mod->extensions);

        if (ext->diseqc != NULL)
        {
            const HRESULT hr = ext->diseqc(mod, cmd);

            if (FAILED(hr))
            {
                BDA_DEBUG("couldn't send DiSEqC command via %s", ext->name);
                out_hr = hr;
            }
        }
    }

    return out_hr;
}
