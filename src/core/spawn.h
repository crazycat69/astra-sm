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

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

enum
{
    PIPE_RD = 0,
    PIPE_WR,
    PIPE_BOTH,
    PIPE_NONE,
};

#ifdef _WIN32

typedef struct
{
    PROCESS_INFORMATION pi;
    HANDLE job;
} asc_process_t;

static inline
pid_t asc_process_id(const asc_process_t *proc)
{
    return proc->pi.dwProcessId;
}

static inline
void asc_process_free(asc_process_t *proc)
{
    ASC_FREE(proc->pi.hProcess, CloseHandle);
    ASC_FREE(proc->pi.hThread, CloseHandle);
    ASC_FREE(proc->job, CloseHandle);
}

pid_t asc_process_wait(const asc_process_t *proc, int *status, bool block);

#else /* _WIN32 */

#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>

typedef pid_t asc_process_t;

static inline
pid_t asc_process_id(const asc_process_t *proc)
{
    return *proc;
}

static inline
void asc_process_free(asc_process_t *proc)
{
    *proc = -1;
}

static inline
pid_t asc_process_wait(const asc_process_t *proc, int *status, bool block)
{
    return waitpid(*proc, status, (block ? 0 : WNOHANG));
}

#endif /* !_WIN32 */

int asc_process_spawn(const char *command, asc_process_t *proc
                      , int *parent_sin, int *parent_sout, int *parent_serr);
int asc_process_kill(const asc_process_t *proc, bool forced);

int asc_pipe_open(int fds[2], int *nb_fd, unsigned int nb_side);
int asc_pipe_inherit(int fd, bool inherit);
int asc_pipe_close(int fd);

#endif /* _ASC_SPAWN_H_ */
