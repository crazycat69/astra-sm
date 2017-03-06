/*
 * Astra Core (Array list)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
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

#ifndef _ASC_LIST_H_
#define _ASC_LIST_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

typedef struct
{
    size_t count;
    size_t size;
    size_t current;
    void **items;
} asc_list_t;

asc_list_t *asc_list_init(void) __wur;
void asc_list_destroy(asc_list_t *list);

void asc_list_insert_head(asc_list_t *list, void *data);
void asc_list_insert_tail(asc_list_t *list, void *data);

void asc_list_purge(asc_list_t *list);
void asc_list_remove_index(asc_list_t *list, size_t index);
void asc_list_remove_item(asc_list_t *list, const void *data);

/* calculate optimum allocation size based on the number of items */
static inline __func_const __wur
size_t asc_list_calc_size(size_t count, size_t size, size_t min_size)
{
    if (size < min_size)
        size = min_size;

    /* double list size when it's full */
    while (count >= size)
        size *= 2;

    /* halve list size if it uses less than 1/4th of its allocated space */
    while (count < (size / 4) && size > min_size)
        size /= 2;

    return size;
}

static inline
void asc_list_remove_current(asc_list_t *list)
{
    asc_list_remove_index(list, list->current);
}

static inline
void asc_list_first(asc_list_t *list)
{
    list->current = 0;
}

static inline
void asc_list_next(asc_list_t *list)
{
    if (list->current < list->count)
        list->current++;
    else
        list->current = list->count;
}

static inline __func_pure __wur
bool asc_list_eol(const asc_list_t *list)
{
    return (list->current >= list->count);
}

static inline __func_pure __wur
void *asc_list_data(asc_list_t *list)
{
    ASC_ASSERT(!asc_list_eol(list), "[core/list] index out of bounds");
    return list->items[list->current];
}

static inline __func_pure __wur
size_t asc_list_count(const asc_list_t *list)
{
    return list->count;
}

/* iterator macros */
#define asc_list_for(_list) \
    for (size_t __i = 0 \
         ; ((_list)->current = __i, true) && __i < (_list)->count \
         ; __i++)

#define asc_list_clear(_list) \
    for ((_list)->current = 0 \
         ; (_list)->count > 0 \
         ; (_list)->current = 0, asc_list_remove_current((_list)))

#define asc_list_till_empty(_list) \
    for ((_list)->current = 0 \
         ; (_list)->count > 0 \
         ; (_list)->current = 0)

#endif /* _ASC_LIST_H_ */
