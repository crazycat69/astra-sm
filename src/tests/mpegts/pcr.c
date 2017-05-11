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
#include <astra/mpegts/pcr.h>

ASC_STATIC_ASSERT(TS_PCR_FREQ == 27000000);
ASC_STATIC_ASSERT(TS_PCR_MAX == 2576980377600LL);
ASC_STATIC_ASSERT(TS_TIME_NONE > TS_PCR_MAX);

static
void setup(void)
{
    asc_srand();
}

/* write and retrieve PCR value */
START_TEST(get_set)
{
    uint8_t ts[TS_PACKET_SIZE];

    TS_INIT(ts);
    ck_assert(TS_IS_SYNC(ts));
    TS_SET_AF(ts, TS_BODY_SIZE - 1);
    ck_assert(TS_IS_AF(ts));
    ck_assert(ts[5] == 0);
    for (size_t i = 6; i < TS_PACKET_SIZE; i++)
        ck_assert(ts[i] == 0xff);

    int mul = 1;
    if (TS_PCR_FREQ > RAND_MAX)
        mul = TS_PCR_FREQ / RAND_MAX;

    for (uint64_t val = 0; val < TS_PCR_MAX
         ; val += (rand() % TS_PCR_FREQ) * mul)
    {
        ck_assert(!TS_IS_PCR(ts));
        TS_SET_PCR(ts, val);
        ck_assert(TS_IS_PCR(ts));
        const uint64_t pcr = TS_GET_PCR(ts);
        ck_assert(val == pcr);

        /* NOTE: only the PCR flag is cleared, not the timestamp itself */
        TS_CLEAR_PCR(ts);
        const uint64_t pcr2 = TS_GET_PCR(ts);
        ck_assert(pcr2 == pcr);
    }
}
END_TEST

/* PCR wrapover test */
START_TEST(delta)
{
    uint64_t total = 0, add = 0, pa = 0, pb = 0;

    while (total < TS_PCR_MAX * 5)
    {
        const uint64_t diff = TS_PCR_DELTA(pa, pb);
        ck_assert(diff == add);
        pa = pb;

        add = TS_PCR_FREQ * (1 + (rand() % 10));
        add += rand() % TS_PCR_FREQ;
        total += add;

        pb += add;
        pb %= TS_PCR_MAX;
    }
}
END_TEST

/* interval to packet count */
START_TEST(interval)
{
    for (unsigned int rate = 10000; rate <= 100000000; rate += 1000)
    {
        const unsigned int pkt = TS_PCR_PACKETS(1000, rate);
        const unsigned int bits = pkt * TS_PACKET_SIZE * 8;

        ck_assert(bits <= rate);
        ck_assert((rate - bits) <= (TS_PACKET_SIZE * 8));
    }

    static const unsigned int tests[][3] =
    {
        { 35, 1000000,   23  },
        { 1,  10000000,  6   },
        { 90, 500000,    29  },
        { 25, 15000000,  249 },
        { 10, 90000000,  598 },
        { 5,  2000000,   6   },
        { 13, 3000000,   25  },
        { 31, 150000,    3   },
        { 21, 392000,    5   },
        { 39, 4500000,   116 },
    };

    for (size_t i = 0; i < ASC_ARRAY_SIZE(tests); i++)
    {
        const unsigned int ms = tests[i][0];
        const unsigned int rate = tests[i][1];
        const unsigned int pkt = tests[i][2];

        const unsigned int pkt_got = TS_PCR_PACKETS(ms, rate);
        ck_assert(pkt_got == pkt);
    }
}
END_TEST

/* PCR (re)stamping formula */
START_TEST(calc)
{
    for (int rate = 500000; rate <= 100000000; rate += 250000)
    {
        const int ms = 5 + (rand() % 95); /* PCR interval 5-99 ms */
        unsigned int pkt = TS_PCR_PACKETS(ms, rate);
        ck_assert(pkt > 0);

        /* simulate packet offset */
        uint32_t offset = 0;
        const uint64_t pcr_a = TS_PCR_CALC(offset, rate);
        ck_assert(pcr_a == 0);

        while (pkt-- > 0)
            offset += TS_PACKET_SIZE;

        const uint64_t pcr_b = TS_PCR_CALC(offset, rate);
        ck_assert(pcr_b > pcr_a);

        /* check PCR delta vs configured interval */
        const uint64_t delta = TS_PCR_DELTA(pcr_a, pcr_b);
        const int pcr_ms = delta / (TS_PCR_FREQ / 1000);

        int drift = ms - pcr_ms;
        if (drift < 0)
            drift = -drift;

        ck_assert(drift >= 0 && drift <= 3);

        /* check PCR bitrate */
        const int pcr_rate = offset * ((double)TS_PCR_FREQ / delta) * 8;

        drift = rate - pcr_rate;
        if (drift < 0)
            drift = -drift;

        ck_assert(drift >= 0 && drift <= 1000);
    }
}
END_TEST

/* pre-defined test packets */
typedef struct
{
    ts_packet_t data;

    struct
    {
        uint64_t value;
        bool present;
    } pcr;
} pcr_test_t;

#include "pcr_packets.h"

START_TEST(test_vectors)
{
    for (size_t i = 0; i < ASC_ARRAY_SIZE(test_packets); i++)
    {
        const pcr_test_t *const t = &test_packets[i];

        ck_assert(TS_IS_PCR(t->data) == t->pcr.present);
        const uint64_t pcr = TS_GET_PCR(t->data);
        ck_assert(pcr == t->pcr.value);

        uint8_t ts[TS_PACKET_SIZE];
        TS_SET_PCR(ts, pcr);
        ck_assert(TS_GET_PCR(ts) == t->pcr.value);
    }
}
END_TEST

Suite *mpegts_pcr(void)
{
    Suite *const s = suite_create("mpegts/pcr");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, setup, NULL);

    tcase_add_test(tc, get_set);
    tcase_add_test(tc, delta);
    tcase_add_test(tc, interval);
    tcase_add_test(tc, calc);
    tcase_add_test(tc, test_vectors);

    suite_add_tcase(s, tc);

    return s;
}
