/*
 * Astra Core (Logging)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *               2015-2016, Artem Kharitonov <artem@3phase.pw>
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
#include <astra/core/log.h>
#include <astra/core/mutex.h>

#ifndef _WIN32
#   include <syslog.h>
#endif /* !_WIN32 */

#define MSG(_msg) "[log] " _msg

typedef struct
{
    bool color;
    bool debug;

    bool sout;
    int fd;
    char *filename;
#ifndef _WIN32
    char *syslog;
#else
    HANDLE con;
    WORD attr;
#endif

    asc_mutex_t lock;
} asc_logger_t;

static
asc_logger_t *logger = NULL;

/*
 * helper functions and maps
 */

/* log level string map */
static
const char *type_strings[] = {
    "ERROR", "WARNING", "INFO", "DEBUG"
};

#ifndef _WIN32

/* syslog severity map */
static
const int type_syslog[] = {
    LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG
};

/* ANSI color codes for each log level */
static
const char *type_colors[] = {
    /* error = red */
    "\x1b[31m",

    /* warning = yellow */
    "\x1b[33m",

    /* info = default */
    NULL,

    /* debug = default */
    NULL,
};

/* ANSI color reset code */
static
const char *type_color_reset = "\x1b[0m";

/* write a log message to standard output */
static
void sout_write(asc_log_type_t type, const char *str)
{
    const char *color_on = "";
    const char *color_off = "";

    if (logger->color && type_colors[type] != NULL
        && isatty(STDOUT_FILENO))
    {
        color_on = type_colors[type];
        color_off = type_color_reset;
    }

    printf("%s%s%s\n", color_on, str, color_off);
}

#else /* !_WIN32 */

/* console attributes for each log level */
static
const WORD type_colors[] = {
    /* error = red */
    FOREGROUND_INTENSITY | FOREGROUND_RED,

    /* warning = yellow */
    FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,

    /* info = default */
    0,

    /* debug = default */
    0,
};

/* write a log message to standard output */
static
void sout_write(asc_log_type_t type, const char *str)
{
    if (logger->con != NULL)
    {
        /* stdout is a console: write Unicode directly without using CRT */
        BOOL on = FALSE;
        if (logger->color && type_colors[type] != 0)
            on = SetConsoleTextAttribute(logger->con, type_colors[type]);

        wchar_t *const wbuf = cx_widen(str);
        if (wbuf != NULL)
        {
            size_t wlen = wcslen(wbuf);
            wbuf[wlen++] = L'\n'; /* replace null with newline */

            DWORD written = 0;
            WriteConsoleW(logger->con, wbuf, wlen, &written, NULL);
            free(wbuf);
        }

        if (on)
            SetConsoleTextAttribute(logger->con, logger->attr);
    }
    else
    {
        /* stdout is not a console: print unaltered UTF-8 string */
        printf("%s\n", str);
    }
}

/* localtime_r() replacement function */
static
struct tm *localtime_r(const time_t *timep, struct tm *result)
{
    /* NOTE: localtime() is thread-safe on Windows */
    struct tm *ptr = localtime(timep);
    if (ptr != NULL)
        memcpy(result, ptr, sizeof(struct tm));

    return ptr;
}

#endif /* _WIN32 */

/*
 * init and deinit
 */

void asc_log_core_init(void)
{
    logger = ASC_ALLOC(1, asc_logger_t);

    logger->sout = true;
    logger->fd = -1;

    asc_mutex_init(&logger->lock);

#ifdef _WIN32
    /* get default text color */
    const HANDLE con = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (con != NULL && GetConsoleScreenBufferInfo(con, &csbi))
    {
        logger->con = con;
        logger->attr = csbi.wAttributes;
    }
#endif /* _WIN32 */
}

void asc_log_core_destroy(void)
{
    if (logger == NULL)
        return;

    if (logger->fd != -1)
        close(logger->fd);

#ifndef _WIN32
    if (logger->syslog != NULL)
    {
        closelog();
        ASC_FREE(logger->syslog, free);
    }
#endif /* !_WIN32 */

    asc_mutex_destroy(&logger->lock);

    ASC_FREE(logger->filename, free);
    ASC_FREE(logger, free);
}

/*
 * setters and getters
 */

void asc_log_set_color(bool val)
{
    logger->color = val;
}

void asc_log_set_debug(bool val)
{
    logger->debug = val;
}

void asc_log_set_stdout(bool val)
{
    logger->sout = val;
}

void asc_log_set_file(const char *val)
{
    asc_mutex_lock(&logger->lock);

    ASC_FREE(logger->filename, free);
    if (val != NULL && strlen(val) > 0)
        logger->filename = strdup(val);

    asc_mutex_unlock(&logger->lock);
    asc_log_reopen();
}

#ifndef _WIN32
void asc_log_set_syslog(const char *val)
{
    asc_mutex_lock(&logger->lock);

    if (logger->syslog != NULL)
    {
        closelog();
        ASC_FREE(logger->syslog, free);
    }

    if (val != NULL && strlen(val) > 0)
    {
        const int option = LOG_PID | LOG_CONS | LOG_NOWAIT | LOG_NDELAY;
        const int facility = LOG_USER;

        logger->syslog = strdup(val);
        openlog(logger->syslog, option, facility);
    }

    asc_mutex_unlock(&logger->lock);
}
#endif /* !_WIN32 */

void asc_log_reopen(void)
{
    asc_mutex_lock(&logger->lock);

    if (logger->fd != -1)
    {
        close(logger->fd);
        logger->fd = -1;
    }

    if (logger->filename != NULL)
    {
        const int flags = O_WRONLY | O_CREAT | O_APPEND;
        const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

        logger->fd = open(logger->filename, flags, mode);
        if (logger->fd == -1)
        {
            fprintf(stderr, MSG("failed to open %s: %s\n")
                    , logger->filename, strerror(errno));
        }
    }

    asc_mutex_unlock(&logger->lock);
}

bool asc_log_is_debug(void)
{
    return logger->debug;
}

/*
 * public interface
 */

void asc_log_va(asc_log_type_t type, const char *msg, va_list ap)
{
    if (type == ASC_LOG_DEBUG)
    {
        if (logger != NULL && !logger->debug)
            return;
    }

    char buf[2048];
    ssize_t space = sizeof(buf);
    size_t len_prefix = 0;
    int ret;

    /* add timestamp and severity */
    tzset();
    const time_t ct = time(NULL);
    struct tm sct;

    if (localtime_r(&ct, &sct) != NULL)
    {
        len_prefix += strftime(buf, space, "%b %d %X: ", &sct);
        space -= len_prefix;
    }

    ret = snprintf(&buf[len_prefix], space, "%s: ", type_strings[type]);
    if (ret > 0 && ret < space)
    {
        len_prefix += ret;
        space -= ret;
    }

    /* add message */
    size_t len = 0;
    ret = vsnprintf(&buf[len_prefix], space, msg, ap);
    if (ret > 0 && ret < space)
        len = len_prefix + ret; /* success */
    else if (ret >= space)
        len = sizeof(buf) - 1; /* string truncated */
    else
        return; /* error or empty string */

    /* send it out through configured channels */
    if (logger == NULL)
    {
        fprintf(stderr, "%s\n", &buf[len_prefix]);
        return;
    }

    asc_mutex_lock(&logger->lock);

#ifndef _WIN32
    if (logger->syslog != NULL)
        syslog(type_syslog[type], "%s", &buf[len_prefix]);
#endif /* !_WIN32 */

    if (logger->sout)
        sout_write(type, buf);

    if (logger->fd != -1)
    {
        /* replace null with newline before writing to file */
        buf[len++] = '\n';

        if (write(logger->fd, buf, len) == -1)
        {
            fprintf(stderr, MSG("failed to write to log file: %s\n")
                    , strerror(errno));
        }
    }

    asc_mutex_unlock(&logger->lock);
}

void asc_log(asc_log_type_t type, const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    asc_log_va(type, msg, ap);
    va_end(ap);
}

void asc_log_error(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    asc_log_va(ASC_LOG_ERROR, msg, ap);
    va_end(ap);
}

void asc_log_warning(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    asc_log_va(ASC_LOG_WARNING, msg, ap);
    va_end(ap);
}

void asc_log_info(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    asc_log_va(ASC_LOG_INFO, msg, ap);
    va_end(ap);
}

void asc_log_debug(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    asc_log_va(ASC_LOG_DEBUG, msg, ap);
    va_end(ap);
}
