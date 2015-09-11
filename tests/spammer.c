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
#define WRITE_SIZE ((256 * 1024) / TS_PACKET_SIZE)

int main(void)
{
#ifdef _WIN32
    setvbuf(stderr, NULL, _IONBF, 0);
#endif /* _WIN32 */

    uint8_t buffer[WRITE_SIZE][TS_PACKET_SIZE];
    for (size_t i = 0; i < ASC_ARRAY_SIZE(buffer); i++)
        memcpy(buffer[i], null_ts, TS_PACKET_SIZE);

    size_t written = 0;
    time_t last = time(NULL);
    while (true)
    {
        const time_t now = time(NULL);
        if (now != last)
        {
            fprintf(stderr, "written: %.2f MiB\n", written / 1048576.0);

            written = 0;
            last = now;
        }

        const ssize_t ret = write(STDOUT_FILENO, buffer, sizeof(buffer));
        if (ret != sizeof(buffer))
            return 1;

        written += ret;
    }

    return 0;
}
