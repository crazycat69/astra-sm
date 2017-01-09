/*
 * Astra Lua Library (Windows Service Installer)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2016-2017, Artem Kharitonov <artem@3phase.pw>
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
 * Install and remove Windows service
 *
 * Methods:
 *      winsvc.install({ options })
 *                  - install service
 *      winsvc.uninstall({ options })
 *                  - uninstall service
 *
 * Module Options:
 *      name        - string, service name
 *                      optional, defaults to "astra-sm"
 *      displayname - string, name displayed in the services MSC
 *                      optional, defaults to PACKAGE_NAME
 *      description - string, description displayed in the services MSC
 *                      optional, defaults to PACKAGE_NAME
 *      arguments   - string, command line arguments for service
 *                      this option is required
 *      start       - string, startup mode: auto, manual or disabled
 *                      optional, defaults to "manual"
 *      force       - boolean, skip safety checks when removing service
 *                      optional, default is false
 *
 * NOTE: When removing services, all options except for 'name' and 'force'
 *       are ignored.
 */

#include <astra/astra.h>
#include <astra/luaapi/module.h>

#define MSG(_msg) "[winsvc] " _msg

/* hardcode default name in case PACKAGE gets changed later on */
#define SVC_DEFAULT_NAME "astra-sm"

/* configure it as a network service */
#define SVC_DEPENDENCIES "Tcpip\0"
#define SVC_STARTNAME "NT AUTHORITY\\LocalService"

/* option getter to prevent the user from passing empty strings */
#define SVC_OPTION(__opt, __default) \
    const char *__opt = __default; \
    module_option_string(L, #__opt, &__opt, NULL); \
    if (__opt == NULL || strlen(__opt) == 0) \
        luaL_error(L, MSG("option '" #__opt "' cannot be empty"));

/* push error string before cleanup to get correct last error no. */
#define SVC_ERROR(...) \
    do { \
        lua_pushfstring(L, __VA_ARGS__); \
        goto out; \
    } while (0)

#define SVC_PERROR(_func) \
    SVC_ERROR(_func ": %s", asc_error_msg())

/* check if a user account exists on the local system */
static
bool check_account(const char *acct)
{
    wchar_t *const wacct = cx_widen(acct);
    if (wacct == NULL)
        return false;

    /* first call should fail and set required buffer sizes */
    bool found = false;

    DWORD sidbuf = 0, rdnbuf = 0;
    SID_NAME_USE type;

    BOOL ret = LookupAccountNameW(NULL, wacct, NULL, &sidbuf
                                  , NULL, &rdnbuf, &type);

    if (!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER
        && sidbuf > 0 && rdnbuf > 0)
    {
        /* create buffers and get account SID */
        wchar_t *const rdn = (wchar_t *)calloc(rdnbuf, sizeof(*rdn));
        SID *const sid = (SID *)calloc(1, sidbuf);

        if (rdn != NULL && sid != NULL)
        {
            ret = LookupAccountNameW(NULL, wacct, sid, &sidbuf
                                     , rdn, &rdnbuf, &type);

            found = (ret && IsValidSid(sid));
        }
        else
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        }

        free(sid);
        free(rdn);
    }

    free(wacct);

    return found;
}

/* return full .exe path, quoted and followed by a space */
static
char *quoted_exepath(void)
{
    char *const path = cx_exepath();
    if (path == NULL)
        return NULL;

    char *const quoted = (char *)malloc(4 + (strlen(path) * 2));
    if (quoted != NULL)
    {
        const char *s = path;
        char *d = quoted;

        *d++ = '"';
        while (*s != '\0')
        {
            if (*s == '"')
                *d++ = '\\';

            *d++ = *s++;
        }
        *d++ = '"';
        *d++ = ' ';
        *d++ = '\0';
    }
    else
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    }

    free(path);

    return quoted;
}

/* install service after converting arguments into wide strings */
static
SC_HANDLE create_service(SC_HANDLE scm, const char *name
                         , const char *displayname, DWORD stype
                         , const char *cmdline, const char *startname)
{
    SC_HANDLE svc = NULL;

    wchar_t *wname = NULL, *wdisplayname = NULL;
    wchar_t *wcmdline = NULL, *wstartname = NULL;

    wname = cx_widen(name);
    if (wname == NULL) return NULL;

    if (displayname != NULL)
    {
        wdisplayname = cx_widen(displayname);
        if (wdisplayname == NULL) goto out;
    }

    if (cmdline != NULL)
    {
        wcmdline = cx_widen(cmdline);
        if (wcmdline == NULL) goto out;
    }

    if (startname != NULL)
    {
        wstartname = cx_widen(startname);
        if (wstartname == NULL) goto out;
    }

    svc = CreateServiceW(scm, wname, wdisplayname
                         , SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS
                         , stype, SERVICE_ERROR_NORMAL, wcmdline, NULL, NULL
                         , L"" SVC_DEPENDENCIES, wstartname, NULL);

out:
    free(wstartname);
    free(wcmdline);
    free(wdisplayname);
    free(wname);

    return svc;
}

static
int method_install(lua_State *L)
{
    char *exepath = NULL, *cmdline = NULL;
    size_t exelen = 0, arglen = 0;
    SC_HANDLE scm = NULL, svc = NULL;
    SERVICE_DESCRIPTIONW info = { NULL };

    lua_pushvalue(L, -1);
    SVC_OPTION(name, SVC_DEFAULT_NAME);
    SVC_OPTION(displayname, PACKAGE_NAME);
    SVC_OPTION(description, PACKAGE_NAME);
    SVC_OPTION(arguments, NULL);
    SVC_OPTION(start, "manual");

    /* check account name */
    const char *startname = SVC_STARTNAME;
    if (!check_account(startname))
        startname = NULL;

    /* check service start type */
    DWORD stype = -1;
    if (!strcmp(start, "auto"))
        stype = SERVICE_AUTO_START;
    else if (!strcmp(start, "manual"))
        stype = SERVICE_DEMAND_START;
    else if (!strcmp(start, "disabled"))
        stype = SERVICE_DISABLED;
    else
        SVC_ERROR(MSG("invalid service startup mode: '%s'"), start);

    /* build command line */
    exepath = quoted_exepath();
    if (exepath == NULL)
        SVC_PERROR(MSG("quoted_exepath()"));

    exelen = strlen(exepath);
    arglen = strlen(arguments) + 1; /* terminator */

    cmdline = (char *)malloc(exelen + arglen);
    if (cmdline == NULL)
        SVC_ERROR(MSG("malloc() failed"));

    memcpy(cmdline, exepath, exelen);
    memcpy(&cmdline[exelen], arguments, arglen);

    /* register service in the database */
    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scm == NULL)
        SVC_PERROR(MSG("OpenSCManager()"));

    svc = create_service(scm, name, displayname, stype, cmdline, startname);
    if (svc == NULL)
        SVC_PERROR(MSG("create_service()"));

    info.lpDescription = cx_widen(description);
    if (info.lpDescription == NULL)
        SVC_PERROR(MSG("cx_widen()"));

    if (!ChangeServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, &info))
        SVC_PERROR(MSG("ChangeServiceConfig2()"));

    /* that's all, folks */
    lua_pushboolean(L, 1);

out:
    ASC_FREE(info.lpDescription, free);
    ASC_FREE(svc, CloseServiceHandle);
    ASC_FREE(scm, CloseServiceHandle);
    ASC_FREE(cmdline, free);
    ASC_FREE(exepath, free);

    if (lua_isstring(L, -1))
        lua_error(L);

    return 1;
}

static
int method_uninstall(lua_State *L)
{
    wchar_t *wname = NULL;
    SC_HANDLE scm = NULL, svc = NULL;
    DWORD qbuf = 0, qbufwant = 0;
    QUERY_SERVICE_CONFIGW *query = NULL;
    char *exepath = NULL, *binpath = NULL;

    lua_pushvalue(L, -1);
    SVC_OPTION(name, SVC_DEFAULT_NAME);

    bool force = false;
    module_option_boolean(L, "force", &force);

    /* open SCM database and service */
    wname = cx_widen(name);
    if (wname == NULL)
        SVC_PERROR(MSG("cx_widen()"));

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scm == NULL)
        SVC_PERROR(MSG("OpenSCManager()"));

    svc = OpenServiceW(scm, wname, SC_MANAGER_ALL_ACCESS);
    if (svc == NULL)
        SVC_PERROR(MSG("OpenService()"));

    /* delete the service after running a safety check */
    if (!force)
    {
        /* get command line from registry */
        if (QueryServiceConfigW(svc, NULL, 0, &qbufwant)
            || GetLastError() != ERROR_INSUFFICIENT_BUFFER
            || qbufwant == 0) /* should fail and set qbufwant */
        {
            SVC_PERROR(MSG("QueryServiceConfig()"));
        }

        qbuf = qbufwant;
        query = (QUERY_SERVICE_CONFIGW *)LocalAlloc(LMEM_FIXED, qbuf);
        if (query == NULL)
            SVC_PERROR(MSG("LocalAlloc()"));

        if (!QueryServiceConfigW(svc, query, qbuf, &qbufwant))
            SVC_PERROR(MSG("QueryServiceConfig()"));

        /* compare with actual binary path */
        exepath = quoted_exepath();
        if (exepath == NULL)
            SVC_PERROR(MSG("quoted_exepath()"));

        binpath = cx_narrow(query->lpBinaryPathName);
        if (binpath == NULL)
            SVC_PERROR(MSG("cx_narrow()"));

        if (strncmp(exepath, binpath, strlen(exepath)))
        {
            SVC_ERROR(MSG("ImagePath in service '%s' points to a different "
                          "binary; use 'force' to override"), name);
        }
    }

    if (!DeleteService(svc))
        SVC_PERROR(MSG("DeleteService()"));

    /* return true on success */
    lua_pushboolean(L, 1);

out:
    ASC_FREE(binpath, free);
    ASC_FREE(exepath, free);
    ASC_FREE(query, LocalFree);
    ASC_FREE(svc, CloseServiceHandle);
    ASC_FREE(scm, CloseServiceHandle);
    ASC_FREE(wname, free);

    if (lua_isstring(L, -1))
        lua_error(L);

    return 1;
}

static
void module_load(lua_State *L)
{
    static const luaL_Reg api[] =
    {
        { "install", method_install },
        { "uninstall", method_uninstall },
        { NULL, NULL },
    };

    luaL_newlib(L, api);
    lua_setglobal(L, "winsvc");
}

BINDING_REGISTER(winsvc)
{
    .load = module_load,
};
