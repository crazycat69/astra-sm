/*
 * Astra Core (Array list)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
 *               2015-2017, Artem Kharitonov <artem@3phase.pw>
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

#include <astra/astra.h>
#include <astra/core/list.h>

#define MSG(_msg) "[core/list] " _msg

/* minimum list size, items */
#define LIST_MIN_SIZE 32

/* change list allocation size if needed */
static
void resize_list(asc_list_t *list, size_t count)
{
    const size_t new_size = asc_list_calc_size(count, list->size
                                               , LIST_MIN_SIZE);

    if (list->size != new_size)
    {
        const size_t block = new_size * sizeof(void *);
        list->items = (void **)realloc(list->items, block);
        ASC_ASSERT(list->items != NULL, MSG("realloc() failed"));

        list->size = new_size;
    }
}

asc_list_t *asc_list_init(void)
{
    asc_list_t *const list = ASC_ALLOC(1, asc_list_t);
    resize_list(list, 0);

    return list;
}

void asc_list_destroy(asc_list_t *list)
{
    free(list->items);
    free(list);
}

void asc_list_insert_head(asc_list_t *list, void *data)
{
    const size_t block = list->count * sizeof(void *);
    resize_list(list, ++list->count);

    if (block > 0)
        memmove(&list->items[1], list->items, block);

    list->items[0] = data;
    list->current++;
}

void asc_list_insert_tail(asc_list_t *list, void *data)
{
    resize_list(list, ++list->count);
    list->items[list->count - 1] = data;
}

void asc_list_purge(asc_list_t *list)
{
    list->count = list->current = 0;
    resize_list(list, 0);
}

void asc_list_remove_index(asc_list_t *list, size_t idx)
{
    ASC_ASSERT(idx < list->count, MSG("index out of bounds"));

    const size_t more = list->count - (idx + 1);
    if (more > 0)
    {
        const size_t block = more * sizeof(void *);
        memmove(&list->items[idx], &list->items[idx + 1], block);
    }

    resize_list(list, --list->count);

    if (idx < list->current)
        list->current--;

    if (list->current > list->count)
        list->current = list->count;
}

void asc_list_remove_item(asc_list_t *list, const void *data)
{
    for (size_t i = 0; i < list->count; i++)
    {
        if (list->items[i] == data)
        {
            asc_list_remove_index(list, i);
            return;
        }
    }
}
