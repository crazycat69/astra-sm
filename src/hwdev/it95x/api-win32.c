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
#include <winioctl.h>
#include <ks.h>
#include <ksproxy.h>

/*
 * IKsObject declaration (it's not in MinGW-w64 headers).
 */

typedef struct IKsObject IKsObject;

typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IKsObject *obj, REFIID iid, void **out);
    ULONG (WINAPI *AddRef)(IKsObject *obj);
    ULONG (WINAPI *Release)(IKsObject *obj);
    HANDLE (WINAPI *KsGetObjectHandle)(IKsObject *obj);
} IKsObjectVtbl;

struct IKsObject
{
    IKsObjectVtbl *lpVtbl;
};

#define IKsObject_QueryInterface(_obj, _iid, _out) \
    (_obj)->lpVtbl->QueryInterface(_obj, _iid, _out)

#define IKsObject_AddRef(_obj) \
    (_obj)->lpVtbl->AddRef(_obj)

#define IKsObject_Release(_obj) \
    (_obj)->lpVtbl->Release(_obj)

#define IKsObject_KsGetObjectHandle(_obj) \
    (_obj)->lpVtbl->KsGetObjectHandle(_obj)

/*
 * Main property set. This is used to control device operation as well
 * as send TS data blocks for transmission.
 */

#define STATIC_KSPROPSETID_IT9500Properties \
    0xf23fac2d,0xe1af,0x48e0,{0x8b,0xbe,0xa1,0x40,0x29,0xc9,0x2f,0x11}

enum
{
    KSPROPERTY_IT95X_DRV_INFO = 0,
    KSPROPERTY_IT95X_IOCTL = 1,
};

/*
 * Auxiliary property set. The only thing we can do here is query USB mode
 * and device IDs.
 *
 * NOTE: This is actually KSPROPERTYSET_Wd3KsproxySample, an example GUID
 *       used by some vendors whose engineers are too cool to run guidgen.exe.
 */

#define STATIC_KSPROPSETID_IT9500PropertiesAux \
    0xc6efe5eb,0x855a,0x4f1b,{0xb7,0xaa,0x87,0xb5,0xe1,0xdc,0x41,0x13}

enum
{
    KSPROPERTY_IT95X_BUS_INFO = 5,
};

/*
 * KS property list for DeviceIoControl
 */

enum
{
    KSLIST_DRV_INFO_GET = 0,
    KSLIST_DRV_INFO_SET = 1,
    KSLIST_IOCTL_GET = 2,
    KSLIST_IOCTL_SET = 3,
    KSLIST_BUS_INFO_GET = 4,
    KSLIST_MAX = 5,
};

static
const KSPROPERTY kslist[KSLIST_MAX] =
{
    /* KSLIST_DRV_INFO_GET */
    {
        {
            {
                .Set = { STATIC_KSPROPSETID_IT9500Properties },
                .Id = KSPROPERTY_IT95X_DRV_INFO,
                .Flags = KSPROPERTY_TYPE_GET,
            },
        },
    },

    /* KSLIST_DRV_INFO_SET */
    {
        {
            {
                .Set = { STATIC_KSPROPSETID_IT9500Properties },
                .Id = KSPROPERTY_IT95X_DRV_INFO,
                .Flags = KSPROPERTY_TYPE_SET,
            },
        },
    },

    /* KSLIST_IOCTL_GET */
    {
        {
            {
                .Set = { STATIC_KSPROPSETID_IT9500Properties },
                .Id = KSPROPERTY_IT95X_IOCTL,
                .Flags = KSPROPERTY_TYPE_GET,
            },
        },
    },

    /* KSLIST_IOCTL_SET */
    {
        {
            {
                .Set = { STATIC_KSPROPSETID_IT9500Properties },
                .Id = KSPROPERTY_IT95X_IOCTL,
                .Flags = KSPROPERTY_TYPE_SET,
            },
        },
    },

    /* KSLIST_BUS_INFO_GET */
    {
        {
            {
                .Set = { STATIC_KSPROPSETID_IT9500PropertiesAux },
                .Id = KSPROPERTY_IT95X_BUS_INFO,
                .Flags = KSPROPERTY_TYPE_GET,
            },
        },
    },
};

/*
 * modulator ioctl's
 */

enum
{
    IOCTL_IT95X_GET_DRV_INFO = 1,
    IOCTL_IT95X_SET_POWER = 4,
    IOCTL_IT95X_SET_DVBT_MODULATION = 8,
    IOCTL_IT95X_SET_RF_OUTPUT = 9,
    IOCTL_IT95X_SEND_TS_DATA = 30,
    IOCTL_IT95X_SET_CHANNEL = 31,
    IOCTL_IT95X_SET_DEVICE_TYPE = 32,
    IOCTL_IT95X_GET_DEVICE_TYPE = 33,
    IOCTL_IT95X_SET_GAIN = 34,
    IOCTL_IT95X_RD_REG_OFDM = 35,
    IOCTL_IT95X_WR_REG_OFDM = 36,
    IOCTL_IT95X_RD_REG_LINK = 37,
    IOCTL_IT95X_WR_REG_LINK = 38,
    IOCTL_IT95X_SEND_PSI_ONCE = 39,
    IOCTL_IT95X_SET_PSI_PACKET = 40,
    IOCTL_IT95X_SET_PSI_TIMER = 41,
    IOCTL_IT95X_GET_GAIN_RANGE = 42,
    IOCTL_IT95X_SET_TPS = 43,
    IOCTL_IT95X_GET_TPS = 44,
    IOCTL_IT95X_GET_GAIN = 45,
    IOCTL_IT95X_SET_IQ_TABLE = 46,
    IOCTL_IT95X_SET_DC_CAL = 47,
    IOCTL_IT95X_SET_ISDBT_MODULATION = 60,
    IOCTL_IT95X_ADD_ISDBT_PID_FILTER = 61,
    IOCTL_IT95X_SET_TMCC = 62,
    IOCTL_IT95X_SET_TMCC2 = 63,
    IOCTL_IT95X_GET_TMCC = 64,
    IOCTL_IT95X_GET_TS_BITRATE = 65,
    IOCTL_IT95X_CONTROL_ISDBT_PID_FILTER = 66,
    IOCTL_IT95X_SET_PCR_MODE = 67,
    IOCTL_IT95X_SET_PCR_ENABLE = 68,
    IOCTL_IT95X_RESET_ISDBT_PID_FILTER = 69,
    IOCTL_IT95X_SET_OFS_CAL = 70,
    IOCTL_IT95X_ENABLE_TPS_CRYPT = 71,
    IOCTL_IT95X_DISABLE_TPS_CRYPT = 72,
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

struct ioctl_bus_info
{
    uint16_t usb_mode;
    uint16_t vendor_id;
    uint16_t product_id;
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

struct ioctl_iq_table
{
    uint32_t code;
    uint32_t version;
    uint32_t size;

    struct
    {
        uint32_t frequency;
        int32_t amp;
        int32_t phi;
    } data[IT95X_IQ_TABLE_SIZE];
};

struct ioctl_psi
{
    uint32_t code;
    uint8_t timer_id;
    uint8_t interval_ms;
    uint8_t packet[TS_PACKET_SIZE];
};

struct ioctl_tmcc
{
    uint32_t code;
    uint32_t a_constellation;
    uint32_t a_coderate;
    uint32_t b_constellation;
    uint32_t b_coderate;
    uint32_t partial;
    uint32_t sysid;
};

struct ioctl_tps
{
    uint32_t code;
    uint8_t high_coderate;
    uint8_t low_coderate;
    uint8_t tx_mode;
    uint8_t constellation;
    uint8_t guardinterval;
    uint16_t cell_id;
};

struct ioctl_tps_crypt
{
    uint32_t code;
    uint8_t reserved[12];
    uint32_t key;
};

struct ioctl_dc_cal
{
    uint32_t code;
    int32_t dc_i;
    int32_t dc_q;
    uint8_t reserved[8];
};

struct ioctl_ofs_cal
{
    uint32_t code;
    uint8_t reserved[8];
    uint8_t ofs_i;
    uint8_t ofs_q;
};

struct ioctl_dvbt
{
    uint32_t code;
    uint8_t coderate;
    uint8_t tx_mode;
    uint8_t constellation;
    uint8_t guardinterval;
};

struct ioctl_isdbt
{
    uint32_t code;
    uint32_t frequency; // XXX: unused?
    uint32_t bandwidth; // XXX: also unused? test w/signal meter
    uint32_t tx_mode;
    uint32_t guardinterval;
    uint32_t a_constellation;
    uint32_t a_coderate;
    uint32_t b_constellation;
    uint32_t b_coderate;
    uint32_t partial;
};

struct ioctl_add_pid
{
    uint32_t code;
    uint16_t idx;
    uint16_t pid;
    uint32_t layer;
    uint32_t reserved;
};

struct ioctl_ctl_pid
{
    uint32_t code;
    uint8_t reserved[8];
    uint8_t control;
    uint8_t layer;
};

/*
 * device context
 */

struct it95x_dev_t
{
    it95x_dev_info_t info;

    IBaseFilter *filter;
    HANDLE file;
    KSPROPERTY kslist[KSLIST_MAX];
    OVERLAPPED overlapped;
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

/* access KS property via device handle */
static
HRESULT ks_prop(HANDLE file, OVERLAPPED *overlapped
                , KSPROPERTY *prop, void *data, DWORD size)
{
    DWORD written;
    BOOL ok = DeviceIoControl(file, IOCTL_KS_PROPERTY
                              , prop, sizeof(*prop), data, size
                              , &written, overlapped);

    if (!ok && GetLastError() == ERROR_IO_PENDING)
        ok = GetOverlappedResult(file, overlapped, &written, TRUE);

    return (ok ? S_OK : HRESULT_FROM_WIN32(GetLastError()));
}

/* issue modulator ioctl */
static
HRESULT ioctl_set(it95x_dev_t *dev, void *data, DWORD size)
{
    return ks_prop(dev->file, &dev->overlapped
                   , &dev->kslist[KSLIST_IOCTL_SET], data, size);
}

/* retrieve ioctl result data */
static
HRESULT ioctl_get(it95x_dev_t *dev, void *data, DWORD size)
{
    return ks_prop(dev->file, &dev->overlapped
                   , &dev->kslist[KSLIST_IOCTL_GET], data, size);
}

/* get device handle from filter */
static
HRESULT handle_from_filter(IBaseFilter *filter, HANDLE *out)
{
    *out = NULL;

    IKsObject *obj = NULL;
    HRESULT hr = IBaseFilter_QueryInterface(filter, &IID_IKsObject
                                            , (void **)&obj);
    ASC_WANT_PTR(hr, obj);

    if (SUCCEEDED(hr))
    {
        *out = IKsObject_KsGetObjectHandle(obj);
        if (*out == NULL)
            hr = E_POINTER;

        ASC_RELEASE(obj);
    }

    return hr;
}

/* check property set support */
static
HRESULT check_properties(it95x_dev_t *dev)
{
    for (size_t i = 0; i < ASC_ARRAY_SIZE(dev->kslist); i++)
    {
        KSPROPERTY prop = dev->kslist[i];
        const ULONG flags = prop.Flags;
        prop.Flags = KSPROPERTY_TYPE_BASICSUPPORT;

        DWORD got = 0;
        HRESULT hr = ks_prop(dev->file, &dev->overlapped, &prop
                             , &got, sizeof(got));

        if (SUCCEEDED(hr))
        {
            DWORD want = 0;
            if (flags == KSPROPERTY_TYPE_GET)
                want = KSPROPERTY_SUPPORT_GET;
            else if (flags == KSPROPERTY_TYPE_SET)
                want = KSPROPERTY_SUPPORT_SET;

            if (!(got & want))
                hr = E_PROP_ID_UNSUPPORTED;
        }

        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

/* get bus mode and device IDs */
static
HRESULT get_bus_info(it95x_dev_t *dev, struct ioctl_bus_info *bus_info)
{
    memset(bus_info, 0, sizeof(*bus_info));
    return ks_prop(dev->file, &dev->overlapped
                   , &dev->kslist[KSLIST_BUS_INFO_GET]
                   , bus_info, sizeof(*bus_info));
}

/* get driver and firmware versions */
static
HRESULT get_drv_info(it95x_dev_t *dev, struct ioctl_drv_info *drv_info)
{
    memset(drv_info, 0, sizeof(*drv_info));

    struct ioctl_generic ioc =
    {
        .code = IOCTL_IT95X_GET_DRV_INFO,
    };

    HRESULT hr = ks_prop(dev->file, &dev->overlapped
                         , &dev->kslist[KSLIST_DRV_INFO_SET]
                         , &ioc, sizeof(ioc));
    if (SUCCEEDED(hr))
    {
        hr = ks_prop(dev->file, &dev->overlapped
                     , &dev->kslist[KSLIST_DRV_INFO_GET]
                     , drv_info, sizeof(*drv_info));
    }

    return hr;
}

/* get chip type */
#define REG_CHIP_VERSION 0x1222

static
HRESULT get_chip_type(it95x_dev_t *dev, uint16_t *chip_type)
{
    *chip_type = 0;

    struct ioctl_generic ioc_lsb =
    {
        .code = IOCTL_IT95X_RD_REG_LINK,
        .param1 = REG_CHIP_VERSION + 1,
    };

    HRESULT hr = ioctl_set(dev, &ioc_lsb, sizeof(ioc_lsb));
    if (SUCCEEDED(hr))
    {
        uint32_t lsb = 0;

        hr = ioctl_get(dev, &lsb, sizeof(lsb));
        if (SUCCEEDED(hr))
        {
            struct ioctl_generic ioc_msb =
            {
                .code = IOCTL_IT95X_RD_REG_LINK,
                .param1 = REG_CHIP_VERSION + 2,
            };

            hr = ioctl_set(dev, &ioc_msb, sizeof(ioc_msb));
            if (SUCCEEDED(hr))
            {
                uint32_t msb = 0;

                hr = ioctl_get(dev, &msb, sizeof(msb));
                if (SUCCEEDED(hr))
                {
                    *chip_type = ((msb << 8) & 0xff00) | (lsb & 0xff);
                }
            }
        }
    }

    return hr;
}

/* get device type */
static
HRESULT get_dev_type(it95x_dev_t *dev, uint8_t *dev_type)
{
    *dev_type = 0;

    struct ioctl_generic ioc =
    {
        .code = IOCTL_IT95X_GET_DEVICE_TYPE,
    };

    HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    if (SUCCEEDED(hr))
    {
        hr = ioctl_get(dev, &ioc, sizeof(ioc));
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

    it95x_dev_t *dev = NULL;
    char *name = NULL, *devpath = NULL;
    HANDLE file = NULL, event = NULL;

    IBaseFilter *filter = NULL;

    /* allocate device context */
    dev = (it95x_dev_t *)calloc(1, sizeof(*dev));
    if (dev == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto out;
    }

    memcpy(dev->kslist, kslist, sizeof(kslist));

    /* get device strings and check its property sets */
    hr = dshow_filter_from_moniker(moniker, &filter, &name);
    if (FAILED(hr)) goto out;

    hr = dshow_get_property(moniker, "DevicePath", &devpath);
    if (FAILED(hr)) goto out;

    hr = handle_from_filter(filter, &file);
    if (FAILED(hr)) goto out;

    event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (event == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto out;
    }

    dev->filter = filter;
    dev->file = file;
    dev->overlapped.hEvent = event;

    hr = check_properties(dev);
    if (FAILED(hr)) goto out;

    /* cache device information for application use */
    struct ioctl_bus_info bus_info;
    hr = get_bus_info(dev, &bus_info);
    if (FAILED(hr)) goto out;

    struct ioctl_drv_info drv_info;
    hr = get_drv_info(dev, &drv_info);
    if (FAILED(hr)) goto out;

    uint16_t chip_type;
    hr = get_chip_type(dev, &chip_type);
    if (FAILED(hr)) goto out;

    uint8_t dev_type;
    hr = get_dev_type(dev, &dev_type);
    if (FAILED(hr)) goto out;

    /* fill in device information */
    dev->info.name = name;
    dev->info.devpath = devpath;

    switch (bus_info.usb_mode)
    {
        case 0x0110: dev->info.usb_mode = IT95X_USB_11; break;
        case 0x0200: dev->info.usb_mode = IT95X_USB_20; break;
        default:
            dev->info.usb_mode = IT95X_USB_UNKNOWN;
    }

    dev->info.vendor_id = bus_info.vendor_id;
    dev->info.product_id = bus_info.product_id;

    dev->info.drv_pid = drv_info.drv_pid;
    dev->info.drv_version = drv_info.drv_version;
    dev->info.fw_link = drv_info.fw_link;
    dev->info.fw_ofdm = drv_info.fw_ofdm;
    dev->info.tuner_id = drv_info.tuner_id;

    dev->info.chip_type = chip_type;
    dev->info.dev_type = dev_type;
    dev->info.eagle2 = (chip_type >= 0x9510);

    *out = dev;

out:
    if (FAILED(hr))
    {
        ASC_FREE(event, CloseHandle);
        ASC_FREE(devpath, free);
        ASC_FREE(name, free);
        ASC_FREE(dev, free);

        /* NOTE: file handle is closed when filter is released */
        ASC_RELEASE(filter);
    }

    return hr;
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
#define IT95X_NAME_FILTER "IT95"

static
HRESULT check_moniker(IMoniker *moniker, bool *result)
{
    *result = false;

    char *prop_name = NULL;
    HRESULT hr = dshow_get_property(moniker, "FriendlyName", &prop_name);

    if (SUCCEEDED(hr))
    {
        if (strstr(prop_name, IT95X_NAME_FILTER) == prop_name)
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

/*
 * public API
 */

int it95x_dev_count(size_t *count)
{
    *count = 0;

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return ret_hr(hr);

    IEnumMoniker *enum_moniker = NULL;
    hr = dshow_enum(&KSCATEGORY_AUDIO_DEVICE, &enum_moniker
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

    CoUninitialize();

    return ret_hr(hr);
}

int it95x_open(ssize_t idx, const char *path, it95x_dev_t **dev)
{
    *dev = NULL;

    const size_t pathlen = (path != NULL ? strlen(path) : 0);
    if (idx < 0 && pathlen == 0)
        return ret_win32(ERROR_BAD_ARGUMENTS);

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return ret_hr(hr);

    /* search for requested device */
    IEnumMoniker *enum_moniker = NULL;
    IMoniker *dev_moniker = NULL;

    hr = dshow_enum(&KSCATEGORY_AUDIO_DEVICE, &enum_moniker
                    , CDEF_DEVMON_PNP_DEVICE);

    if (hr == S_OK)
    {
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
    }

    /* attempt initialization if found */
    if (dev_moniker != NULL)
    {
        hr = dev_from_moniker(dev_moniker, dev);
        IMoniker_Release(dev_moniker);
    }

    int ret = 0;
    if (*dev == NULL)
    {
        if (FAILED(hr))
            ret = ret_hr(hr);
        else
            ret = ret_win32(ERROR_BAD_UNIT);

        CoUninitialize();
    }

    return ret;
}

void it95x_get_info(const it95x_dev_t *dev, it95x_dev_info_t *info)
{
    memcpy(info, &dev->info, sizeof(*info));
}

void it95x_close(it95x_dev_t *dev)
{
    free(dev->info.name);
    free(dev->info.devpath);

    ASC_RELEASE(dev->filter);
    ASC_FREE(dev->overlapped.hEvent, CloseHandle);

    free(dev);

    CoUninitialize();
}

int it95x_get_gain(it95x_dev_t *dev, int *gain)
{
    *gain = 0;

    struct ioctl_generic ioc =
    {
        .code = IOCTL_IT95X_GET_GAIN,
    };

    HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    if (SUCCEEDED(hr))
    {
        hr = ioctl_get(dev, &ioc, sizeof(ioc));
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

    struct ioctl_gain_range ioc =
    {
        .code = IOCTL_IT95X_GET_GAIN_RANGE,
        .frequency = frequency,
        .bandwidth = bandwidth,
    };

    HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    if (SUCCEEDED(hr))
    {
        hr = ioctl_get(dev, &ioc, sizeof(ioc));
        if (SUCCEEDED(hr))
        {
            *max = ioc.max_gain;
            *min = ioc.min_gain;
        }
    }

    return ret_hr(hr);
}

int it95x_get_tmcc(it95x_dev_t *dev, it95x_tmcc_t *tmcc)
{
    memset(tmcc, 0, sizeof(*tmcc));

    struct ioctl_tmcc ioc =
    {
        .code = IOCTL_IT95X_GET_TMCC,
    };

    HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    if (SUCCEEDED(hr))
    {
        hr = ioctl_get(dev, &ioc, sizeof(ioc));
        if (SUCCEEDED(hr))
        {
            tmcc->partial = ioc.partial;
            tmcc->sysid = (it95x_sysid_t)ioc.sysid;

            tmcc->a.coderate = (it95x_coderate_t)ioc.a_coderate;
            tmcc->a.constellation = (it95x_constellation_t)ioc.a_constellation;

            tmcc->b.coderate = (it95x_coderate_t)ioc.b_coderate;
            tmcc->b.constellation = (it95x_constellation_t)ioc.b_constellation;
        }
    }

    return ret_hr(hr);
}

int it95x_get_tps(it95x_dev_t *dev, it95x_tps_t *tps)
{
    memset(tps, 0, sizeof(*tps));

    struct ioctl_tps ioc =
    {
        .code = IOCTL_IT95X_GET_TPS,
    };

    HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    if (SUCCEEDED(hr))
    {
        hr = ioctl_get(dev, &ioc, sizeof(ioc));
        if (SUCCEEDED(hr))
        {
            tps->high_coderate = (it95x_coderate_t)ioc.high_coderate;
            tps->low_coderate = (it95x_coderate_t)ioc.low_coderate;
            tps->tx_mode = (it95x_tx_mode_t)ioc.tx_mode;
            tps->constellation = (it95x_constellation_t)ioc.constellation;
            tps->guardinterval = (it95x_guardinterval_t)ioc.guardinterval;
            tps->cell_id = ntohs(ioc.cell_id);
        }
    }

    return ret_hr(hr);
}

int it95x_set_channel(it95x_dev_t *dev
                      , uint32_t frequency, uint32_t bandwidth)
{
    struct ioctl_generic ioc =
    {
        .code = IOCTL_IT95X_SET_CHANNEL,
        .param1 = frequency,
        .param2 = bandwidth,
    };

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_set_gain(it95x_dev_t *dev, int *gain)
{
    struct ioctl_generic ioc =
    {
        .code = IOCTL_IT95X_SET_GAIN,
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

    HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    if (SUCCEEDED(hr))
    {
        hr = ioctl_get(dev, &ioc, sizeof(ioc));
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

int it95x_set_iq(it95x_dev_t *dev, uint32_t version
                 , size_t size, const it95x_iq_t *data)
{
    if (size < 1 || size > IT95X_IQ_TABLE_SIZE)
        return ret_win32(ERROR_BAD_ARGUMENTS);

    struct ioctl_iq_table ioc =
    {
        .code = IOCTL_IT95X_SET_IQ_TABLE,
        .version = version,
        .size = size,
    };

    for (size_t i = 0; i < size; i++)
    {
        ioc.data[i].frequency = data[i].frequency;
        ioc.data[i].amp = data[i].amp;
        ioc.data[i].phi = data[i].phi;
    }

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_set_power(it95x_dev_t *dev, bool enable)
{
    struct ioctl_generic ioc =
    {
        .code = IOCTL_IT95X_SET_POWER,
        .param1 = enable,
    };

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_set_pcr(it95x_dev_t *dev, it95x_pcr_mode_t mode)
{
    struct ioctl_generic ioc =
    {
        .code = IOCTL_IT95X_SET_PCR_MODE,
        .param2 = mode,
    };

    // XXX: use "pcr enable" ioctl?
    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_set_psi(it95x_dev_t *dev
                  , unsigned int timer_id, unsigned int interval_ms
                  , const uint8_t packet[TS_PACKET_SIZE])
{
    if (timer_id < 1 || timer_id > IT95X_PSI_TIMER_CNT)
        return ret_win32(ERROR_BAD_ARGUMENTS);

    if (dev->info.eagle2)
    {
        /* NOTE: Eagle II drivers have zero-based timer numbering */
        timer_id--;
    }

    HRESULT hr = S_OK;

    if (packet != NULL && interval_ms > 0)
    {
        struct ioctl_psi ioc =
        {
            .code = IOCTL_IT95X_SET_PSI_PACKET,
            .timer_id = timer_id,
            .interval_ms = 0xff,
        };

        memcpy(ioc.packet, packet, TS_PACKET_SIZE);
        hr = ioctl_set(dev, &ioc, sizeof(ioc));
    }

    if (SUCCEEDED(hr))
    {
        struct ioctl_psi ioc =
        {
            .code = IOCTL_IT95X_SET_PSI_TIMER,
            .timer_id = timer_id,
            .interval_ms = interval_ms,
        };

        hr = ioctl_set(dev, &ioc, sizeof(ioc));
    }

    return ret_hr(hr);
}

int it95x_set_rf(it95x_dev_t *dev, bool enable)
{
    struct ioctl_generic ioc =
    {
        .code = IOCTL_IT95X_SET_RF_OUTPUT,
        .param1 = enable,
    };

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_set_tmcc(it95x_dev_t *dev, const it95x_tmcc_t *tmcc)
{
    struct ioctl_tmcc ioc =
    {
        .code = IOCTL_IT95X_SET_TMCC,
        .a_constellation = tmcc->a.constellation,
        .a_coderate = tmcc->a.coderate,
        .b_constellation = tmcc->b.constellation,
        .b_coderate = tmcc->b.coderate,
        .partial = tmcc->partial,
        .sysid = tmcc->sysid,
    };

    if (!ioc.partial)
    {
        ioc.b_constellation = ioc.a_constellation;
        ioc.b_coderate = ioc.a_coderate;
    }

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_set_tps(it95x_dev_t *dev, const it95x_tps_t *tps)
{
    struct ioctl_tps ioc =
    {
        .code = IOCTL_IT95X_SET_TPS,
        .high_coderate = tps->high_coderate,
        .low_coderate = tps->low_coderate,
        .tx_mode = tps->tx_mode,
        .constellation = tps->constellation,
        .guardinterval = tps->guardinterval,
        .cell_id = htons(tps->cell_id),
    };

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_set_tps_crypt(it95x_dev_t *dev, uint32_t key)
{
    struct ioctl_tps_crypt ioc;
    memset(&ioc, 0, sizeof(ioc));

    if (key != 0)
    {
        ioc.code = IOCTL_IT95X_ENABLE_TPS_CRYPT;
        ioc.key = key;
    }
    else
    {
        ioc.code = IOCTL_IT95X_DISABLE_TPS_CRYPT;
    }

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_set_dc_cal(it95x_dev_t *dev, int dc_i, int dc_q)
{
    struct ioctl_dc_cal ioc =
    {
        .code = IOCTL_IT95X_SET_DC_CAL,
        .dc_i = dc_i,
        .dc_q = dc_q,
    };

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_set_ofs_cal(it95x_dev_t *dev
                      , unsigned int ofs_i, unsigned int ofs_q)
{
    struct ioctl_ofs_cal ioc =
    {
        .code = IOCTL_IT95X_SET_OFS_CAL,
        .ofs_i = ofs_i,
        .ofs_q = ofs_q,
    };

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_set_dvbt(it95x_dev_t *dev, const it95x_dvbt_t *dvbt)
{
    struct ioctl_dvbt ioc =
    {
        .code = IOCTL_IT95X_SET_DVBT_MODULATION,
        .coderate = dvbt->coderate,
        .tx_mode = dvbt->tx_mode,
        .constellation = dvbt->constellation,
        .guardinterval = dvbt->guardinterval,
    };

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_set_isdbt(it95x_dev_t *dev, const it95x_isdbt_t *isdbt)
{
    struct ioctl_isdbt ioc =
    {
        .code = IOCTL_IT95X_SET_ISDBT_MODULATION,
        .tx_mode = isdbt->tx_mode,
        .guardinterval = isdbt->guardinterval,
        .a_constellation = isdbt->a.constellation,
        .a_coderate = isdbt->a.coderate,
        .b_constellation = isdbt->b.constellation,
        .b_coderate = isdbt->b.coderate,
        .partial = isdbt->partial,
    };

    if (!ioc.partial)
    {
        ioc.b_constellation = ioc.a_constellation;
        ioc.b_coderate = ioc.a_coderate;
    }

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_add_pid(it95x_dev_t *dev, unsigned int idx
                  , unsigned int pid, it95x_layer_t layer)
{
    if (idx < 1 || idx > IT95X_PID_LIST_SIZE)
        return ret_win32(ERROR_BAD_ARGUMENTS);

    struct ioctl_add_pid ioc =
    {
        .code = IOCTL_IT95X_ADD_ISDBT_PID_FILTER,
        .idx = idx,
        .pid = pid,
        .layer = layer,
    };

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_ctl_pid(it95x_dev_t *dev, it95x_layer_t layer)
{
    struct ioctl_ctl_pid ioc =
    {
        .code = IOCTL_IT95X_CONTROL_ISDBT_PID_FILTER,
        .layer = layer,
    };

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_reset_pid(it95x_dev_t *dev)
{
    struct ioctl_generic ioc =
    {
        .code = IOCTL_IT95X_RESET_ISDBT_PID_FILTER,
    };

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
}

int it95x_send_psi(it95x_dev_t *dev, const uint8_t packet[TS_PACKET_SIZE])
{
    struct ioctl_psi ioc =
    {
        .code = IOCTL_IT95X_SEND_PSI_ONCE,
    };

    memcpy(ioc.packet, packet, TS_PACKET_SIZE);

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return hr;
}

int it95x_send_ts(it95x_dev_t *dev, it95x_tx_block_t *data)
{
    data->code = IOCTL_IT95X_SEND_TS_DATA;

    const HRESULT hr = ioctl_set(dev, data, sizeof(*data));
    return ret_hr(hr);
}

int it95x_rd_reg(it95x_dev_t *dev, it95x_processor_t processor
                 , uint32_t address, uint8_t *value)
{
    *value = 0;

    struct ioctl_generic ioc =
    {
        .param1 = address,
    };

    switch (processor)
    {
        case IT95X_PROCESSOR_LINK:
            ioc.code = IOCTL_IT95X_RD_REG_LINK;
            break;

        case IT95X_PROCESSOR_OFDM:
            ioc.code = IOCTL_IT95X_RD_REG_OFDM;
            break;

        default:
            return ret_win32(ERROR_BAD_ARGUMENTS);
    }

    HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    if (SUCCEEDED(hr))
    {
        uint32_t out = 0;
        hr = ioctl_get(dev, &out, sizeof(out));

        if (SUCCEEDED(hr))
            *value = out;
    }

    return ret_hr(hr);
}

int it95x_wr_reg(it95x_dev_t *dev, it95x_processor_t processor
                 , uint32_t address, uint8_t value)
{
    struct ioctl_generic ioc =
    {
        .param1 = address,
        .param2 = value,
    };

    switch (processor)
    {
        case IT95X_PROCESSOR_LINK:
            ioc.code = IOCTL_IT95X_WR_REG_LINK;
            break;

        case IT95X_PROCESSOR_OFDM:
            ioc.code = IOCTL_IT95X_WR_REG_OFDM;
            break;

        default:
            return ret_win32(ERROR_BAD_ARGUMENTS);
    }

    const HRESULT hr = ioctl_set(dev, &ioc, sizeof(ioc));
    return ret_hr(hr);
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
        return strdup("Modulator firmware error");

        /*
         * NOTE: Windows drivers don't set firmware error code on failed
         *       ioctl, so no API in this implementation can actually return
         *       a positive value.
         */
    }
}
