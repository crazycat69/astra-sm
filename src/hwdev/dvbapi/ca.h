/*
 * Astra Module: DVB (en50221)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
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

#ifndef _DVB_CA_H_
#define _DVB_CA_H_ 1

typedef struct dvb_ca_t dvb_ca_t;

// 1 - sessions[0] is empty
#define MAX_SESSIONS (32 + 1)
#define MAX_TPDU_SIZE 2048

typedef enum
{
    CA_MODULE_STATUS_NONE = 0x00,
    CA_MODULE_STATUS_APP_INFO = 0x01,
    CA_MODULE_STATUS_CA_INFO = 0x02,
    CA_MODULE_STATUS_READY = 0x03,
} ca_module_status_t;

typedef struct
{
    uint8_t buffer[MAX_TPDU_SIZE];
    uint32_t buffer_size;
} ca_tpdu_message_t;

typedef struct
{
    uint32_t resource_id;

    void (*event)(dvb_ca_t *ca, uint8_t slot_id, uint16_t session_id);
    void (*close)(dvb_ca_t *ca, uint8_t slot_id, uint16_t session_id);
    void (*manage)(dvb_ca_t *ca, uint8_t slot_id, uint16_t session_id);

    void *data;
} ca_session_t;

typedef struct
{
    bool is_active;
    bool is_busy;
    bool is_first_ca_pmt;

    // send
    asc_list_t *queue;

    // recv
    uint8_t buffer[MAX_TPDU_SIZE];
    uint16_t buffer_size;

    // session
    uint16_t pending_session_id;
    ca_session_t sessions[MAX_SESSIONS];
} ca_slot_t;

typedef struct
{
    uint16_t pnr;
    uint32_t crc;
} pmt_checksum_t;

typedef struct
{
    uint16_t pnr;

    mpegts_psi_t *psi;

    uint8_t buffer[PSI_MAX_SIZE];
    uint16_t buffer_size;
} ca_pmt_t;

struct dvb_ca_t
{
    int adapter;
    int frontend;

    /* CA Base */
    int ca_fd;
    int slots_num;
    ca_slot_t *slots;

    uint8_t ca_buffer[MAX_TPDU_SIZE];

    /* CA PMT */

    mpegts_packet_type_t stream[TS_MAX_PID];
    mpegts_psi_t *pat;
    mpegts_psi_t *pmt;

    int pmt_count;
    pmt_checksum_t *pmt_checksum_list;

    asc_list_t *ca_pmt_list;
    asc_list_t *ca_pmt_list_new;
    asc_list_t *ca_pmt_list_del;
    pthread_mutex_t ca_mutex;

    /* */

    ca_module_status_t status;

    uint64_t pmt_delay;
    uint64_t pmt_check_delay;
};

void ca_on_ts(dvb_ca_t *ca, const uint8_t *ts);
void ca_append_pnr(dvb_ca_t *ca, uint16_t pnr);
void ca_remove_pnr(dvb_ca_t *ca, uint16_t pnr);
void ca_open(dvb_ca_t *ca);
void ca_close(dvb_ca_t *ca);
void ca_loop(dvb_ca_t *ca, int is_data);

#endif /* _DVB_CA_H_ */
