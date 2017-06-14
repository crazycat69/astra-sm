/*
 * Astra Module: IT95x (Worker thread)
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

#include "it95x.h"

/* subroutines for error handling during initialization */
#define INIT_FATAL(_mod, _errnum, _label, ...) \
    do { \
        init_log(_mod, _errnum, ASC_LOG_ERROR, __VA_ARGS__); \
        goto _label; \
    } while (0)

#define INIT_WARN(_mod, _errnum, ...) \
    do { \
        init_log(_mod, _errnum, ASC_LOG_WARNING, __VA_ARGS__); \
    } while (0)

static __asc_printf(4, 5)
void init_log(const module_data_t *mod, int errnum
              , asc_log_type_t type, const char *fmt, ...)
{
    char buf[128] = { '\0' };

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    char *const err = it95x_strerror(errnum);
    asc_log(type, MSG("%s: %s"), buf, err);
    free(err);
}

/* stop transmitting, power down and close device */
static
void close_dev(const module_data_t *mod, it95x_dev_t *dev)
{
    asc_log_debug(MSG("disabling RF output"));
    int ret = it95x_set_rf(dev, false);
    if (ret != 0)
        INIT_WARN(mod, ret, "failed to disable RF output");

    asc_log_debug(MSG("powering down"));
    ret = it95x_set_power(dev, false);
    if (ret != 0)
        INIT_WARN(mod, ret, "failed to turn power off");

    asc_log_debug(MSG("cleaning up device context"));
    it95x_close(dev);
}

/* initialize device and apply user configuration */
static
bool open_dev(const module_data_t *mod, it95x_dev_t **result)
{
    it95x_dev_t *dev = NULL;
    bool success = false;

    asc_log_debug(MSG("creating device context"));
    int ret = it95x_open(mod->adapter, mod->devpath, &dev);
    if (ret != 0)
        INIT_FATAL(mod, ret, out, "failed to initialize modulator");

    /* report device type */
    it95x_dev_info_t info;
    it95x_get_info(dev, &info);

    asc_log_info(MSG("modulator: %s, chip ID: %04x (%s)")
                 , info.name, info.chip_type
                 , (info.eagle2 ? "Eagle II" : "Eagle"));

    /* wake up from power saving mode */
    asc_log_debug(MSG("powering up"));
    ret = it95x_set_power(dev, true);
    if (ret != 0)
        INIT_FATAL(mod, ret, out, "failed to turn power on");

    /* disable output in case some other process left it open */
    asc_log_debug(MSG("disabling RF output while device is being set up"));
    ret = it95x_set_rf(dev, false);
    if (ret != 0)
        INIT_FATAL(mod, ret, out, "failed to disable RF output");

    /* load I/Q table if configured */
    if (mod->iq_size > 0)
    {
        asc_log_debug(MSG("loading custom I/Q calibration table"));
        ret = it95x_set_iq(dev, 0, mod->iq_size, mod->iq_table);
        if (ret != 0)
            INIT_WARN(mod, ret, "failed to load I/Q calibration table");
    }

    /* system-specific initialization */
    if (mod->system == IT95X_SYSTEM_DVBT)
    {
        /* switch to DVB-T mode */
        asc_log_debug(MSG("setting DVB-T modulation"));
        ret = it95x_set_dvbt(dev, &mod->dvbt);
        if (ret != 0)
            INIT_FATAL(mod, ret, out, "failed to set DVB-T modulation");

        /* load TPS settings */
        asc_log_debug(MSG("setting TPS parameters"));
        ret = it95x_set_tps(dev, &mod->tps);
        if (ret != 0)
            INIT_FATAL(mod, ret, out, "failed to set TPS parameters");

        /* disable ISDB-T PID filter */
        if (info.eagle2)
        {
            asc_log_debug(MSG("disabling PID filter"));
            ret = it95x_ctl_pid(dev, IT95X_LAYER_NONE);
            if (ret != 0)
                INIT_WARN(mod, ret, "failed to disable PID filter");
        }
    }
    else if (mod->system == IT95X_SYSTEM_ISDBT)
    {
        /* switch to ISDB-T mode */
        asc_log_debug(MSG("setting ISDB-T modulation"));
        ret = it95x_set_isdbt(dev, &mod->isdbt);
        if (ret != 0)
            INIT_FATAL(mod, ret, out, "failed to set ISDB-T modulation");

        /* load TMCC settings */
        asc_log_debug(MSG("setting TMCC parameters"));
        ret = it95x_set_tmcc(dev, &mod->tmcc);
        if (ret != 0)
            INIT_FATAL(mod, ret, out, "failed to set TMCC parameters");

        /* configure ISDB-T PID filter */
        asc_log_debug(MSG("resetting PID filter to initial state"));
        ret = it95x_reset_pid(dev);
        if (ret != 0)
            INIT_FATAL(mod, ret, out, "failed to reset PID filter");

        if (mod->isdbt.partial)
        {
            for (size_t i = 0; i < mod->pid_cnt; i++)
            {
                const it95x_pid_t *const pid = &mod->pid_list[i];
                const unsigned int idx = i + 1;

                asc_log_debug(MSG("adding PID %u (index %u, layer %d)")
                              , pid->pid, idx, pid->layer);

                ret = it95x_add_pid(dev, idx, pid->pid, pid->layer);
                if (ret != 0)
                {
                    INIT_FATAL(mod, ret, out, "failed to add PID %u to filter"
                               , pid->pid);
                }
            }

            asc_log_debug(MSG("enabling PID filter"));
            ret = it95x_ctl_pid(dev, mod->pid_layer);
            if (ret != 0)
                INIT_FATAL(mod, ret, out, "failed to enable PID filter");
        }
        else
        {
            asc_log_debug(MSG("disabling PID filter"));
            ret = it95x_ctl_pid(dev, IT95X_LAYER_NONE);
            if (ret != 0)
                INIT_WARN(mod, ret, "failed to disable PID filter");
        }
    }
    else
    {
        /* shouldn't happen */
        asc_log_error(MSG("unknown delivery system: '%d'"), mod->system);
        goto out;
    }

    /* set frequency and bandwidth */
    asc_log_debug(MSG("setting channel frequency and bandwidth"));
    ret = it95x_set_channel(dev, mod->frequency, mod->bandwidth);
    if (ret != 0)
        INIT_FATAL(mod, ret, out, "failed to set frequency and bandwidth");

    /* set output gain */
    int gain_max, gain_min;

    asc_log_debug(MSG("retrieving output gain range"));
    ret = it95x_get_gain_range(dev, mod->frequency, mod->bandwidth
                               , &gain_max, &gain_min);

    if (ret == 0)
    {
        asc_log_debug(MSG("output gain range: min %ddB, max %ddB")
                      , gain_min, gain_max);

        int gain = mod->gain;
        if (gain > gain_max)
            gain = gain_max;
        else if (gain < gain_min)
            gain = gain_min;

        const int gain_want = gain;
        if (gain != mod->gain)
            asc_log_warning(MSG("capping output gain at %ddB"), gain);

        asc_log_debug(MSG("setting output gain"));
        ret = it95x_set_gain(dev, &gain);

        if (ret != 0)
        {
            INIT_WARN(mod, ret, "failed to set output gain");
        }
        else if (gain != gain_want)
        {
            asc_log_warning(MSG("requested output gain of %ddB, got %ddB")
                            , gain_want, gain);
        }
    }
    else
    {
        INIT_WARN(mod, ret, "failed to retrieve output gain range");
    }

    /* configure DC calibration */
    asc_log_debug(MSG("setting DC offset compensation values"));
    ret = it95x_set_dc_cal(dev, mod->dc_i, mod->dc_q);
    if (ret != 0)
        INIT_WARN(mod, ret, "failed to set DC offset compensation values");

    /* these are only supported by IT9517 and newer (probably) */
    if (info.eagle2)
    {
        /* configure OFS calibration */
        asc_log_debug(MSG("setting OFS calibration values"));
        ret = it95x_set_ofs_cal(dev, mod->ofs_i, mod->ofs_q);
        if (ret != 0)
            INIT_WARN(mod, ret, "failed to set OFS calibration values");

        /* configure PCR restamping */
        asc_log_debug(MSG("setting PCR restamping mode"));
        ret = it95x_set_pcr(dev, mod->pcr_mode);
        if (ret != 0)
            INIT_WARN(mod, ret, "failed to set PCR restamping mode");

        /* configure TPS encryption */
        asc_log_debug(MSG("setting TPS encryption key"));
        ret = it95x_set_tps_crypt(dev, mod->tps_crypt);
        if (ret != 0)
            INIT_WARN(mod, ret, "failed to set TPS encryption key");
    }

    /* reset PSI insertion timers */
    for (unsigned int i = 1; i <= IT95X_PSI_TIMER_CNT; i++)
    {
        asc_log_debug(MSG("disabling PSI timer %u"), i);
        ret = it95x_set_psi(dev, i, 0, NULL);
        if (ret != 0)
            INIT_WARN(mod, ret, "failed to disable PSI timer %u", i);
    }

    /* turn on transmitter */
    asc_log_debug(MSG("enabling RF output"));
    ret = it95x_set_rf(dev, true);
    if (ret != 0)
        INIT_FATAL(mod, ret, out, "failed to enable RF output");

    *result = dev;
    success = true;

out:
    if (!success && dev != NULL)
        close_dev(mod, dev);

    return success;
}

void it95x_worker_loop(void *arg)
{
    module_data_t *const mod = (module_data_t *)arg;
    it95x_dev_t *dev = NULL;
    int ret = 0;

    /* setup */
    asc_log_debug(MSG("worker thread started"));

    if (!open_dev(mod, &dev))
    {
        asc_log_debug(MSG("worker thread exiting due to failed init"));
        return;
    }

    asc_log_info(MSG("now transmitting at %.3f MHz with %u MHz bandwidth")
                 , mod->frequency / 1000.0, (int)mod->bandwidth / 1000);

    /* modulator transmit loop */
    asc_mutex_lock(&mod->mutex);
    mod->transmitting = true;

    do
    {
        while (!mod->quitting && mod->tx_tail != mod->tx_head)
        {
            it95x_tx_block_t *const blk = &mod->tx_ring[mod->tx_tail];

            /*
             * NOTE: It is possible for the transmit op to block due to
             *       TS bitrate spikes, bus latency, hardware issues, etc.
             *       Unlocking the ring allows the main thread to queue
             *       more data in the meantime.
             */
            asc_mutex_unlock(&mod->mutex);
            ret = it95x_send_ts(dev, blk);
            asc_mutex_lock(&mod->mutex);

            if (ret == 0)
                mod->tx_tail = (mod->tx_tail + 1) % mod->tx_size;
            else
                mod->quitting = true;
        }

        if (mod->quitting)
            break;

        asc_cond_wait(&mod->cond, &mod->mutex);
    } while (true);

    mod->transmitting = false;
    asc_mutex_unlock(&mod->mutex);

    /* teardown */
    if (ret != 0)
    {
        char *const err = it95x_strerror(ret);
        asc_log_error(MSG("TS transmit failed: %s"), err);
        free(err);
    }

    close_dev(mod, dev);
    asc_log_debug(MSG("worker thread exiting"));
}
