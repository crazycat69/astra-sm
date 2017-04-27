/*
 * Astra Unit Tests
 * http://cesbo.com/astra
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

#include "../libastra.h"
#include <astra/core/timer.h>
#include <astra/core/mainloop.h>
#include <astra/mpegts/pcr.h>
#include <astra/mpegts/sync.h>

static
void fail_on_ts(void *arg, const uint8_t *ts)
{
    ASC_UNUSED(arg);
    ASC_UNUSED(ts);

    ck_abort_msg("didn't expect to reach this code");
}

/* simple TS generator with PCR insertion */
#define GEN_PCR_PID 0x100
#define GEN_DATA_PID 0x200

typedef struct
{
    uint32_t bitrate; /* desired bitrate, bits per second */
    uint32_t duration; /* requested duration, msec */

    unsigned int cc;
    unsigned int left;
    unsigned int total;
    unsigned int offset;
    uint64_t pcr_base;
    bool insert_pcr;
} ts_generator_t;

static
bool ts_generator(ts_generator_t *gen, uint8_t ts[TS_PACKET_SIZE])
{
    if (gen->left == 0)
    {
        if (gen->duration > 0)
        {
            ck_assert(gen->bitrate > 0);
            gen->total = TS_PCR_PACKETS(gen->duration, gen->bitrate);
            gen->left = gen->total;
            gen->duration = 0;
        }
        else
        {
            return false;
        }
    }

    if ((!gen->insert_pcr && gen->left == gen->total) || gen->left == 1)
        gen->insert_pcr = true;
    else
        gen->insert_pcr = false;

    gen->offset += TS_PACKET_SIZE;
    if (gen->left > 0)
        gen->left--;

    memset(ts, 0, TS_PACKET_SIZE);
    TS_INIT(ts);

    if (gen->insert_pcr)
    {
        TS_SET_PID(ts, GEN_PCR_PID);
        TS_SET_AF(ts, TS_BODY_SIZE - 1);

        gen->pcr_base += (gen->offset * TS_PCR_FREQ * 8) / gen->bitrate;
        gen->pcr_base %= TS_PCR_MAX;
        gen->offset = 0;

        TS_SET_PCR(ts, gen->pcr_base);
    }
    else
    {
        TS_SET_PID(ts, GEN_DATA_PID);
        TS_SET_PAYLOAD(ts, true);
        TS_SET_CC(ts, gen->cc);

        gen->cc = (gen->cc + 1) & 0xf;
    }

    return true;
}

/* basic runtime configuration test */
static
unsigned int setters_pulled = 0;

static
void setters_on_ready(void *arg)
{
    ASC_UNUSED(arg);

    setters_pulled++;
}

START_TEST(setters)
{
    typedef struct
    {
        const char *opts;
        bool expect;
        unsigned int enough;
        unsigned int low;
        unsigned int mbytes;
    } opt_test_t;

    ts_sync_t *const sx = ts_sync_init(fail_on_ts, NULL);
    ck_assert(sx != NULL);

    /* default configuration */
    ts_sync_stat_t st;
    ts_sync_query(sx, &st);

    ck_assert(st.enough_blocks > 0);
    ck_assert(st.low_blocks > 0);
    ck_assert(st.max_size > 0);

    const opt_test_t def =
    {
        .enough = st.enough_blocks,
        .low = st.low_blocks,
        .mbytes = (st.max_size * TS_PACKET_SIZE) / 1048576,
    };

    ck_assert(st.size > 0);
    ck_assert(st.filled == 0);
    ck_assert(st.want > 0);
    ck_assert(st.bitrate < 0.00001 && st.bitrate > -0.00001);
    ck_assert(st.num_blocks == 0);

    /* set block thresholds */
    ck_assert(ts_sync_set_blocks(sx, 40, 20) == true);
    ck_assert(ts_sync_set_blocks(sx, 1, 10000) == false);

    ts_sync_query(sx, &st);
    ck_assert(st.enough_blocks == 40);
    ck_assert(st.low_blocks == 20);

    /* set maximum size */
    ck_assert(ts_sync_set_max_size(sx, 64) == true);
    ck_assert(ts_sync_set_max_size(sx, 0) == false);

    ts_sync_query(sx, &st);
    ck_assert(st.max_size == (64 * 1048576) / TS_PACKET_SIZE);

    /* go back to default settings */
    ck_assert(ts_sync_set_blocks(sx, def.enough, def.low) == true);
    ck_assert(ts_sync_set_max_size(sx, def.mbytes) == true);

    /* string option setter */
    const opt_test_t tests[] =
    {
        /* NOTE: passing empty string leaves configuration unchanged */
        { "", true,
          def.enough, def.low, def.mbytes },

        { "20,10,32", true,
          20, 10, 32 },

        { "", true,
          20, 10, 32 },

        { ",,", true,
          20, 10, 32 },

        { ",,,", false,
          20, 10, 32 },

        { "1,1001,", false,
          20, 10, 32 },

        { ",,16", true,
          20, 10, 16 },

        { "40,10", true,
          40, 10, 16 },

        { ",", true,
          40, 10, 16},
    };

    for (size_t i = 0; i < ASC_ARRAY_SIZE(tests); i++)
    {
        const opt_test_t *const t = &tests[i];
        ck_assert(ts_sync_set_opts(sx, t->opts) == t->expect);

        ts_sync_query(sx, &st);
        ck_assert(st.enough_blocks == t->enough);
        ck_assert(st.low_blocks == t->low);
        ck_assert(st.max_size == (t->mbytes * 1048576) / TS_PACKET_SIZE);

        /* reset doesn't affect buffer configuration */
        ts_sync_reset(sx);
    }

    /* on_ready callback */
    setters_pulled = 0;
    for (size_t i = 0; i < 10; i++)
    {
        ts_sync_loop(sx);
        asc_usleep(SYNC_INTERVAL_MSEC * 1000);
    }
    ck_assert(setters_pulled == 0);

    ts_sync_set_on_ready(sx, setters_on_ready);
    for (size_t i = 0; i < 10; i++)
    {
        ts_sync_loop(sx);
        asc_usleep(SYNC_INTERVAL_MSEC * 1000);
    }
    ck_assert(setters_pulled == 10);

    ts_sync_destroy(sx);
}
END_TEST

/* stream without PCR packets */
static
unsigned int no_pcr_pulled = 0;

static
void no_pcr_ready(void *arg)
{
    ASC_UNUSED(arg);
    no_pcr_pulled++;
}

START_TEST(no_pcr)
{
    ts_sync_t *sx = ts_sync_init(fail_on_ts, NULL);
    ts_sync_set_on_ready(sx, no_pcr_ready);

    ts_sync_stat_t def;
    ts_sync_query(sx, &def);

    ck_assert(def.size > 0);
    ck_assert(def.filled == 0);
    ck_assert(def.want > 0);
    ck_assert(def.bitrate < 0.00001 && def.bitrate > -0.00001);
    ck_assert(def.num_blocks == 0);

    size_t total = 0;
    bool ret;

    do
    {
        const size_t cnt = rand() % 500;
        ts_packet_t *const ts = ASC_ALLOC(cnt, ts_packet_t);

        for (size_t i = 0; i < cnt; i++)
            memcpy(ts[i], ts_null_pkt, TS_PACKET_SIZE);

        ret = ts_sync_push(sx, ts, cnt);
        if (ret)
            total += cnt;

        free(ts);
    } while (ret);

    asc_log_debug("no_pcr: pushed %zu packets", total);
    ck_assert(total > 0);
    ck_assert(ts_sync_push(sx, NULL, 0) == true);

    /* buffer should have expanded dynamically */
    ts_sync_stat_t st;
    ts_sync_query(sx, &st);

    ck_assert(st.filled == total);
    ck_assert(st.size > def.size);
    ck_assert(st.want > 0);

    /*
     * Expect buffer to request more data as it can't work with
     * a PCR-less stream.
     */
    no_pcr_pulled = 0;
    ts_sync_loop(sx);
    ck_assert(no_pcr_pulled == 1);

    /* reset should revert buffer to default settings */
    ts_sync_reset(sx);
    ts_sync_query(sx, &st);
    ck_assert(!memcmp(&st, &def, sizeof(ts_sync_stat_t)));

    ASC_FREE(sx, ts_sync_destroy);
}
END_TEST

/* CBR stream with PCR generated from system clock */
#define CLK_BENCH_COUNT 30 /* test duration is 3 seconds */
#define CLK_TS_RATE 10000000 /* 10 mbps */
#define CLK_TS_INTERVAL 35000 /* 35 ms */

typedef struct
{
    ts_sync_t *sx;

    uint64_t push_last;
    uint64_t pcr_elapsed;
    uint64_t pcr_value;
    unsigned int tx_cc;

    uint64_t rx_last;
    size_t rx_clk_packets;
    size_t rx_pcr_packets;
    size_t rx_pcr;
    uint64_t rx_pcr_val;
    size_t rx_data;
    unsigned int rx_cc;

    double clk_rate[CLK_BENCH_COUNT];
    double pcr_rate[CLK_BENCH_COUNT];
    size_t clk_idx;
    size_t pcr_idx;

    size_t sd_size;
    size_t sd_filled;
    unsigned int sd_blocks;
    bool spindown;
} clk_test_t;

static
void clk_on_push(void *arg)
{
    clk_test_t *const t = (clk_test_t *)arg;

    if (t->spindown)
    {
        ts_sync_stat_t st;
        ts_sync_query(t->sx, &st);

        ck_assert(st.size <= t->sd_size);
        ck_assert(st.filled <= t->sd_filled);
        ck_assert(st.num_blocks <= t->sd_blocks);

        t->sd_size = st.size;
        t->sd_filled = st.filled;
        t->sd_blocks = st.num_blocks;

        if (t->sd_filled == 0)
            asc_main_loop_shutdown();

        return;
    }

    const uint64_t now = asc_utime();
    const uint64_t elapsed = now - t->push_last;
    t->push_last = now;

    if (elapsed == 0)
        return;

    t->pcr_elapsed += elapsed;
    double pending = (CLK_TS_RATE / 8.0) / (1000000.0 / elapsed);

    while (pending > TS_PACKET_SIZE)
    {
        pending -= TS_PACKET_SIZE;

        if (t->pcr_elapsed >= CLK_TS_INTERVAL)
        {
            const uint64_t inc = t->pcr_elapsed * (TS_PCR_FREQ / 1000000);
            t->pcr_value = (t->pcr_value + inc) % TS_PCR_MAX;

            /* push PCR packet */
            uint8_t ts[TS_PACKET_SIZE];

            TS_INIT(ts);
            TS_SET_PID(ts, GEN_PCR_PID);
            TS_SET_AF(ts, TS_BODY_SIZE - 1);
            TS_SET_PCR(ts, t->pcr_value);
            ck_assert(ts_sync_push(t->sx, ts, 1) == true);

            t->pcr_elapsed = 0;
        }

        /* push data packet */
        uint8_t ts[TS_PACKET_SIZE] = { 0 };

        TS_INIT(ts);
        TS_SET_PID(ts, GEN_DATA_PID);
        TS_SET_PAYLOAD(ts, true);
        TS_SET_CC(ts, t->tx_cc);
        ck_assert(ts_sync_push(t->sx, ts, 1) == true);

        t->tx_cc = (t->tx_cc + 1) & 0xf;
    }
}

static
void clk_on_ts(void *arg, const uint8_t *ts)
{
    clk_test_t *const t = (clk_test_t *)arg;

    ck_assert(TS_IS_SYNC(ts));
    const unsigned int pid = TS_GET_PID(ts);

    t->rx_clk_packets++;
    t->rx_pcr_packets++;

    if (pid == GEN_DATA_PID)
    {
        ck_assert(TS_IS_PAYLOAD(ts));
        const unsigned int cc = TS_GET_CC(ts);

        if (t->rx_data++ == 0)
            t->rx_cc = cc;

        ck_assert(!TS_IS_PCR(ts) && TS_IS_PAYLOAD(ts));
        if (cc != t->rx_cc)
            asc_log_error("sys_clock: expected %u got %u", t->rx_cc, cc);

        ck_assert(cc == t->rx_cc);
        t->rx_cc = (cc + 1) & 0xf;
    }
    else if (pid == GEN_PCR_PID)
    {
        ck_assert(TS_IS_PCR(ts) && !TS_IS_PAYLOAD(ts));
        const uint64_t pcr = TS_GET_PCR(ts);

        if (t->rx_pcr++ != 0)
        {
            const uint64_t delta = TS_PCR_DELTA(t->rx_pcr_val, pcr);
            const size_t bytes = t->rx_pcr_packets * TS_PACKET_SIZE;

            /* store bitrate (PCR) */
            const double br = bytes * ((double)TS_PCR_FREQ / delta);

            if (t->pcr_idx < CLK_BENCH_COUNT)
                t->pcr_rate[t->pcr_idx++] = br;
        }

        t->rx_pcr_packets = 0;
        t->rx_pcr_val = pcr;
    }
    else
    {
        ck_abort_msg("unknown PID: %u", pid);
    }

    const uint64_t now = asc_utime();
    const uint64_t clk_delta = now - t->rx_last;

    if (clk_delta > 100000) /* 100ms */
    {
        if (t->rx_last != 0)
        {
            ts_sync_stat_t st;
            ts_sync_query(t->sx, &st);

            ck_assert(st.size < st.max_size);
            ck_assert(st.filled < st.size);
            ck_assert(st.bitrate > 0.0);
            ck_assert(st.num_blocks >= st.enough_blocks
                      || st.want > 0);

            /* store bitrate (system clock) */
            const double br =
                (t->rx_clk_packets * TS_PACKET_SIZE * 1000000.0) / clk_delta;

            if (t->clk_idx < CLK_BENCH_COUNT)
                t->clk_rate[t->clk_idx++] = br;
        }

        t->rx_clk_packets = 0;
        t->rx_last = now;
    }

    if (t->clk_idx >= CLK_BENCH_COUNT
        && t->pcr_idx >= CLK_BENCH_COUNT
        && !t->spindown)
    {
        asc_log_info("finished collecting bitrate stats");

        ts_sync_stat_t st;
        ts_sync_query(t->sx, &st);

        t->sd_size = st.size;
        t->sd_filled = st.filled;
        t->sd_blocks = st.num_blocks;

        t->spindown = true;
    }
}

START_TEST(sys_clock)
{
    clk_test_t t;
    memset(&t, 0, sizeof(t));

    ts_sync_t *sx = ts_sync_init(clk_on_ts, &t);
    asc_timer_t *push = asc_timer_init(5, clk_on_push, &t);
    asc_timer_t *dequeue = asc_timer_init(SYNC_INTERVAL_MSEC
                                          , ts_sync_loop, sx);

    t.push_last = asc_utime();
    t.pcr_value = TS_PCR_MAX - TS_PCR_FREQ;
    t.sx = sx;

    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    ck_assert(t.clk_idx == CLK_BENCH_COUNT
              && t.pcr_idx == CLK_BENCH_COUNT);

    /* check average bitrate */
    double total_clk = 0.0, total_pcr = 0.0;

    for (size_t i = 0; i < CLK_BENCH_COUNT; i++)
    {
        total_clk += t.clk_rate[i];
        total_pcr += t.pcr_rate[i];
    }

    total_clk /= CLK_BENCH_COUNT;
    total_pcr /= CLK_BENCH_COUNT;

    const double hi = (CLK_TS_RATE / 8) * 1.10;
    const double lo = (CLK_TS_RATE / 8) * 0.90;

    asc_log_debug("sys_clock: clk: %.2f, pcr: %.2f (diff %.2f)"
                  , total_clk, total_pcr, total_clk - total_pcr);

    ck_assert((total_clk < hi && total_clk > lo)
              && (total_pcr < hi && total_pcr > lo));

    ASC_FREE(dequeue, asc_timer_destroy);
    ASC_FREE(push, asc_timer_destroy);
    ASC_FREE(sx, ts_sync_destroy);
}
END_TEST

/* buffer stopping output due to low fill level */
#define UNDER_TS_RATE 256000 /* 256 Kbps */
#define UNDER_ROUNDS 20 /* cause buffer underflow 20 times */
#define UNDER_PCR_INTERVAL 10 /* 10 ms */
#define UNDER_MIN_DELAY 50 /* 50 ms */
#define UNDER_MAX_DELAY 125 /* 125 ms */

typedef struct
{
    ts_sync_t *sx;
    ts_generator_t gen;
    asc_timer_t *timer;
    size_t rx_packets;
    size_t tx_packets;
    uint64_t bench_time;
    size_t bench_bits;
    unsigned int rounds;
    unsigned int cc;
    bool spindown;
} under_test_t;

static
void under_on_ts(void *arg, const uint8_t ts[TS_PACKET_SIZE])
{
    under_test_t *const t = (under_test_t *)arg;
    const unsigned int pid = TS_GET_PID(ts);

    if (pid == GEN_DATA_PID)
    {
        ck_assert(TS_IS_PAYLOAD(ts));
        const unsigned int cc = TS_GET_CC(ts);
        ck_assert(cc == t->cc);
        t->cc = (cc + 1) & 0xf;
    }
    else
    {
        ck_assert(pid == GEN_PCR_PID);
        ck_assert(TS_IS_PCR(ts));
        ck_assert(!TS_IS_PAYLOAD(ts));
    }

    const uint64_t now = asc_utime();
    if (now - t->bench_time > 1000000) /* 1 sec */
    {
        if (t->bench_time != 0)
        {
            /* not expecting full bitrate here */
            ck_assert(t->bench_bits >= (UNDER_TS_RATE / 10)
                      && t->bench_bits < UNDER_TS_RATE);
        }

        t->bench_time = now;
        t->bench_bits = 0;
    }

    t->bench_bits += (TS_PACKET_SIZE * 8);
    t->rx_packets++;
}

static
void under_on_ready(void *arg);

static
void under_on_timer(void *arg)
{
    under_test_t *const t = (under_test_t *)arg;
    ts_sync_stat_t st;

    do
    {
        uint8_t ts[TS_PACKET_SIZE];

        if (ts_generator(&t->gen, ts))
        {
            ck_assert(ts_sync_push(t->sx, ts, 1) == true);
            t->tx_packets++;
        }
        else
        {
            t->gen.bitrate = UNDER_TS_RATE;
            t->gen.duration = UNDER_PCR_INTERVAL;
        }

        ts_sync_query(t->sx, &st);
    } while (st.num_blocks < st.enough_blocks);

    t->timer = NULL;
    ts_sync_set_on_ready(t->sx, under_on_ready);
}

static
void under_on_ready(void *arg)
{
    under_test_t *const t = (under_test_t *)arg;

    ts_sync_stat_t st;
    ts_sync_query(t->sx, &st);

    if (t->spindown)
    {
        if (st.filled == 0)
        {
            ts_sync_set_on_ready(t->sx, NULL);
            asc_main_loop_shutdown();
        }
    }
    else if (st.num_blocks < st.low_blocks)
    {
        if (t->rounds < UNDER_ROUNDS)
        {
            unsigned int ms = UNDER_MIN_DELAY;
            ms += rand() % (UNDER_MAX_DELAY - UNDER_MIN_DELAY);
            t->timer = asc_timer_one_shot(ms, under_on_timer, t);

            asc_log_debug("underflow: %u: refilling buffer in %ums"
                          , t->rounds, ms);

            ts_sync_set_on_ready(t->sx, NULL);
            t->rounds++;
        }
        else
        {
            t->spindown = true;
        }
    }
}

START_TEST(underflow)
{
    under_test_t t;
    memset(&t, 0, sizeof(t));

    ts_sync_t *sx = ts_sync_init(under_on_ts, &t);
    asc_timer_t *loop = asc_timer_init(SYNC_INTERVAL_MSEC, ts_sync_loop, sx);
    ts_sync_set_on_ready(sx, under_on_ready);

    t.sx = sx;

    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    ck_assert(t.rounds == UNDER_ROUNDS);
    ck_assert(t.rx_packets > 0 && t.tx_packets > 0);
    ck_assert(t.cc == t.gen.cc);
    ck_assert(t.spindown == true);
    ck_assert(t.bench_time > 0);
    ck_assert(t.bench_bits > 0);

    ASC_FREE(loop, asc_timer_destroy);
    ASC_FREE(sx, ts_sync_destroy);
}
END_TEST

/* PCR value doesn't increase */
#define STILL_PUSH 15
#define STILL_ENOUGH 9
#define STILL_LOW 4
#define STILL_MIB 2

#define STILL_ZERO_PCR 0
#define STILL_NULL_PID 1

static
void still_on_ready(void *arg)
{
    int *const triggered = (int *)arg;

    (*triggered)++;
}

START_TEST(pcr_still)
{
    unsigned int triggered = 0;
    ts_sync_t *sx = ts_sync_init(fail_on_ts, &triggered);
    ck_assert(sx != NULL);

    ts_sync_set_on_ready(sx, still_on_ready);
    ts_sync_set_max_size(sx, STILL_MIB);
    const size_t max_pkts = (STILL_MIB * 1024 * 1024) / TS_PACKET_SIZE;
    ck_assert(ts_sync_set_blocks(sx, STILL_ENOUGH, STILL_LOW) == true);

    ts_generator_t gen =
    {
        .bitrate = 128000,
    };

    ts_sync_stat_t def;
    ts_sync_query(sx, &def);
    ck_assert(def.enough_blocks == STILL_ENOUGH);
    ck_assert(def.low_blocks == STILL_LOW);
    ck_assert(def.max_size == max_pkts);
    ck_assert(def.bitrate < 0.1 && def.bitrate > -0.1);
    ck_assert(def.size > 0 && def.size < def.max_size);
    ck_assert(def.filled == 0);
    ck_assert(def.num_blocks == 0);
    ck_assert(def.want > 0);

    ts_sync_stat_t st;
    ts_sync_loop(sx); /* establish time base */
    ck_assert(triggered == 1);
    ts_sync_query(sx, &st);
    ck_assert(!memcmp(&def, &st, sizeof(st)));

    for (unsigned int i = 0; i < 2; i++)
    {
        asc_log_debug("pcr_still: test %u", i);

        ts_sync_query(sx, &st);
        size_t packets = st.filled;

        for (unsigned int blocks = 0; blocks < STILL_PUSH;)
        {
            uint8_t ts[TS_PACKET_SIZE];

            if (ts_generator(&gen, ts))
            {
                if (i == STILL_ZERO_PCR)
                {
                    /* test 1: replace PCR value with zeroes */
                    if (TS_IS_PCR(ts))
                    {
                        ck_assert(TS_GET_PID(ts) == GEN_PCR_PID);
                        TS_SET_PCR(ts, 0);
                    }
                }
                else if (i == STILL_NULL_PID)
                {
                    /* test 2: remap PCR's to null PID */
                    if (TS_GET_PID(ts) == GEN_PCR_PID)
                    {
                        ck_assert(TS_IS_PCR(ts) && !TS_IS_PAYLOAD(ts));
                        TS_SET_PID(ts, TS_NULL_PID);
                    }
                }

                ck_assert(ts_sync_push(sx, ts, 1) == true);
                packets++;
            }
            else
            {
                gen.duration = 35;
                blocks++;
            }
        }

        ts_sync_query(sx, &st);
        ck_assert(st.bitrate > -0.1 && st.bitrate < 0.1);
        ck_assert(st.size == def.size);
        ck_assert(st.filled == packets);

        if (i == STILL_ZERO_PCR)
        {
            /*
             * NOTE: initial buffering doesn't do any kind of PCR processing
             *       apart from incrementing the block counter. However, it
             *       should still ignore PCR packets on PID 8191.
             */
            ck_assert(st.num_blocks == STILL_ENOUGH);
            ck_assert(st.want == 0);
        }

        triggered = 0;
        asc_usleep(25000);
        ts_sync_loop(sx); /* discard all packets and reset buffer (i == 0) */
        asc_usleep(25000);
        ts_sync_loop(sx); /* request more data */

        if (i == STILL_ZERO_PCR)
            ck_assert(triggered == 1);
        else
            ck_assert(triggered == 2);

        /* expect queued blocks to have been discarded */
        ts_sync_query(sx, &st);
        ck_assert(st.bitrate > -0.1 && st.bitrate < 0.1);
        ck_assert(st.size == def.size);
        ck_assert(st.num_blocks == 0);
        ck_assert(st.want == def.want);

        if (i == STILL_ZERO_PCR)
            ck_assert(st.filled == 1);
    }

    ASC_FREE(sx, ts_sync_destroy);
}
END_TEST

/* PCR delta out of range */
#define JUMP_BATCH 10
#define JUMP_MAX_DELTA 1080000 /* 40 ms */
#define JUMP_TS_RATE 10000000 /* 10 Mbps */

typedef struct
{
    ts_sync_t *sx;
    ts_generator_t gen;
    uint64_t rx_pcr;
    size_t tx_packets;
    size_t tx_bogus;
    size_t rx_pre;
    size_t rx_post;
    unsigned int cc;
    bool spindown;
} jump_test_t;

static
void jump_on_ts(void *arg, const uint8_t ts[TS_PACKET_SIZE])
{
    jump_test_t *const t = (jump_test_t *)arg;

    ts_sync_stat_t st;
    ts_sync_query(t->sx, &st);
    ck_assert(st.filled > 0);
    ck_assert(st.num_blocks > 0);
    ck_assert(st.bitrate > (JUMP_TS_RATE * 0.995)
              && st.bitrate < (JUMP_TS_RATE * 1.005));

    const unsigned int pid = TS_GET_PID(ts);
    if (pid == GEN_PCR_PID)
    {
        ck_assert(TS_IS_PCR(ts) && !TS_IS_PAYLOAD(ts));
        ck_assert(ts[TS_PACKET_SIZE - 1] != 0xff);

        const uint64_t pcr = TS_GET_PCR(ts);
        if (t->rx_pcr != 0)
        {
            const uint64_t delta = TS_PCR_DELTA(t->rx_pcr, pcr);
            ck_assert(delta > 0 && delta < JUMP_MAX_DELTA);
        }

        t->rx_pcr = pcr;
    }
    else if (pid == GEN_DATA_PID)
    {
        ck_assert(TS_IS_PAYLOAD(ts) && !TS_IS_AF(ts));

        switch (ts[TS_PACKET_SIZE - 1])
        {
            case 0x10: t->rx_pre++;  break;
            case 0x20: t->rx_post++; break;
            default:
                ck_abort_msg("buffer sent out invalid block!");
        }

        const unsigned int cc = TS_GET_CC(ts);
        ck_assert(cc == t->cc);
        t->cc = (cc + 1) & 0xf;
    }
    else
    {
        ck_abort_msg("unknown PID: %u", pid);
    }
}

static
void jump_insert_normal(jump_test_t *t, unsigned int marker)
{
    for (unsigned int pushed = 0; pushed < JUMP_BATCH;)
    {
        uint8_t ts[TS_PACKET_SIZE];

        if (ts_generator(&t->gen, ts))
        {
            ts[TS_PACKET_SIZE - 1] = marker;
            ck_assert(ts_sync_push(t->sx, ts, 1) == true);
            t->tx_packets++;
        }
        else
        {
            t->gen.bitrate = JUMP_TS_RATE;
            t->gen.duration = 15;
            pushed++;
        }
    }
}

static
void jump_insert_bogus(jump_test_t *t, unsigned int marker)
{
    uint64_t pcr = TS_PCR_MAX - 1;

    for (unsigned int i = 0; i < 100; i++)
    {
        uint8_t ts[TS_PACKET_SIZE] = { 0 };

        TS_INIT(ts);
        ts[TS_PACKET_SIZE - 1] = marker;

        if (!(i % 10))
        {
            TS_SET_PID(ts, GEN_PCR_PID);
            TS_SET_AF(ts, TS_BODY_SIZE - 1);
            TS_SET_PCR(ts, pcr);
            pcr -= TS_PCR_FREQ;
        }
        else
        {
            TS_SET_PID(ts, GEN_DATA_PID);
        }

        ck_assert(ts_sync_push(t->sx, ts, 1) == true);
        t->tx_bogus++;
    }
}

static
void jump_on_ready(void *arg)
{
    jump_test_t *const t = (jump_test_t *)arg;
    ts_sync_stat_t st;

    if (t->spindown)
    {
        ts_sync_query(t->sx, &st);

        if (st.filled == 0)
        {
            ck_assert(st.num_blocks == 0);
            ck_assert(st.bitrate > -0.1 && st.bitrate < 0.1);

            ts_sync_set_on_ready(t->sx, NULL);
            asc_main_loop_shutdown();
        }
        else
        {
            ck_assert(st.num_blocks > 0);
            ck_assert(st.bitrate > (JUMP_TS_RATE * 0.995)
                      && st.bitrate < (JUMP_TS_RATE * 1.005));
        }
    }
    else
    {
        jump_insert_normal(t, 0x10);
        jump_insert_bogus(t, 0xff); /* buffer should skip these */
        t->gen.insert_pcr = false;
        jump_insert_normal(t, 0x20);

        ts_sync_query(t->sx, &st);
        ck_assert(st.num_blocks >= st.enough_blocks);
        ck_assert(st.bitrate > -0.1 && st.bitrate < 0.1);
        ck_assert(st.filled > 0 && st.want == 0);
        ck_assert(st.size < st.max_size);

        t->spindown = true;
    }
}

START_TEST(pcr_jump)
{
    jump_test_t t;
    memset(&t, 0, sizeof(t));

    ts_sync_t *sx = ts_sync_init(jump_on_ts, &t);
    asc_timer_t *loop = asc_timer_init(SYNC_INTERVAL_MSEC, ts_sync_loop, sx);
    ts_sync_set_on_ready(sx, jump_on_ready);
    ck_assert(ts_sync_set_blocks(sx, 7, 2) == true);

    t.sx = sx;

    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    ck_assert(t.rx_pcr > 0);
    ck_assert(t.tx_packets > 0);
    ck_assert(t.tx_bogus > 0);
    ck_assert(t.rx_pre > 0);
    ck_assert(t.rx_post > 0);
    ck_assert(t.spindown == true);

    ASC_FREE(loop, asc_timer_destroy);
    ASC_FREE(sx, ts_sync_destroy);
}
END_TEST

/* bitrate anomalies not normally encountered in the real world */
#define OUTER_CASE_HUGE 0
#define OUTER_RATE_HUGE 40608000000.0 /* ~40.6 Gbps */
#define OUTER_CASE_TINY 1
#define OUTER_STEP_TINY (((TS_PCR_FREQ * 150) / 1000) - 1)
#define OUTER_RATE_TINY 10026.0 /* ~10 Kbps */

typedef struct
{
    ts_sync_t *sx;
    uint64_t rx_pcr;
    size_t rx_tiny;
    unsigned int idx;
} outer_test_t;

static
void outer_on_ts(void *arg, const uint8_t ts[TS_PACKET_SIZE])
{
    outer_test_t *const t = (outer_test_t *)arg;

    ck_assert(TS_GET_PID(ts) == GEN_PCR_PID);
    ck_assert(!TS_IS_PAYLOAD(ts) && TS_IS_PCR(ts));

    ts_sync_stat_t st;
    ts_sync_query(t->sx, &st);

    const uint64_t pcr = TS_GET_PCR(ts);
    if (t->idx == OUTER_CASE_HUGE)
    {
        ck_assert(st.bitrate > (OUTER_RATE_HUGE * 0.9995)
                  && st.bitrate < (OUTER_RATE_HUGE * 1.0005));

        ck_assert(pcr == t->rx_pcr);
        t->rx_pcr = pcr + 1;
    }
    else if (t->idx == OUTER_CASE_TINY)
    {
        ck_assert(st.bitrate > (OUTER_RATE_TINY * 0.9995)
                  && st.bitrate < (OUTER_RATE_TINY * 1.0005));

        ck_assert(pcr == t->rx_pcr);
        t->rx_pcr = pcr + OUTER_STEP_TINY;
        t->rx_tiny++;
    }
    else
    {
        ck_abort_msg("didn't expect to reach this code");
    }
}

START_TEST(outer_limits)
{
    outer_test_t t;
    memset(&t, 0, sizeof(t));

    ts_sync_t *sx = ts_sync_init(outer_on_ts, &t);
    ck_assert(sx != NULL);
    ck_assert(ts_sync_set_blocks(sx, 2, 2) == true);
    t.sx = sx;

    ts_sync_loop(sx); /* establish time base */
    asc_usleep(25000);

    /*
     * Case A: put PCR in every packet, increment it by 1 each time.
     * Resulting bitrate should be around 40 Gbps.
     */
    t.idx = OUTER_CASE_HUGE;

    uint8_t ts_tpl[TS_PACKET_SIZE];
    TS_INIT(ts_tpl);
    TS_SET_AF(ts_tpl, TS_BODY_SIZE - 1);

    for (unsigned int i = 0; i < 1000; i++)
    {
        uint8_t ts[TS_PACKET_SIZE];
        memcpy(ts, ts_tpl, TS_PACKET_SIZE);

        TS_SET_PID(ts, GEN_PCR_PID);
        TS_SET_PCR(ts, i);

        ck_assert(ts_sync_push(sx, ts, 1) == true);
    }

    ts_sync_stat_t st;
    ts_sync_query(sx, &st);
    ck_assert(st.num_blocks >= st.enough_blocks);
    ck_assert(st.bitrate > -0.1 && st.bitrate < 0.1);
    ck_assert(st.filled == 1000 && st.want == 0);

    /* expect buffer to spit everything out in one go */
    ts_sync_loop(sx);
    ck_assert(t.rx_pcr == 1000);

    ts_sync_query(sx, &st);
    ck_assert(st.num_blocks == 0);
    ck_assert(st.bitrate > -0.1 && st.bitrate < 0.1);
    ck_assert(st.filled == 0 && st.want > 0);

    /*
     * Case B: increment PCR by 150ms (maximum allowed delta).
     * This should result in a bitrate of app. 10 Kbps.
     */
    t.idx = OUTER_CASE_TINY;

    uint64_t pcr = t.rx_pcr;
    for (unsigned int i = 0; i < 1000; i++)
    {
        uint8_t ts[TS_PACKET_SIZE];
        memcpy(ts, ts_tpl, TS_PACKET_SIZE);

        TS_SET_PID(ts, GEN_PCR_PID);
        TS_SET_PCR(ts, pcr);
        pcr += OUTER_STEP_TINY;

        ck_assert(ts_sync_push(sx, ts, 1) == true);
    }

    const uint64_t time_start = asc_utime();
    while (asc_utime() - time_start < 1000000)
    {
        /* spin the CPU for better timing accuracy */
        ts_sync_loop(sx);
    }

    const unsigned int clk_br = t.rx_tiny * TS_PACKET_SIZE * 8;
    asc_log_debug("outer: tiny br = %u bps", clk_br);

    ck_assert(clk_br > (OUTER_RATE_TINY * 0.9)
              && clk_br < (OUTER_RATE_TINY * 1.1));

    ASC_FREE(sx, ts_sync_destroy);
}
END_TEST

/* large delay between dequeue calls */
typedef struct
{
    ts_sync_t *sx;
    ts_generator_t gen;
    size_t rx_cnt;
} delay_test_t;

static
void delay_on_ts(void *arg, const uint8_t *ts)
{
    delay_test_t *const t = (delay_test_t *)arg;

    ck_assert(TS_IS_SYNC(ts));
    t->rx_cnt++;
}

START_TEST(time_travel)
{
    delay_test_t t;
    memset(&t, 0, sizeof(t));

    ts_sync_t *sx = ts_sync_init(delay_on_ts, &t);
    ck_assert(sx != NULL);
    ts_sync_set_blocks(sx, 2, 2);

    ts_sync_stat_t st, def;
    ts_sync_query(sx, &st);
    ck_assert(st.enough_blocks == 2 && st.low_blocks == 2);
    ck_assert(st.filled == 0 && st.want > 0);
    ck_assert(st.bitrate > -0.1 && st.bitrate < 0.1);
    ck_assert(st.size > 0);
    memcpy(&def, &st, sizeof(st));
    const size_t def_size = st.size;

    unsigned int pushed = 0;
    while (pushed < 50)
    {
        uint8_t ts[TS_PACKET_SIZE];

        if (ts_generator(&t.gen, ts))
        {
            ck_assert(ts_sync_push(sx, ts, 1) == true);
        }
        else
        {
            t.gen.bitrate = 10000000;
            t.gen.duration = 35;

            pushed++;
        }
    }

    /* NOTE: bitrate is not updated during initial buffering */
    ts_sync_query(sx, &st);
    ck_assert(st.num_blocks > 0);
    ck_assert(st.filled > 0 && st.want == 0);
    ck_assert(st.bitrate > -0.1 && st.bitrate < 0.1);
    ck_assert(st.size > def_size);

    while (t.rx_cnt == 0)
    {
        ts_sync_loop(sx);
        asc_usleep(5000); /* 5ms */
    }

    ts_sync_query(sx, &st);
    ck_assert(st.num_blocks > 0);
    ck_assert(st.bitrate > 9999980.0 && st.bitrate < 10000020.0);

    asc_usleep(1500000); /* 1.5s */
    ts_sync_loop(sx);

    ts_sync_query(sx, &st);
    ck_assert(!memcmp(&st, &def, sizeof(st)));

    ASC_FREE(sx, ts_sync_destroy);
}
END_TEST

/* queue packets when buffer requests more data */
#define PULL_MAX_RATE 100000000 /* 100 Mbit */
#define PULL_MIN_RATE 1000000 /* 1 Mbit */
#define PULL_MIN_PCR 5 /* 5 ms */
#define PULL_MAX_PCR 100 /* 100 ms */
#define PULL_DURATION 4000 /* total TS duration no more than 4 seconds */
#define PULL_BENCH_COUNT 2500 /* max. 2500 PCR measurements */
#define PULL_LOW_THRESH 2 /* buffer won't dequeue last 2 blocks */

typedef struct
{
    uint32_t cfg_br;
    uint32_t cfg_ms;

    double pcr_br;
    double clk_br;
    double br_drift;

    unsigned int pcr_us;
    unsigned int clk_us;
    unsigned int us_drift;
} pull_bench_t;

typedef struct
{
    ts_sync_t *sx;
    ts_generator_t gen;

    uint64_t pcr_val;
    uint64_t pcr_time;
    unsigned int offset;

    pull_bench_t bench[PULL_BENCH_COUNT];
    size_t tx_idx;
    size_t rx_idx;
    unsigned int duration;
    bool spindown;

    unsigned int cc;
} pull_test_t;

static
void pull_on_ready(void *arg)
{
    pull_test_t *const t = (pull_test_t *)arg;

    if (t->spindown)
    {
        ts_sync_set_on_ready(t->sx, NULL);
        asc_main_loop_shutdown();

        return;
    }

    ts_sync_stat_t st;
    ts_sync_query(t->sx, &st);
    ck_assert(st.want > 0);

    while (st.want > 0)
    {
        /* NOTE: zero-packet push is expected to work correctly */
        const size_t portion = rand() % (st.want + 1);

        ts_packet_t *const pkts = ASC_ALLOC(portion, ts_packet_t);
        ck_assert(pkts != NULL);

        for (size_t i = 0; i < portion;)
        {
            if (ts_generator(&t->gen, pkts[i]))
            {
                i++;
            }
            else
            {
                t->gen.bitrate = PULL_MIN_RATE;
                t->gen.bitrate += rand() % (PULL_MAX_RATE - PULL_MIN_RATE);
                ck_assert(t->gen.bitrate >= PULL_MIN_RATE
                          && t->gen.bitrate <= PULL_MAX_RATE);

                t->gen.duration = PULL_MIN_PCR;
                t->gen.duration += rand() % (PULL_MAX_PCR - PULL_MIN_PCR);
                ck_assert(t->gen.duration >= PULL_MIN_PCR
                          && t->gen.duration <= PULL_MAX_PCR);

                pull_bench_t *const b = &t->bench[t->tx_idx];
                b->cfg_br = t->gen.bitrate;
                b->cfg_ms = t->gen.duration;

                t->tx_idx++;
                t->duration += t->gen.duration;
            }
        }

        ck_assert(ts_sync_push(t->sx, pkts, portion) == true);
        free(pkts);

        st.want -= portion;
    }
}

static
void pull_on_ts(void *arg, const uint8_t *ts)
{
    pull_test_t *const t = (pull_test_t *)arg;

    if (!t->spindown
        && (t->tx_idx >= PULL_BENCH_COUNT || t->duration >= PULL_DURATION))
    {
        asc_log_debug("ts_pull: queued %zu blocks, total duration %ums"
                      , t->tx_idx, t->duration);

        t->spindown = true;
    }

    t->offset += TS_PACKET_SIZE;

    ck_assert(TS_IS_SYNC(ts));
    const unsigned int pid = TS_GET_PID(ts);

    if (pid == GEN_DATA_PID)
    {
        ck_assert(TS_IS_PAYLOAD(ts));
        unsigned int cc = TS_GET_CC(ts);

        ck_assert(t->cc == cc);
        t->cc = (cc + 1) & 0xf;
    }
    else if (pid == GEN_PCR_PID)
    {
        ck_assert(TS_IS_PCR(ts) && !TS_IS_PAYLOAD(ts));
        const bool seq = (t->pcr_val != 0);

        /* PCR time difference */
        const uint64_t pcr_now = TS_GET_PCR(ts);
        const uint64_t pcr_delta = TS_PCR_DELTA(t->pcr_val, pcr_now);
        const int pcr_timediff = pcr_delta / (TS_PCR_FREQ / 1000000);
        t->pcr_val = pcr_now;

        /* system clock time difference */
        const uint64_t clk_now = asc_utime();
        ck_assert(clk_now >= t->pcr_time);
        const int clk_timediff = clk_now - t->pcr_time;
        t->pcr_time = clk_now;

        /* PCR vs. clock time drift */
        int time_drift = pcr_timediff - clk_timediff;
        if (time_drift < 0)
            time_drift = -time_drift;

        if (seq)
        {
            ck_assert(clk_timediff < 1000000);
            ck_assert(pcr_timediff < 1000000);
            ck_assert(time_drift < 1000000);

            /* PCR bitrate */
            const double pcr_br =
                ((double)t->offset * 8 * TS_PCR_FREQ) / pcr_delta;

            /* system clock bitrate */
            double clk_br = 0.0;
            if (clk_timediff > 0)
                clk_br = ((double)t->offset * 8 * 1000000) / clk_timediff;

            /* PCR vs. clock bitrate drift */
            double br_drift = pcr_br - clk_br;
            if (br_drift < 0.0)
                br_drift = -br_drift;

            ck_assert(t->rx_idx <= t->tx_idx);
            pull_bench_t *const b = &t->bench[t->rx_idx++];
            ck_assert(b->cfg_br > 0 && b->cfg_ms > 0);

            b->pcr_br = pcr_br;
            b->clk_br = clk_br;
            b->br_drift = br_drift;
            b->pcr_us = pcr_timediff;
            b->clk_us = clk_timediff;
            b->us_drift = time_drift;
        }

        t->offset = 0;
    }
    else
    {
        ck_abort_msg("unknown PID: %u", pid);
    }
}

START_TEST(ts_pull)
{
    pull_test_t t;
    memset(&t, 0, sizeof(t));

    ts_sync_t *sx = ts_sync_init(pull_on_ts, &t);
    asc_timer_t *dequeue = asc_timer_init(1, ts_sync_loop, sx);

    ts_sync_set_max_size(sx, 64);
    ts_sync_set_blocks(sx, 8, PULL_LOW_THRESH);
    ts_sync_set_on_ready(sx, pull_on_ready);
    t.sx = sx;

    const uint64_t time_a = asc_utime();
    const bool again = asc_main_loop_run();
    ck_assert(again == false);

    const uint64_t time_b = asc_utime();
    ck_assert(time_b > time_a);

    ck_assert(t.pcr_val > 0);
    ck_assert(t.pcr_time > time_a);
    ck_assert(t.tx_idx > 0 && t.rx_idx > 0);
    ck_assert(t.tx_idx >= t.rx_idx);
    ck_assert(t.duration > 0);
    ck_assert(t.spindown == true);

    for (size_t i = t.rx_idx; i < t.tx_idx; i++)
    {
        t.duration -= t.bench[i].cfg_ms;
    }

    const int diff = (time_b - time_a) / 1000;
    const int drift = diff - t.duration;

    asc_log_debug("ts_pull: PCR duration %ums, took %ums to dequeue (%s%dms)"
                  , t.duration, diff, ((drift > 0) ? "+" : ""), drift);

    ck_assert(drift <= 500);

    unsigned int pass = 0, fail = 0;
    unsigned int pass_ms = 0, fail_ms = 0;
    unsigned int pass_br = 0, fail_br = 0;
    unsigned int pass_us = 0, fail_us = 0;
    unsigned int pass_tol = 0, fail_tol = 0;

    for (size_t i = 0; i < t.rx_idx; i++)
    {
        pull_bench_t *const b = &t.bench[i];

        ck_assert(b->cfg_br > 0 && b->pcr_br > 0.0);
        ck_assert(b->cfg_ms > 0 && b->pcr_us > 0);

        /* duration cfg vs. pcr */
        const int ms_pc = (b->pcr_us / 1000) - b->cfg_ms;
        if (ms_pc > -5 && ms_pc < 5)
            { pass++; pass_ms++; } else { fail++; fail_ms++; }

        /* bitrate cfg vs. pcr */
        const double br_pc = b->pcr_br - (double)b->cfg_br;
        if (br_pc > -1000.0 && br_pc < 1000.0)
            { pass++; pass_br++; } else { fail++; fail_br++; }

        /* duration drift clk vs. pcr */
        if (b->us_drift < 15000) /* 15 ms */
            { pass++; pass_us++; } else { fail++; fail_us++; }

        /* bitrate drift clk vs. pcr */
        const double tolerance = b->pcr_br * 0.25; /* 25% */
        if (b->br_drift < tolerance)
            { pass++; pass_tol++; } else { fail++; fail_tol++; }
    }

    const double rate = ((double)pass / (pass + fail)) * 100.0;
    asc_log_debug("ts_pull: total stats: %u/%u (%.2f%%)"
                  , pass, pass + fail, rate);

    const double rate_ms = ((double)pass_ms / (pass_ms + fail_ms)) * 100.0;
    asc_log_debug("ts_pull: duration cfg vs. pcr: %u/%u (%.2f%%)"
                  , pass_ms, pass_ms + fail_ms, rate_ms);

    const double rate_br = ((double)pass_br / (pass_br + fail_br)) * 100.0;
    asc_log_debug("ts_pull: bitrate cfg vs. pcr: %u/%u (%.2f%%)"
                  , pass_br, pass_br + fail_br, rate_br);

    const double rate_us = ((double)pass_us / (pass_us + fail_us)) * 100.0;
    asc_log_debug("ts_pull: duration clock vs. pcr: %u/%u (%.2f%%)"
                  , pass_us, pass_us + fail_us, rate_us);

    const double rate_tol = ((double)pass_tol / (pass_tol + fail_tol)) * 100.0;
    asc_log_debug("ts_pull: bitrate clock vs. pcr: %u/%u (%.2f%%)"
                  , pass_tol, pass_tol + fail_tol, rate_tol);

    const unsigned int res = get_timer_res();
    if (res <= 10000) /* 10ms or better */
    {
        ck_assert(rate > 80.0);
        ck_assert(rate_us > 70.0);
        ck_assert(rate_tol > 65.0);
    }
    else
    {
        asc_log_debug("ts_pull: system clock resolution is too low, "
                      "won't check timing accuracy");
    }

    ck_assert(rate_ms > 95.0);
    ck_assert(rate_br > 95.0);

    ASC_FREE(dequeue, asc_timer_destroy);
    ASC_FREE(sx, ts_sync_destroy);
}
END_TEST

/* push whole test stream at once, measure dequeue time */
#define BENCH_BITRATE 10000000 /* 10 Mbps */
#define BENCH_DURATION 4000 /* push 4 second long stream */
#define BENCH_PCR_INTERVAL 20 /* 20 ms */
#define BENCH_LOW_THRESH 2

typedef struct
{
    ts_sync_t *sx;
    ts_generator_t gen;

    uint64_t pcr_time;
    uint64_t pcr_val;
    size_t rx_idx;
    size_t tx_idx;
    unsigned int pass;
    unsigned int fail;
    unsigned int pcr_duration;
    unsigned int clk_duration;
    unsigned int offset;
    unsigned int rx_cc;
    double pcr_bits;
    double clk_bits;

    bool running;
} bench_test_t;

static
void bench_on_ts(void *arg, const uint8_t *ts)
{
    bench_test_t *const t = (bench_test_t *)arg;

    if (!t->running)
        return;

    ck_assert(TS_IS_SYNC(ts));
    const unsigned int pid = TS_GET_PID(ts);

    t->offset += TS_PACKET_SIZE;

    if (pid == GEN_DATA_PID)
    {
        ck_assert(TS_IS_PAYLOAD(ts));
        const unsigned int cc = TS_GET_CC(ts);

        ck_assert(cc == t->rx_cc);
        t->rx_cc = (cc + 1) & 0xf;
    }
    else if (pid == GEN_PCR_PID)
    {
        ck_assert(TS_IS_PCR(ts) && !TS_IS_PAYLOAD(ts));

        const uint64_t now = asc_utime();
        const uint64_t pcr = TS_GET_PCR(ts);

        if (t->pcr_val > 0 && t->pcr_time > 0)
        {
            const unsigned int pcr_delta = TS_PCR_DELTA(t->pcr_val, pcr);
            ck_assert(pcr_delta > 0);

            const int pcr_timediff = pcr_delta / (TS_PCR_FREQ / 1000000);
            const int clk_timediff = now - t->pcr_time;
            ck_assert(pcr_timediff > 0);
            t->pcr_duration += pcr_timediff;
            t->clk_duration += clk_timediff;

            const double pcr_bitrate =
                ((double)t->offset * 8 * TS_PCR_FREQ) / pcr_delta;

            double clk_bitrate = 0.0;
            if (clk_timediff > 0)
                clk_bitrate = ((double)t->offset * 8 * 1000000) / clk_timediff;

            t->pcr_bits += pcr_bitrate;
            t->clk_bits += clk_bitrate;

            const int time_drift = pcr_timediff - clk_timediff;
            if (time_drift > -1500 && time_drift < 1500) /* 1.5 ms */
                { t->pass++; } else { t->fail++; }

            const double rate_drift = pcr_bitrate - clk_bitrate;
            if (rate_drift > -500000.0 && rate_drift < 500000.0) /* 500 Kbps */
                { t->pass++; } else { t->fail++; }

            if (++t->rx_idx >= t->tx_idx - BENCH_LOW_THRESH)
            {
                t->running = false;
                return;
            }
        }

        t->pcr_time = now;
        t->pcr_val = pcr;

        t->offset = 0;
    }
    else
    {
        ck_abort_msg("unknown PID: %u", pid);
    }
}

START_TEST(ts_bench)
{
    bench_test_t t;
    memset(&t, 0, sizeof(t));

    ts_sync_t *sx = ts_sync_init(bench_on_ts, &t);
    ck_assert(sx != NULL);

    ts_sync_set_blocks(sx, BENCH_LOW_THRESH, BENCH_LOW_THRESH);
    ts_sync_set_max_size(sx, 64);

    const size_t blocks = (BENCH_DURATION / BENCH_PCR_INTERVAL) + 2 + 1;
    unsigned int duration = 0;

    while (t.tx_idx < blocks)
    {
        uint8_t ts[TS_PACKET_SIZE];

        if (ts_generator(&t.gen, ts))
        {
            ck_assert(ts_sync_push(sx, ts, 1) == true);
        }
        else
        {
            t.gen.bitrate = BENCH_BITRATE;
            t.gen.duration = BENCH_PCR_INTERVAL;

            if (t.tx_idx++ < blocks - 3)
                duration += t.gen.duration;
        }
    }

    asc_log_debug("ts_bench: queued %zu blocks, spinning for %ums"
                  , t.tx_idx, duration);

    t.sx = sx;
    t.running = true;

    const uint64_t time_a = asc_utime();
    while (t.running)
    {
        /* spin the CPU for better timing accuracy */
        ts_sync_loop(sx);
    }
    const uint64_t time_b = asc_utime();

    ck_assert(time_b > time_a);
    const int diff = time_b - time_a;
    asc_log_debug("ts_bench: time elapsed: %dus", diff);

    const double pass_rate = ((double)t.pass / (t.pass + t.fail)) * 100.0;
    asc_log_debug("ts_bench: pass rate %.2f%% (%u/%u)"
                  , pass_rate, t.pass, t.pass + t.fail);

    const int cfg_drift = duration - (t.clk_duration / 1000);
    asc_log_debug("ts_bench: configured for %ums, got %ums (%s%dms)"
                  , duration, t.clk_duration / 1000
                  , (((t.clk_duration / 1000) > duration) ? "+" : "")
                  , (t.clk_duration / 1000) - duration);

    const int pcr_clk_drift = t.pcr_duration - t.clk_duration;
    asc_log_debug("ts_bench: pcr_clk_drift: %dus", pcr_clk_drift);

    const int clk_diff = diff - t.clk_duration;
    asc_log_debug("ts_bench: clk_diff: %dus", clk_diff);

    const int pcr_diff = diff - t.pcr_duration;
    asc_log_debug("ts_bench: pcr_diff: %dus", pcr_diff);

    /* check average bitrate */
    t.clk_bits /= t.rx_idx;
    t.pcr_bits /= t.rx_idx;
    asc_log_debug("avg rate: clk: %.2f, pcr: %.2f\n", t.clk_bits, t.pcr_bits);

    const unsigned int res = get_timer_res();
    if (res <= 10000) /* 10ms or better */
    {
        const double lo = BENCH_BITRATE * 0.9;
        const double hi = BENCH_BITRATE * 1.1;

        ck_assert((t.clk_bits >= lo && t.clk_bits <= hi)
                  && (t.pcr_bits >= lo && t.pcr_bits <= hi));

        ck_assert(pass_rate >= 80.0);
    }

    /* 50 ms tolerance */
    ck_assert(cfg_drift > -50 && cfg_drift < 50);
    ck_assert(pcr_clk_drift > -50000 && pcr_clk_drift < 50000);
    ck_assert(clk_diff > -50000 && clk_diff < 50000);
    ck_assert(pcr_diff > -50000 && pcr_diff < 50000);

    ASC_FREE(sx, ts_sync_destroy);
}
END_TEST

Suite *mpegts_sync(void)
{
    Suite *const s = suite_create("mpegts/sync");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, lib_setup, lib_teardown);

    if (can_fork != CK_NOFORK)
        tcase_set_timeout(tc, 10);

    tcase_add_test(tc, setters);
    tcase_add_test(tc, no_pcr);
    tcase_add_test(tc, sys_clock);
    tcase_add_test(tc, underflow);
    tcase_add_test(tc, pcr_still);
    tcase_add_test(tc, pcr_jump);
    tcase_add_test(tc, outer_limits);
    tcase_add_test(tc, time_travel);
    tcase_add_test(tc, ts_pull);
    tcase_add_test(tc, ts_bench);

    suite_add_tcase(s, tc);

    return s;
}
