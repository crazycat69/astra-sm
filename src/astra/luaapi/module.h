/*
 * Astra Lua API (Module)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2015, Andrey Dyldin <and@cesbo.com>
 *               2015-2016, Artem Kharitonov <artem@3phase.pw>
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

#ifndef _LUA_MODULE_H_
#define _LUA_MODULE_H_ 1

#ifndef _ASTRA_H_
#   error "Please include <astra/astra.h> first"
#endif /* !_ASTRA_H_ */

#include <astra/luaapi/luaapi.h>

typedef struct module_data_t module_data_t;
typedef int (*module_callback_t)(lua_State *L, module_data_t *);

typedef struct
{
    const char *name;
    module_callback_t func;
} module_method_t;

typedef enum
{
    MODULE_TYPE_BASIC = 0,
    MODULE_TYPE_STREAM,
    MODULE_TYPE_BINDING,
} module_type_t;

typedef struct
{
    void (*load)(lua_State *);
    void (*init)(lua_State *, module_data_t *);
    void (*destroy)(module_data_t *);
    const module_method_t *methods;
} module_registry_t;

typedef struct
{
    const char *name;
    size_t size;
    module_type_t type;
    const module_registry_t *reg;
} module_manifest_t;

void module_add_methods(lua_State *L, const module_data_t *mod
                        , const module_method_t *list);
void module_register(lua_State *L, const module_manifest_t *manifest);
lua_State *module_lua(const module_data_t *mod) __func_pure;

bool module_option_integer(lua_State *L, const char *name, int *integer);
bool module_option_string(lua_State *L, const char *name, const char **string
                          , size_t *length);
bool module_option_boolean(lua_State *L, const char *name, bool *boolean);

#define MODULE_OPTIONS_IDX 2

#define MODULE_DATA_SIZE \
    (sizeof(void *) * 64)

#define MODULE_DATA() \
    char __padding[MODULE_DATA_SIZE]

#define MODULE_SYMBOL(_name) \
    __manifest_##_name

#define MODULE_MANIFEST_DEF(_name) \
    const module_manifest_t MODULE_SYMBOL(_name)

#define MODULE_MANIFEST_DECL(_name) \
    extern MODULE_MANIFEST_DEF(_name)

#define MODULE_REGISTER(_name) \
    extern module_registry_t __registry_##_name; \
    MODULE_MANIFEST_DEF(_name) = \
    { \
        .name = #_name, \
        .size = sizeof(module_data_t), \
        .type = MODULE_TYPE_BASIC, \
        .reg = &__registry_##_name, \
    }; \
    module_registry_t __registry_##_name =

#define BINDING_REGISTER(_name) \
    extern module_registry_t __registry_##_name; \
    MODULE_MANIFEST_DEF(_name) = \
    { \
        .name = #_name, \
        .type = MODULE_TYPE_BINDING, \
        .reg = &__registry_##_name, \
    }; \
    module_registry_t __registry_##_name =

#endif /* _LUA_MODULE_H_ */
