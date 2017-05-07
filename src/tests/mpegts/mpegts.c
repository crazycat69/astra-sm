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

ASC_STATIC_ASSERT(TS_PACKET_SIZE == 188);
ASC_STATIC_ASSERT(TS_PACKET_BITS == 1504);
ASC_STATIC_ASSERT(TS_HEADER_SIZE == 4);
ASC_STATIC_ASSERT(TS_BODY_SIZE == 184);
ASC_STATIC_ASSERT(TS_MAX_PIDS == 8192);
ASC_STATIC_ASSERT(TS_MAX_PROGS == 65536);
ASC_STATIC_ASSERT(TS_NULL_PID == 0x1fff);

ASC_STATIC_ASSERT(sizeof(ts_packet_t) == TS_PACKET_SIZE);

/* test bounds checking functions */
START_TEST(pid_pnr_range)
{
    /* PID: 0 to 8191 */
    for (int i = INT16_MIN; i < INT16_MAX; i++)
    {
        const bool expect = !(i < 0 || i > TS_NULL_PID);
        ck_assert(ts_pid_valid(i) == expect);
    }

    ck_assert(ts_pid_valid(-1) == false);
    ck_assert(ts_pid_valid(0) == true);
    ck_assert(ts_pid_valid(8191) == true);
    ck_assert(ts_pid_valid(8192) == false);

    /* program number: 1 to 65535 */
    for (int i = INT16_MIN * 2; i < INT16_MAX * 2; i++)
    {
        const bool expect = !(i < 1 || i >= TS_MAX_PROGS);
        ck_assert(ts_pnr_valid(i) == expect);
    }

    ck_assert(ts_pnr_valid(0) == false);
    ck_assert(ts_pnr_valid(1) == true);
    ck_assert(ts_pnr_valid(65535) == true);
    ck_assert(ts_pnr_valid(65536) == false);
}
END_TEST

/* basic TS header manipulation */
START_TEST(ts_header)
{
    uint8_t ts[TS_PACKET_SIZE];
    memset(ts, 0xff, sizeof(ts));

    /* initializer macro */
    ck_assert(!TS_IS_SYNC(ts));
    TS_INIT(ts);
    ck_assert(TS_IS_SYNC(ts));
    ck_assert(ts[0] == 0x47);
    ck_assert(ts[1] == 0x0);
    ck_assert(ts[2] == 0x0);
    ck_assert(ts[3] == 0x0);

    /* NOTE: only the first 4 bytes are rewritten */
    for (size_t i = 4; i < TS_PACKET_SIZE; i++)
        ck_assert(ts[i] == 0xff);

    memset(&ts[TS_HEADER_SIZE], 0, TS_BODY_SIZE);

    uint8_t orig_ts[TS_PACKET_SIZE];
    memcpy(orig_ts, ts, TS_PACKET_SIZE);

    /* transport error indicator */
    ck_assert(!TS_IS_ERROR(ts) && ts[1] == 0x0);
    TS_SET_ERROR(ts, true);
    ck_assert(TS_IS_ERROR(ts) && ts[1] == 0x80);
    TS_SET_ERROR(ts, false);
    ck_assert(!TS_IS_ERROR(ts) && ts[1] == 0x0);
    ck_assert(!memcmp(ts, orig_ts, TS_PACKET_SIZE));

    /* payload presence bit */
    ck_assert(!TS_IS_PAYLOAD(ts) && ts[3] == 0x0);
    TS_SET_PAYLOAD(ts, true);
    ck_assert(TS_IS_PAYLOAD(ts) && ts[3] == 0x10);
    TS_SET_PAYLOAD(ts, false);
    ck_assert(!TS_IS_PAYLOAD(ts) && ts[3] == 0x0);
    ck_assert(!memcmp(ts, orig_ts, TS_PACKET_SIZE));

    /* payload unit start indicator */
    ck_assert(!TS_IS_PUSI(ts) && ts[1] == 0x0);
    TS_SET_PUSI(ts, true);
    ck_assert(ts[1] == 0x40);
    ck_assert(!TS_IS_PUSI(ts)); /* no payload bit */
    TS_SET_PAYLOAD(ts, true);
    ck_assert(TS_IS_PUSI(ts)); /* should return true now */
    TS_SET_PUSI(ts, false);
    ck_assert(!TS_IS_PUSI(ts) && ts[1] == 0x0);
    TS_SET_PAYLOAD(ts, false);
    ck_assert(!memcmp(ts, orig_ts, TS_PACKET_SIZE));

    /* transport priority bit */
    ck_assert(!TS_IS_PRIORITY(ts) && ts[1] == 0x0);
    TS_SET_PRIORITY(ts, true);
    ck_assert(TS_IS_PRIORITY(ts) && ts[1] == 0x20);
    TS_SET_PRIORITY(ts, false);
    ck_assert(!TS_IS_PRIORITY(ts) && ts[1] == 0x0);
    ck_assert(!memcmp(ts, orig_ts, TS_PACKET_SIZE));

    /* packet identifier */
    ck_assert(TS_GET_PID(ts) == 0x0 && ts[1] == 0x0 && ts[2] == 0x0);
    TS_SET_PID(ts, 0x1234);
    ck_assert(TS_GET_PID(ts) == 0x1234 && ts[1] == 0x12 && ts[2] == 0x34);
    for (int i = INT16_MIN; i < INT16_MAX; i++)
    {
        TS_SET_PID(ts, i);
        const unsigned int ref = i & 0x1fff;
        ck_assert(TS_GET_PID(ts) == ref
                  && ts[1] == ((ref >> 8) & 0xff)
                  && ts[2] == (ref & 0xff));
    }
    TS_SET_PID(ts, 0x0);
    ck_assert(TS_GET_PID(ts) == 0x0 && ts[1] == 0x0 && ts[2] == 0x0);
    ck_assert(!memcmp(ts, orig_ts, TS_PACKET_SIZE));

    /* scrambling control */
    ck_assert(TS_GET_SC(ts) == TS_SC_NONE && ts[3] == 0x0);
    TS_SET_SC(ts, TS_SC_RESERVED);
    ck_assert(TS_GET_SC(ts) == TS_SC_RESERVED && ts[3] == 0x40);
    TS_SET_SC(ts, TS_SC_EVEN);
    ck_assert(TS_GET_SC(ts) == TS_SC_EVEN && ts[3] == 0x80);
    TS_SET_SC(ts, TS_SC_ODD);
    ck_assert(TS_GET_SC(ts) == TS_SC_ODD && ts[3] == 0xc0);
    for (int i = INT8_MIN; i < INT8_MAX; i++)
    {
        TS_SET_SC(ts, i);
        const unsigned int ref = i & 0x3;
        ck_assert(TS_GET_SC(ts) == ref && ts[3] == (ref << 6));
    }
    TS_SET_SC(ts, TS_SC_NONE);
    ck_assert(TS_GET_SC(ts) == TS_SC_NONE && ts[3] == 0x0);
    ck_assert(!memcmp(ts, orig_ts, TS_PACKET_SIZE));

    /* continuity counter */
    ck_assert(TS_GET_CC(ts) == 0x0 && ts[3] == 0x0);
    for (int i = INT8_MIN; i < INT8_MAX; i++)
    {
        TS_SET_CC(ts, i);
        const unsigned int ref = (i & 0xf);
        ck_assert(TS_GET_CC(ts) == ref && ts[3] == ref);
    }
    TS_SET_CC(ts, 0x0);
    ck_assert(TS_GET_CC(ts) == 0x0 && ts[3] == 0x0);
    ck_assert(!memcmp(ts, orig_ts, TS_PACKET_SIZE));

    /* adaptation field presence bit */
    ck_assert(!TS_IS_AF(ts) && ts[3] == 0x0);
    ck_assert(TS_AF_LEN(ts) == -1);
    memset(&ts[4], 0xff, 2);

    TS_SET_AF(ts, 0); /* presence only */
    ck_assert(TS_IS_AF(ts) && ts[3] == 0x20);
    ck_assert(TS_AF_LEN(ts) == 0 && ts[4] == 0);
    ck_assert(ts[5] == 0xff); /* no flags */

    TS_SET_AF(ts, 1); /* presence and flags */
    ck_assert(TS_IS_AF(ts) && ts[3] == 0x20);
    ck_assert(TS_AF_LEN(ts) == 1 && ts[4] == 1);
    ck_assert(ts[5] == 0x0); /* flags set to zero */

    TS_SET_AF(ts, 10); /* presence, flags and stuffing */
    ck_assert(TS_IS_AF(ts) && ts[3] == 0x20);
    ck_assert(TS_AF_LEN(ts) == 10 && ts[4] == 10);
    ck_assert(ts[5] == 0x0); /* flags set to zero */

    for (size_t i = 6; i < 15; i++)
        ck_assert(ts[i] == 0xff); /* stuffing (0xff), 9 bytes */

    TS_SET_PAYLOAD(ts, true);
    ck_assert(TS_IS_AF(ts) && TS_IS_PAYLOAD(ts) && ts[3] == 0x30);
    ck_assert(ts[15] == 0x0); /* payload starts here */
    ck_assert(TS_GET_PAYLOAD(ts) == &ts[15]);

    TS_CLEAR_AF(ts);
    TS_SET_PAYLOAD(ts, false);

    ck_assert(!TS_IS_AF(ts) && !TS_IS_PAYLOAD(ts) && ts[3] == 0x0);
    /* NOTE: only the AF presence bit is cleared (data is left intact) */
    ck_assert(ts[4] == 10 && ts[5] == 0x0 && ts[6] == 0xff);
}
END_TEST

/* adaptation field (AF) manipulation */
START_TEST(adaptation_field)
{
    uint8_t ts[TS_PACKET_SIZE] = { 0 };

    /* prepare test packet */
    TS_INIT(ts);
    ck_assert(TS_IS_SYNC(ts) == true);
    TS_SET_ERROR(ts, false);
    TS_SET_PAYLOAD(ts, true);
    TS_SET_PUSI(ts, true);
    TS_SET_PRIORITY(ts, false);
    TS_SET_PID(ts, 0x2fa);
    TS_SET_SC(ts, TS_SC_EVEN);
    TS_SET_CC(ts, 9);

    ck_assert(!TS_IS_ERROR(ts));
    ck_assert(TS_IS_PAYLOAD(ts));
    ck_assert(TS_IS_PUSI(ts));
    ck_assert(!TS_IS_PRIORITY(ts));
    ck_assert(TS_GET_PID(ts) == 0x2fa);
    ck_assert(TS_GET_SC(ts) == TS_SC_EVEN);
    ck_assert(TS_GET_CC(ts) == 9);

    /*
     * Set all supported AF flags, but not the AF presence bit. Expect the
     * flags to be ignored as ts[5] is technically a part of the payload.
     */
    ts[5] = 0xf0;
    ck_assert(!TS_IS_DISCONT(ts));
    ck_assert(!TS_IS_RANDOM(ts));
    ck_assert(!TS_IS_ES_PRIO(ts));
    ck_assert(!TS_IS_PCR(ts));

    /* setters don't check the AF bit */
    TS_SET_DISCONT(ts, false);
    ck_assert(ts[5] == 0x70);
    TS_SET_RANDOM(ts, false);
    ck_assert(ts[5] == 0x30);
    TS_SET_ES_PRIO(ts, false);
    ck_assert(ts[5] == 0x10);
    TS_CLEAR_PCR(ts);
    ck_assert(ts[5] == 0x00);

    ts[5] = 0xff;
    TS_SET_AF(ts, 7);
    ck_assert(TS_IS_AF(ts) && TS_AF_LEN(ts) == 7);
    ck_assert(ts[5] == 0x0);

    uint8_t ref_ts[] =
    {
        0x47, 0x42, 0xfa, 0xb9, /* TS header */
        0x07, /* AF length */
        0x00, /* AF flags */
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* AF stuffing */
        0x00, /* payload */
    };

    ck_assert(!memcmp(ts, ref_ts, sizeof(ref_ts)));

    /* discontinuity indicator */
    TS_SET_DISCONT(ts, true);
    ck_assert(TS_IS_DISCONT(ts) && ts[5] == 0x80);
    TS_SET_DISCONT(ts, false);
    ck_assert(!TS_IS_DISCONT(ts) && ts[5] == 0x0);

    /* random access indicator */
    TS_SET_RANDOM(ts, true);
    ck_assert(TS_IS_RANDOM(ts) && ts[5] == 0x40);
    TS_SET_RANDOM(ts, false);
    ck_assert(!TS_IS_RANDOM(ts) && ts[5] == 0x0);

    /* ES priority indicator */
    TS_SET_ES_PRIO(ts, true);
    ck_assert(TS_IS_ES_PRIO(ts) && ts[5] == 0x20);
    TS_SET_ES_PRIO(ts, false);
    ck_assert(!TS_IS_ES_PRIO(ts) && ts[5] == 0x0);

    /* PCR presence bit */
    ts[5] = 0x10;
    ck_assert(TS_IS_PCR(ts));
    TS_CLEAR_PCR(ts);
    ck_assert(ts[5] == 0x0);

    /* multiple flags */
    TS_SET_DISCONT(ts, true);
    TS_SET_RANDOM(ts, true);
    TS_SET_ES_PRIO(ts, true);
    ck_assert(ts[5] == 0xe0);

    ref_ts[5] = 0xa0;
    TS_SET_RANDOM(ts, false);
    ck_assert(!memcmp(ts, ref_ts, sizeof(ref_ts)));

    ck_assert(TS_IS_DISCONT(ts)
              && !TS_IS_RANDOM(ts)
              && TS_IS_ES_PRIO(ts));

    /* get payload */
    ts[13] = 0xca;
    ts[14] = 0xfe;

    const uint8_t *const pay = TS_GET_PAYLOAD(ts);
    ck_assert(pay != NULL && pay == &ts[12]);
    ck_assert(pay[0] == 0x00 && pay[1] == 0xca && pay[2] == 0xfe);

    /* 188 - 4 (header) - 1 (AF length field) - 7 (AF bytes) = 176 */
    ck_assert(ts_payload_len(ts, pay) == 176);
}
END_TEST

/* payload retrieval */
START_TEST(ts_payload)
{
    uint8_t buf[TS_PACKET_SIZE];

    TS_INIT(buf);
    ck_assert(TS_IS_SYNC(buf));

    /* no AF, no payload */
    ck_assert(!TS_IS_AF(buf) && !TS_IS_PAYLOAD(buf));
    const uint8_t *pay = TS_GET_PAYLOAD(buf);
    unsigned int len = ts_payload_len(buf, pay);
    ck_assert(pay == NULL && len == 0);

    /* no AF, payload only */
    TS_SET_PAYLOAD(buf, true);
    ck_assert(!TS_IS_AF(buf) && TS_IS_PAYLOAD(buf));
    pay = TS_GET_PAYLOAD(buf);
    len = ts_payload_len(buf, pay);
    ck_assert(pay != NULL && len == 184);
    intptr_t off = (intptr_t)pay - (intptr_t)buf;
    ck_assert(off == 4);

    for (unsigned int i = 0; i < 255; i++)
    {
        TS_INIT(buf);
        ck_assert(!TS_IS_AF(buf) && !TS_IS_PAYLOAD(buf));

        /* AF only, no payload */
        TS_SET_AF(buf, 0);
        ck_assert(TS_IS_AF(buf) && !TS_IS_PAYLOAD(buf));
        ck_assert(TS_AF_LEN(buf) == 0);

        buf[4] = i;
        buf[5] = 0x0;
        ck_assert(TS_AF_LEN(buf) == (int)i);

        pay = TS_GET_PAYLOAD(buf);
        len = ts_payload_len(buf, pay);
        ck_assert(pay == NULL && len == 0);

        /* AF followed by payload */
        TS_SET_PAYLOAD(buf, true);
        ck_assert(TS_IS_AF(buf) && TS_IS_PAYLOAD(buf));
        pay = TS_GET_PAYLOAD(buf);
        len = ts_payload_len(buf, pay);

        if (i < TS_BODY_SIZE - 1)
        {
            off = (intptr_t)pay - (intptr_t)buf;
            ck_assert(off > 0);
            ck_assert(off == (TS_HEADER_SIZE + 1 + i));
            ck_assert(len == (TS_BODY_SIZE - 1 - i));
        }
        else
        {
            /* invalid AF length */
            ck_assert(pay == NULL && len == 0);
        }
    }
}
END_TEST

/* pre-defined test packets */
typedef struct
{
    ts_packet_t data;

    struct
    {
        bool sync;
        bool error;
        bool pay;
        bool pusi;
        bool prio;
        bool af;
        uint8_t sc;
        uint8_t cc;
        uint16_t pid;
    } hdr;

    struct
    {
        uint8_t len;
        bool discont;
        bool random;
        bool es_prio;
        bool pcr;
    } af;

    struct
    {
        uint8_t off;
        uint8_t len;
    } pay;
} pkt_test_t;

#include "mpegts_packets.h"

static
uint32_t xor_mask;

static
void flip_error(void *arg, const uint8_t *ts)
{
    pkt_test_t *const t = (pkt_test_t *)arg;
    ASC_UNUSED(ts);

    const bool flag = TS_IS_ERROR(t->data);
    ck_assert((bool)(t->data[1] & 0x80) == flag);
    TS_SET_ERROR(t->data, !flag);
    ck_assert((bool)(t->data[1] & 0x80) != flag);
}

static
void flip_pusi(void *arg, const uint8_t *ts)
{
    pkt_test_t *const t = (pkt_test_t *)arg;

    if (!TS_IS_PAYLOAD(ts))
        return;

    const bool flag = TS_IS_PUSI(t->data);
    ck_assert((bool)(t->data[1] & 0x40) == flag);
    TS_SET_PUSI(t->data, !flag);
    ck_assert((bool)(t->data[1] & 0x40) != flag);
}

static
void flip_prio(void *arg, const uint8_t *ts)
{
    pkt_test_t *const t = (pkt_test_t *)arg;
    ASC_UNUSED(ts);

    const bool flag = TS_IS_PRIORITY(t->data);
    ck_assert((bool)(t->data[1] & 0x20) == flag);
    TS_SET_PRIORITY(t->data, !flag);
    ck_assert((bool)(t->data[1] & 0x20) != flag);
}

static
void xor_sc(void *arg, const uint8_t *ts)
{
    pkt_test_t *const t = (pkt_test_t *)arg;
    ASC_UNUSED(ts);

    unsigned int sc = TS_GET_SC(t->data);
    ck_assert(((t->data[3] & 0xc0) >> 6) == sc);
    sc ^= xor_mask;
    TS_SET_SC(t->data, sc);
    ck_assert(((t->data[3] & 0xc0) >> 6) == (sc & 0x3));
}

static
void xor_cc(void *arg, const uint8_t *ts)
{
    pkt_test_t *const t = (pkt_test_t *)arg;
    ASC_UNUSED(ts);

    unsigned int cc = TS_GET_CC(t->data);
    ck_assert((t->data[3] & 0xf) == cc);
    cc ^= xor_mask;
    TS_SET_CC(t->data, cc);
    ck_assert((t->data[3] & 0xf) == (cc & 0xf));
}

static
void xor_pid(void *arg, const uint8_t *ts)
{
    pkt_test_t *const t = (pkt_test_t *)arg;
    ASC_UNUSED(ts);

    unsigned int pid = TS_GET_PID(t->data);
    ck_assert((((t->data[1] << 8) & 0x1f00) | t->data[2]) == pid);
    pid ^= xor_mask;
    TS_SET_PID(t->data, pid);
    ck_assert((((t->data[1] << 8) & 0x1f00) | t->data[2]) == (pid & 0x1fff));
}

static
void flip_discont(void *arg, const uint8_t *ts)
{
    pkt_test_t *const t = (pkt_test_t *)arg;

    if (TS_AF_LEN(ts) >= 1)
    {
        const bool flag = TS_IS_DISCONT(t->data);
        ck_assert((bool)(t->data[5] & 0x80) == flag);
        TS_SET_DISCONT(t->data, !flag);
        ck_assert((bool)(t->data[5] & 0x80) == !flag);
    }
}

static
void flip_random(void *arg, const uint8_t *ts)
{
    pkt_test_t *const t = (pkt_test_t *)arg;

    if (TS_AF_LEN(ts) >= 1)
    {
        const bool flag = TS_IS_RANDOM(t->data);
        ck_assert((bool)(t->data[5] & 0x40) == flag);
        TS_SET_RANDOM(t->data, !flag);
        ck_assert((bool)(t->data[5] & 0x40) == !flag);
    }
}

static
void flip_es_prio(void *arg, const uint8_t *ts)
{
    pkt_test_t *const t = (pkt_test_t *)arg;

    if (TS_AF_LEN(ts) >= 1)
    {
        const bool flag = TS_IS_ES_PRIO(t->data);
        ck_assert((bool)(t->data[5] & 0x20) == flag);
        TS_SET_ES_PRIO(t->data, !flag);
        ck_assert((bool)(t->data[5] & 0x20) == !flag);
    }
}

static
void xor_payload(void *arg, const uint8_t *ts)
{
    pkt_test_t *const t = (pkt_test_t *)arg;
    ASC_UNUSED(ts);

    uint8_t *pay = TS_GET_PAYLOAD(t->data);
    const unsigned int len = ts_payload_len(t->data, pay);

    if (len > 0)
    {
        for (unsigned int i = 0; i < len; i++)
            pay[i] ^= xor_mask;
    }
}

static
const ts_callback_t cb_list[] =
{
    flip_error,
    flip_pusi,
    flip_prio,
    xor_sc,
    xor_cc,
    xor_pid,
    flip_discont,
    flip_random,
    flip_es_prio,
    xor_payload,
};

static
void cb_shuffle(ts_callback_t list[], size_t len)
{
    ck_assert(len > 1);

    for (size_t i = 0; i < len - 1; i++)
    {
        const size_t j = i + rand() / (RAND_MAX / (len - i) + 1);
        ts_callback_t tmp = list[j];
        list[j] = list[i];
        list[i] = tmp;
    }
}

START_TEST(test_vectors)
{
    asc_srand();

    for (size_t i = 0; i < ASC_ARRAY_SIZE(test_packets); i++)
    {
        pkt_test_t t = test_packets[i];
        const uint8_t *const orig_ts = test_packets[i].data;

        /* test getters */
        ck_assert(t.hdr.sync == TS_IS_SYNC(t.data));
        ck_assert(t.hdr.error == TS_IS_ERROR(t.data));
        ck_assert(t.hdr.pay == TS_IS_PAYLOAD(t.data));
        ck_assert(t.hdr.pusi == TS_IS_PUSI(t.data));
        ck_assert(t.hdr.prio == TS_IS_PRIORITY(t.data));
        ck_assert(t.hdr.af == TS_IS_AF(t.data));
        ck_assert(t.hdr.sc == TS_GET_SC(t.data));
        ck_assert(t.hdr.cc == TS_GET_CC(t.data));
        ck_assert(t.hdr.pid == TS_GET_PID(t.data));

        if (t.hdr.af)
        {
            const int af_len = TS_AF_LEN(t.data);
            ck_assert(t.af.len == af_len);
            ck_assert(TS_IS_DISCONT(t.data) == t.af.discont);
            ck_assert(TS_IS_RANDOM(t.data) == t.af.random);
            ck_assert(TS_IS_ES_PRIO(t.data) == t.af.es_prio);
            ck_assert(TS_IS_PCR(t.data) == t.af.pcr);
        }
        else
        {
            ck_assert(TS_AF_LEN(t.data) == -1);
            ck_assert(TS_IS_DISCONT(t.data) == false);
            ck_assert(TS_IS_RANDOM(t.data) == false);
            ck_assert(TS_IS_ES_PRIO(t.data) == false);
            ck_assert(TS_IS_PCR(t.data) == false);
        }

        const uint8_t *const payload = TS_GET_PAYLOAD(t.data);
        const int len = ts_payload_len(t.data, payload);
        const int off = TS_PACKET_SIZE - len;
        ck_assert(len >= 0 && off >= 0);

        if (t.hdr.pay)
        {
            ck_assert(payload != NULL && payload > t.data);
            ck_assert(len > 0 && len <= TS_BODY_SIZE);
            ck_assert(off >= TS_HEADER_SIZE && off < TS_PACKET_SIZE);
            ck_assert(t.pay.off == off);
            ck_assert(t.pay.len == len);
        }
        else
        {
            ck_assert(payload == NULL);
            ck_assert(len == 0 && off == TS_PACKET_SIZE);
        }

        /* test setters */
        size_t cb_num = ASC_ARRAY_SIZE(cb_list);
        ts_callback_t list[cb_num];
        memcpy(list, cb_list, sizeof(ts_callback_t) * cb_num);

        for (unsigned int k = 0; k < 512; k++)
        {
            if (!(k % 2))
            {
                xor_mask = 0;
                for (uint32_t b = 0; b < 32; b++)
                {
                    const uint32_t bit = rand() % 2;
                    xor_mask |= (bit << b);
                }
            }

            cb_shuffle(list, cb_num);
            for (size_t j = 0; j < cb_num; j++)
                (list[j])(&t, orig_ts);

            if (!(k % 2))
            {
                ck_assert(memcmp(t.data, orig_ts, TS_PACKET_SIZE) != 0);

                /* not altered by callbacks */
                ck_assert(TS_IS_AF(t.data) == TS_IS_AF(orig_ts));
                ck_assert(TS_IS_PAYLOAD(t.data) == TS_IS_PAYLOAD(orig_ts));
                ck_assert(TS_AF_LEN(t.data) == TS_AF_LEN(orig_ts));

                /* header flags */
                ck_assert(TS_IS_ERROR(t.data) != TS_IS_ERROR(orig_ts));
                ck_assert(TS_IS_PRIORITY(t.data) != TS_IS_PRIORITY(orig_ts));

                if (TS_IS_PAYLOAD(orig_ts))
                    ck_assert(TS_IS_PUSI(t.data) != TS_IS_PUSI(orig_ts));

                /* AF flags */
                if (TS_AF_LEN(orig_ts) > 0)
                {
                    ck_assert(TS_IS_DISCONT(t.data) != TS_IS_DISCONT(orig_ts));
                    ck_assert(TS_IS_RANDOM(t.data) != TS_IS_RANDOM(orig_ts));
                    ck_assert(TS_IS_ES_PRIO(t.data) != TS_IS_ES_PRIO(orig_ts));
                }
            }
        }

        ck_assert(!memcmp(t.data, orig_ts, TS_PACKET_SIZE));
    }
}
END_TEST

Suite *mpegts_mpegts(void)
{
    Suite *const s = suite_create("mpegts/mpegts");

    TCase *const tc = tcase_create("default");
    tcase_add_test(tc, pid_pnr_range);
    tcase_add_test(tc, ts_header);
    tcase_add_test(tc, adaptation_field);
    tcase_add_test(tc, ts_payload);
    tcase_add_test(tc, test_vectors);
    suite_add_tcase(s, tc);

    return s;
}
