/*
 * Astra Module: IT95x (Modulator API for Windows)
 *
 * Copyright (C) 2017, Artem Kharitonov <artem@3phase.pw>
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

#include "api.h"

#include "../dshow/dshow.h"
#include <ks.h>
#include <ksproxy.h>

struct it95x_dev_t
{
    IKsPropertySet *prop;
    it95x_dev_info_t info;
};

/*
 * Main property set. This is used to control device operation as well
 * as send TS data blocks for transmission.
 */

static
const GUID KSPROPSETID_IT9500Properties =
    {0xf23fac2d,0xe1af,0x48e0,{0x8b,0xbe,0xa1,0x40,0x29,0xc9,0x2f,0x11}};

enum
{
    KSPROPERTY_IT95X_DRIVER_INFO = 0,
    KSPROPERTY_IT95X_IOCTL = 1,
};

/*
 * Auxiliary property set. The only thing we can do here is query USB mode
 * and device IDs.
 *
 * NOTE: This is actually KSPROPERTYSET_Wd3KsproxySample, an example GUID
 *       used by some vendors whose engineers are too lazy to run guidgen.exe.
 */

static
const GUID KSPROPSETID_IT9500PropertiesAux =
    {0xc6efe5eb,0x855a,0x4f1b,{0xb7,0xaa,0x87,0xb5,0xe1,0xdc,0x41,0x13}};

enum
{
    KSPROPERTY_IT95X_BUS_INFO = 5,
};

struct ks_bus_info
{
    uint16_t usb_mode; /* see it95x_usb_mode_t */
    uint16_t vendor_id;
    uint16_t product_id;
};

/*
 * modulator ioctl's
 */

enum
{
    IOCTL_GET_DRIVER_INFO = 1,
    IOCTL_SET_POWER = 4,
    IOCTL_SET_DVBT_MODULATION = 8,
    IOCTL_SET_RF_OUTPUT = 9,
    IOCTL_SEND_TS_DATA = 30,
    IOCTL_SET_CHANNEL = 31,
    IOCTL_SET_DEVICE_TYPE = 32,
    IOCTL_GET_DEVICE_TYPE = 33,
    IOCTL_SET_GAIN = 34,
    IOCTL_RD_REG_OFDM = 35,
    IOCTL_WR_REG_OFDM = 36,
    IOCTL_RD_REG_LINK = 37,
    IOCTL_WR_REG_LINK = 38,
    IOCTL_SEND_PSI_ONCE = 39,
    IOCTL_SET_PSI_PACKET = 40,
    IOCTL_SET_PSI_TIMER = 41,
    IOCTL_GET_GAIN_RANGE = 42,
    IOCTL_SET_TPS = 43,
    IOCTL_GET_TPS = 44,
    IOCTL_GET_GAIN = 45,
    IOCTL_SET_IQ_TABLE = 46,
    IOCTL_SET_DC_CAL = 47,
    IOCTL_SET_ISDBT_MODULATION = 60,
    IOCTL_ADD_ISDBT_PID_FILTER = 61,
    IOCTL_SET_TMCC = 62, /* XXX: ITE_TxSetTMCCInfo2() uses 63 (0x3F) */
    IOCTL_GET_TMCC = 64,
    IOCTL_GET_TS_BITRATE = 65,
    IOCTL_SET_ISDBT_PID_FILTER = 66,
    IOCTL_SET_PCR_MODE = 67,
    IOCTL_SET_PCR_ENABLE = 68,
    IOCTL_RESET_ISDBT_PID_FILTER = 69,
    IOCTL_SET_OFS_CAL = 70,
    IOCTL_ENABLE_TPS_CRYPT = 71,
    IOCTL_DISABLE_TPS_CRYPT = 72,
};

enum
{
    GAIN_POSITIVE = 1,
    GAIN_NEGATIVE = 2,
};

struct ioctl_generic
{
    uint32_t code;
    uint32_t param1;
    uint32_t param2;
};

struct ioctl_drv_info
{
    uint32_t drv_pid;
    uint32_t drv_version;
    uint32_t fw_link;
    uint32_t fw_ofdm;
    uint32_t tuner_id;
};

struct ioctl_gain_range
{
    uint32_t code;
    uint32_t frequency;
    uint32_t bandwidth;
    int32_t max_gain;
    int32_t min_gain;
};

/*
 * helper functions
 */

/* convert HRESULT into return value */
static inline
int ret_hr(HRESULT hr)
{
    /* squash success codes */
    return (SUCCEEDED(hr) ? 0 : hr);
}

/* convert Win32 error code into return value */
static inline
int ret_win32(DWORD error)
{
    return HRESULT_FROM_WIN32(error);
}

/* IKsPropertySet::Set shorthand for ioctls */
static inline
HRESULT ioctl_set(IKsPropertySet *prop, void *data, DWORD size)
{
    return IKsPropertySet_Set(prop, &KSPROPSETID_IT9500Properties
                              , KSPROPERTY_IT95X_IOCTL, NULL, 0
                              , data, size);
}

/* IKsPropertySet::Get shorthand for ioctls */
static inline
HRESULT ioctl_get(IKsPropertySet *prop, void *data, DWORD size)
{
    DWORD written = 0;
    return IKsPropertySet_Get(prop, &KSPROPSETID_IT9500Properties
                              , KSPROPERTY_IT95X_IOCTL, NULL, 0
                              , data, size, &written);
}

/* compare moniker's device path against string argument */
static
HRESULT check_devpath(IMoniker *moniker, const char *path, bool *result)
{
    *result = false;

    char *buf = NULL;
    const HRESULT hr = dshow_get_property(moniker, "DevicePath", &buf);

    if (SUCCEEDED(hr))
    {
        if (strstr(buf, path) == buf)
            *result = true;

        free(buf);
    }

    return hr;
}

/* check whether a moniker points to a supported device */
static
HRESULT check_moniker(IMoniker *moniker, bool *result)
{
    *result = false;

    char *prop_name = NULL;
    HRESULT hr = dshow_get_property(moniker, "FriendlyName", &prop_name);

    if (SUCCEEDED(hr))
    {
        if (strstr(prop_name, "IT95") == prop_name)
        {
            char *prop_clsid = NULL;
            wchar_t *wbuf = NULL;

            hr = dshow_get_property(moniker, "CLSID", &prop_clsid);
            if (SUCCEEDED(hr))
            {
                wbuf = cx_widen(prop_clsid);
                if (wbuf == NULL)
                    hr = E_OUTOFMEMORY;

                free(prop_clsid);
            }

            if (wbuf != NULL)
            {
                CLSID clsid;
                memset(&clsid, 0, sizeof(clsid));

                hr = CLSIDFromString(wbuf, &clsid);
                if (SUCCEEDED(hr))
                    *result = IsEqualCLSID(&clsid, &CLSID_Proxy);

                free(wbuf);
            }
        }

        free(prop_name);
    }

    return hr;
}

/* get property set object from filter */
static
HRESULT prop_from_filter(IBaseFilter *filter, IKsPropertySet **out)
{
    *out = NULL;

    IKsPropertySet *prop = NULL;
    HRESULT hr = IBaseFilter_QueryInterface(filter, &IID_IKsPropertySet
                                            , (void **)&prop);
    ASC_WANT_PTR(hr, prop);

    if (SUCCEEDED(hr))
    {
        const unsigned int want_sets = 3;
        unsigned int got_sets = 0;

        /* main set, driver info */
        DWORD type = 0;
        hr = prop->lpVtbl->QuerySupported(prop
                                          , &KSPROPSETID_IT9500Properties
                                          , KSPROPERTY_IT95X_DRIVER_INFO
                                          , &type);
        if (SUCCEEDED(hr)
            && (type & KSPROPERTY_SUPPORT_GET)
            && (type & KSPROPERTY_SUPPORT_SET))
        {
            got_sets++;
        }

        /* main set, ioctl */
        type = 0;
        hr = prop->lpVtbl->QuerySupported(prop
                                          , &KSPROPSETID_IT9500Properties
                                          , KSPROPERTY_IT95X_IOCTL
                                          , &type);
        if (SUCCEEDED(hr)
            && (type & KSPROPERTY_SUPPORT_GET)
            && (type & KSPROPERTY_SUPPORT_SET))
        {
            got_sets++;
        }

        /* aux set, device info */
        type = 0;
        hr = prop->lpVtbl->QuerySupported(prop
                                          , &KSPROPSETID_IT9500PropertiesAux
                                          , KSPROPERTY_IT95X_BUS_INFO
                                          , &type);
        if (SUCCEEDED(hr)
            && (type & KSPROPERTY_SUPPORT_GET))
        {
            /* NOTE: this one's read-only */
            got_sets++;
        }

        if (got_sets == want_sets)
        {
            *out = prop;
            hr = S_OK;
        }
        else
        {
            ASC_RELEASE(prop);
            hr = E_PROP_SET_UNSUPPORTED;
        }
    }

    return hr;
}

/* get bus mode and device IDs */
static
HRESULT get_bus_info(IKsPropertySet *prop, struct ks_bus_info *bus_info)
{
    memset(bus_info, 0, sizeof(*bus_info));

    DWORD written = 0;
    return prop->lpVtbl->Get(prop, &KSPROPSETID_IT9500PropertiesAux
                             , KSPROPERTY_IT95X_BUS_INFO, NULL, 0
                             , bus_info, sizeof(*bus_info), &written);
}

/* get driver and firmware versions */
static
HRESULT get_drv_info(IKsPropertySet *prop, struct ioctl_drv_info *drv_info)
{
    memset(drv_info, 0, sizeof(*drv_info));

    struct ioctl_generic ioc =
    {
        .code = IOCTL_GET_DRIVER_INFO,
    };

    HRESULT hr = prop->lpVtbl->Set(prop, &KSPROPSETID_IT9500Properties
                                   , KSPROPERTY_IT95X_DRIVER_INFO, NULL, 0
                                   , &ioc, sizeof(ioc));

    if (SUCCEEDED(hr))
    {
        DWORD written = 0;
        hr = prop->lpVtbl->Get(prop, &KSPROPSETID_IT9500Properties
                               , KSPROPERTY_IT95X_DRIVER_INFO, NULL, 0
                               , drv_info, sizeof(*drv_info), &written);
    }

    return hr;
}

/* get chip type */
// TODO

/* get device type */
static
HRESULT get_dev_type(IKsPropertySet *prop, uint8_t *dev_type)
{
    *dev_type = 0;

    struct ioctl_generic ioc =
    {
        .code = IOCTL_GET_DEVICE_TYPE,
    };

    HRESULT hr = ioctl_set(prop, &ioc, sizeof(ioc));
    if (SUCCEEDED(hr))
    {
        hr = ioctl_get(prop, &ioc, sizeof(ioc));
        if (SUCCEEDED(hr))
            *dev_type = ioc.param2;
    }

    return hr;
}

/* initialize device pointed to by moniker */
static
HRESULT dev_from_moniker(IMoniker *moniker, it95x_dev_t **out)
{
    HRESULT hr = E_FAIL;

    char *name = NULL, *devpath = NULL;
    struct ks_bus_info bus_info = { 0 };
    struct ioctl_drv_info drv_info = { 0 };
    uint16_t chip_type = 0;
    uint8_t dev_type = 0;

    IBaseFilter *filter = NULL;
    IKsPropertySet *prop = NULL;

    /* get device strings and its property set interface */
    hr = dshow_filter_from_moniker(moniker, &filter, &name);
    if (FAILED(hr)) goto out;

    hr = dshow_get_property(moniker, "DevicePath", &devpath);
    if (FAILED(hr)) goto out;

    hr = prop_from_filter(filter, &prop);
    if (FAILED(hr)) goto out;

    /* cache device information for application use */
    hr = get_bus_info(prop, &bus_info);
    if (FAILED(hr)) goto out;

    hr = get_drv_info(prop, &drv_info);
    if (FAILED(hr)) goto out;

    // TODO: chip type

    hr = get_dev_type(prop, &dev_type);
    if (FAILED(hr)) goto out;

out:
    if (SUCCEEDED(hr))
    {
        it95x_dev_t *const dev = ASC_ALLOC(1, it95x_dev_t);

        dev->prop = prop;

        dev->info.name = name;
        dev->info.devpath = devpath;

        dev->info.usb_mode = bus_info.usb_mode;
        dev->info.vendor_id = bus_info.vendor_id;
        dev->info.product_id = bus_info.product_id;

        dev->info.drv_pid = drv_info.drv_pid;
        dev->info.drv_version = drv_info.drv_version;
        dev->info.fw_link = drv_info.fw_link;
        dev->info.fw_ofdm = drv_info.fw_ofdm;
        dev->info.tuner_id = drv_info.tuner_id;

        dev->info.chip_type = chip_type;
        dev->info.dev_type = dev_type;

        *out = dev;
    }
    else
    {
        ASC_FREE(devpath, free);
        ASC_FREE(name, free);

        ASC_RELEASE(prop);
    }

    ASC_RELEASE(filter);

    return hr;
}

/*
 * public API
 */

int it95x_dev_count(size_t *count)
{
    *count = 0;

    IEnumMoniker *enum_moniker = NULL;
    HRESULT hr = dshow_enum(&KSCATEGORY_AUDIO_DEVICE, &enum_moniker
                            , CDEF_DEVMON_PNP_DEVICE);

    if (hr == S_OK)
    {
        do
        {
            IMoniker *moniker = NULL;
            hr = IEnumMoniker_Next(enum_moniker, 1, &moniker, NULL);
            ASC_WANT_ENUM(hr, moniker);

            if (hr == S_OK)
            {
                bool valid = false;
                hr = check_moniker(moniker, &valid);

                if (SUCCEEDED(hr) && valid)
                    (*count)++;

                ASC_RELEASE(moniker);
            }
            else
            {
                /* no more devices */
                break;
            }
        } while (true);

        ASC_RELEASE(enum_moniker);
    }

    return ret_hr(hr);
}

int it95x_open(ssize_t idx, const char *path, it95x_dev_t **dev)
{
    *dev = NULL;

    /* search for requested device */
    const size_t pathlen = (path != NULL ? strlen(path) : 0);
    if (idx < 0 && pathlen == 0)
        return ret_win32(ERROR_BAD_ARGUMENTS);

    IEnumMoniker *enum_moniker = NULL;
    HRESULT hr = dshow_enum(&KSCATEGORY_AUDIO_DEVICE, &enum_moniker
                            , CDEF_DEVMON_PNP_DEVICE);

    if (FAILED(hr))
        return ret_hr(hr);
    else if (hr != S_OK)
        return ret_win32(ERROR_BAD_UNIT);

    IMoniker *dev_moniker = NULL;
    ssize_t count = -1;

    do
    {
        IMoniker *moniker = NULL;
        hr = IEnumMoniker_Next(enum_moniker, 1, &moniker, NULL);
        ASC_WANT_ENUM(hr, moniker);

        if (hr == S_OK)
        {
            bool match = false;
            hr = check_moniker(moniker, &match);

            if (SUCCEEDED(hr) && match)
            {
                count++;

                if (pathlen > 0)
                {
                    hr = check_devpath(moniker, path, &match);
                }
                else
                {
                    match = (idx == count);
                }

                if (SUCCEEDED(hr) && match)
                {
                    /* found it */
                    dev_moniker = moniker;
                    break;
                }
            }

            ASC_RELEASE(moniker);
        }
        else
        {
            /* no more devices */
            break;
        }
    } while (true);

    ASC_RELEASE(enum_moniker);

    /* attempt initialization if found */
    int ret = ret_win32(ERROR_BAD_UNIT);

    if (dev_moniker != NULL)
    {
        ret = ret_hr(dev_from_moniker(dev_moniker, dev));
        IMoniker_Release(dev_moniker);
    }
    else if (FAILED(hr))
    {
        ret = ret_hr(hr);
    }

    return ret;
}

void it95x_get_info(const it95x_dev_t *dev, it95x_dev_info_t *info)
{
    memcpy(info, &dev->info, sizeof(*info));
}

void it95x_close(it95x_dev_t *dev)
{
    ASC_RELEASE(dev->prop);

    free(dev->info.name);
    free(dev->info.devpath);

    free(dev);
}

int it95x_get_gain(it95x_dev_t *dev, int *gain)
{
    *gain = 0;

    struct ioctl_generic ioc =
    {
        .code = IOCTL_GET_GAIN,
    };

    HRESULT hr = ioctl_set(dev->prop, &ioc, sizeof(ioc));
    if (SUCCEEDED(hr))
    {
        hr = ioctl_get(dev->prop, &ioc, sizeof(ioc));
        if (SUCCEEDED(hr))
        {
            if (ioc.param2 == GAIN_POSITIVE)
                *gain = ioc.param1;
            else if (ioc.param2 == GAIN_NEGATIVE)
                *gain = -(ioc.param1);
            else
                hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }
    }

    return ret_hr(hr);
}

int it95x_get_gain_range(it95x_dev_t *dev
                         , uint32_t frequency, uint32_t bandwidth
                         , int *max, int *min)
{
    *max = *min = 0;

    struct ioctl_gain_range gain_range =
    {
        .code = IOCTL_GET_GAIN_RANGE,
        .frequency = frequency,
        .bandwidth = bandwidth,
    };

    HRESULT hr = ioctl_set(dev->prop, &gain_range, sizeof(gain_range));
    if (SUCCEEDED(hr))
    {
        hr = ioctl_get(dev->prop, &gain_range, sizeof(gain_range));
        if (SUCCEEDED(hr))
        {
            // TODO: double check integer cast
            //       try using 'long long' for min/max
            *max = gain_range.max_gain;
            *min = gain_range.min_gain;
        }
    }

    return ret_hr(hr);
}

int it95x_set_channel(it95x_dev_t *dev
                      , uint32_t frequency, uint32_t bandwidth)
{
    struct ioctl_generic ioc =
    {
        .code = IOCTL_SET_CHANNEL,
        .param1 = frequency,
        .param2 = bandwidth,
    };

    return ret_hr(ioctl_set(dev->prop, &ioc, sizeof(ioc)));
}

int it95x_set_gain(it95x_dev_t *dev, int *gain)
{
    struct ioctl_generic ioc =
    {
        .code = IOCTL_SET_GAIN,
    };

    if (*gain >= 0)
    {
        /* gain */
        ioc.param1 = *gain;
        ioc.param2 = GAIN_POSITIVE;
    }
    else
    {
        /* attenuation */
        ioc.param1 = -(*gain);
        ioc.param2 = GAIN_NEGATIVE;
    }

    HRESULT hr = ioctl_set(dev->prop, &ioc, sizeof(ioc));
    if (SUCCEEDED(hr))
    {
        hr = ioctl_get(dev->prop, &ioc, sizeof(ioc));
        if (SUCCEEDED(hr))
        {
            if (ioc.param2 == GAIN_POSITIVE)
                *gain = ioc.param1;
            else if (ioc.param2 == GAIN_NEGATIVE)
                *gain = -(ioc.param1);
            else
                hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }
    }

    return ret_hr(hr);
}

int it95x_set_power(it95x_dev_t *dev, bool power)
{
    struct ioctl_generic ioc =
    {
        .code = IOCTL_SET_POWER,
        .param1 = power,
    };

    return ret_hr(ioctl_set(dev->prop, &ioc, sizeof(ioc)));
}

int it95x_set_pcr(it95x_dev_t *dev, it95x_pcr_mode_t mode)
{
    struct ioctl_generic ioc =
    {
        .code = IOCTL_SET_PCR_MODE,
        .param2 = mode,
    };

    return ret_hr(ioctl_set(dev->prop, &ioc, sizeof(ioc)));
}

char *it95x_strerror(int error)
{
    if (error <= 0)
    {
        /* OS error code (in this case, HRESULT) */
        return dshow_error_msg(error);
    }
    else
    {
        /* modulator error code */
        return strdup("TODO");

        /*
         * TODO: get listing of modulator firmware error codes
         */
    }
}
