/*
 * Astra Core
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

#ifndef _ASC_LIST_H_
#define _ASC_LIST_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra.h> first"
#endif /* !_ASTRA_H_ */

typedef struct asc_list_t asc_list_t;

asc_list_t *asc_list_init(void) __wur;
void asc_list_destroy(asc_list_t *list);

void asc_list_first(asc_list_t *list);
void asc_list_next(asc_list_t *list);
bool asc_list_eol(const asc_list_t *list) __wur;
void *asc_list_data(const asc_list_t *list) __wur;
size_t asc_list_size(const asc_list_t *list) __wur;

void asc_list_insert_head(asc_list_t *list, void *data);
void asc_list_insert_tail(asc_list_t *list, void *data);

void asc_list_remove_current(asc_list_t *list);
void asc_list_remove_item(asc_list_t *list, const void *data);

#define asc_list_for(__list) \
    for(asc_list_first(__list); !asc_list_eol(__list); asc_list_next(__list))

#endif /* _ASC_LIST_H_ */
