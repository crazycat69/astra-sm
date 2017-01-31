/*
 * Astra Module: DirectShow (GUIDs)
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

/*
 * Apparently, MinGW's version of the strmiids library is incomplete
 * with many GUIDs missing. This is a miniature version of it made by
 * hand-picking needed GUIDs from the Windows SDK.
 *
 * Including initguid.h causes the DEFINE_GUID macro to emit actual
 * GUID definitions instead of extern references. In order to avoid
 * duplicating symbols, it should not be included from more than one
 * source file.
 */

/* clang doesn't support this */
#define DECLSPEC_SELECTANY

#include <initguid.h>
#include "guids.h"
