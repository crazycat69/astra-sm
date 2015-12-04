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

#include <astra.h>
#include <core/log.h>

#ifndef _WIN32
#   include <syslog.h>
#endif /* !_WIN32 */

#define MSG(_msg) "[core/log] " _msg

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
} asc_logger_t;

static asc_logger_t *logger = NULL;

typedef enum
{
    ASC_LOG_ERROR = 0,
    ASC_LOG_WARNING,
    ASC_LOG_INFO,
    ASC_LOG_DEBUG,
} asc_log_type_t;

#ifndef _WIN32
static const int type_syslog[] = {
    LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG
};

static const char *type_color_reset = "\x1b[0m";

static const char *type_colors[] = {
    /* error = red */
    "\x1b[31m",

    /* warning = yellow */
    "\x1b[33m",

    /* default color for other types */
    NULL,
    NULL,
};
#else /* !_WIN32 */
static const WORD type_colors[] = {
    /* error = red */
    FOREGROUND_INTENSITY | FOREGROUND_RED,

    /* warning = yellow */
    FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,

    /* default color for other types */
    0,
    0,
};

static struct tm *localtime_r(const time_t *timep, struct tm *result)
{
    /* NOTE: localtime() is thread-safe on Windows */
    struct tm *ptr = localtime(timep);
    if (ptr != NULL)
        memcpy(result, ptr, sizeof(struct tm));

    return ptr;
}
#endif /* _WIN32 */

static const char *type_strings[] = {
    "ERROR", "WARNING", "INFO", "DEBUG"
};

static __fmt_printf(2, 0)
void log_write(asc_log_type_t type, const char *msg, va_list ap)
{
    char buf[512];
    ssize_t space = sizeof(buf);
    int ret;

    /* add timestamp and severity */
    size_t len_prefix = 0;
    const time_t ct = time(NULL);
    struct tm sct;

    tzset();
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

    if (logger == NULL)
    {
        fprintf(stderr, "%s\n", &buf[len_prefix]);
        return;
    }

    /* send it out through configured channels */
#ifndef _WIN32
    if (logger->syslog != NULL)
        syslog(type_syslog[type], "%s", &buf[len_prefix]);
#endif /* !_WIN32 */

    if (logger->sout)
    {
#ifndef _WIN32
        const char *color_on = "";
        const char *color_off = "";

        if (logger->color && type_colors[type] != NULL
            && isatty(STDOUT_FILENO))
        {
            color_on = type_colors[type];
            color_off = type_color_reset;
        }

        printf("%s%s%s\n", color_on, buf, color_off);
#else /* !_WIN32 */
        bool reset_color = false;

        if (logger->color && type_colors[type] != 0 && logger->con != NULL)
        {
            if (SetConsoleTextAttribute(logger->con, type_colors[type]))
                reset_color = true;
        }

        printf("%s\n", buf);

        if (reset_color)
            SetConsoleTextAttribute(logger->con, logger->attr);
#endif /* _WIN32 */
    }

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
}

void asc_log_info(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    log_write(ASC_LOG_INFO, msg, ap);
    va_end(ap);
}

void asc_log_error(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    log_write(ASC_LOG_ERROR, msg, ap);
    va_end(ap);
}

void asc_log_warning(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    log_write(ASC_LOG_WARNING, msg, ap);
    va_end(ap);
}

void asc_log_debug(const char *msg, ...)
{
    if (logger != NULL && !logger->debug)
        return;

    va_list ap;
    va_start(ap, msg);
    log_write(ASC_LOG_DEBUG, msg, ap);
    va_end(ap);
}

bool asc_log_is_debug(void)
{
    return logger->debug;
}

void asc_log_core_init(void)
{
    logger = (asc_logger_t *)calloc(1, sizeof(*logger));
    asc_assert(logger != NULL, MSG("calloc() failed"));

    logger->sout = true;
    logger->fd = -1;

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
    if (logger->fd != -1)
        close(logger->fd);

#ifndef _WIN32
    if (logger->syslog)
    {
        closelog();
        ASC_FREE(logger->syslog, free);
    }
#endif /* !_WIN32 */

    ASC_FREE(logger->filename, free);
    ASC_FREE(logger, free);
}

void asc_log_reopen(void)
{
    if (logger->fd != -1)
    {
        close(logger->fd);
        logger->fd = -1;
    }

    if (logger->filename == NULL)
        return;

    const int flags = O_WRONLY | O_CREAT | O_APPEND;
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    logger->fd = open(logger->filename, flags, mode);
    if (logger->fd == -1)
    {
        fprintf(stderr, MSG("failed to open %s: %s\n"), logger->filename
                , strerror(errno));
    }
}

void asc_log_set_stdout(bool val)
{
    logger->sout = val;
}

void asc_log_set_debug(bool val)
{
    logger->debug = val;
}

void asc_log_set_color(bool val)
{
    logger->color = val;
}

void asc_log_set_file(const char *val)
{
    ASC_FREE(logger->filename, free);

    if (val != NULL && strlen(val))
        logger->filename = strdup(val);

    asc_log_reopen();
}

#ifndef _WIN32
void asc_log_set_syslog(const char *val)
{
    if (logger->syslog != NULL)
    {
        closelog();
        ASC_FREE(logger->syslog, free);
    }

    if (val == NULL)
        return;

    logger->syslog = strdup(val);
    openlog(logger->syslog, LOG_PID | LOG_CONS | LOG_NOWAIT | LOG_NDELAY
            , LOG_USER);
}
#endif /* !_WIN32 */
