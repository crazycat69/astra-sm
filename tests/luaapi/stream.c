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

static void teardown(void)
{
    module_stream_destroy(mod_source_a);
    module_stream_destroy(mod_source_b);
    module_stream_destroy(mod_selector);
    module_stream_destroy(mod_foobar);
    module_stream_destroy(mod_sink_a);
    module_stream_destroy(mod_sink_b);
}

static void setup(void)
{
    teardown();

    /* source_a */
    memset(&st_source_a, 0, sizeof(st_source_a));
    module_stream_init(NULL, mod_source_a, ts_thunk);
    /* source module, no parent */

    /* source_b */
    memset(&st_source_b, 0, sizeof(st_source_b));
    module_stream_init(NULL, mod_source_b, ts_thunk);
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

Suite *luaapi_stream(void)
{
    Suite *const s = suite_create("luaapi/stream");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, input_select);
    suite_add_tcase(s, tc);

    if (can_fork != CK_NOFORK)
    {
        TCase *const tc_f = tcase_create("fail");
        tcase_add_checked_fixture(tc_f, lib_setup, lib_teardown);
        tcase_add_exit_test(tc_f, double_init, EXIT_ABORT);
        tcase_add_exit_test(tc_f, bad_attach, EXIT_ABORT);
        suite_add_tcase(s, tc_f);
    }

    return s;
}
