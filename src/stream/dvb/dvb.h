/*
 * Astra Module: DVB
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
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

#ifndef _DVB_H_
#define _DVB_H_ 1

#include <astra.h>
#include <luaapi/stream.h>
#include <mpegts/psi.h>

#include <poll.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/ca.h>

#include "frontend.h"
#include "ca.h"

#ifndef DTV_STREAM_ID
#   define DTV_STREAM_ID DTV_ISDBS_TS_ID
#endif

#ifndef NO_STREAM_ID_FILTER
#   define NO_STREAM_ID_FILTER (~0U)
#endif

#ifndef DTV_MODCODE
#   define DTV_MODCODE (DTV_STREAM_ID + 1)
#endif

#ifndef ALL_MODCODES
#   define ALL_MODCODES (~0U)
#endif

#ifndef HAVE_DVBAPI_C_ANNEX_AC
#   define SYS_DVBC_ANNEX_A SYS_DVBC_ANNEX_AC
#   define SYS_DVBC_ANNEX_C SYS_DVBC_ANNEX_AC
#endif

#endif /* _DVB_H_ */
