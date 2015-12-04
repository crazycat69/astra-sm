/*
 * Astra Core (Logging)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *                    2015, Artem Kharitonov <artem@sysert.ru>
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

#ifndef _ASC_LOG_H_
#define _ASC_LOG_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

void asc_log_set_stdout(bool val);
void asc_log_set_debug(bool val);
void asc_log_set_color(bool val);
void asc_log_set_file(const char *val);
#ifndef _WIN32
void asc_log_set_syslog(const char *val);
#endif
bool asc_log_is_debug(void) __func_pure;

void asc_log_core_init(void);
void asc_log_core_destroy(void);

void asc_log_reopen(void);

void asc_log_info(const char *msg, ...) __fmt_printf(1, 2);
void asc_log_error(const char *msg, ...) __fmt_printf(1, 2);
void asc_log_warning(const char *msg, ...) __fmt_printf(1, 2);
void asc_log_debug(const char *msg, ...) __fmt_printf(1, 2);

#endif /* _ASC_LOG_H_ */
