/*
 * Astra Module: Hardware Device (DirectShow utilities)
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

// FIXME: move these to astra.h
#define UNICODE
#define _UNICODE
// FIXME

#include <astra/astra.h>
#include "dshow.h"

/* format DirectShow error message. the result must be freed using free() */
char *dshow_error_msg(HRESULT hr)
{
    wchar_t buf[MAX_ERROR_TEXT_LEN] = { L'\0' };
    static const size_t bufsiz = ASC_ARRAY_SIZE(buf);

    const DWORD ret = AMGetErrorText(hr, buf, bufsiz);
    if (ret == 0)
        StringCchPrintf(buf, bufsiz, L"Unknown Error: 0x%2x", hr);

    char *const msg = cx_narrow(buf);
    if (msg != NULL)
    {
        /* remove trailing punctuation */
        for (ssize_t i = strlen(msg) - 1; i >= 0; i--)
        {
            if (msg[i] != '.' && !isspace(msg[i]))
                break;

            msg[i] = '\0';
        }
    }

    return msg;
}

/* look for a filter pin with matching direction */
HRESULT dshow_find_pin(IBaseFilter *filter, PIN_DIRECTION dir, IPin **out)
{
    HRESULT hr = E_FAIL;

    IEnumPins *enum_pins = NULL;
    IPin *pin = NULL;

    /* initialize output argument */
    *out = NULL;

    hr = IBaseFilter_EnumPins(filter, &enum_pins);
    if (FAILED(hr))
        return hr;

    do
    {
        SAFE_RELEASE(pin);

        hr = IEnumPins_Next(enum_pins, 1, &pin, NULL);
        if (hr != S_OK)
        {
            /* no more pins */
            if (SUCCEEDED(hr))
                hr = E_FAIL;

            break;
        }

        PIN_DIRECTION pin_dir;
        hr = IPin_QueryDirection(pin, &pin_dir);
        if (hr == S_OK && pin_dir == dir)
        {
            *out = pin;
            break;
        }
    } while (true);

    SAFE_RELEASE(enum_pins);

    return hr;
}
