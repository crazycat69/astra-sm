/*
 * Astra Module: IT95x (Modulator API)
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

#ifndef _IT95X_API_H_
#define _IT95X_API_H_ 1

/* maximum block size to send to modulator */
#define IT95X_TX_BLOCK_PKTS 384
#define IT95X_TX_BLOCK_SIZE (IT95X_TX_BLOCK_PKTS * TS_PACKET_SIZE)

/* TPS encryption key size, bytes */
#define IT95X_TPS_KEY_SIZE 4

/* device context */
typedef struct it95x_device_t it95x_device_t;

/* code rate */
typedef enum
{
    IT95X_CODE_RATE_NOT_SET = -1,
    IT95X_CODE_RATE_1_2 = 0,
    IT95X_CODE_RATE_2_3 = 1,
    IT95X_CODE_RATE_3_4 = 2,
    IT95X_CODE_RATE_5_6 = 3,
    IT95X_CODE_RATE_7_8 = 4,
} it95x_code_rate_t;

/* constellation */
typedef enum
{
    IT95X_CONSTELLATION_NOT_SET = -1,
    IT95X_CONSTELLATION_QPSK = 0,
    IT95X_CONSTELLATION_16QAM = 1,
    IT95X_CONSTELLATION_32QAM = 2,
} it95x_constellation_t;

/* transmission mode */
typedef enum
{
    IT95X_TX_MODE_NOT_SET = -1,
    IT95X_TX_MODE_2K = 0,
    IT95X_TX_MODE_8K = 1,
    IT95X_TX_MODE_4K = 2,
} it95x_tx_mode_t;

/* guard interval */
typedef enum
{
    IT95X_GUARD_NOT_SET = -1,
    IT95X_GUARD_1_32 = 0,
    IT95X_GUARD_1_16 = 1,
    IT95X_GUARD_1_8 = 2,
    IT95X_GUARD_1_4 = 3,
} it95x_guard_interval_t;

/* PCR restamping mode */
typedef enum
{
    IT95X_PCR_NOT_SET = -1,
    IT95X_PCR_DISABLED = 0,
    IT95X_PCR_MODE1 = 1,
    IT95X_PCR_MODE2 = 2,
    IT95X_PCR_MODE3 = 3,
} it95x_pcr_mode_t;

/* delivery system */
typedef enum
{
    IT95X_SYSTEM_NOT_SET = -1,
    IT95X_SYSTEM_DVBT = 0,
    IT95X_SYSTEM_ISDBT = 1,
} it95x_system_t;

/* ISDB-T layers */
typedef enum
{
    IT95X_LAYER_B = 1,
    IT95X_LAYER_A = 2,
    IT95X_LAYER_AB = 3,
} it95x_layer_t;

/* driver and firmware information */
typedef struct
{
    uint32_t driver_pid;
    uint32_t driver_version;
    uint32_t fw_link;
    uint32_t fw_ofdm;
    uint32_t tuner_id;
} it95x_drv_info_t;

/* bus information */
typedef struct
{
    uint16_t usb_mode; /* 0x0200: USB 2.0, 0x0110: USB 1.1 */
    uint16_t vendor_id;
    uint16_t product_id;
} it95x_bus_info_t;

/* DVB-T modulation settings */
typedef struct
{
    it95x_code_rate_t code_rate;
    it95x_constellation_t constellation;
    it95x_tx_mode_t tx_mode;
    it95x_guard_interval_t guard;
} it95x_dvbt_t;

/* ISDB-T modulation settings */
typedef struct
{
    it95x_tx_mode_t tx_mode;
    it95x_guard_interval_t guard;
    bool partial;

    struct
    {
        it95x_code_rate_t code_rate;
        it95x_constellation_t constellation;
    } layer_a;

    struct
    {
        it95x_code_rate_t code_rate;
        it95x_constellation_t constellation;
    } layer_b;
} it95x_isdbt_t;

/* TS data block for transmit */
typedef struct
{
    uint32_t code;
    uint32_t size;
    uint8_t data[IT95X_TX_BLOCK_SIZE];
} it95x_tx_block_t;

#endif /* _IT95X_API_H_ */
