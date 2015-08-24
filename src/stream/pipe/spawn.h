/*
 * Astra Core (Process spawning)
 *
 * Copyright (C) 2015, Artem Kharitonov <artem@sysert.ru>
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

#ifndef _ASC_SPAWN_H_
#define _ASC_SPAWN_H_ 1

/* TODO: move this to core/spawn */

// XXX: should we use handles or pids on Windows?
typedef pid_t asc_pid_t;

asc_pid_t asc_child_spawn(const char *command
                          , int *sin, int *sout, int *serr);

int asc_pipe_open(int fds[2], int *parent_fd, int parent_side);
int asc_pipe_close(int fd);

#endif /* _ASC_SPAWN_H_ */
