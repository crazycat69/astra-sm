/*
 * Astra Unit Tests
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

#include "../libastra.h"
#include <astra/core/child.h>
#include <astra/core/mainloop.h>
#include <astra/core/timer.h>
#include <astra/utils/crc8.h>

#ifndef _WIN32
#   include <signal.h>
#endif /* !_WIN32 */

#define TEST_SLAVE "./tests/spawn_slave"
#define TEST_SPAMMER "./tests/ts_spammer"

static void fail_on_read(void *arg, const void *buf, size_t len)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(buf);
    ASC_UNUSED(len);

    ck_abort_msg("unexpected read event");
}

static void fail_on_close(void *arg, int status)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(status);

    ck_abort_msg("unexpected close event");
}

/* get child's pid */
static asc_child_t *pid_child = NULL;
static pid_t pid_value = -1;

static void pid_on_read(void *arg, const void *buf, size_t len)
{
    ck_assert(arg == (void *)0x1234);

    char pid_str[32] = { 0 };
    int ret = snprintf(pid_str, sizeof(pid_str), "%lld", (long long)pid_value);
    ck_assert(ret > 0);

    /* line-buffered mode removes newlines at the end of each string */
    ck_assert(strlen((char *)buf) == len);
    ck_assert(strlen(pid_str) == len);
    ck_assert(strcmp(pid_str, (char *)buf) == 0);

    /*
     * NOTE: calling asc_child_close() *OR* asc_child_destroy() directly
     *       from inside an event handler is dangerous: it could free()
     *       the child before the buffering routine in core/child has a
     *       chance to complete its work and return.
     */
    asc_job_queue(NULL, (loop_callback_t)asc_child_close, pid_child);
}

static void pid_on_close(void *arg, int status)
{
#ifdef _WIN32
    ck_assert(status == (int)STATUS_CONTROL_C_EXIT);
#else /* WIN32 */
    ck_assert(status == (128 + SIGTERM));
#endif /* !_WIN32 */

    ck_assert(arg == (void *)0x1234);

    pid_child = NULL;
    asc_main_loop_shutdown();
}

START_TEST(read_pid)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_pid";
    cfg.command = TEST_SLAVE " pid";
    cfg.sout.mode = cfg.serr.mode = CHILD_IO_TEXT;
    cfg.sout.on_flush = pid_on_read;
    cfg.serr.on_flush = fail_on_read; /* not expecting stderr */
    cfg.on_close = pid_on_close;
    cfg.arg = (void *)0x1234;

    pid_child = asc_child_init(&cfg);
    ck_assert(pid_child != NULL);

    pid_value = asc_child_pid(pid_child);
    ck_assert(pid_value > 0);

    ck_assert(asc_main_loop_run() == false);
    ck_assert(pid_child == NULL);
}
END_TEST

/* terminate unresponsive child */
#define BANDIT_TIME (1.5 * 1000000) /* 1.5 sec */

static asc_child_t *bandit_child = NULL;
static uint64_t bandit_time = 0;

static void bandit_on_read(void *arg, const void *buf, size_t len)
{
    ck_assert(len == 4 && strcmp("peep", (char *)buf) == 0);

    if (arg != NULL)
    {
        bandit_time = asc_utime();
        asc_job_queue(NULL, (loop_callback_t)asc_child_close, bandit_child);
    }
    else
    {
        asc_main_loop_shutdown();
    }
}

static void bandit_on_close(void *arg, int status)
{
    ASC_UNUSED(arg);

#ifdef _WIN32
    ck_assert(status == EXIT_FAILURE);
#else /* _WIN32 */
    ck_assert(status == (128 + SIGKILL));
#endif /* !_WIN32 */

    /* forced shutdown should take around 1.5sec */
    const uint64_t bench = asc_utime() - bandit_time;
    ck_assert((BANDIT_TIME * 0.7) <= bench
              && bench <= (BANDIT_TIME * 1.3));

    bandit_child = NULL;
    asc_main_loop_shutdown();
}

START_TEST(bandit_no_block)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_bandit";
    cfg.command = TEST_SLAVE " bandit";
    cfg.sout.mode = cfg.serr.mode = CHILD_IO_TEXT;
    cfg.sout.on_flush = fail_on_read; /* not expecting stdout */
    cfg.serr.on_flush = bandit_on_read;
    cfg.on_close = bandit_on_close;
    cfg.arg = (void *)0x1234;

    /* normal kill via main loop */
    bandit_child = asc_child_init(&cfg);
    ck_assert(bandit_child != NULL);

    ck_assert(asc_main_loop_run() == false);
    ck_assert(bandit_child == NULL);
}
END_TEST

START_TEST(bandit_block)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_bandit";
    cfg.command = TEST_SLAVE " bandit";
    cfg.sout.mode = cfg.serr.mode = CHILD_IO_TEXT;
    cfg.sout.on_flush = fail_on_read; /* not expecting stdout */
    cfg.serr.on_flush = bandit_on_read;
    cfg.on_close = fail_on_close; /* or close event */
    cfg.arg = NULL;

    /* start child and wait until it disables signals */
    bandit_child = asc_child_init(&cfg);
    ck_assert(bandit_child != NULL);

    ck_assert(asc_main_loop_run() == false);

    /* do blocking kill */
    bandit_time = asc_utime();
    asc_child_destroy(bandit_child);
    bandit_child = NULL;

    const uint64_t bench = asc_utime() - bandit_time;
    ck_assert((BANDIT_TIME * 0.7) <= bench
              && bench <= (BANDIT_TIME * 1.3));
}
END_TEST

/* stdio pipes closed on far side */
static void far_on_close(void *arg, int status)
{
    ASC_UNUSED(arg);

    /*
     * NOTE: whenever an stdio pipe is closed on the far side,
     *       core/child should terminate the process if it hadn't
     *       already quit.
     */
#ifdef _WIN32
    ck_assert(status == (int)STATUS_CONTROL_C_EXIT);
#else /* _WIN32 */
    ck_assert(status == (128 + SIGTERM));
#endif /* !_WIN32 */

    asc_main_loop_shutdown();
}

START_TEST(far_close)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_close";
    cfg.command = TEST_SLAVE " close";
    cfg.sout.mode = cfg.serr.mode = CHILD_IO_TEXT;
    cfg.sout.on_flush = cfg.serr.on_flush = fail_on_read;
    cfg.on_close = far_on_close;

    asc_child_t *const child = asc_child_init(&cfg);
    ck_assert(child != NULL);

    ck_assert(asc_main_loop_run() == false);
}
END_TEST

/* try to close child multiple times in a row */
asc_child_t *double_child = NULL;

static void double_on_read(void *arg, const void *buf, size_t len)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(buf);
    ASC_UNUSED(len);

    asc_main_loop_shutdown();
}

static void double_on_close(void *arg, int status)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(status);

#ifdef _WIN32
    ck_assert(status == EXIT_FAILURE);
#else /* _WIN32 */
    ck_assert(status == (128 + SIGKILL));
#endif /* !_WIN32 */

    double_child = NULL;
}

START_TEST(double_kill)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_double";
    cfg.command = TEST_SLAVE " bandit";
    cfg.sout.mode = cfg.serr.mode = CHILD_IO_TEXT;
    cfg.sout.on_flush = fail_on_read;
    cfg.serr.on_flush = double_on_read;
    cfg.on_close = double_on_close;

    double_child = asc_child_init(&cfg);
    ck_assert(double_child != NULL);

    ck_assert(asc_main_loop_run() == false);

    while (double_child != NULL)
    {
        asc_child_close(double_child);
        asc_usleep(10 * 1000); /* 10ms */
    }
}
END_TEST

/* frame aligner test */
#define ALIGNER_PID 0x100
#define ALIGNER_LIMIT 2500
#define ALIGNER_LIMIT_STR "2500"

static asc_child_t *aligner = NULL;
static unsigned int aligner_cc = 15;
static unsigned int aligner_cnt = 0;
static bool aligner_closed = false;

static void aligner_on_read(void *arg, const void *buf, size_t len)
{
    ASC_UNUSED(arg);

    const uint8_t *ts = (uint8_t *)buf;

    ck_assert(len > 0);
    while (len > 0)
    {
        ck_assert(TS_IS_SYNC(ts));
        ck_assert(TS_GET_PID(ts) == ALIGNER_PID);

        aligner_cc = (aligner_cc + 1) & 0xf;
        ck_assert(TS_GET_CC(ts) == aligner_cc);

        if (++aligner_cnt > ALIGNER_LIMIT && !aligner_closed)
        {
            aligner_closed = true;
            asc_job_queue(NULL, (loop_callback_t)asc_child_close, aligner);
        }

        ts += TS_PACKET_SIZE;
        len--;
    }
}

static void aligner_on_close(void *arg, int status)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(status);

    asc_main_loop_shutdown();
    aligner = NULL;
}

START_TEST(ts_aligner)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_aligner";
    cfg.command = TEST_SLAVE " unaligned " ALIGNER_LIMIT_STR;
    cfg.sout.mode = CHILD_IO_MPEGTS;
    cfg.sout.on_flush = aligner_on_read;
    cfg.serr.mode = CHILD_IO_TEXT;
    cfg.serr.on_flush = fail_on_read;
    cfg.on_close = aligner_on_close;

    aligner = asc_child_init(&cfg);
    ck_assert(aligner != NULL);

    ck_assert(asc_main_loop_run() == false);
    ck_assert(aligner_cnt >= ALIGNER_LIMIT);
}
END_TEST

/* TS reassembly */
#define ASSY_LIMIT 1000
#define ASSY_PID 0x200

static asc_child_t *assy_child = NULL;
static unsigned int assy_rcvd = 0;
static unsigned int assy_cc_out = 15;
static unsigned int assy_cc_in = 15;

static void assy_on_ts(void *arg, const void *buf, size_t len)
{
    ASC_UNUSED(arg);

    const uint8_t *ts = (uint8_t *)buf;
    while (len > 0)
    {
        ck_assert(TS_IS_SYNC(ts));
        ck_assert(TS_GET_PID(ts) == ASSY_PID);

        assy_cc_in = (assy_cc_in + 1) & 0xf;
        ck_assert(TS_GET_CC(ts) == assy_cc_in);

        const uint8_t c8 = au_crc8(&ts[5], TS_BODY_SIZE - 1);
        ck_assert(c8 == ts[4]);

        assy_rcvd++;
        ts += TS_PACKET_SIZE;
        len--;
    }
}

static void assy_on_ready(void *arg)
{
    ASC_UNUSED(arg);

    uint8_t ts[TS_PACKET_SIZE] = { 0x47 };
    TS_SET_PID(ts, ASSY_PID);

    assy_cc_out = (assy_cc_out + 1) & 0xf;
    TS_SET_CC(ts, assy_cc_out);

    for (size_t i = 5; i < TS_PACKET_SIZE; i++)
        ts[i] = rand();

    ts[4] = au_crc8(&ts[5], TS_BODY_SIZE - 1);

    for (size_t i = 0; i < sizeof(ts); i++)
    {
        /* send TS packet one byte at a time */
        const ssize_t ret = asc_child_send(assy_child, &ts[i], 1);
        ck_assert(ret == 1);
    }

    if (assy_rcvd >= ASSY_LIMIT)
    {
        asc_child_set_on_ready(assy_child, NULL);
        asc_child_close(assy_child);
    }
}

static void assy_on_close(void *arg, int status)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(status);

    assy_child = NULL;
    asc_main_loop_shutdown();
}

START_TEST(ts_assembly)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_assy";
    cfg.command = TEST_SLAVE " cat 1";
    cfg.sin.mode = CHILD_IO_RAW;
    cfg.sout.mode = CHILD_IO_MPEGTS;
    cfg.sout.on_flush = assy_on_ts;
    cfg.serr.mode = CHILD_IO_TEXT;
    cfg.serr.on_flush = fail_on_read;
    cfg.on_ready = assy_on_ready;
    cfg.on_close = assy_on_close;

    assy_child = asc_child_init(&cfg);
    ck_assert(assy_child != NULL);

    ck_assert(asc_main_loop_run() == false);
    ck_assert(assy_child == NULL);
    ck_assert(assy_rcvd >= ASSY_LIMIT);
}
END_TEST

/* TS write buffering */
#define PUSH_LIMIT 10000
#define PUSH_MAX_BATCH 1000UL
#define PUSH_INTERVAL 25 /* 25ms */
#define PUSH_PID 0x300

static asc_child_t *push_child = NULL;
static asc_timer_t *push_timer = NULL;
static uint8_t *push_batch = NULL;

static unsigned int push_cc_out = 15;
static unsigned int push_cc_in = 15;
static unsigned int push_rcvd = 0;

static void push_on_timer(void *arg)
{
    ASC_UNUSED(arg);

    if (push_rcvd >= PUSH_LIMIT)
    {
        asc_child_close(push_child);
        return;
    }

    const size_t bsize = 1 + (rand() % PUSH_MAX_BATCH);
    for (size_t i = 0; i < bsize; i++)
    {
        uint8_t *const ts = &push_batch[i * TS_PACKET_SIZE];

        ts[0] = 0x47;
        TS_SET_PID(ts, PUSH_PID);

        push_cc_out = (push_cc_out + 1) & 0xf;
        TS_SET_CC(ts, push_cc_out);

        for (size_t j = 5; j < TS_PACKET_SIZE; j++)
            ts[j] = rand();

        ts[4] = au_crc8(&ts[5], TS_BODY_SIZE - 1);
    }

    const ssize_t ret = asc_child_send(push_child, push_batch, bsize);
    ck_assert(ret == (ssize_t)bsize);
}

static void push_on_ts(void *arg, const void *buf, size_t len)
{
    ASC_UNUSED(arg);

    const uint8_t *ts = (uint8_t *)buf;
    while (len > 0)
    {
        ck_assert(TS_IS_SYNC(ts));
        ck_assert(TS_GET_PID(ts) == PUSH_PID);

        push_cc_in = (push_cc_in + 1) & 0xf;
        ck_assert(TS_GET_CC(ts) == push_cc_in);

        const uint8_t c8 = au_crc8(&ts[5], TS_BODY_SIZE - 1);
        ck_assert(c8 == ts[4]);

        push_rcvd++;
        ts += TS_PACKET_SIZE;
        len--;
    }
}

static void push_on_close(void *arg, int status)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(status);

    asc_main_loop_shutdown();
    push_child = NULL;
}

START_TEST(ts_push_pull)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_push";
    cfg.command = TEST_SLAVE " cat 2"; /* echo on stderr */
    cfg.sin.mode = CHILD_IO_MPEGTS;
    cfg.sout.mode = CHILD_IO_TEXT;
    cfg.sout.on_flush = fail_on_read;
    cfg.serr.mode = CHILD_IO_MPEGTS;
    cfg.serr.on_flush = push_on_ts;
    cfg.on_close = push_on_close;

    push_timer = asc_timer_init(PUSH_INTERVAL, push_on_timer, NULL);
    ck_assert(push_timer != NULL);

    push_child = asc_child_init(&cfg);
    ck_assert(push_child != NULL);

    push_batch = ASC_ALLOC(TS_PACKET_SIZE * PUSH_MAX_BATCH, uint8_t);

    ck_assert(asc_main_loop_run() == false);
    ck_assert(push_rcvd >= PUSH_LIMIT);
    ck_assert(push_child == NULL);

    ASC_FREE(push_timer, asc_timer_destroy);
    ASC_FREE(push_batch, free);
}
END_TEST

/* single character echo */
#define RAW_LIMIT 300

static asc_child_t *raw_child = NULL;
static unsigned int raw_cnt = 0;
static char raw_char = '\0';

static void raw_on_ready(void *arg)
{
    ASC_UNUSED(arg);

    raw_char++;
    const ssize_t ret = asc_child_send(raw_child, &raw_char, sizeof(raw_char));
    ck_assert(ret == sizeof(raw_char));

    asc_child_toggle_input(raw_child, STDOUT_FILENO, true);
    asc_child_set_on_ready(raw_child, NULL);
}

static void raw_on_read(void *arg, const void *buf, size_t len)
{
    ASC_UNUSED(arg);

    ck_assert(len == sizeof(raw_char));

    const char c = *((char *)buf);
    ck_assert(c == raw_char);

    asc_child_toggle_input(raw_child, STDOUT_FILENO, false);
    if (++raw_cnt >= RAW_LIMIT)
        asc_job_queue(NULL, (loop_callback_t)asc_child_close, raw_child);
    else
        asc_child_set_on_ready(raw_child, raw_on_ready);
}

static void raw_on_close(void *arg, int status)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(status);

    asc_main_loop_shutdown();
    raw_child = NULL;
}

START_TEST(raw_push_pull)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_raw";
    cfg.command = TEST_SLAVE " cat 1";
    cfg.sin.mode = CHILD_IO_RAW;
    cfg.sout.mode = CHILD_IO_RAW;
    cfg.sout.on_flush = raw_on_read;
    cfg.sout.ignore_read = true;
    cfg.serr.mode = CHILD_IO_TEXT;
    cfg.serr.on_flush = fail_on_read;
    cfg.on_ready = raw_on_ready;
    cfg.on_close = raw_on_close;

    raw_child = asc_child_init(&cfg);
    ck_assert(raw_child != NULL);

    ck_assert(asc_main_loop_run() == false);
    ck_assert(raw_child == NULL);
    ck_assert(raw_cnt >= RAW_LIMIT);
}
END_TEST

/* test discard setting */
static asc_child_t *discard_child = NULL;
static asc_timer_t *discard_timer = NULL;
static bool discard_timer_fired = false;

static void discard_on_timer(void *arg)
{
    ASC_UNUSED(arg);

    discard_timer_fired = true;
    asc_child_close(discard_child);
}

static void discard_on_ready(void *arg)
{
    ASC_UNUSED(arg);

    char buf[] = "Test";
    const ssize_t ret = asc_child_send(discard_child, buf, sizeof(buf));
    ck_assert(ret == sizeof(buf));

    asc_child_set_on_ready(discard_child, NULL);
    discard_timer = asc_timer_one_shot(100, discard_on_timer, NULL);
}

static void discard_on_close(void *arg, int status)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(status);

    asc_main_loop_shutdown();
    discard_child = NULL;
}

START_TEST(discard)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_discard";
    cfg.sout.on_flush = cfg.serr.on_flush = fail_on_read;
    cfg.on_ready = discard_on_ready;
    cfg.on_close = discard_on_close;

    /* discard on receive */
    cfg.command = TEST_SLAVE " cat 1";
    cfg.sin.mode = CHILD_IO_RAW;
    cfg.sout.mode = cfg.serr.mode = CHILD_IO_NONE;

    discard_child = asc_child_init(&cfg);
    ck_assert(discard_child != NULL);

    ck_assert(asc_main_loop_run() == false);

    ck_assert(discard_child == NULL);
    ck_assert(discard_timer != NULL);
    ck_assert(discard_timer_fired == true);

    /* discard on send */
    cfg.command = TEST_SLAVE " cat 2";
    cfg.sin.mode = CHILD_IO_NONE;
    cfg.sout.mode = cfg.serr.mode = CHILD_IO_TEXT;

    discard_timer = NULL;
    discard_timer_fired = false;

    discard_child = asc_child_init(&cfg);
    ck_assert(discard_child != NULL);

    ck_assert(asc_main_loop_run() == false);

    ck_assert(discard_timer != NULL);
    ck_assert(discard_timer_fired == true);
    ck_assert(discard_child == NULL);
}
END_TEST

/* run TS spammer for 1 second */
static size_t spammer_rcvd = 0;
static asc_child_t *spammer_child = NULL;
static asc_timer_t *spammer_timer = NULL;

static void spammer_on_timer(void *arg)
{
    ASC_UNUSED(arg);

    asc_child_close(spammer_child);
    spammer_timer = NULL;
}

static void spammer_on_read(void *arg, const void *ts, size_t len)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(ts);

    spammer_rcvd += len;
}

static void spammer_on_close(void *arg, int status)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(status);

    asc_main_loop_shutdown();
    spammer_child = NULL;
}

START_TEST(ts_spammer)
{
    asc_child_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "ts_spammer";
    cfg.command = TEST_SPAMMER;
    cfg.sout.on_flush = spammer_on_read;
    cfg.sout.mode = CHILD_IO_MPEGTS;
    cfg.serr.mode = CHILD_IO_NONE;
    cfg.on_close = spammer_on_close;

    spammer_child = asc_child_init(&cfg);
    ck_assert(spammer_child != NULL);

    spammer_timer = asc_timer_one_shot(1000, spammer_on_timer, NULL);
    ck_assert(asc_main_loop_run() == false);

    asc_log_info("received %zu packets from spammer", spammer_rcvd);
    ck_assert(spammer_rcvd > 0);
    ck_assert(spammer_timer == NULL);
    ck_assert(spammer_child == NULL);
}
END_TEST

Suite *core_child(void)
{
    Suite *const s = suite_create("core/child");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);

#ifndef _WIN32
    if (can_fork != CK_NOFORK)
        tcase_set_timeout(tc, 30);
#endif /* !_WIN32 */

    tcase_add_test(tc, read_pid);
    tcase_add_test(tc, bandit_no_block);
    tcase_add_test(tc, bandit_block);
    tcase_add_test(tc, far_close);
    tcase_add_test(tc, double_kill);
    tcase_add_test(tc, ts_aligner);
    tcase_add_test(tc, ts_assembly);
    tcase_add_test(tc, ts_push_pull);
    tcase_add_test(tc, raw_push_pull);
    tcase_add_test(tc, discard);
    tcase_add_test(tc, ts_spammer);

    suite_add_tcase(s, tc);

    return s;
}
