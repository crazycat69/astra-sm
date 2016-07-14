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

#include "../test_libastra.h"
#include <luaapi/stream.h>

/*
 * Test graph:
 *
 * source_a  *OR*                           /---> sink_a
 *             \                           /
 *              -> selector --> foobar -->|
 *             /                           \
 * source_b  *OR*                           \---> sink_b
 */

struct test_stream
{
    STREAM_MODULE_DATA();
    stream_callback_t on_ts;
};

#define TEST_MODULE(_name) \
    static struct test_stream st_##_name; \
    static module_data_t *mod_##_name = (module_data_t *)&st_##_name

TEST_MODULE(source_a);
TEST_MODULE(source_b);
TEST_MODULE(selector);
TEST_MODULE(foobar);
TEST_MODULE(sink_a);
TEST_MODULE(sink_b);

static void ts_thunk(module_data_t *mod, const uint8_t *ts)
{
    struct test_stream *const st = (struct test_stream *)mod;
    if (st->on_ts != NULL)
        st->on_ts(mod, ts);
}

static void stream_teardown(void)
{
    module_stream_destroy(mod_source_a);
    module_stream_destroy(mod_source_b);
    module_stream_destroy(mod_selector);
    module_stream_destroy(mod_foobar);
    module_stream_destroy(mod_sink_a);
    module_stream_destroy(mod_sink_b);
}

static void teardown(void)
{
    stream_teardown();
    lib_teardown();
}

static void setup(void)
{
    lib_setup();
    stream_teardown(); /* try destroying uninit'd stream */

    /* source_a */
    memset(&st_source_a, 0, sizeof(st_source_a));
    module_stream_init(NULL, mod_source_a, NULL);
    /* source module, no parent */

    /* source_b */
    memset(&st_source_b, 0, sizeof(st_source_b));
    module_stream_init(NULL, mod_source_b, NULL);
    /* source module, no parent */

    /* selector */
    memset(&st_selector, 0, sizeof(st_selector));
    module_stream_init(NULL, mod_selector, ts_thunk);
    /* parent assigned during tests */

    /* foobar */
    memset(&st_foobar, 0, sizeof(st_foobar));
    module_stream_init(NULL, mod_foobar, ts_thunk);
    /* attach to selector */
    module_stream_attach(mod_selector, mod_foobar);

    /* sink_a */
    memset(&st_sink_a, 0, sizeof(st_sink_a));
    module_stream_init(NULL, mod_sink_a, ts_thunk);
    /* attach to foobar */
    module_stream_attach(mod_foobar, mod_sink_a);

    /* sink_b */
    memset(&st_sink_b, 0, sizeof(st_sink_b));
    module_stream_init(NULL, mod_sink_b, ts_thunk);
    /* attach to foobar */
    module_stream_attach(mod_foobar, mod_sink_b);
}

static void bulk_send(module_data_t *mod, uint16_t pid, unsigned int cnt)
{
    uint8_t ts[188] = { 0x47 };
    TS_SET_PID(ts, pid);

    for (size_t i = 0; i < cnt; i++)
        module_stream_send(mod, ts);
}

/* input selector test */
static unsigned int select_cnt[MAX_PID];

static void select_on_ts(module_data_t *mod, const uint8_t *ts)
{
    const uint16_t pid = TS_GET_PID(ts);
    select_cnt[pid]++;

    module_stream_send(mod, ts);
}

START_TEST(input_select)
{
    memset(&select_cnt, 0, sizeof(select_cnt));
    st_selector.on_ts = select_on_ts;
    st_foobar.on_ts = select_on_ts;
    st_sink_a.on_ts = select_on_ts;
    st_sink_b.on_ts = select_on_ts;

    /* round 1: selector not attached */
    bulk_send(mod_source_a, 100, 1000);
    bulk_send(mod_source_b, 200, 1000);

    for (size_t i = 0; i < ASC_ARRAY_SIZE(select_cnt); i++)
        ck_assert(select_cnt[i] == 0);

    /* round 2: attach to source_a */
    module_stream_attach(mod_source_a, mod_selector);

    bulk_send(mod_source_a, 500, 1234); /* x4 */
    bulk_send(mod_source_b, 501, 4321);

    for (size_t i = 0; i < ASC_ARRAY_SIZE(select_cnt); i++)
        ck_assert(select_cnt[i] == (i == 500 ? 4936 : 0));

    /* round 3: attach to source_b */
    memset(&select_cnt, 0, sizeof(select_cnt));
    module_stream_attach(mod_source_b, mod_selector);

    bulk_send(mod_source_b, 1000, 9999); /* x4 */
    bulk_send(mod_source_a, 1100, 5120);

    for (size_t i = 0; i < ASC_ARRAY_SIZE(select_cnt); i++)
        ck_assert(select_cnt[i] == (i == 1000 ? 39996 : 0));

    /* round 4: detach */
    memset(&select_cnt, 0, sizeof(select_cnt));
    module_stream_attach(NULL, mod_selector);

    for (size_t i = 0; i < ASC_ARRAY_SIZE(select_cnt); i++)
    {
        bulk_send(mod_source_a, i, 100);
        bulk_send(mod_source_b, i, 100);
        ck_assert(select_cnt[i] == 0);
    }
}
END_TEST

/* move pid membership between parents */
typedef struct
{
    struct test_stream *st;
    uint16_t pid;
    bool is_member;
} demux_test_t;

static void test_demux(const demux_test_t *test)
{
    for (const demux_test_t *p = test; p->st != NULL; p++)
    {
        const module_data_t *const mod = (module_data_t *)p->st;
        ck_assert(p->is_member == module_demux_check(mod, p->pid));
    }
}

#define MOVE_PID_A 0x100
#define MOVE_PID_B 0x200

START_TEST(demux_move)
{
    /* round 1: selector is unattached */
    module_demux_join(mod_selector, MOVE_PID_A);
    module_demux_join(mod_foobar, MOVE_PID_A);
    static const demux_test_t round_1[] =
    {
        { &st_source_a, MOVE_PID_A, false },
        { &st_source_b, MOVE_PID_A, false },
        { &st_selector, MOVE_PID_A, true },
        { &st_foobar, MOVE_PID_A, true },
        { &st_sink_a, MOVE_PID_A, false },
        { &st_sink_b, MOVE_PID_A, false },
        { NULL, 0, 0 },
    };
    test_demux(round_1);

    /* round 2: attach selector to source_a */
    module_stream_attach(mod_source_a, mod_selector);
    static const demux_test_t round_2[] =
    {
        { &st_source_a, MOVE_PID_A, true },
        { &st_source_b, MOVE_PID_A, false },
        { NULL, 0, 0 },
    };
    test_demux(round_2);

    module_demux_join(mod_selector, MOVE_PID_B);

    /* round 3: attach selector to source_b */
    module_stream_attach(mod_source_b, mod_selector);
    static const demux_test_t round_3[] =
    {
        { &st_source_a, MOVE_PID_A, false },
        { &st_source_a, MOVE_PID_B, false },
        { &st_source_b, MOVE_PID_A, true },
        { &st_source_b, MOVE_PID_B, true },
        { NULL, 0, 0 },
    };
    test_demux(round_3);

    /* round 4: detach */
    module_stream_attach(NULL, mod_selector);
    static const demux_test_t round_4[] =
    {
        { &st_source_a, MOVE_PID_A, false },
        { &st_source_a, MOVE_PID_B, false },
        { &st_source_b, MOVE_PID_A, false },
        { &st_source_b, MOVE_PID_B, false },
        { NULL, 0, 0 },
    };
    test_demux(round_4);

    /* check refcounting */
    module_demux_leave(mod_selector, MOVE_PID_A); /* still ref'd by foobar */
    ck_assert(module_demux_check(mod_selector, MOVE_PID_A));
    module_demux_leave(mod_foobar, MOVE_PID_A); /* remove last reference */
    ck_assert(!module_demux_check(mod_selector, MOVE_PID_A));
}
END_TEST

/* discard downstream pid requests */
#define DISCARD_PID_A 0x400
#define DISCARD_PID_B 0x200

START_TEST(demux_discard)
{
    /* round 1: sinks attached to foobar */
    module_demux_set(mod_foobar, NULL, NULL);
    module_demux_set(mod_sink_a, NULL, NULL);
    module_demux_set(mod_sink_b, NULL, NULL);

    module_demux_join(mod_sink_a, DISCARD_PID_A);
    module_demux_join(mod_sink_b, DISCARD_PID_B);

    static const demux_test_t round_1[] =
    {
        { &st_sink_a, DISCARD_PID_A, true },
        { &st_sink_a, DISCARD_PID_B, false },
        { &st_sink_b, DISCARD_PID_A, false },
        { &st_sink_b, DISCARD_PID_B, true },
        { &st_foobar, DISCARD_PID_A, false },
        { &st_foobar, DISCARD_PID_B, false },
        { NULL, 0, 0 },
    };

    test_demux(round_1);

    /* round 2: attach sinks to sources */
    module_stream_attach(mod_source_a, mod_sink_a);
    module_stream_attach(mod_source_b, mod_sink_b);

    static const demux_test_t round_2[] =
    {
        { &st_sink_a, DISCARD_PID_A, true },
        { &st_sink_b, DISCARD_PID_B, true },
        { &st_source_a, DISCARD_PID_A, true },
        { &st_source_b, DISCARD_PID_B, true },
        { NULL, 0, 0 },
    };

    test_demux(round_2);

    /* round 3: set foobar demux mode to default and reattach sinks */
    module_demux_set(mod_foobar, module_demux_join, module_demux_leave);
    /* NOTE: don't call this function outside of module_init() */

    module_stream_attach(mod_foobar, mod_sink_a);
    module_stream_attach(mod_foobar, mod_sink_b);

    static const demux_test_t round_3[] =
    {
        { &st_foobar, DISCARD_PID_A, true },
        { &st_foobar, DISCARD_PID_B, true },
        { NULL, 0, 0 },
    };

    test_demux(round_3);
}
END_TEST

/* make sure requested pids are flooded to all children */
static bool flood_pids[MAX_PID];
static unsigned int flood_sink_cnt[2][MAX_PID];

static void flood_join(module_data_t *mod, uint16_t pid)
{
    if (!module_demux_check(mod, pid))
        flood_pids[pid] = true;

    module_demux_join(mod, pid);
}

static void flood_leave(module_data_t *mod, uint16_t pid)
{
    module_demux_leave(mod, pid);

    if (!module_demux_check(mod, pid))
        flood_pids[pid] = false;
}

static void flood_send(void)
{
    for (unsigned int i = 0; i < MAX_PID; i++)
    {
        if (flood_pids[i])
        {
            uint8_t ts[TS_PACKET_SIZE] = { 0x47 };
            TS_SET_PID(ts, i);
            module_stream_send(mod_foobar, ts);
        }
    }
}

static void flood_on_sink_ts(module_data_t *mod, const uint8_t *ts)
{
    const unsigned int idx = (mod == mod_sink_a ? 0 : 1);
    const uint16_t pid = TS_GET_PID(ts);

    flood_sink_cnt[idx][pid]++;
}

#define FLOOD_COMMON_PID 0x10
#define FLOOD_PID_A 0x400
#define FLOOD_PID_B 0x800

START_TEST(demux_flood)
{
    /* set up pid membership */
    memset(flood_pids, 0, sizeof(flood_pids));
    memset(flood_sink_cnt, 0, sizeof(flood_sink_cnt));

    module_demux_set(mod_foobar, flood_join, flood_leave);

    st_sink_a.on_ts = flood_on_sink_ts;
    module_demux_join(mod_sink_a, FLOOD_COMMON_PID);
    module_demux_join(mod_sink_a, FLOOD_PID_A);

    st_sink_b.on_ts = flood_on_sink_ts;
    module_demux_join(mod_sink_b, FLOOD_COMMON_PID);
    module_demux_join(mod_sink_b, FLOOD_PID_B);

    /* send packets from foobar, make sure both sinks get them */
    for (unsigned int i = 0; i < 1000; i++)
        flood_send();

    for (unsigned int i = 0; i <= 1; i++)
    {
        ck_assert(flood_sink_cnt[i][FLOOD_COMMON_PID] == 1000);
        ck_assert(flood_sink_cnt[i][FLOOD_PID_A] == 1000);
        ck_assert(flood_sink_cnt[i][FLOOD_PID_B] == 1000);
    }

    /* test refcounting */
    module_demux_leave(mod_sink_a, FLOOD_COMMON_PID);
    ck_assert(module_demux_check(mod_foobar, FLOOD_COMMON_PID));
    module_demux_leave(mod_sink_b, FLOOD_COMMON_PID);
    ck_assert(!module_demux_check(mod_foobar, FLOOD_COMMON_PID));
}
END_TEST

/* stacking pid memberships */
#define STACK_PID 0x1500

START_TEST(demux_stack)
{
    /* make foobar and selector join pid */
    module_demux_join(mod_foobar, STACK_PID);
    module_demux_join(mod_selector, STACK_PID);
    ck_assert(module_demux_check(mod_foobar, STACK_PID));
    ck_assert(module_demux_check(mod_selector, STACK_PID));

    /* attach foobar to source_a */
    module_stream_attach(mod_source_a, mod_foobar);
    ck_assert(module_demux_check(mod_source_a, STACK_PID));
    ck_assert(!module_demux_check(mod_source_b, STACK_PID));

    /* attach foobar to source_b */
    module_stream_attach(mod_source_b, mod_foobar);
    ck_assert(!module_demux_check(mod_source_a, STACK_PID));
    ck_assert(module_demux_check(mod_source_b, STACK_PID));

    /* have selector leave pid */
    module_demux_leave(mod_selector, STACK_PID); /* only one ref */
    ck_assert(!module_demux_check(mod_selector, STACK_PID));
    module_demux_join(mod_selector, STACK_PID);

    /* detach foobar */
    module_stream_attach(NULL, mod_foobar);
    ck_assert(!module_demux_check(mod_source_a, STACK_PID));
    ck_assert(!module_demux_check(mod_source_b, STACK_PID));

    /* reattach foobar to selector */
    module_stream_attach(mod_selector, mod_foobar);
    ck_assert(module_demux_check(mod_selector, STACK_PID));
    module_demux_leave(mod_selector, STACK_PID); /* ref'd by foobar */
    ck_assert(module_demux_check(mod_selector, STACK_PID));
    module_demux_leave(mod_foobar, STACK_PID); /* last reference */
    ck_assert(!module_demux_check(mod_selector, STACK_PID));
}
END_TEST

/* make sure modules leave their pids when destroyed */
#define DESTROY_PID_A 0x100
#define DESTROY_PID_B 0x200
#define DESTROY_PID_C 0x300
#define DESTROY_PID_COMMON 0xff0

START_TEST(demux_destroy)
{
    /* set up pid membership */
    module_stream_attach(mod_source_a, mod_selector);

    module_demux_join(mod_sink_a, DESTROY_PID_A);
    module_demux_join(mod_sink_a, DESTROY_PID_COMMON);

    module_demux_join(mod_sink_b, DESTROY_PID_B);
    module_demux_join(mod_sink_b, DESTROY_PID_COMMON);

    module_demux_join(mod_foobar, DESTROY_PID_C);
    module_demux_join(mod_foobar, DESTROY_PID_COMMON);

    static const demux_test_t pid_setup[] =
    {
        { &st_selector, DESTROY_PID_A, true },
        { &st_selector, DESTROY_PID_B, true },
        { &st_selector, DESTROY_PID_C, true },
        { &st_selector, DESTROY_PID_COMMON, true },
        { &st_source_a, DESTROY_PID_A, true },
        { &st_source_a, DESTROY_PID_B, true },
        { &st_source_a, DESTROY_PID_C, true },
        { &st_source_a, DESTROY_PID_COMMON, true },
        { NULL, 0, 0 },
    };

    test_demux(pid_setup);

    /* destroy sink_a */
    module_stream_destroy(mod_sink_a);

    static const demux_test_t pid_a_gone[] =
    {
        { &st_foobar, DESTROY_PID_A, false },
        { &st_foobar, DESTROY_PID_COMMON, true },
        { &st_selector, DESTROY_PID_A, false },
        { &st_selector, DESTROY_PID_COMMON, true },
        { &st_source_a, DESTROY_PID_A, false },
        { &st_source_a, DESTROY_PID_COMMON, true },
        { NULL, 0, 0 },
    };

    test_demux(pid_a_gone);

    /* attach selector to source_b */
    module_stream_attach(mod_source_b, mod_selector);

    static const demux_test_t reattached[] =
    {
        { &st_source_b, DESTROY_PID_B, true },
        { &st_source_b, DESTROY_PID_C, true },
        { &st_source_b, DESTROY_PID_COMMON, true },
        { NULL, 0, 0 },
    };

    test_demux(reattached);

    /* destroy sink_b */
    module_stream_destroy(mod_sink_b);

    static const demux_test_t pid_b_gone[] =
    {
        { &st_foobar, DESTROY_PID_B, false },
        { &st_foobar, DESTROY_PID_COMMON, true },
        { &st_selector, DESTROY_PID_B, false },
        { &st_selector, DESTROY_PID_COMMON, true },
        { &st_source_b, DESTROY_PID_B, false },
        { &st_source_b, DESTROY_PID_COMMON, true },
        { NULL, 0, 0 },
    };

    test_demux(pid_b_gone);

    /* destroy foobar */
    module_stream_destroy(mod_foobar);

    static const demux_test_t foobar_gone[] =
    {
        { &st_selector, DESTROY_PID_C, false },
        { &st_selector, DESTROY_PID_COMMON, false },
        { &st_source_b, DESTROY_PID_C, false },
        { &st_source_b, DESTROY_PID_COMMON, false },
        { NULL, 0, 0 },
    };

    test_demux(foobar_gone);
}
END_TEST

/* make sure double leave doesn't cause refcount underflow */
#define DOUBLE_PID 0x1000

START_TEST(double_leave)
{
    module_demux_join(mod_selector, DOUBLE_PID);
    ck_assert(module_demux_check(mod_selector, DOUBLE_PID));

    module_demux_leave(mod_selector, DOUBLE_PID);
    module_demux_leave(mod_selector, DOUBLE_PID);
    ck_assert(!module_demux_check(mod_selector, DOUBLE_PID));
}
END_TEST

/* trying to initialize twice */
START_TEST(double_init)
{
    struct test_stream test;
    memset(&test, 0, sizeof(test));

    module_data_t *const mod = (module_data_t *)&test;
    module_stream_init(NULL, mod, NULL);
    module_stream_init(NULL, mod, NULL); /* will abort */
}
END_TEST

/* attach to uninitialized module */
START_TEST(bad_attach)
{
    struct test_stream parent;
    memset(&parent, 0, sizeof(parent));
    module_data_t *const mod_parent = (module_data_t *)&parent;

    struct test_stream child;
    memset(&child, 0, sizeof(child));
    module_data_t *const mod_child = (module_data_t *)&child;

    module_stream_init(NULL, mod_child, NULL);
    module_stream_attach(mod_parent, mod_child); /* will abort */
}
END_TEST

/* attach module to itself */
static size_t loop_cnt;

static void loop_on_ts(module_data_t *mod, const uint8_t *ts)
{
    if (++loop_cnt >= 1000)
        asc_lib_abort();

    module_stream_send(mod, ts);
}

START_TEST(ouroboros)
{
    loop_cnt = 0;

    st_source_a.on_ts = loop_on_ts;
    module_stream_attach(mod_source_a, mod_source_a);

    static const uint8_t ts[TS_PACKET_SIZE] = { 0x47 };
    module_stream_send(mod_source_a, ts);
}
END_TEST

/* try attaching a source module to parent */
START_TEST(no_on_ts)
{
    module_stream_attach(mod_selector, mod_source_a);
}
END_TEST

/* demux calls with invalid pids */
START_TEST(range_join)
{
    module_demux_join(mod_foobar, 0x2000);
}
END_TEST

START_TEST(range_leave)
{
    module_demux_leave(mod_foobar, 0x2000);
}
END_TEST

START_TEST(range_check)
{
    const bool ret = module_demux_check(mod_foobar, 0x2000);

    /* should be unreachable */
    ck_assert(ret == false);
    ck_abort_msg("didn't expect to reach this code");
}
END_TEST

Suite *luaapi_stream(void)
{
    Suite *const s = suite_create("luaapi/stream");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, input_select);
    tcase_add_test(tc, demux_move);
    tcase_add_test(tc, demux_discard);
    tcase_add_test(tc, demux_flood);
    tcase_add_test(tc, demux_stack);
    tcase_add_test(tc, demux_destroy);
    tcase_add_test(tc, double_leave);
    suite_add_tcase(s, tc);

    if (can_fork != CK_NOFORK)
    {
        TCase *const tc_f = tcase_create("fail");
        tcase_add_checked_fixture(tc_f, setup, teardown);
        tcase_add_exit_test(tc_f, double_init, EXIT_ABORT);
        tcase_add_exit_test(tc_f, bad_attach, EXIT_ABORT);
        tcase_add_exit_test(tc_f, ouroboros, EXIT_ABORT);
        tcase_add_exit_test(tc_f, no_on_ts, EXIT_ABORT);
        tcase_add_exit_test(tc_f, range_join, EXIT_ABORT);
        tcase_add_exit_test(tc_f, range_leave, EXIT_ABORT);
        tcase_add_exit_test(tc_f, range_check, EXIT_ABORT);
        suite_add_tcase(s, tc_f);
    }

    return s;
}
