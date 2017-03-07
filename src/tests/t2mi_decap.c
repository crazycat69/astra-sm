/*
 * T2-MI test program
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
#include <astra/mpegts/t2mi.h>
#include <astra/luaapi/stream.h>

#define MSG(_msg) "[main] " _msg

#define fatal(__fmt, ...) \
    { \
        fprintf(stderr, "error: " __fmt "\n", __VA_ARGS__); \
        exit(1); \
    }

static void on_ts(void *arg, const uint8_t *ts)
{
    if (fwrite(ts, 188, 1, (FILE *)arg) != 1)
        fatal("fwrite: %s", strerror(errno));
}

static void join_pid(module_data_t *arg, uint16_t pid)
{
    ASC_UNUSED(arg);
    asc_log_info(MSG("joining pid %hu"), pid);
}

static void leave_pid(module_data_t *arg, uint16_t pid)
{
    ASC_UNUSED(arg);
    asc_log_info(MSG("leaving pid %hu"), pid);
}

int main(int argc, char *argv[])
{
    asc_log_core_init();
    asc_log_set_debug(true);

    /* parse command line */
    const char *infile = NULL;
    const char *outfile = NULL;
    unsigned plp_id = 0x100;
    unsigned outer_pid = 0;
    unsigned outer_pnr = 0;

    bool show_usage = false;

    int c;
    while ((c = getopt(argc, argv, "i:o:p:P:s:")) != -1)
    {
        switch (c)
        {
            case 'i':
                /* in file */
                infile = optarg;
                break;

            case 'o':
                /* out file */
                outfile = optarg;
                break;

            case 'p':
                /* plp id */
                plp_id = atoi(optarg);
                asc_log_info(MSG("option: PLP ID = %u"), plp_id);
                break;

            case 'P':
                /* force payload pid */
                outer_pid = atoi(optarg);
                asc_log_info(MSG("option: Payload PID = %u"), outer_pid);
                break;

            case 's':
                /* force payload pnr (sid) */
                outer_pnr = atoi(optarg);
                asc_log_info(MSG("option: Payload PNR = %u"), outer_pnr);
                break;

            default:
                show_usage = true;
        }
    }

    if (infile == NULL || outfile == NULL
        || outer_pid & ~0x1FFF || outer_pnr > 0xFFFF
        || plp_id & ~0x1FF)
    {
        show_usage = true;
    }

    if (show_usage)
    {
        fatal(
            "usage: %s OPTIONS -i <infile> -o <outfile>\n"
            "options:\n"
            "\t-p <plp_id>\n"
            "\t-P <payload_pid>\n"
            "\t-s <payload_pnr>"
            , argv[0]
        );
    }

    asc_log_info(MSG("in: %s, out: %s"), infile, outfile);

    /* open files */
    FILE *f_in = fopen(infile, "rb");
    if (!f_in)
        fatal("fopen: %s: %s", infile, strerror(errno));

    FILE *f_out = fopen(outfile, "wb");
    if (!f_out)
        fatal("fopen: %s: %s", outfile, strerror(errno));

    /* feed TS to decapsulator */
    mpegts_t2mi_t *const mi = mpegts_t2mi_init();
    mpegts_t2mi_set_fname(mi, "decap");

    mpegts_t2mi_set_demux(mi, NULL, join_pid, leave_pid);
    mpegts_t2mi_set_payload(mi, outer_pnr, outer_pid);
    mpegts_t2mi_set_plp(mi, plp_id);

    mpegts_t2mi_set_callback(mi, on_ts, f_out);

    uint8_t ts[TS_PACKET_SIZE];
    while (fread(ts, sizeof(ts), 1, f_in) == 1)
        mpegts_t2mi_decap(mi, ts);

    /* clean up */
    asc_log_info(MSG("cleaning up"));
    mpegts_t2mi_destroy(mi);

    fclose(f_in);
    fclose(f_out);

    asc_log_core_destroy();

    return 0;
}
