/*
 * Astra Core (Linked lists)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
 *                    2015, Artem Kharitonov <artem@sysert.ru>
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
#include <core/list.h>

#define MSG(_msg) "[core/list] " _msg

asc_list_t *asc_list_init(void)
{
    asc_list_t *const list = (asc_list_t *)calloc(1, sizeof(*list));
    asc_assert(list != NULL, MSG("calloc() failed"));

    TAILQ_INIT(&list->list);

    return list;
}

void asc_list_destroy(asc_list_t *list)
{
    asc_assert(list->current == NULL, MSG("list is not empty"));
    free(list);
}

void asc_list_insert_head(asc_list_t *list, void *data)
{
    ++list->size;

    asc_item_t *const item = (asc_item_t *)calloc(1, sizeof(*item));
    asc_assert(item != NULL, MSG("calloc() failed"));

    item->data = data;
    TAILQ_INSERT_HEAD(&list->list, item, entries);
}

void asc_list_insert_tail(asc_list_t *list, void *data)
{
    ++list->size;

    asc_item_t *const item = (asc_item_t *)calloc(1, sizeof(*item));
    asc_assert(item != NULL, MSG("calloc() failed"));

    item->data = data;
    TAILQ_INSERT_TAIL(&list->list, item, entries);
}

void asc_list_remove_current(asc_list_t *list)
{
    --list->size;
    asc_assert(list->current != NULL, MSG("failed to remove item"));
    asc_item_t *const next = TAILQ_NEXT(list->current, entries);
    TAILQ_REMOVE(&list->list, list->current, entries);
    free(list->current);
    list->current = next;
}

void asc_list_remove_item(asc_list_t *list, const void *data)
{
    asc_list_for(list)
    {
        if(data == asc_list_data(list))
        {
            asc_list_remove_current(list);
            return;
        }
    }
}
