/*
 * Sine wave generator
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

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

/* M_PI may not be present in math.h */
#ifndef M_PI
#   define M_PI 3.14159265358979323846264338327
#endif /* M_PI */
#define M_2PI (M_PI * 2)

#define WAVE_FORMAT_PCM 0x0001

#define optfail(__fmt, ...) \
    { \
        fprintf(stderr, "ERROR: " __fmt "\n\n", __VA_ARGS__); \
        usage(true, argv[0]); \
    }

#define fatal(__fmt, ...) \
    { \
        fprintf(stderr, __fmt "\n", __VA_ARGS__); \
        exit(EXIT_FAILURE); \
    }

/* WAV file header */
typedef struct
{
    uint8_t riff[4];
    uint8_t filesize[4];
    uint8_t wave[4];
    uint8_t fmt[4];
    uint8_t hdrsize[4];
    uint8_t format[2];
    uint8_t channels[2];
    uint8_t sample_rate[4];
    uint8_t bps[4];
    uint8_t block_align[2];
    uint8_t sample_bits[2];
    uint8_t data[4];
    uint8_t datasize[4];
} wav_header_t;

static
void usage(bool error, const char *argv0)
{
    fprintf(error ? stderr : stdout,
        "Usage: %s [options]\n"
        "Options:\n"
        "    -s  sample rate (8000..192000, default 48000 Hz)\n"
        "    -f  tone frequency (20..20000, default 600 Hz)\n"
        "    -a  volume (1..100, default 60%%)\n"
        "    -t  duration (at least 1, default 10 secs)\n"
        "    -c  channels (1..8, default 2)\n"
        "    -o  output file (out.wav)\n"
        "    -h  show this message\n",
        argv0
    );
    exit(error ? EXIT_FAILURE : EXIT_SUCCESS);
}

static inline
void put_le16(void *ptr, uint16_t val)
{
    uint8_t *const byte = (uint8_t *)ptr;

    byte[0] = (uint8_t)val;
    byte[1] = (uint8_t)(val >> 8);
}

static inline
void put_le32(void *ptr, uint32_t val)
{
    uint8_t *const byte = (uint8_t *)ptr;

    byte[0] = (uint8_t)val;
    byte[1] = (uint8_t)(val >> 8);
    byte[2] = (uint8_t)(val >> 16);
    byte[3] = (uint8_t)(val >> 24);
}

int main(int argc, char *argv[])
{
    unsigned int sample_rate = 48000;
    unsigned int frequency = 600;
    unsigned int volume = 60;
    unsigned int duration = 10;
    unsigned int channels = 2;
    const char *outfile = "out.wav";

    /* parse command line */
    int idx = 1;
    while (idx < argc)
    {
        const char *opt = argv[idx++];
        if (strcmp(opt, "-h") == 0)
        {
            usage(false, argv[0]);
        }
        if (idx < argc && opt[0] == '-')
        {
            const char *arg = argv[idx++];
            switch (opt[1])
            {
                case 's':
                    sample_rate = atoi(arg);
                    if (sample_rate < 8000 || sample_rate > 192000)
                        optfail("invalid sample rate: %s", arg);

                    break;

                case 'f':
                    frequency = atoi(arg);
                    if (frequency < 20 || frequency > 20000)
                        optfail("invalid tone frequency: %s", arg);

                    break;

                case 'a':
                    volume = atoi(arg);
                    if (volume < 1 || volume > 100)
                        optfail("invalid volume: %s", arg);

                    break;

                case 't':
                    duration = atoi(arg);
                    if (duration < 1)
                        optfail("invalid duration: %s", arg);

                    break;

                case 'c':
                    channels = atoi(arg);
                    if (channels < 1 || channels > 8)
                        optfail("invalid channel number: %s", arg);

                    break;

                case 'o':
                    outfile = arg;
                    break;

                default:
                    optfail("unknown option: %s %s", opt, arg);
            }
        }
        else
            optfail("unknown option: %s", opt);
    }

    /* open output file */
    FILE *out = fopen(outfile, "wb");
    if (out == NULL)
        fatal("fopen: %s: %s", outfile, strerror(errno));

    /* write wav header */
    const size_t num_samples = sample_rate / frequency;
    const size_t total_samples = num_samples * frequency * duration;
    const size_t block_align = channels * sizeof(int16_t);
    const size_t datasize = total_samples * block_align;

    wav_header_t hdr;

    memcpy(hdr.riff, "RIFF", 4);
    memcpy(hdr.wave, "WAVE", 4);
    memcpy(hdr.fmt,  "fmt ", 4);
    memcpy(hdr.data, "data", 4);

    put_le32(hdr.filesize, datasize + sizeof(hdr) - 8);
    put_le32(hdr.hdrsize, 16); /* fmt chunk size */
    put_le16(hdr.format, WAVE_FORMAT_PCM);
    put_le16(hdr.channels, channels);
    put_le32(hdr.sample_rate, sample_rate);
    put_le32(hdr.bps, sample_rate * block_align);
    put_le16(hdr.block_align, block_align);
    put_le16(hdr.sample_bits, 16);
    put_le32(hdr.datasize, datasize);

    size_t written = fwrite(&hdr, sizeof(hdr), 1, out);
    assert(written == 1);

    /* write samples */
    for (size_t i = 0; i < total_samples; i++)
    {
        double val = sin((M_2PI / num_samples) * i);
        val = ((val * INT16_MAX) / 100) * volume;

        uint8_t buf[2];
        put_le16(buf, val);

        for (size_t ch = 0; ch < channels; ch++)
        {
            written = fwrite(buf, sizeof(buf), 1, out);
            assert(written == 1);
        }
    }

    long pos = ftell(out);
    assert(pos == (long)(sizeof(hdr) + datasize));

    fclose(out);
    return 0;
}
