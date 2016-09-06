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

#include <astra/astra.h>
#include <astra/core/mainloop.h>

#include "sig.h"

#define lock_enter() \
    WaitForSingleObject(signal_lock, INFINITE)

#define lock_enter_timed() \
    (WaitForSingleObject(signal_lock, SIGNAL_LOCK_WAIT) == WAIT_OBJECT_0)

#define lock_leave() \
    do { \
        while (ReleaseMutex(signal_lock)) \
            ; /* nothing */ \
    } while (0)

#define SERVICE_NAME (char *)PACKAGE_NAME

static SERVICE_STATUS service_status;
static SERVICE_STATUS_HANDLE service_status_handle = NULL;
static HANDLE service_event = NULL;
static HANDLE service_thread = NULL;

static HANDLE signal_lock = NULL;
static bool ignore_ctrl = true;

#ifdef DEBUG
static
void reopen(const char *path, FILE *origfile, int origfd)
{
    FILE *const newfile = freopen(path, "ab", origfile);
    if (newfile != NULL)
    {
        setvbuf(origfile, NULL, _IONBF, 0);
        dup2(fileno(newfile), origfd);
    }
}

static
void redirect_stdio(void)
{
    char buf[MAX_PATH] = { 0 };
    const DWORD ret = GetModuleFileName(NULL, buf, sizeof(buf));

    if (ret == 0 || ret >= sizeof(buf))
        buf[0] = '\0';

    char *const p = strrchr(buf, '\\');
    if (p != NULL)
        *p = '\0';
    else
        memcpy(buf, ".", sizeof("."));

    static const char logfile[] = "\\stdio.log";
    strncat(buf, logfile, sizeof(buf) - strlen(buf) - 1);

    reopen(buf, stdout, STDOUT_FILENO);
    reopen(buf, stderr, STDERR_FILENO);
}
#endif /* DEBUG */

static inline
void service_set_state(DWORD state)
{
    service_status.dwCurrentState = state;
    SetServiceStatus(service_status_handle, &service_status);
}

static WINAPI
void service_handler(DWORD control)
{
    switch (control)
    {
        case SERVICE_CONTROL_STOP:
            if (service_status.dwCurrentState == SERVICE_RUNNING)
            {
                /*
                 * NOTE: stop pending state should really be set by
                 *       signal_enable(), but then we'd have to somehow
                 *       filter out soft restart requests.
                 */
                service_set_state(SERVICE_STOP_PENDING);

                if (lock_enter_timed())
                {
                    if (!ignore_ctrl)
                        asc_main_loop_shutdown();

                    lock_leave();
                }
                else
                {
                    signal_timeout();
                }
            }
            break;

        case SERVICE_CONTROL_INTERROGATE:
            service_set_state(service_status.dwCurrentState);
            break;

        default:
            break;
    }
}

static WINAPI
BOOL console_handler(DWORD type)
{
    /* NOTE: handlers are run in separate threads */
    switch (type)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            if (lock_enter_timed())
            {
                if (!ignore_ctrl)
                    asc_main_loop_shutdown();

                lock_leave();
            }
            else
            {
                signal_timeout();
            }
            return true;

        default:
            return false;
    }
}

static WINAPI
void service_main(DWORD argc, LPTSTR *argv)
{
    __uarg(argc);
    __uarg(argv);

    /* register control handler */
    ignore_ctrl = false;
    service_status_handle =
        RegisterServiceCtrlHandler(SERVICE_NAME, service_handler);

    if (service_status_handle == NULL)
        signal_perror(GetLastError(), "RegisterServiceCtrlHandler()");

    /* report to SCM */
    service_set_state(SERVICE_START_PENDING);

    /* notify main thread */
    SetEvent(service_event);
}

static __stdcall
unsigned int service_thread_proc(void *arg)
{
    __uarg(arg);

    /*
     * NOTE: here we use a dedicated thread for the blocking call
     *       to StartServiceCtrlDispatcher(), which allows us to keep
     *       the normal startup routine unaltered.
     */
    static const SERVICE_TABLE_ENTRY svclist[] = {
        { SERVICE_NAME, service_main },
        { NULL, NULL },
    };

    if (!StartServiceCtrlDispatcher(svclist))
        return GetLastError();

    return ERROR_SUCCESS;
}

static
bool service_initialize(void)
{
    /* initialize service state struct */
    memset(&service_status, 0, sizeof(service_status));
    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    service_status.dwCurrentState = SERVICE_STOPPED;

    /* attempt to connect to SCM */
    service_event = CreateEvent(NULL, false, false, NULL);
    if (service_event == NULL)
        signal_perror(GetLastError(), "CreateEvent()");

    const intptr_t thr = _beginthreadex(NULL, 0, service_thread_proc, NULL
                                        , 0, NULL);
    if (thr <= 0)
    {
        fprintf(stderr, "_beginthreadex(): %s\n", strerror(errno));
        _exit(EXIT_SIGHANDLER);
    }
    service_thread = (HANDLE)thr;

    const HANDLE handles[2] = { service_event, service_thread };
    const DWORD ret = WaitForMultipleObjects(2, handles, false, INFINITE);
    ASC_FREE(service_event, CloseHandle);

    if (ret == WAIT_OBJECT_0)
    {
        /* service_event fired; SCM connection successful */
        return true;
    }
    else if (ret == WAIT_OBJECT_0 + 1)
    {
        /* service_thread exited, which means SCM connection failed */
        DWORD exit_code = ERROR_INTERNAL_ERROR;
        if (GetExitCodeThread(service_thread, &exit_code))
        {
            if (exit_code == ERROR_SUCCESS)
                /* shouldn't return success this early */
                exit_code = ERROR_INTERNAL_ERROR;
        }

        ASC_FREE(service_thread, CloseHandle);
        if (exit_code != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
            signal_perror(exit_code, "StartServiceCtrlDispatcher()");
    }
    else
    {
        /* shouldn't happen */
        if (ret != WAIT_FAILED)
            SetLastError(ERROR_INTERNAL_ERROR);

        signal_perror(GetLastError(), "WaitForMultipleObjects()");
    }

    return false;
}

static
bool service_destroy(void)
{
    if (service_thread != NULL)
    {
        /* notify SCM if we're exiting because of an error */
        if (asc_exit_status != EXIT_SUCCESS)
        {
            service_status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
            service_status.dwServiceSpecificExitCode = asc_exit_status;
        }

        /* report service shutdown, join dispatcher thread */
        if (service_status_handle != NULL)
            service_set_state(SERVICE_STOPPED);
        else
            TerminateThread(service_thread, ERROR_SUCCESS);

        WaitForSingleObject(service_thread, INFINITE);
        ASC_FREE(service_thread, CloseHandle);

        /* reset service state */
        memset(&service_status, 0, sizeof(service_status));
        service_status_handle = NULL;

        return true;
    }

    return false;
}

static
void signal_cleanup(void)
{
    /* dismiss any threads waiting for lock */
    lock_enter();
    ignore_ctrl = true;
    lock_leave();

    if (!service_destroy())
    {
        /* remove ctrl handler; this also joins handler threads */
        if (!SetConsoleCtrlHandler(console_handler, false))
            signal_perror(GetLastError(), "SetConsoleCtrlHandler()");
    }

    /* free mutex */
    ASC_FREE(signal_lock, CloseHandle);
}

void signal_setup(void)
{
    /* create and lock signal handling mutex */
    signal_lock = CreateMutex(NULL, true, NULL);
    if (signal_lock == NULL)
        signal_perror(GetLastError(), "CreateMutex()");

    bool is_service = false;

    if (GetStdHandle(STD_INPUT_HANDLE) == NULL
        && GetStdHandle(STD_OUTPUT_HANDLE) == NULL
        && GetStdHandle(STD_ERROR_HANDLE) == NULL)
    {
        /* no console handles; probably running under SCM */
        if (AllocConsole())
        {
            const HWND cons = GetConsoleWindow();
            if (cons != NULL)
                ShowWindow(cons, SW_HIDE);
        }

#ifdef DEBUG
        redirect_stdio();
#endif
        is_service = service_initialize();
    }

    if (!is_service)
    {
        /* set up regular console control handler */
        ignore_ctrl = false;
        if (!SetConsoleCtrlHandler(console_handler, true))
            signal_perror(GetLastError(), "SetConsoleCtrlHandler()");
    }

    atexit(signal_cleanup);
}

void signal_enable(bool running)
{
    lock_enter();

    if (running)
    {
        /* mark service as running on first init */
        if (service_status.dwCurrentState == SERVICE_START_PENDING)
            service_set_state(SERVICE_RUNNING);

        lock_leave();
    }
}
