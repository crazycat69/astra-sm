/*
 * Astra Module: BDA
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

#include "bda.h"

#define MSG(_msg) "[dvb_input] %s" _msg, dev->name

static
void execute_tune(hw_device_t *dev, const bda_tune_cmd_t *tune)
{
    __uarg(dev);
    __uarg(tune);
}

static
void execute_close(hw_device_t *dev)
{
    __uarg(dev);
}

void bda_graph_loop(void *arg)
{
    hw_device_t *const dev = (hw_device_t *)arg;
    bool quit = false;

    // in case someone actually tries to compile this
    asc_usleep(100000);
    return;

    do
    {
        /* run queued user commands */
        asc_mutex_lock(&dev->queue_lock);
        asc_list_clear(dev->queue)
        {
            bda_user_cmd_t *const item =
                (bda_user_cmd_t *)asc_list_data(dev->queue);

            /* execute command with mutex unlocked */
            asc_mutex_unlock(&dev->queue_lock);
            switch (item->cmd)
            {
                case BDA_COMMAND_TUNE:
                    execute_tune(dev, &item->tune);
                    break;

                case BDA_COMMAND_DEMUX:
                    // dev->demux[pid] = true/false;
                    // if (graph != NULL)
                    //     execute_demux(dev, &item->demux);
                    break;

                case BDA_COMMAND_CA:
                    // dev->ca[] = true;
                    // if (graph != NULL)
                    //     execute_ca(dev, &item->ca);
                    break;

                case BDA_COMMAND_DISEQC:
                    // if (graph != NULL)
                    //     execute_diseqc(dev, &item->diseqc);
                    break;

                case BDA_COMMAND_QUIT:
                default:
                    quit = true;
                    /* fallthrough */

                case BDA_COMMAND_CLOSE:
                    ASC_FREE(dev->tune, free);
                    break;
            }
            free(item);
            asc_mutex_lock(&dev->queue_lock);
        }
        asc_mutex_unlock(&dev->queue_lock);

        /* handle state change */
        if (dev->tune != NULL && dev->graph == NULL)
        {
            const uint64_t now = asc_utime();

            if (now >= dev->next_tune)
            {
                // HRESULT hr = execute_tune(dev, dev->tune);
                // if (FAILED(hr))
                // {
                //     asc_log_error(MSG("retrying in 5 seconds"));
                //     dev->next_tune = now + 5000000;
                // }
            }
        }
        else if (dev->graph != NULL && dev->tune == NULL)
        {
            execute_close(dev);
        }

        /* dispatch graph events */
        if (dev->graph != NULL)
        {
            HRESULT hr;
            long ec;
            LONG_PTR lp1, lp2;

            hr = IMediaEvent_GetEvent(dev->event, &ec, &lp1, &lp2, 100);
            if (SUCCEEDED(hr))
            {
                // TODO: handle event
                IMediaEvent_FreeEventParams(dev->event, ec, lp1, lp2);
            }
            else if (hr != E_ABORT)
            {
                // error; close down and schedule restart
                asc_log_error(MSG("retrying in 5 seconds"));

                execute_close(dev);
                dev->next_tune = asc_utime() + 5000000; /* 5 sec */
                // TODO: add RESTART_INTERVAL define
            }
        }
        else
        {
            /* dry run; sleep for 100 ms */
            asc_usleep(100000);
        }
    } while (!quit);
}
