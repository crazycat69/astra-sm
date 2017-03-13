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

#include <astra/astra.h>

/* maximum device name length, characters */
#define IT95X_MAX_NAME 128

/* TPS encryption key size, bytes */
#define IT95X_TPS_KEY_SIZE 4

/* maximum block size to send to modulator */
#define IT95X_TX_BLOCK_PKTS 384
#define IT95X_TX_BLOCK_SIZE (IT95X_TX_BLOCK_PKTS * TS_PACKET_SIZE)

/* opaque type for device context */
typedef struct it95x_dev_t it95x_dev_t;

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
} it95x_transmission_mode_t;

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
    IT95X_LAYER_NONE = 0,
    IT95X_LAYER_B = 1,
    IT95X_LAYER_A = 2,
    IT95X_LAYER_AB = 3,
} it95x_layer_t;

/* IT9500 processor */
typedef enum
{
    IT95X_PROCESSOR_LINK = 0,
    IT95X_PROCESSOR_OFDM = 1,
} it95x_processor_t;

/* USB mode */
typedef enum
{
    IT95X_USB_NOT_SET = -1,
    IT95X_USB_20 = 0x0200,
    IT95X_USB_11 = 0x0110,
} it95x_usb_mode_t;

/* DVB-T modulation settings */
typedef struct
{
    it95x_code_rate_t code_rate;
    it95x_transmission_mode_t transmission_mode;
    it95x_constellation_t constellation;
    it95x_guard_interval_t guard_interval;
} it95x_dvbt_t;

/* DVB-T TPS (Transmission Parameter Signalling) */
typedef struct
{
    it95x_code_rate_t high_code_rate;
    it95x_code_rate_t low_code_rate;
    it95x_transmission_mode_t transmission_mode;
    it95x_constellation_t constellation;
    it95x_guard_interval_t guard_interval;
    uint16_t cell_id;
} it95x_tps_t;

/* ISDB-T modulation settings */
typedef struct
{
    it95x_transmission_mode_t transmission_mode;
    it95x_guard_interval_t guard_interval;
    bool partial;

    struct
    {
        it95x_code_rate_t code_rate;
        it95x_constellation_t constellation;
    } a;

    struct
    {
        it95x_code_rate_t code_rate;
        it95x_constellation_t constellation;
    } b;
} it95x_isdbt_t;

/* ISDB-T TMCC (Transmission and Multiplexing Configuration Control) */
typedef struct
{
    bool partial;

    struct
    {
        it95x_code_rate_t code_rate;
        it95x_constellation_t constellation;
    } a;

    struct
    {
        it95x_code_rate_t code_rate;
        it95x_constellation_t constellation;
    } b;
} it95x_tmcc_t;

/* TS data block for transmission */
typedef struct
{
    uint32_t code;
    uint32_t size;
    uint8_t data[IT95X_TX_BLOCK_SIZE];
} it95x_tx_block_t;

/* device information */
typedef struct
{
    char *name;
    char *devpath;

    /* bus information */
    it95x_usb_mode_t usb_mode;
    uint16_t vendor_id;
    uint16_t product_id;

    /* driver and firmware */
    uint32_t drv_pid;
    uint32_t drv_version;
    uint32_t fw_link;
    uint32_t fw_ofdm;
    uint32_t tuner_id;

    /* chip and device type */
    uint16_t chip_type;
    uint8_t dev_type;
} it95x_dev_info_t;

/* API functions */
int it95x_dev_count(size_t *count);
int it95x_open(ssize_t idx, const char *path, it95x_dev_t **dev);
void it95x_get_info(const it95x_dev_t *dev, it95x_dev_info_t *info);
void it95x_close(it95x_dev_t *dev);

int it95x_get_gain(it95x_dev_t *dev, int *gain);
int it95x_get_gain_range(it95x_dev_t *dev
                         , uint32_t frequency, uint32_t bandwidth
                         , int *max, int *min);
int it95x_get_tmcc(it95x_dev_t *dev, it95x_tmcc_t *tmcc);
int it95x_get_tps(it95x_dev_t *dev, it95x_tps_t *tps);

int it95x_set_channel(it95x_dev_t *dev
                      , uint32_t frequency, uint32_t bandwidth);
int it95x_set_crypt(it95x_dev_t *dev, bool enable
                    , const uint8_t key[IT95X_TPS_KEY_SIZE]);
int it95x_set_gain(it95x_dev_t *dev, int *gain);
int it95x_set_power(it95x_dev_t *dev, bool power);
int it95x_set_pcr(it95x_dev_t *dev, it95x_pcr_mode_t mode);
int it95x_set_psi(it95x_dev_t *dev, unsigned int timer, uint8_t msecs
                  , const uint8_t packet[TS_PACKET_SIZE]);
int it95x_set_rf(it95x_dev_t *dev, bool rf);
int it95x_set_tmcc(it95x_dev_t *dev, const it95x_tmcc_t *tmcc);
int it95x_set_tps(it95x_dev_t *dev, const it95x_tps_t *tps);

int it95x_set_dvbt(it95x_dev_t *dev, const it95x_dvbt_t *dvbt);
int it95x_set_isdbt(it95x_dev_t *dev, const it95x_isdbt_t *isdbt);

int it95x_set_dc_cal(it95x_dev_t *dev, int dc_i, int dc_q);
int it95x_set_ofs_cal(it95x_dev_t *dev
                      , unsigned int ofs_i, unsigned int ofs_q);

int it95x_add_pid(it95x_dev_t *dev, unsigned int idx
                  , uint16_t pid, it95x_layer_t layer);
int it95x_ctl_pid(it95x_dev_t *dev, it95x_layer_t layer);
int it95x_reset_pid(it95x_dev_t *dev);

int it95x_send_ts(it95x_dev_t *dev, const it95x_tx_block_t *data);

int it95x_rd_reg(it95x_dev_t *dev, it95x_processor_t processor
                 , uint32_t address, uint8_t *value);
int it95x_wr_reg(it95x_dev_t *dev, it95x_processor_t processor
                 , uint32_t address, uint8_t value);

char *it95x_strerror(int error);

#endif /* _IT95X_API_H_ */
