/*
 * Astra Core (Error Messages)
 * http://cesbo.com/astra
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

#include <astra/astra.h>
#include <astra/core/error.h>

/* FIXME: use platform-specific TLS routines */
static /*__thread*/ char msg_buf[1024];

char *asc_strerror(int errnum, char *buf, size_t buflen)
{
    char *msg = NULL;

#ifdef _WIN32
    const DWORD lang_id = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    const DWORD ret = FormatMessageA((FORMAT_MESSAGE_FROM_SYSTEM
                                      | FORMAT_MESSAGE_ALLOCATE_BUFFER
                                      | FORMAT_MESSAGE_IGNORE_INSERTS
                                      | FORMAT_MESSAGE_MAX_WIDTH_MASK)
                                     , NULL, errnum, lang_id
                                     , (LPTSTR)&msg, 0, NULL);

    if (ret > 0 && msg != NULL)
    {
        /* remove trailing punctuation */
        for (ssize_t i = strlen(msg) - 1; i >= 0; i--)
        {
            if (msg[i] != '.' && !isspace(msg[i]))
                break;

            msg[i] = '\0';
        }

        strncpy(buf, msg, buflen);
        LocalFree(msg);
    }
    else
    {
        static const char fail_msg[] = "Unknown error";
        strncpy(buf, fail_msg, buflen);
    }
#else
    msg = strerror(errnum);
    strncpy(buf, msg, buflen);
#endif /* _WIN32 */

    buf[buflen - 1] = '\0';

    /* append error number */
    const size_t slen = strlen(buf);
    snprintf(&buf[slen], buflen - slen, " (%d)", errnum);

    return buf;
}

const char *asc_error_msg(void)
{
#ifdef _WIN32
    const int errnum = GetLastError();
#else
    const int errnum = errno;
#endif /* _WIN32 */
    return asc_strerror(errnum, msg_buf, sizeof(msg_buf));
}
