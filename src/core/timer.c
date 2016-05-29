/*
 * Astra Core
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
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

#include <astra.h>
#include <core/timer.h>
#include <core/list.h>

#define TIMER_DELAY_MIN 1000 /* 1ms */
#define TIMER_DELAY_MAX 100000 /* 100ms */

struct asc_timer_t
{
    timer_callback_t callback;
    void *arg;

    uint64_t interval;
    uint64_t next_shot;
};

static asc_list_t *timer_list = NULL;

void asc_timer_core_init(void)
{
    timer_list = asc_list_init();
}

void asc_timer_core_destroy(void)
{
    if (timer_list == NULL)
        return;

    asc_list_clear(timer_list)
    {
        free(asc_list_data(timer_list));
    }

    ASC_FREE(timer_list, asc_list_destroy);
}

unsigned int asc_timer_core_loop(void)
{
    uint64_t nearest = UINT64_MAX;
    uint64_t now = asc_utime();

    asc_list_first(timer_list);
    while (!asc_list_eol(timer_list))
    {
        asc_timer_t *const timer = (asc_timer_t *)asc_list_data(timer_list);

        if (timer->callback != NULL && now >= timer->next_shot)
        {
            timer->callback(timer->arg);

            /* refresh timestamp */
            now = asc_utime();

            if (timer->interval > 0)
                timer->next_shot = now + timer->interval; /* periodic timer */
            else
                timer->callback = NULL; /* one shot timer, so remove it */
        }

        if (timer->callback == NULL)
        {
            asc_list_remove_current(timer_list);
            free(timer);
        }
        else
        {
            if (timer->next_shot < nearest)
                nearest = timer->next_shot;

            asc_list_next(timer_list);
        }
    }

    uint64_t diff;
    if (nearest < now + TIMER_DELAY_MIN)
        diff = TIMER_DELAY_MIN;
    else if (nearest > now + TIMER_DELAY_MAX)
        diff = TIMER_DELAY_MAX;
    else
        diff = nearest - now;

    return (diff / 1000);
}

asc_timer_t *asc_timer_init(unsigned int ms, timer_callback_t callback
                            , void *arg)
{
    asc_timer_t *const timer = ASC_ALLOC(1, asc_timer_t);

    timer->interval = ms * 1000ULL;
    timer->callback = callback;
    timer->arg = arg;

    timer->next_shot = asc_utime() + timer->interval;

    asc_list_insert_tail(timer_list, timer);

    return timer;
}

asc_timer_t *asc_timer_one_shot(unsigned int ms, timer_callback_t callback
                                , void *arg)
{
    asc_timer_t *const timer = asc_timer_init(ms, callback, arg);
    timer->interval = 0;

    return timer;
}

void asc_timer_destroy(asc_timer_t *timer)
{
    if (timer == NULL)
        return;

    /* setting callback to NULL causes timer removal by loop function */
    timer->callback = NULL;
}
