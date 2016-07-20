/*
 * Built-in script converter
 * https://cesbo.com/astra
 *
 * Copyright (C) 2014-2015, Andrey Dyldin <and@cesbo.com>
 *                    2016, Artem Kharitonov <artem@3phase.pw>
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

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE 1
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#ifndef O_BINARY
#   ifdef _O_BINARY
#       define O_BINARY _O_BINARY
#   else
#       define O_BINARY 0
#   endif
#endif

static char *buffer = NULL;
static size_t buffer_skip = 0;
static size_t buffer_size = 0;

static inline
void addchar(char c)
{
    if (buffer_skip >= buffer_size)
    {
        /* shouldn't happen */
        fprintf(stderr, "buffer overrun\n");
        exit(EXIT_FAILURE);
    }

    buffer[buffer_skip++] = c;
}

static __attribute__((__pure__))
const char *skip_sp(const char *source)
{
    if (source == NULL)
        return NULL;

    while (*source != '\0')
    {
        switch (*source)
        {
            case '\0':
                return NULL;

            case '\t':
            case ' ':
            case '\r':
                ++source;
                break;

            default:
                return source;
        }
    }

    return NULL;
}

static
bool check_string_tail(const char *source, size_t len)
{
    if (source[0] != ']' || source[len + 1] != ']')
        return false;

    for (size_t i = 0; i < len; ++i)
    {
        if (source[i + 1] != '=')
            return false;
    }

    return true;
}

static
const char *skip_comment(const char *source)
{
    if (*source == '[')
    {
        ++source;
        size_t len = 0;
        for (; *source == '='; ++source, ++len)
            ;

        if (*source == '[')
        {
            ++source;
            for (; *source != '\0'; ++source)
            {
                if (*source == ']' && check_string_tail(source, len))
                    return source + len + 2;

                if (*source == '\n')
                    addchar('\n');
            }
        }
    }
    else
    {
        for (; *source != '\0' && *source != '\n'; ++source)
            ;

        if (*source == '\n')
            return source;
    }

    fprintf(stderr, "wrong comment format\n");
    exit(EXIT_FAILURE);

    return NULL;
}

static
const char *parse_string(const char *source)
{
    if (*source == '[')
    {
        addchar('[');
        ++source;
        size_t len = 0;
        for (; *source == '='; ++source, ++len)
            addchar('=');

        if (*source == '[')
        {
            addchar('[');
            ++source;

            for (; *source != '\0'; ++source)
            {
                if (*source == ']' && check_string_tail(source, len))
                {
                    addchar(']');
                    for (size_t i = 0; i < len; ++i)
                        addchar('=');
                    addchar(']');
                    return source + len + 2;
                }

                addchar(*source);
            }
        }
    }
    else
    {
        const char c = *source;
        addchar(c);
        ++source;

        for (; *source != '\0'; ++source)
        {
            addchar(*source);

            if (*source == c)
                return source + 1;

            if (source[0] == '\\' && source[1] == c)
            {
                addchar(c);
                ++source;
            }
        }
    }

    fprintf(stderr, "wrong string format\n");
    exit(EXIT_FAILURE);

    return NULL;
}

static
void parse(const char *source)
{
    bool is_new_line = true;

    for (; source != NULL && *source != '\0'; ++source)
    {
        if (is_new_line)
        {
            is_new_line = false;
            source = skip_sp(source);
            if (source == NULL)
                break;
        }

        if (source[0] == '-' && source[1] == '-')
            source = skip_comment(&source[2]);

        if (source[0] == '\'' || source[0] == '"')
            source = parse_string(source);

        if (source[0] == '[' && (source[1] == '=' || source[1] == '['))
            source = parse_string(source);

        if (source[0] == '\r')
            continue;

        addchar(source[0]);

        if (source[0] == '\n')
            is_new_line = true;
    }
}

static
void print_block(const char *block, size_t len)
{
    printf("   ");
    for (size_t i = 0; i < len; ++i)
        printf(" 0x%02X,", block[i]);
    printf("\n");
}

#define BYTES_PER_ROW 12
#define FILE_EXT ".lua"

#define fatal(...) \
    do { \
        fprintf(stderr, __VA_ARGS__); \
        if (script != NULL) \
            free(script); \
        if (buffer != NULL) \
            free(buffer); \
        if (fd != -1) \
            close(fd); \
        exit(EXIT_FAILURE); \
    } while (0)

int main(int argc, const char *argv[])
{
    char *script = NULL;
    int fd = -1;

    if (argc < 3)
        fatal("usage: %s <dir> <file...>\n", argv[0]);

    if (chdir(argv[1]) != 0)
        fatal("chdir(): %s: %s\n", argv[1], strerror(errno));

    bool first = true;

    for (int i = 2; i < argc; i++)
    {
        const char *const path = argv[i];

        /* check extension */
        const size_t plen = strlen(path);
        const size_t slen = strlen(FILE_EXT);
        if (plen < (slen + 1) || strcmp(&path[plen - slen], FILE_EXT) != 0)
            fatal("wrong file extension (expected %s): %s\n", FILE_EXT, path);

        /* read source file */
        fd = open(path, O_RDONLY | O_BINARY);
        if (fd == -1)
            fatal("open(): %s: %s\n", path, strerror(errno));

        ssize_t ret = lseek(fd, 0, SEEK_END);
        if (ret == -1 || lseek(fd, 0, SEEK_SET) == -1)
            fatal("lseek(): %s: %s\n", path, strerror(errno));

        if (ret <= 0)
            fatal("%s: file is empty\n", path);

        const size_t filesize = (size_t)ret;
        script = (char *)calloc(1, filesize + 1);
        if (script == NULL)
            fatal("calloc() failed\n");

        ret = read(fd, script, filesize);
        if (ret == -1)
            fatal("read(): %s: %s\n", path, strerror(errno));

        if (filesize != (size_t)ret)
            fatal("read(): %s: short read\n", path);

        close(fd);
        fd = -1;

        /* remove whitespace and comments */
        buffer_size = filesize;
        buffer = (char *)calloc(1, buffer_size);
        if (buffer == NULL)
            fatal("calloc() failed\n");

        for (size_t j = 0; j < 2; j++)
        {
            buffer_skip = 0;
            parse(script);
            memcpy(script, buffer, buffer_skip);
            script[buffer_skip] = '\0';
        }

        free(buffer);
        buffer = NULL;

        /* dump output to stdout */
        if (first)
        {
            printf("/* automatically generated file; do not edit */\n");
            first = false;
        }

        char *const name = strdup(path);
        name[plen - slen] = '\0';
        printf("\nstatic const uint8_t %s[] = {\n", name);
        free(name);

        const size_t tail = buffer_skip % BYTES_PER_ROW;
        const size_t limit = buffer_skip - tail;
        for (size_t j = 0; j < limit; j += BYTES_PER_ROW)
            print_block(&script[j], BYTES_PER_ROW);
        if (limit < buffer_skip)
            print_block(&script[limit], tail);

        printf("};\n");

        free(script);
        script = NULL;
    }

    return 0;
}
