/*
 * I/Q table converter for IT95x-based modulators
 *
 * Copyright (C) 2017, Artem Kharitonov <artem@3phase.pw>
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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#   include <fcntl.h>
#   include <io.h>
#endif

#define HDR_SIZE 16U
#define ENTRY_SIZE 8U
#define BUF_SIZE (UINT16_MAX * ENTRY_SIZE)

int main(int argc, const char *argv[])
{
    const char *fn = NULL;
    FILE *fp = NULL;
    uint8_t hdr[HDR_SIZE] = { 0 };
    uint8_t buf[BUF_SIZE] = { 0 };
    size_t i = 0, ret = 0;
    size_t entries = 0, bytes = 0;
    unsigned long version = 0;

    /* open source file */
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <infile>\n", argv[0]);
        return EXIT_FAILURE;
    }

    fn = argv[1];
    if (!strcmp(fn, "-"))
    {
#ifdef _WIN32
        _setmode(fileno(stdin), _O_BINARY);
#endif
        fp = fdopen(fileno(stdin), "rb");
    }
    else
    {
        fp = fopen(fn, "rb");
    }

    if (fp == NULL)
    {
        fprintf(stderr, "%s: %s\n", fn, strerror(errno));
        return EXIT_FAILURE;
    }

    /* read header and data */
    ret = fread(hdr, 1, HDR_SIZE, fp);
    if (ret != HDR_SIZE)
    {
        fclose(fp);
        fprintf(stderr, "short header read: expected %u bytes, got %lu!\n"
                , HDR_SIZE, (long)ret);

        return EXIT_FAILURE;
    }

    /*
     * NOTE: This should be 32-bit LE according to SDK documentation,
     *       but the ITE test kit treats it as a 24-bit big-endian uint.
     */
    version = ((unsigned long)hdr[10] << 16)
              | ((unsigned long)hdr[11] << 8)
              | hdr[12];

    /* calibration entry count: 16-bit unsigned int, big-endian */
    entries = ((unsigned)hdr[14] << 8) | hdr[15];
    bytes = entries * ENTRY_SIZE;

    ret = fread(buf, 1, bytes, fp);
    if (ret != bytes)
    {
        fclose(fp);
        fprintf(stderr, "short data read: expected %lu bytes, got %lu!\n"
                , (long)bytes, (long)ret);

        return EXIT_FAILURE;
    }

    /* print out Lua table */
    printf("--\n");
    printf("-- table version: %lx\n", version);
    printf("-- table size: %lu entries\n", (long)entries);
    printf("--\n");
    printf("-- { <frequency>, <amp>, <phi> }\n");
    printf("--\n");
    printf("iq_table = {\n");

    for (i = 0; i < entries; i++)
    {
        const uint8_t *const p = &buf[i * ENTRY_SIZE];

        /* everything is little-endian in here */
        const unsigned long frequency = ((unsigned long)p[3] << 24)
                                        | ((unsigned long)p[2] << 16)
                                        | ((unsigned long)p[1] << 8)
                                        | p[0];

        const int amp = (int16_t)(p[5] << 8) | p[4];
        const int phi = (int16_t)(p[7] << 8) | p[6];

        printf("    { %lu, %d, %d },\n", frequency, amp, phi);
    }

    printf("}\n");
    fclose(fp);

    return EXIT_SUCCESS;
}
