/*
 * Astra Unit Tests
 * http://cesbo.com/astra
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

#include "../libastra.h"
#include <astra/utils/json.h>

static lua_State *L = NULL;

static
void setup(void)
{
    asc_srand();
    L = luaL_newstate();
}

static
void teardown(void)
{
    ck_assert(lua_gettop(L) == 0);
    ASC_FREE(L, lua_close);
}

/* objects and arrays */
START_TEST(table_crawl)
{
    bool values[1000] = { 0 };

    /* encode */
    lua_newtable(L);

    lua_newtable(L);
    for (size_t i = 0; i < ASC_ARRAY_SIZE(values); i++)
    {
        values[i] = rand() % 2;

        char buf[8] = { 0 };
        snprintf(buf, sizeof(buf), "%zu", i);
        lua_pushboolean(L, values[i]);
        lua_setfield(L, -2, buf);
    }
    lua_setfield(L, -2, "obj");

    lua_newtable(L);
    for (size_t i = 0; i < ASC_ARRAY_SIZE(values); i++)
    {
        lua_pushboolean(L, values[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "arr");

    ck_assert(au_json_enc(L) == 0);
    ck_assert(lua_gettop(L) == 1);

    /* decode */
    size_t json_len = 0;
    const char *const json = lua_tolstring(L, -1, &json_len);
    ck_assert(au_json_dec(L, json, json_len) == 0);
    ck_assert(lua_gettop(L) == 2 && lua_istable(L, -1));
    lua_remove(L, -2);

    lua_foreach(L, -2)
    {
        ck_assert(lua_isstring(L, -2));
        const char *const key = lua_tostring(L, -2);
        ck_assert(key != NULL);

        if (!strcmp(key, "arr"))
        {
            ck_assert(lua_istable(L, -1));
            ck_assert(luaL_len(L, -1) == ASC_ARRAY_SIZE(values));

            lua_foreach(L, -2)
            {
                ck_assert(lua_isnumber(L, -2));
                ck_assert(lua_isboolean(L, -1));

                int isnum = 0;
                const size_t idx = lua_tointegerx(L, -2, &isnum) - 1;
                ck_assert(isnum && idx < ASC_ARRAY_SIZE(values));
                const bool val = lua_toboolean(L, -1);
                ck_assert(values[idx] == val);
            }
        }
        else if (!strcmp(key, "obj"))
        {
            ck_assert(lua_istable(L, -1));
            ck_assert(luaL_len(L, -1) == 0);

            size_t pairs = 0;
            lua_foreach(L, -2)
            {
                ck_assert(lua_isstring(L, -2));
                ck_assert(lua_isboolean(L, -1));

                const char *const ikey = lua_tostring(L, -2);
                ck_assert(ikey != NULL);
                const size_t idx = atol(ikey);
                ck_assert(idx < ASC_ARRAY_SIZE(values));
                const bool val = lua_toboolean(L, -1);
                ck_assert(values[idx] == val);

                pairs++;
            }

            ck_assert(pairs == ASC_ARRAY_SIZE(values));
        }
        else
        {
            ck_abort_msg("unknown table key: '%s'", key);
        }
    }

    lua_pop(L, 1);
}
END_TEST

/* absent argument */
START_TEST(no_arg)
{
    /* encoding empty stack yields "null" */
    ck_assert(lua_gettop(L) == 0);
    ck_assert(au_json_enc(L) == 0);
    ck_assert(lua_gettop(L) == 1 && lua_isstring(L, -1));
    ck_assert(!strcmp("null", lua_tostring(L, -1)));
    lua_pop(L, 1);

    /* decode shouldn't access input if it's zero length */
    ck_assert(au_json_dec(L, NULL, 0) == 0);
    ck_assert(lua_gettop(L) == 1 && lua_isnil(L, -1));
    lua_pop(L, 1);
}
END_TEST

Suite *utils_json(void)
{
    Suite *const s = suite_create("utils/json");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, table_crawl);
    tcase_add_test(tc, no_arg);

    suite_add_tcase(s, tc);

    return s;
}
