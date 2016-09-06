/*
 * Astra Lua Library (Windows Service Installer)
 * http://cesbo.com/astra
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

/* push error string before cleanup to get correct last error */
#define SVC_ERROR(...) \
    do { \
        lua_pushfstring(L, __VA_ARGS__); \
        goto out; \
    } while (0)

#define SVC_PERROR(_func) \
    SVC_ERROR(_func ": %s", asc_error_msg())

/* return full .exe path, quoted and followed by a space */
static
char *quoted_exe_path(void)
{
    char path[MAX_PATH] = { 0 };
    const DWORD ret = GetModuleFileName(NULL, path, sizeof(path));

    if (ret == 0 || ret >= sizeof(path))
        return NULL;

    /* escape double quotes */
    char *const quoted = ASC_ALLOC(4 + (strlen(path) * 2), char);
    char *s = path, *d = quoted;

    *d++ = '"';
    while (*s != '\0')
    {
        if (*s == '"')
            *d++ = '\\';

        *d++ = *s++;
    }
    *d++ = '"';
    *d++ = ' ';

    return quoted;
}

/* check if a user account exists on the system */
static
bool check_account(const char *acct)
{
    /* get required buffer size */
    DWORD sidbuf = 0, rdnbuf = 0;
    SID_NAME_USE type;

    if (LookupAccountName(NULL, acct, NULL, &sidbuf, NULL, &rdnbuf, &type)
        || GetLastError() != ERROR_INSUFFICIENT_BUFFER
        || sidbuf == 0 || rdnbuf == 0) /* should fail and set buffer sizes */
    {
        return false;
    }

    /* create buffers and get account SID */
    TCHAR *const rdn = ASC_ALLOC(rdnbuf, TCHAR);
    SID *const sid = (SID *)calloc(1, sidbuf);
    asc_assert(sid != NULL, MSG("calloc() failed"));

    bool ok = false;
    if (LookupAccountName(NULL, acct, sid, &sidbuf, rdn, &rdnbuf, &type)
        && IsValidSid(sid))
    {
        ok = true;
    }

    free(sid);
    free(rdn);

    return ok;
}

static
int method_install(lua_State *L)
{
    SC_HANDLE scm = NULL, svc = NULL;
    SERVICE_DESCRIPTION info = { NULL };
    char *exepath = NULL, *cmdline = NULL;
    size_t cmdsize = 0;

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

    /* open SCM database */
    scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scm == NULL)
        SVC_PERROR(MSG("OpenSCManager()"));

    /* build command line */
    exepath = quoted_exe_path();
    if (exepath == NULL)
        SVC_PERROR(MSG("GetModuleFileName()"));

    cmdsize = strlen(exepath) + strlen(arguments) + 1;
    cmdline = ASC_ALLOC(cmdsize, char);
    snprintf(cmdline, cmdsize, "%s%s", exepath, arguments);

    /* register service in the database */
    svc = CreateService(scm, name, displayname
                        , SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS
                        , stype, SERVICE_ERROR_NORMAL, cmdline, NULL, NULL
                        , SVC_DEPENDENCIES, startname, NULL);
    if (svc == NULL)
        SVC_PERROR(MSG("CreateService()"));

    /* set description string */
    info.lpDescription = strdup(description);
    if (info.lpDescription == NULL)
        SVC_ERROR(MSG("strdup() failed"));

    if (!ChangeServiceConfig2(svc, SERVICE_CONFIG_DESCRIPTION, &info))
        SVC_PERROR(MSG("ChangeServiceConfig2()"));

    /* that's all, folks */
    lua_pushboolean(L, 1);

out:
    ASC_FREE(info.lpDescription, free);
    ASC_FREE(cmdline, free);
    ASC_FREE(exepath, free);
    ASC_FREE(svc, CloseServiceHandle);
    ASC_FREE(scm, CloseServiceHandle);

    if (lua_isstring(L, -1))
        lua_error(L);

    return 1;
}

static
int method_uninstall(lua_State *L)
{
    SC_HANDLE scm = NULL, svc = NULL;
    DWORD qbuf = 0, qbufwant = 0;
    QUERY_SERVICE_CONFIG *query = NULL;
    char *exepath = NULL;
    bool force = false;

    lua_pushvalue(L, -1);
    SVC_OPTION(name, SVC_DEFAULT_NAME);
    module_option_boolean(L, "force", &force);

    /* open SCM database and service */
    scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scm == NULL)
        SVC_PERROR(MSG("OpenSCManager()"));

    svc = OpenService(scm, name, SC_MANAGER_ALL_ACCESS);
    if (svc == NULL)
        SVC_PERROR(MSG("OpenService()"));

    /* delete the service after running a safety check */
    if (!force)
    {
        /* get command line from registry */
        if (QueryServiceConfig(svc, NULL, 0, &qbufwant)
            || GetLastError() != ERROR_INSUFFICIENT_BUFFER
            || qbufwant == 0) /* should fail and set qbufwant */
        {
            SVC_PERROR(MSG("QueryServiceConfig()"));
        }

        qbuf = qbufwant;
        query = (QUERY_SERVICE_CONFIG *)LocalAlloc(LMEM_FIXED, qbuf);
        if (query == NULL)
            SVC_PERROR(MSG("LocalAlloc()"));

        if (!QueryServiceConfig(svc, query, qbuf, &qbufwant))
            SVC_PERROR(MSG("QueryServiceConfig()"));

        /* compare with actual binary path */
        exepath = quoted_exe_path();
        if (exepath == NULL)
            SVC_PERROR(MSG("GetModuleFileName()"));

        if (strncmp(exepath, query->lpBinaryPathName, strlen(exepath)))
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
    ASC_FREE(exepath, free);
    ASC_FREE(query, LocalFree);
    ASC_FREE(svc, CloseServiceHandle);
    ASC_FREE(scm, CloseServiceHandle);

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
