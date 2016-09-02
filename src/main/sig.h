/*
 * Astra (OS signal handling)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015-2016, Artem Kharitonov <artem@3phase.pw>
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

#ifndef _MAIN_SIG_H_
#define _MAIN_SIG_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

#define SIGNAL_LOCK_WAIT 5000 /* ms */

void signal_setup(void);
void signal_enable(bool running);

static inline
void signal_timeout(void)
{
    fprintf(stderr, "wait timeout for signal lock\n");
    _exit(EXIT_SIGHANDLER);
}

static inline
void signal_perror(int errnum, const char *str)
{
    char buf[512] = { 0 };
    asc_strerror(errnum, buf, sizeof(buf));
    fprintf(stderr, "%s: %s\n", str, buf);
    _exit(EXIT_SIGHANDLER);
}

#endif /* _MAIN_SIG_H_ */
