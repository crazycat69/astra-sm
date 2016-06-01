/*
 * Astra: Unit tests
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

#include "unit_tests.h"
#include <core/spawn.h>
#include <core/event.h>
#include <core/mainloop.h>
#include <core/socket.h>
#include <core/thread.h>
#include <utils/crc32b.h>

#ifdef HAVE_SYS_SELECT_H
#   include <sys/select.h>
#endif

static void pipe_check_nb(int fd, bool expect)
{
#ifdef _WIN32
    /* NOTE: on Win32, there's no way to test if a socket is non-blocking */
    __uarg(fd);
    __uarg(expect);
#else /* _WIN32 */
    const int ret = fcntl(fd, F_GETFL);
    ck_assert(ret != -1);
    ck_assert(expect == ((ret & O_NONBLOCK) != 0));
#endif /* !_WIN32 */
}

static bool pipe_get_inherit(int fd)
{
#ifdef _WIN32
    DWORD flags = 0;
    const bool ret = GetHandleInformation(ASC_TO_HANDLE(fd), &flags);
    ck_assert(ret == true);

    return flags & HANDLE_FLAG_INHERIT;
#else /* _WIN32 */
    const int ret = fcntl(fd, F_GETFD);
    ck_assert(ret != -1);

    return (!(ret & FD_CLOEXEC));
#endif /* !_WIN32 */
}

/* open and close pipe */
static void pipe_open_test(unsigned int nb_side)
{
    int fds[2] = { -1, -1 };
    int nb_fd = -1;

    int ret = asc_pipe_open(fds, &nb_fd, nb_side);
    ck_assert(ret == 0 && fds[PIPE_RD] != -1 && fds[PIPE_WR] != -1);

    ck_assert(pipe_get_inherit(fds[PIPE_RD]) == false);
    ck_assert(pipe_get_inherit(fds[PIPE_WR]) == false);

    /* verify non-blocking flag */
    switch (nb_side)
    {
        case PIPE_RD:
            ck_assert(nb_fd == fds[PIPE_RD]);
            pipe_check_nb(fds[PIPE_RD], true);
            pipe_check_nb(fds[PIPE_WR], false);
            break;

        case PIPE_WR:
            ck_assert(nb_fd == fds[PIPE_WR]);
            pipe_check_nb(fds[PIPE_RD], false);
            pipe_check_nb(fds[PIPE_WR], true);
            break;

        case PIPE_BOTH:
            /* nb_fd is irrelevant here */
            pipe_check_nb(fds[PIPE_RD], true);
            pipe_check_nb(fds[PIPE_WR], true);
            break;

        case PIPE_NONE:
        default:
            ck_assert(nb_fd == -1);
            pipe_check_nb(fds[PIPE_RD], false);
            pipe_check_nb(fds[PIPE_WR], false);
    }

    /* try using pipe with select() */
    fd_set rs, ws, es;

    FD_ZERO(&rs);
    FD_SET((unsigned)fds[PIPE_RD], &rs);
    FD_SET((unsigned)fds[PIPE_WR], &rs);

    FD_ZERO(&ws);
    FD_SET((unsigned)fds[PIPE_RD], &ws);
    FD_SET((unsigned)fds[PIPE_WR], &ws);

    FD_ZERO(&es);
    FD_SET((unsigned)fds[PIPE_RD], &es);
    FD_SET((unsigned)fds[PIPE_WR], &es);

    int nfds;
    if (fds[PIPE_RD] > fds[PIPE_WR])
        nfds = fds[PIPE_RD] + 1;
    else
        nfds = fds[PIPE_WR] + 1;

    struct timeval tv;

    memset(&tv, 0, sizeof(tv));
    ret = select(nfds, &rs, NULL, NULL, &tv);
    ck_assert_msg(ret == 0); /* no read events */

    memset(&tv, 0, sizeof(tv));
    ret = select(nfds, NULL, &ws, NULL, &tv);
    ck_assert_msg(ret == 2); /* expect both ends to be writable */

    memset(&tv, 0, sizeof(tv));
    ret = select(nfds, NULL, NULL, &es, &tv);
    ck_assert_msg(ret == 0); /* no exception events */

    ck_assert(asc_pipe_close(fds[PIPE_RD]) == 0);
    ck_assert(asc_pipe_close(fds[PIPE_WR]) == 0);
}

START_TEST(pipe_open)
{
    static const unsigned int cases[] = {
        PIPE_RD, PIPE_WR, PIPE_BOTH, PIPE_NONE
    };
    for (unsigned int i = 0; i < ASC_ARRAY_SIZE(cases); i++)
        pipe_open_test(cases[i]);
}
END_TEST

/* set child process inheritance on pipe */
START_TEST(pipe_inherit)
{
    int fds[2] = { -1, -1 };

    const int ret = asc_pipe_open(fds, NULL, PIPE_NONE);
    ck_assert(ret == 0 && fds[0] != -1 && fds[1] != -1);

    /* both ends must be non-inheritable by default */
    ck_assert(pipe_get_inherit(fds[0]) == false);
    ck_assert(pipe_get_inherit(fds[1]) == false);

    for (unsigned int i = 0; i < ASC_ARRAY_SIZE(fds); i++)
    {
        /* enable */
        ck_assert(asc_pipe_inherit(fds[i], true) == 0);
        ck_assert(pipe_get_inherit(fds[i]) == true);

        /* disable */
        ck_assert(asc_pipe_inherit(fds[i], false) == 0);
        ck_assert(pipe_get_inherit(fds[i]) == false);
    }
}
END_TEST

/* write to pipe, read from the other end */
#define BUF_SIZE (128 * 1024) /* 128 KiB */

START_TEST(pipe_write)
{
    int fds[2] = { -1, -1 };
    uint8_t data[BUF_SIZE];

    const int ret = asc_pipe_open(fds, NULL, PIPE_NONE);
    ck_assert(ret == 0 && fds[0] != -1 && fds[1] != -1);

    /* both ends must be blocking and non-inheritable */
    ck_assert(pipe_get_inherit(fds[0]) == false);
    ck_assert(pipe_get_inherit(fds[1]) == false);
    pipe_check_nb(fds[0], false);
    pipe_check_nb(fds[1], false);

    for (unsigned int i = 0; i < 1000; i++)
    {
        const int rfd = fds[(i % 2)];
        const int wfd = fds[(i % 2) ^ 1];

        /* generate data packet */
        const size_t data_size = 1 + (rand() % (BUF_SIZE - 1));
        for (size_t j = 0; j < data_size; j++)
            data[j] = rand();

        const uint32_t crc1 = au_crc32b(data, data_size);

        /* send it through pipe */
        const ssize_t out = send(wfd, (char *)data, data_size, 0);
        ck_assert(out == (ssize_t)data_size);

        /* read and compare CRC */
        uint8_t buf[BUF_SIZE];
        const ssize_t in = recv(rfd, (char *)buf, sizeof(buf), 0);
        ck_assert(in == (ssize_t)data_size);

        const uint32_t crc2 = au_crc32b(buf, in);
        ck_assert(crc1 == crc2);
    }

    ck_assert(asc_pipe_close(fds[PIPE_RD]) == 0);
    ck_assert(asc_pipe_close(fds[PIPE_WR]) == 0);
}
END_TEST

/* pipe event notification test */
#define SEND_SIZE (BUF_SIZE / 128)

static uint32_t tx_crc = 0;
static uint32_t rx_crc = 0;
static size_t rx_pos = 0;
static uint8_t rx_buf[BUF_SIZE] = { 0 };

static void thread_proc(void *arg)
{
    const int wfd = *((int *)arg);

    /* generate random data */
    uint8_t tx_buf[BUF_SIZE];
    for (size_t i = 0; i < sizeof(tx_buf); i++)
        tx_buf[i] = rand();

    tx_crc = au_crc32b(tx_buf, sizeof(tx_buf));

    /* send data and close socket */
    size_t tx_pos = 0;

    while (tx_pos < sizeof(tx_buf))
    {
        const ssize_t ret = send(wfd, (char *)&tx_buf[tx_pos], SEND_SIZE, 0);
        ck_assert(ret == SEND_SIZE);

        tx_pos += SEND_SIZE;
    }

    asc_pipe_close(wfd);
}

static void pipe_on_read(void *arg)
{
    const int rfd = *((int *)arg);

    while (true)
    {
        const ssize_t left = sizeof(rx_buf) - rx_pos;
        ck_assert(left >= 0);

        const ssize_t ret = recv(rfd, (char *)&rx_buf[rx_pos], left, 0);
        switch (ret)
        {
            case -1:
                if (asc_socket_would_block())
                    return;

                ck_abort_msg("recv(): %s", asc_error_msg());
                break;

            case 0:
                /* closed on far side */
                ck_assert(rx_pos == sizeof(rx_buf));

                rx_crc = au_crc32b(rx_buf, sizeof(rx_buf));
                ck_assert(rx_crc == tx_crc);

                asc_main_loop_shutdown();
                return;

            default:
                ck_assert(ret > 0);
                rx_pos += ret;
        }
    }
}

START_TEST(pipe_event)
{
    int fds[2] = { -1, -1 };

    const int ret = asc_pipe_open(fds, NULL, PIPE_RD);
    ck_assert(ret == 0 && fds[PIPE_RD] != -1 && fds[PIPE_WR] != -1);

    /* verify our end is non-blocking */
    pipe_check_nb(fds[PIPE_RD], true);
    pipe_check_nb(fds[PIPE_WR], false);

    /* set up event handler */
    asc_event_t *const ev = asc_event_init(fds[PIPE_RD], &fds[PIPE_RD]);
    asc_event_set_on_read(ev, pipe_on_read);

    /* start writing thread */
    asc_thread_t *const thr = asc_thread_init();
    asc_thread_start(thr, &fds[PIPE_WR], thread_proc, NULL);

    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    asc_event_close(ev);
    asc_pipe_close(fds[PIPE_RD]);
}
END_TEST

Suite *core_spawn(void)
{
    Suite *const s = suite_create("spawn");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);

    tcase_add_test(tc, pipe_open);
    tcase_add_test(tc, pipe_inherit);
    tcase_add_test(tc, pipe_write);
    tcase_add_test(tc, pipe_event);

    suite_add_tcase(s, tc);

    return s;
}
