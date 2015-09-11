/*
 * Null TS generator
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

#include <astra.h>

/* 256KiB writes */
#define WRITE_PKTS ((256 * 1024) / TS_PACKET_SIZE)

static const char *unit_list[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", NULL };

int main(void)
{
#ifdef _WIN32
    const HANDLE sout = ASC_TO_HANDLE(_get_osfhandle(STDOUT_FILENO));
    setvbuf(stderr, NULL, _IONBF, 0);
#endif /* _WIN32 */

    /* prepare null packets */
    uint8_t buffer[WRITE_PKTS][TS_PACKET_SIZE];
    for (size_t i = 0; i < ASC_ARRAY_SIZE(buffer); i++)
        memcpy(buffer[i], null_ts, TS_PACKET_SIZE);

    /* spam them until we get an error */
    size_t written = 0;
    time_t last = time(NULL);

    while (true)
    {
        const time_t now = time(NULL);
        if (now != last)
        {
            double stat = written;
            const char *units = NULL;

            for (const char **p = unit_list; *p; p++)
            {
                units = *p;
                if (stat < 1024.0)
                    break;

                stat /= 1024.0;
            }
            fprintf(stderr, "written: %.2f %s\n", stat, units);

            written = 0;
            last = now;
        }

#ifdef _WIN32
        DWORD ret = 0;
        if (!WriteFile(sout, buffer, sizeof(buffer), &ret, NULL))
            return 1;
#else /* _WIN32 */
        const ssize_t ret = write(STDOUT_FILENO, buffer, sizeof(buffer));
#endif /* !_WIN32 */

        if (ret != sizeof(buffer))
            return 1;

        written += ret;
    }

    return 0;
}
