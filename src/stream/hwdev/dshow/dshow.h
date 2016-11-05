/*
 * Astra Module: DirectShow
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

#ifndef _HWDEV_DSHOW_H_
#define _HWDEV_DSHOW_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

/* include GUIDs first to suppress redeclaration warnings */
#include "guids.h"

#include <dshow.h>

typedef void (*sample_callback_t)(void *, const void *, size_t);

char *dshow_error_msg(HRESULT hr);
HRESULT dshow_enum(const CLSID *category, IEnumMoniker **out);
HRESULT dshow_filter_by_index(const CLSID *category, size_t index
                              , IBaseFilter **out, char **fname);
HRESULT dshow_filter_by_path(const CLSID *category, const char *devpath
                             , IBaseFilter **out, char **fname);
HRESULT dshow_filter_from_moniker(IMoniker *moniker, IBaseFilter **out
                                  , char **fname);
HRESULT dshow_find_pin(IBaseFilter *filter, PIN_DIRECTION dir
                       , bool skip_busy, const char *name, IPin **out);
HRESULT dshow_get_graph(IBaseFilter *filter, IFilterGraph2 **out);
HRESULT dshow_get_property(IMoniker *moniker, const char *prop, char **out);
bool dshow_pin_connected(IPin *pin);

HRESULT dshow_grabber(sample_callback_t callback, void *arg
                      , const AM_MEDIA_TYPE *media_type, IBaseFilter **out);

#endif /* _HWDEV_DSHOW_H_ */
