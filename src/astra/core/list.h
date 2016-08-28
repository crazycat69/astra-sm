/*
 * Astra Core (Linked lists)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
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

#ifndef _ASC_LIST_H_
#define _ASC_LIST_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

#ifdef HAVE_SYS_QUEUE_H
#   include <sys/queue.h>
#endif
#include "compat_queue.h"

typedef struct asc_item_s
{
    void *data;
    TAILQ_ENTRY(asc_item_s) entries;
} asc_item_t;

typedef struct
{
    size_t size;
    struct asc_item_s *current;
    TAILQ_HEAD(list_head_s, asc_item_s) list;
} asc_list_t;

asc_list_t *asc_list_init(void) __wur;
void asc_list_destroy(asc_list_t *list);

void asc_list_insert_head(asc_list_t *list, void *data);
void asc_list_insert_tail(asc_list_t *list, void *data);

void asc_list_remove_current(asc_list_t *list);
void asc_list_remove_item(asc_list_t *list, const void *data);

#define asc_list_for(__list) \
    for (asc_list_first(__list) \
         ; !asc_list_eol(__list) \
         ; asc_list_next(__list))

#define asc_list_clear(__list) \
    for (asc_list_first(__list) \
         ; !asc_list_eol(__list) \
         ; asc_list_remove_current(__list))

#define asc_list_till_empty(__list) \
    for (asc_list_first(__list) \
         ; !asc_list_eol(__list) \
         ; asc_list_first(__list))

static inline
void asc_list_first(asc_list_t *list)
{
    list->current = TAILQ_FIRST(&list->list);
}

static inline
void asc_list_next(asc_list_t *list)
{
    if (list->current)
        list->current = TAILQ_NEXT(list->current, entries);
}

static inline __wur
bool asc_list_eol(const asc_list_t *list)
{
    return (list->current == NULL);
}

static inline __wur
void *asc_list_data(const asc_list_t *list)
{
    asc_assert(list->current != NULL, "[core/list] failed to get data");
    return list->current->data;
}

static inline __wur
size_t asc_list_size(const asc_list_t *list)
{
    return list->size;
}

#endif /* _ASC_LIST_H_ */
