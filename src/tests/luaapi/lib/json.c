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

#include "../../libastra.h"
#include <astra/utils/json.h>
#include <astra/luaapi/state.h>

#include <math.h> /* for NAN and INFINITY */

#define JSON_FILE "./libastra.json"

static lua_State *L = NULL;
static int ref_decode = LUA_REFNIL;
static int ref_load = LUA_REFNIL;
static int ref_encode = LUA_REFNIL;
static int ref_save = LUA_REFNIL;

static
void push_decode(lua_State *L2)
{
    lua_rawgeti(L2, LUA_REGISTRYINDEX, ref_decode);
}

static
void push_load(lua_State *L2)
{
    lua_rawgeti(L2, LUA_REGISTRYINDEX, ref_load);
}

static
void push_encode(lua_State *L2)
{
    lua_rawgeti(L2, LUA_REGISTRYINDEX, ref_encode);
}

static
void push_save(lua_State *L2)
{
    lua_rawgeti(L2, LUA_REGISTRYINDEX, ref_save);
}

static
void setup(void)
{
    lib_setup();

    L = lua;
    lua_getglobal(L, "json");
    lua_getfield(L, -1, "decode");
    ref_decode = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, -1, "load");
    ref_load = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, -1, "encode");
    ref_encode = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, -1, "save");
    ref_save = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);
}

static
void teardown(void)
{
    if (ref_decode != LUA_REFNIL)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, ref_decode);
        ref_decode = LUA_REFNIL;
    }

    if (ref_encode != LUA_REFNIL)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, ref_encode);
        ref_encode = LUA_REFNIL;
    }

    ck_assert(lua_gettop(L) == 0);
    ck_assert(unlink(JSON_FILE) != 0);
    L = NULL;

    lib_teardown();
}

/* pre-defined test strings */
static
const char *vec_list[][3] =
{
    /*
     * decode and reencode both succeed
     *   v[0]: case name
     *   v[1]: initial input to json.decode()
     *   v[2]: expected output from json.encode()
     */
    {
        "array_arraysWithSpaces",
        "[[]   ]",
        "[[]]",
    },
    {
        "array_empty",
        "[]",
        "[]",
    },
    {
        "array_empty-string",
        "[\"\"]",
        "[\"\"]",
    },
    {
        "array_ending_with_newline",
        "[\"a\"]",
        "[\"a\"]",
    },
    {
        "array_false",
        "[false]",
        "[false]",
    },
    {
        "array_heterogeneous",
        "[null, 1, \"1\", {}]",
        "[1,\"1\",[]]",
    },
    {
        "array_null",
        "[null]",
        "[]",
    },
    {
        "array_with_1_and_newline",
        "[1\n]",
        "[1]",
    },
    {
        "array_with_leading_space",
        " [1]",
        "[1]",
    },
    {
        "array_with_several_null",
        "[1,null,null,null,2]",
        "[1,2]",
    },
    {
        "array_with_trailing_space",
        "[2] ",
        "[2]",
    },
    {
        "number_0e+1",
        "[0e+1]",
        "[0]",
    },
    {
        "number_0e1",
        "[0e1]",
        "[0]",
    },
    {
        "number_after_space",
        "[ 4]",
        "[4]",
    },
    {
        "number_double_close_to_zero",
        "[-0.00000000000000000000000000000000000000"
        "0000000000000000000000000000000000000001]\n",
#ifdef _WIN32
        /* okay, what the fuck */
        "[-1e-078]",
#else
        "[-1e-78]",
#endif
    },
    {
        "number_int_with_exp",
        "[20e1]",
        "[200]",
    },
    {
        "number",
        "[123e65]",
#ifdef _WIN32
        /* a couple more of these ahead */
        "[1.23e+067]",
#else
        "[1.23e+67]",
#endif
    },
    {
        "number_negative_int",
        "[-123]",
        "[-123]",
    },
    {
        "number_negative_one",
        "[-1]",
        "[-1]",
    },
    {
        "number_negative_zero",
        "[-0]",
#if LUA_VERSION_NUM >= 503
        "[0]",
#else
        "[-0]",
#endif
    },
    {
        "number_real_capital_e",
        "[1E22]",
#ifdef _WIN32
        "[1e+022]",
#else
        "[1e+22]",
#endif
    },
    {
        "number_real_capital_e_neg_exp",
        "[1E-2]",
        "[0.01]",
    },
    {
        "number_real_capital_e_pos_exp",
        "[1E+2]",
        "[100]",
    },
    {
        "number_real_exponent",
        "[123e45]",
#ifdef _WIN32
        "[1.23e+047]",
#else
        "[1.23e+47]",
#endif
    },
    {
        "number_real_fraction_exponent",
        "[123.456e78]",
#ifdef _WIN32
        "[1.23456e+080]",
#else
        "[1.23456e+80]",
#endif
    },
    {
        "number_real_neg_exp",
        "[1e-2]",
        "[0.01]",
    },
    {
        "number_real_pos_exponent",
        "[1e+2]",
        "[100]",
    },
    {
        "number_simple_int",
        "[123]",
        "[123]",
    },
    {
        "number_simple_real",
        "[123.456789]",
        "[123.456789]",
    },
    {
        "object_basic",
        "{\"asd\":\"sdf\"}",
        "{\"asd\":\"sdf\"}",
    },
    {
        "object_duplicated_key_and_value",
        "{\"a\":\"b\",\"a\":\"b\"}",
        "{\"a\":\"b\"}",
    },
    {
        "object_duplicated_key",
        "{\"a\":\"b\",\"a\":\"c\"}",
        "{\"a\":\"c\"}",
    },
    {
        "object_empty",
        "{}",
        "[]",
    },
    {
        "object_empty_key",
        "{\"\":0}",
        "{\"\":0}",
    },
    {
        "object_escaped_null_in_key",
        "{\"foo\\u0000bar\": 42}",
        "{\"foo\\u0000bar\":42}",
    },
    {
        "object_extreme_numbers",
        "[ -1.0e+28, \t1.0e+28 ]",
#ifdef _WIN32
        "[-1e+028,1e+028]",
#else
        "[-1e+28,1e+28]",
#endif
    },
    {
        "object",
        "{\"asd\":\"sdf\" , }",
        "{\"asd\":\"sdf\"}",
    },
    {
        "object_long_strings",
        "{\"x\":[{\"id\": \"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}], }",
        "{\"x\":[{\"id\":\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}]}",
    },
    {
        "object_simple",
        "{\"a\":[]}",
        "{\"a\":[]}",
    },
    {
        "object_string_unicode",
        "{\"title\":\"\\u041f\\u043e\\u043b\\u0442\\u043e\\u0440\\u0430 "
        "\\u0417\\u0435\\u043c\\u043b\\u0435\\u043a\\u043e\\u043f\\u0430\" }",
        "{\"title\":\"\xd0\x9f\xd0\xbe\xd0\xbb\xd1\x82\xd0\xbe\xd1\x80\xd0"
        "\xb0 \xd0\x97\xd0\xb5\xd0\xbc\xd0\xbb\xd0\xb5\xd0\xba\xd0\xbe\xd0"
        "\xbf\xd0\xb0\"}",
    },
    {
        "object_with_newlines",
        "{\n\"a\": \"b\"\n}",
        "{\"a\":\"b\"}",
    },
    {
        "string_1_2_3_bytes_UTF-8_sequences",
        "[\"\\u0060\\u012a\\u12AB\"]",
        "[\"`\xc4\xaa\xe1\x8a\xab\"]",
    },
    {
        "string_accepted_surrogate_pair",
        "[\"\\uD801\\udc37\"]",
        "[\"\xf0\x90\x90\xb7\"]",
    },
    {
        "string_accepted_surrogate_pairs",
        "[\"\\ud83d\\ude39\\ud83d\\udc8d\"]",
        "[\"\xf0\x9f\x98\xb9\xf0\x9f\x92\x8d\"]",
    },
    {
        "string_allowed_escapes",
        "[\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"]",
        "[\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"]",
    },
    {
        "string_backslash_and_u_escaped_zero",
        "[\"\\\\u0000\"]",
        "[\"\\\\u0000\"]",
    },
    {
        "string_backslash_doublequotes",
        "[\"\\\"\"]",
        "[\"\\\"\"]",
    },
    {
        "string_comments",
        "[\"a/*b*/c/*d//e\"]",
        "[\"a\\/*b*\\/c\\/*d\\/\\/e\"]",
    },
    {
        "string_double_escape_a",
        "[\"\\\\a\"]",
        "[\"\\\\a\"]",
    },
    {
        "string_double_escape_n",
        "[\"\\\\n\"]",
        "[\"\\\\n\"]",
    },
    {
        "string_escaped_control_character",
        "[\"\\u0012\"]",
        "[\"\\u0012\"]",
    },
    {
        "string_escaped_noncharacter",
        "[\"\\uFFFF\"]",
        "[\"\xef\xbf\xbf\"]",
    },
    {
        "string_in_array",
        "[\"asd\"]",
        "[\"asd\"]",
    },
    {
        "string_in_array_with_leading_space",
        "[ \"asd\"]",
        "[\"asd\"]",
    },
    {
        "string_last_surrogates_1_and_2",
        "[\"\\uDBFF\\uDFFF\"]",
        "[\"\xf4\x8f\xbf\xbf\"]",
    },
    {
        "string_nbsp_uescaped",
        "[\"new\\u00A0line\"]",
        "[\"new\xc2\xa0line\"]",
    },
    {
        "string_nonCharacterInUTF-8_U+10FFFF",
        "[\"\xf4\x8f\xbf\xbf\"]",
        "[\"\xf4\x8f\xbf\xbf\"]",
    },
    {
        "string_nonCharacterInUTF-8_U+1FFFF",
        "[\"\xf0\x9b\xbf\xbf\"]",
        "[\"\xf0\x9b\xbf\xbf\"]",
    },
    {
        "string_nonCharacterInUTF-8_U+FFFF",
        "[\"\xef\xbf\xbf\"]",
        "[\"\xef\xbf\xbf\"]",
    },
    {
        "string_null_escape",
        "[\"\\u0000\"]",
        "[\"\\u0000\"]",
    },
    {
        "string_one-byte-utf-8",
        "[\"\\u002c\"]",
        "[\",\"]",
    },
    {
        "string_pi",
        "[\"\xcf\x80\"]",
        "[\"\xcf\x80\"]",
    },
    {
        "string_simple_ascii",
        "[\"asd \"]",
        "[\"asd \"]",
    },
    {
        "string_space",
        "\" \"",
        "\" \"",
    },
    {
        "string_surrogates_U+1D11E_MUSICAL_SYMBOL_G_CLEF",
        "[\"\\uD834\\uDd1e\"]",
        "[\"\xf0\x9d\x84\x9e\"]",
    },
    {
        "string_three-byte-utf-8",
        "[\"\\u0821\"]",
        "[\"\xe0\xa0\xa1\"]",
    },
    {
        "string_two-byte-utf-8",
        "[\"\\u0123\"]",
        "[\"\xc4\xa3\"]",
    },
    {
        "string_u+2028_line_sep",
        "[\"\xe2\x80\xa8\"]",
        "[\"\xe2\x80\xa8\"]",
    },
    {
        "string_u+2029_par_sep",
        "[\"\xe2\x80\xa9\"]",
        "[\"\xe2\x80\xa9\"]",
    },
    {
        "string_uescaped_newline",
        "[\"new\\u000Aline\"]",
        "[\"new\\nline\"]",
    },
    {
        "string_uEscape",
        "[\"\\u0061\\u30af\\u30EA\\u30b9\"]",
        "[\"a\xe3\x82\xaf\xe3\x83\xaa\xe3\x82\xb9\"]",
    },
    {
        "string_unescaped_char_delete",
        "[\"\x7f\"]",
        "[\"\x7f\"]",
    },
    {
        "string_unicode_2",
        "[\"\xe2\x8d\x82\xe3\x88\xb4\xe2\x8d\x82\"]",
        "[\"\xe2\x8d\x82\xe3\x88\xb4\xe2\x8d\x82\"]",
    },
    {
        "string_unicodeEscapedBackslash",
        "[\"\\u005C\"]",
        "[\"\\\\\"]",
    },
    {
        "string_unicode_escaped_double_quote",
        "[\"\\u0022\"]",
        "[\"\\\"\"]",
    },
    {
        "string_unicode",
        "[\"\\uA66D\"]",
        "[\"\xea\x99\xad\"]",
    },
    {
        "string_unicode_U+10FFFE_nonchar",
        "[\"\\uDBFF\\uDFFE\"]",
        "[\"\xf4\x8f\xbf\xbe\"]",
    },
    {
        "string_unicode_U+1FFFE_nonchar",
        "[\"\\uD83F\\uDFFE\"]",
        "[\"\xf0\x9f\xbf\xbe\"]",
    },
    {
        "string_unicode_U+200B_ZERO_WIDTH_SPACE",
        "[\"\\u200B\"]",
        "[\"\xe2\x80\x8b\"]",
    },
    {
        "string_unicode_U+2064_invisible_plus",
        "[\"\\u2064\"]",
        "[\"\xe2\x81\xa4\"]",
    },
    {
        "string_unicode_U+FDD0_nonchar",
        "[\"\\uFDD0\"]",
        "[\"\xef\xb7\x90\"]",
    },
    {
        "string_unicode_U+FFFE_nonchar",
        "[\"\\uFFFE\"]",
        "[\"\xef\xbf\xbe\"]",
    },
    {
        "string_utf8",
        "[\"\xe2\x82\xac\xf0\x9d\x84\x9e\"]",
        "[\"\xe2\x82\xac\xf0\x9d\x84\x9e\"]",
    },
    {
        "string_with_del_character",
        "[\"a\x7f" "a\"]",
        "[\"a\x7f" "a\"]",
    },
    {
        "structure_lonely_false",
        "false",
        "false",
    },
    {
        "structure_lonely_int",
        "42",
        "42",
    },
    {
        "structure_lonely_negative_real",
        "-0.1",
        "-0.1",
    },
    {
        "structure_lonely_null",
        "null",
        "null",
    },
    {
        "structure_lonely_string",
        "\"asd\"",
        "\"asd\"",
    },
    {
        "structure_lonely_true",
        "true",
        "true",
    },
    {
        "structure_string_empty",
        "\"\"",
        "\"\"",
    },
    {
        "structure_trailing_newline",
        "[\"a\"]\n",
        "[\"a\"]",
    },
    {
        "structure_true_in_array",
        "[true]",
        "[true]",
    },
    {
        "structure_whitespace_array",
        " [] ",
        "[]",
    },
    {
        "comment_beginning",
        "/*comment*/\n[]",
        "[]",
    },
    {
        "comment_object",
        "{\"a\":\n\t/*test*/\n1}",
        "{\"a\":1}",
    },
    {
        "comment_array",
        "[1, /*a*/ 2, \t/**/]",
        "[1,2]",
    },
    {
        "comment_array_empty",
        "[,,/**/,]\n",
        "[]",
    },
    {
        "comment_lonely_string",
        "/*test*/\n\"str\"",
        "\"str\"",
    },
    {
        "comment_multi",
        "/**/{,/**/\"a\":/**/[1,/**/2],/**/,}",
        "{\"a\":[1,2]}",
    },

    /*
     * decode fails
     *   v[1]: input to json.decode()
     *   v[2]: NULL
     */
    {
        "string_1st_surrogate_but_2nd_missing",
        "[\"\\uDADA\"]",
        NULL,
    },
    {
        "string_1st_valid_surrogate_2nd_invalid",
        "[\"\\uD888\\u1234\"]",
        NULL,
    },
    {
        "string_incomplete_surrogate_and_escape_valid",
        "[\"\\uD800\\n\"]",
        NULL,
    },
    {
        "string_incomplete_surrogates_escape_valid",
        "[\"\\uD800\\uD800\\n\"]",
        NULL,
    },
    {
        "string_invalid_lonely_surrogate",
        "[\"\\ud800\"]",
        NULL,
    },
    {
        "string_invalid_surrogate",
        "[\"\\ud800abc\"]",
        NULL,
    },
    {
        "string_inverted_surrogates_U+1D11E",
        "[\"\\uDd1e\\uD834\"]",
        NULL,
    },
    {
        "string_utf16LE_no_BOM",
        "[\0\"\0\xe9\0\"\0]\0",
        NULL,
    },
    {
        "string_UTF-16LE_with_BOM",
        "\xff\xfe[\0\"\0\xe9\0\"\0]\0",
        NULL,
    },
    {
        "structure_UTF-8_BOM_empty_object",
        "\xef\xbb\xbf{}",
        NULL,
    },
    {
        "array_1_true_without_comma",
        "[1 true]",
        NULL,
    },
    {
        "array_a_invalid_utf8",
        "[a\xe5]",
        NULL,
    },
    {
        "array_colon_instead_of_comma",
        "[\"\": 1]",
        NULL,
    },
    {
        "array_comma_after_close",
        "[\"\"],",
        NULL,
    },
    {
        "array_extra_close",
        "[\"x\"]]",
        NULL,
    },
    {
        "array_incomplete_invalid_value",
        "[x",
        NULL,
    },
    {
        "array_incomplete",
        "[\"x\"",
        NULL,
    },
    {
        "array_inner_array_no_comma",
        "[3[4]]",
        NULL,
    },
    {
        "array_invalid_utf8",
        "[\xff]",
        NULL,
    },
    {
        "array_items_separated_by_semicolon",
        "[1:2]",
        NULL,
    },
    {
        "array_just_minus",
        "[-]",
        NULL,
    },
    {
        "array_newlines_unclosed",
        "[\"a\",\n4\n,1,",
        NULL,
    },
    {
        "array_spaces_vertical_tab_formfeed",
        "[\"\va\"\\f]",
        NULL,
    },
    {
        "array_star_inside",
        "[*]",
        NULL,
    },
    {
        "array_unclosed",
        "[\"\"",
        NULL,
    },
    {
        "array_unclosed_trailing_comma",
        "[1,",
        NULL,
    },
    {
        "array_unclosed_with_new_lines",
        "[1,\n1\n,1",
        NULL,
    },
    {
        "array_unclosed_with_object_inside",
        "[{}",
        NULL,
    },
    {
        "incomplete_false",
        "[fals]",
        NULL,
    },
    {
        "incomplete_null",
        "[nul]",
        NULL,
    },
    {
        "incomplete_true",
        "[tru]",
        NULL,
    },
    {
        "number_0.1.2",
        "[0.1.2]",
        NULL,
    },
    {
        "number_0.3e",
        "[0.3e]",
        NULL,
    },
    {
        "number_0.3e+",
        "[0.3e+]",
        NULL,
    },
    {
        "number_0_capital_E",
        "[0E]",
        NULL,
    },
    {
        "number_0_capital_E+",
        "[0E+]",
        NULL,
    },
    {
        "number_0e",
        "[0e]",
        NULL,
    },
    {
        "number_0e+",
        "[0e+]",
        NULL,
    },
    {
        "number_1_000",
        "[1 000.0]",
        NULL,
    },
    {
        "number_1.0e-",
        "[1.0e-]",
        NULL,
    },
    {
        "number_1.0e",
        "[1.0e]",
        NULL,
    },
    {
        "number_1.0e+",
        "[1.0e+]",
        NULL,
    },
    {
        "number_-1.0.",
        "[-1.0.]",
        NULL,
    },
    {
        "number_1eE2",
        "[1eE2]",
        NULL,
    },
    {
        "number_.-1",
        "[.-1]",
        NULL,
    },
    {
        "number_+1",
        "[+1]",
        NULL,
    },
    {
        "number_9.e+",
        "[9.e+]",
        NULL,
    },
    {
        "number_expression",
        "[1+2]",
        NULL,
    },
    {
        "number_hex_1_digit",
        "[0x1]",
        NULL,
    },
    {
        "number_hex_2_digits",
        "[0x42]",
        NULL,
    },
    {
        "number_infinity",
        "[Infinity]",
        NULL,
    },
    {
        "number_+Inf",
        "[+Inf]",
        NULL,
    },
    {
        "number_Inf",
        "[Inf]",
        NULL,
    },
    {
        "number_invalid+-",
        "[0e+-1]",
        NULL,
    },
    {
        "number_invalid-negative-real",
        "[-123.123foo]",
        NULL,
    },
    {
        "number_invalid-utf-8-in-bigger-int",
        "[123\xe5]",
        NULL,
    },
    {
        "number_invalid-utf-8-in-exponent",
        "[1e1\xe5]",
        NULL,
    },
    {
        "number_invalid-utf-8-in-int",
        "[0\xe5]\n",
        NULL,
    },
    {
        "number_++",
        "[++1234]",
        NULL,
    },
    {
        "number_minus_infinity",
        "[-Infinity]",
        NULL,
    },
    {
        "number_minus_sign_with_trailing_garbage",
        "[-foo]",
        NULL,
    },
    {
        "number_minus_space_1",
        "[- 1]",
        NULL,
    },
    {
        "number_-NaN",
        "[-NaN]",
        NULL,
    },
    {
        "number_NaN",
        "[NaN]",
        NULL,
    },
    {
        "number_neg_with_garbage_at_end",
        "[-1x]",
        NULL,
    },
    {
        "number_real_garbage_after_e",
        "[1ea]",
        NULL,
    },
    {
        "number_real_with_invalid_utf8_after_e",
        "[1e\xe5]",
        NULL,
    },
    {
        "number_U+FF11_fullwidth_digit_one",
        "[\xef\xbc\x91]",
        NULL,
    },
    {
        "number_with_alpha_char",
        "[1.8011670033376514H-308]",
        NULL,
    },
    {
        "number_with_alpha",
        "[1.2a-3]",
        NULL,
    },
    {
        "object_bad_value",
        "[\"x\", truth]",
        NULL,
    },
    {
        "object_bracket_key",
        "{[: \"x\"}\n",
        NULL,
    },
    {
        "object_comma_instead_of_colon",
        "{\"x\", null}",
        NULL,
    },
    {
        "object_double_colon",
        "{\"x\"::\"b\"}",
        NULL,
    },
    {
        "object_emoji",
        "{\xf0\x9f\x87\xa8\xf0\x9f\x87\xad}",
        NULL,
    },
    {
        "object_garbage_at_end",
        "{\"a\":\"a\" 123}",
        NULL,
    },
    {
        "object_key_with_single_quotes",
        "{key: \'value\'}",
        NULL,
    },
    {
        "object_missing_colon",
        "{\"a\" b}",
        NULL,
    },
    {
        "object_missing_key",
        "{:\"b\"}",
        NULL,
    },
    {
        "object_missing_semicolon",
        "{\"a\" \"b\"}",
        NULL,
    },
    {
        "object_missing_value",
        "{\"a\":",
        NULL,
    },
    {
        "object_no-colon",
        "{\"a\"",
        NULL,
    },
    {
        "object_non_string_key_but_huge_number_instead",
        "{9999E9999:1}",
        NULL,
    },
    {
        "object_non_string_key",
        "{1:1}",
        NULL,
    },
    {
        "object_repeated_null_null",
        "{null:null,null:null}",
        NULL,
    },
    {
        "object_single_quote",
        "{\'a\':0}",
        NULL,
    },
    {
        "object_trailing_comment",
        "{\"a\":\"b\"}/**/",
        NULL,
    },
    {
        "object_trailing_comment_open",
        "{\"a\":\"b\"}/**//",
        NULL,
    },
    {
        "object_trailing_comment_slash_open_incomplete",
        "{\"a\":\"b\"}/",
        NULL,
    },
    {
        "object_trailing_comment_slash_open",
        "{\"a\":\"b\"}//",
        NULL,
    },
    {
        "object_unquoted_key",
        "{a: \"b\"}",
        NULL,
    },
    {
        "object_unterminated-value",
        "{\"a\":\"a",
        NULL,
    },
    {
        "object_with_single_string",
        "{ \"foo\" : \"bar\", \"a\" }",
        NULL,
    },
    {
        "object_with_trailing_garbage",
        "{\"a\":\"b\"}#",
        NULL,
    },
    {
        "single_space",
        " ",
        NULL,
    },
    {
        "string_1_surrogate_then_escape",
        "[\"\\uD800\\\"]",
        NULL,
    },
    {
        "string_1_surrogate_then_escape_u1",
        "[\"\\uD800\\u1\"]",
        NULL,
    },
    {
        "string_1_surrogate_then_escape_u1x",
        "[\"\\uD800\\u1x\"]",
        NULL,
    },
    {
        "string_1_surrogate_then_escape_u",
        "[\"\\uD800\\u\"]",
        NULL,
    },
    {
        "string_accentuated_char_no_quotes",
        "[\xc3\xa9]",
        NULL,
    },
    {
        "string_backslash_00",
        "[\"\\\0\"]",
        NULL,
    },
    {
        "string_escaped_backslash_bad",
        "[\"\\\\\\\"]",
        NULL,
    },
    {
        "string_escaped_ctrl_char_tab",
        "[\"\\\t\"]",
        NULL,
    },
    {
        "string_escaped_emoji",
        "[\"\\\xf0\x9f\x8c\x80\"]",
        NULL,
    },
    {
        "string_escape_x",
        "[\"\\x00\"]",
        NULL,
    },
    {
        "string_incomplete_escaped_character",
        "[\"\\u00A\"]",
        NULL,
    },
    {
        "string_incomplete_escape",
        "[\"\\\"]",
        NULL,
    },
    {
        "string_incomplete_unicode",
        "[\"\\u",
        NULL,
    },
    {
        "string_incomplete_surrogate_escape_invalid",
        "[\"\\uD800\\uD800\\x\"]",
        NULL,
    },
    {
        "string_incomplete_surrogate",
        "[\"\\uD834\\uDd\"]",
        NULL,
    },
    {
        "string_incomplete_surrogate2",
        "[\"\\uD832\\",
        NULL,
    },
    {
        "string_incomplete_surrogate3",
        "[\"\\uD832\\u",
        NULL,
    },
    {
        "string_invalid_backslash_esc",
        "[\"\\a\"]",
        NULL,
    },
    {
        "string_invalid_unicode_escape",
        "[\"\\uqqqq\"]",
        NULL,
    },
    {
        "string_invalid_utf8_after_escape",
        "[\"\\\xe5\"]",
        NULL,
    },
    {
        "string_invalid-utf-8-in-escape",
        "[\"\\u\xe5\"]",
        NULL,
    },
    {
        "string_leading_uescaped_thinspace",
        "[\\u0020\"asd\"]",
        NULL,
    },
    {
        "string_no_quotes_with_bad_escape",
        "[\\n]",
        NULL,
    },
    {
        "string_single_doublequote",
        "\"",
        NULL,
    },
    {
        "string_single_quote",
        "[\'single quote\']",
        NULL,
    },
    {
        "string_single_string_no_double_quotes",
        "abc",
        NULL,
    },
    {
        "string_start_escape_unclosed",
        "[\"\\",
        NULL,
    },
    {
        "string_unescaped_crtl_char",
        "[\"a\0a\"]",
        NULL,
    },
    {
        "string_unicode_CapitalU",
        "\"\\UA66D\"",
        NULL,
    },
    {
        "string_with_trailing_garbage",
        "\"\"x",
        NULL,
    },
    {
        "string_lonely_escape_unclosed",
        "\"\\",
        NULL,
    },
    {
        "structure_angle_bracket_.",
        "<.>",
        NULL,
    },
    {
        "structure_angle_bracket_null",
        "[<null>]",
        NULL,
    },
    {
        "structure_array_trailing_garbage",
        "[1]x",
        NULL,
    },
    {
        "structure_array_with_extra_array_close",
        "[1]]",
        NULL,
    },
    {
        "structure_array_with_unclosed_string",
        "[\"asd]",
        NULL,
    },
    {
        "structure_ascii-unicode-identifier",
        "a\xc3\xa5",
        NULL,
    },
    {
        "structure_capitalized_True",
        "[True]",
        NULL,
    },
    {
        "structure_close_unopened_array",
        "1]",
        NULL,
    },
    {
        "structure_comma_instead_of_closing_brace",
        "{\"x\": true,",
        NULL,
    },
    {
        "structure_double_array",
        "[][]",
        NULL,
    },
    {
        "structure_end_array",
        "]",
        NULL,
    },
    {
        "structure_incomplete_UTF8_BOM",
        "\xef\xbb{}",
        NULL,
    },
    {
        "structure_lone-invalid-utf-8",
        "\xe5",
        NULL,
    },
    {
        "structure_lone-open-bracket",
        "[",
        NULL,
    },
    {
        "structure_null-byte-outside-string",
        "[\0]",
        NULL,
    },
    {
        "structure_number_with_trailing_garbage",
        "2@",
        NULL,
    },
    {
        "structure_object_followed_by_closing_object",
        "{}}",
        NULL,
    },
    {
        "structure_object_unclosed_no_value",
        "{\"\":",
        NULL,
    },
    {
        "structure_object_with_trailing_garbage",
        "{\"a\": true} \"x\"",
        NULL,
    },
    {
        "structure_open_array_apostrophe",
        "[\'",
        NULL,
    },
    {
        "structure_open_array_comma",
        "[,",
        NULL,
    },
    {
        "structure_open_array_open_object",
        "[{",
        NULL,
    },
    {
        "structure_open_array_open_string",
        "[\"a",
        NULL,
    },
    {
        "structure_open_array_string",
        "[\"a\"",
        NULL,
    },
    {
        "structure_open_object_close_array",
        "{]",
        NULL,
    },
    {
        "structure_open_object_comma",
        "{,",
        NULL,
    },
    {
        "structure_open_object",
        "{",
        NULL,
    },
    {
        "structure_open_object_open_array",
        "{[",
        NULL,
    },
    {
        "structure_open_object_open_string",
        "{\"a",
        NULL,
    },
    {
        "structure_open_object_string_with_apostrophes",
        "{\'a\'",
        NULL,
    },
    {
        "structure_open_open",
        "[\"\\{[\"\\{[\"\\{[\"\\{",
        NULL,
    },
    {
        "structure_single_eacute",
        "\xe9",
        NULL,
    },
    {
        "structure_single_star",
        "*",
        NULL,
    },
    {
        "structure_trailing_#",
        "{\"a\":\"b\"}#{}",
        NULL,
    },
    {
        "structure_U+2060_word_joined",
        "[\xe2\x81\xa0]",
        NULL,
    },
    {
        "structure_uescaped_LF_before_string",
        "[\\u000A\"\"]",
        NULL,
    },
    {
        "structure_unclosed_array",
        "[1",
        NULL,
    },
    {
        "structure_unclosed_array_partial_null",
        "[ false, nul",
        NULL,
    },
    {
        "structure_unclosed_array_unfinished_false",
        "[ true, fals",
        NULL,
    },
    {
        "structure_unclosed_array_unfinished_true",
        "[ false, tru",
        NULL,
    },
    {
        "structure_unclosed_object",
        "{\"asd\":\"asd\"",
        NULL,
    },
    {
        "structure_unicode-identifier",
        "\xc3\xa5",
        NULL,
    },
    {
        "structure_UTF8_BOM_no_data",
        "\xef\xbb\xbf",
        NULL,
    },
    {
        "structure_whitespace_formfeed",
        "[\f]",
        NULL,
    },
    {
        "structure_whitespace_U+2060_word_joiner",
        "[\xe2\x81\xa0]",
        NULL,
    },
    {
        "comment_no_stars",
        "/comment/{}",
        NULL,
    },
    {
        "comment_eof",
        "{\"a\":/",
        NULL,
    },
    {
        "comment_eofb",
        "{\"a\":/*t",
        NULL,
    },
    {
        "comment_eofc",
        "{\"a\":/*t\n\t\v*",
        NULL,
    },
    {
        "comment_eof2",
        "[/",
        NULL,
    },
    {
        "comment_eof2b",
        "[/*abc",
        NULL,
    },
    {
        "comment_eof2c",
        "[/*abc*",
        NULL,
    },
    {
        "comment_eof3",
        "{/",
        NULL,
    },
    {
        "comment_eof3b",
        "{/*",
        NULL,
    },
    {
        "comment_eof3c",
        "{/**",
        NULL,
    },
    {
        "comment_trailing",
        "{}/*comment*/",
        NULL,
    },
    {
        "comment_lonely",
        "/*test*/",
        NULL,
    },
    {
        "comment_lonely_unclosed",
        "/*test",
        NULL,
    },
    {
        "comment_lonely_unclosedb",
        "/*test*",
        NULL,
    },
    {
        "comment_object_before_colon",
        "{\"a\"/*comment*/:1}",
        NULL,
    },
    {
        "comment_array_before_comma",
        "[1\n/*test*/, 2]",
        NULL,
    },

    /*
     * reencode fails
     *   v[1]: NULL
     *   v[2]: input to json.decode() before trying to reencode
     */
    {
        "number_huge_exp",
        NULL,
        "[0.4e0066999999999999999999999999999999999999999999999999999999999999"
        "9999999999999999999999999999999999999999999999999999999969999999006]",
    },
    {
        "number_neg_int_huge_exp",
        NULL,
        "[-1e+9999]",
    },
    {
        "number_pos_double_huge_exp",
        NULL,
        "[1.5e+9999]",
    },
    {
        "number_real_neg_overflow",
        NULL,
        "[-123123e100000]",
    },
    {
        "number_real_pos_overflow",
        NULL,
        "[123123e100000]",
    },

    { NULL, NULL, NULL },
};

START_TEST(test_vectors)
{
    for (size_t i = 0; i < ASC_ARRAY_SIZE(vec_list); i++)
    {
        const char *const testcase = vec_list[i][0];
        const char *const json1 = vec_list[i][1];
        const char *const json2 = vec_list[i][2];

        if (json1 && json2)
        {
            push_decode(L);
            lua_pushstring(L, json1);
            lua_call(L, 1, 1);
            ck_assert(lua_gettop(L) == 1);

            push_encode(L);
            lua_insert(L, -2);
            lua_call(L, 1, 1);
            ck_assert(lua_gettop(L) == 1);
            ck_assert(lua_type(L, -1) == LUA_TSTRING);
            ck_assert_msg(!strcmp(json2, lua_tostring(L, -1))
                          , "test_vectors: %s:\n%s\n%s"
                          , testcase, lua_tostring(L, -1), json2);
            lua_pop(L, 1);
        }
        else if (json1 && !json2)
        {
            push_decode(L);
            lua_pushstring(L, json1);
            ck_assert_msg(lua_pcall(L, 1, 1, 0) != 0, "expected %s to fail"
                          , testcase);
            ck_assert(lua_gettop(L) == 1);
            ck_assert(lua_type(L, -1) == LUA_TSTRING); /* error msg */
            asc_log_debug("test_vectors: %s (expected error): %s"
                          , testcase, lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        else if (!json1 && json2)
        {
            push_decode(L);
            lua_pushstring(L, json2);
            lua_call(L, 1, 1);
            ck_assert(lua_gettop(L) == 1);
            ck_assert(!lua_isnil(L, -1));

            push_encode(L);
            lua_insert(L, -2);
            ck_assert_msg(lua_pcall(L, 1, 1, 0) != 0, "expected %s to fail"
                          , testcase);
            ck_assert(lua_gettop(L) == 1);
            ck_assert(lua_type(L, -1) == LUA_TSTRING); /* error msg */
            asc_log_debug("test_vectors: %s (expected error): %s"
                          , testcase, lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        else if (!json1 && !json2)
        {
            break;
        }
    }
}
END_TEST

/* encode and decode lonely values */
START_TEST(lonely_values)
{
    /* boolean */
    push_encode(L);
    lua_pushboolean(L, 1);
    lua_call(L, 1, 1);
    ck_assert(lua_gettop(L) == 1 && lua_type(L, -1) == LUA_TSTRING);
    ck_assert(!strcmp(lua_tostring(L, -1), "true"));
    push_decode(L);
    lua_insert(L, -2);
    lua_call(L, 1, 1);
    ck_assert(lua_gettop(L) == 1 && lua_type(L, -1) == LUA_TBOOLEAN);
    ck_assert(lua_toboolean(L, -1) == 1);
    lua_pop(L, 1);

    /* number */
    push_encode(L);
    lua_pushnumber(L, 3.14);
    lua_call(L, 1, 1);
    ck_assert(lua_gettop(L) == 1 && lua_type(L, -1) == LUA_TSTRING);
    ck_assert(!strcmp(lua_tostring(L, -1), "3.14"));
    push_decode(L);
    lua_insert(L, -2);
    lua_call(L, 1, 1);
    ck_assert(lua_gettop(L) == 1 && lua_type(L, -1) == LUA_TNUMBER);
    ck_assert(lua_tonumber(L, -1) > 3.13 && lua_tonumber(L, -1) < 3.15);
    lua_pop(L, 1);

    push_encode(L);
    lua_pushnumber(L, (double)(NAN));
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    ck_assert(lua_isstring(L, -1));
    asc_log_debug("encode NaN: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    push_encode(L);
    lua_pushnumber(L, (double)(-INFINITY));
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    ck_assert(lua_isstring(L, -1));
    asc_log_debug("encode -INFINITY: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    push_encode(L);
    lua_pushnumber(L, (double)(+INFINITY));
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    ck_assert(lua_isstring(L, -1));
    asc_log_debug("encode +INFINITY: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* string */
    push_encode(L);
    lua_pushliteral(L, "testtesttest");
    lua_call(L, 1, 1);
    ck_assert(lua_gettop(L) == 1 && lua_type(L, -1) == LUA_TSTRING);
    ck_assert(!strcmp(lua_tostring(L, -1), "\"testtesttest\""));
    push_decode(L);
    lua_insert(L, -2);
    lua_call(L, 1, 1);
    ck_assert(lua_gettop(L) == 1 && lua_type(L, -1) == LUA_TSTRING);
    ck_assert(!strcmp(lua_tostring(L, -1), "testtesttest"));
    lua_pop(L, 1);

    push_decode(L);
    lua_pushlstring(L, "\"123\"\0", 6); /* trailing garbage NUL */
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    ck_assert(lua_isstring(L, -1));
    asc_log_debug("decode trailing NUL: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    push_decode(L);
    lua_pushlstring(L, "\0\"test\"", 7); /* string starts with NUL */
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    ck_assert(lua_isstring(L, -1));
    asc_log_debug("decode starting NUL: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* null */
    push_encode(L);
    lua_pushnil(L);
    lua_call(L, 1, 1);
    ck_assert(lua_gettop(L) == 1 && lua_type(L, -1) == LUA_TSTRING);
    ck_assert(!strcmp(lua_tostring(L, -1), "null"));
    push_decode(L);
    lua_insert(L, -2);
    lua_call(L, 1, 1);
    ck_assert(lua_gettop(L) == 1 && lua_type(L, -1) == LUA_TNIL);
    lua_pop(L, 1);

    /* lightuserdata (should fail) */
    push_encode(L);
    lua_pushlightuserdata(L, NULL);
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    ck_assert(lua_isstring(L, -1));
    asc_log_debug("encode userdata: %s", lua_tostring(L, -1));
    lua_pop(L, 1);
}
END_TEST

/* infinite nested tables */
#define TEST_DEPTH 1000000

START_TEST(nesting_depth)
{
    /* encode infinite Lua table */
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_rawseti(L, -2, 1);
    push_encode(L);
    lua_insert(L, -2);
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    ck_assert(lua_isstring(L, -1));
    asc_log_debug("encode depth array: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushstring(L, "key");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);
    push_encode(L);
    lua_insert(L, -2);
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    ck_assert(lua_isstring(L, -1));
    asc_log_debug("encode depth object: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* decode million opening braces */
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    for (size_t i = rand() % 2; i < TEST_DEPTH; i++)
    {
        if (i % 2)
            luaL_addstring(&b, "[");
        else
            luaL_addstring(&b, "{\"a\":");
    }
    luaL_pushresult(&b);

    push_decode(L);
    lua_insert(L, -2);
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    ck_assert(lua_isstring(L, -1));
    asc_log_debug("decode depth: %s", lua_tostring(L, -1));
    lua_pop(L, 1);
}
END_TEST

/* escape sequences */
static inline
unsigned int decode_surrogate(unsigned int hi, unsigned int lo)
{
    return (((hi & 0x3ff) << 10) | (lo & 0x3ff)) + 0x10000;
}

static inline
bool hi_surrogate(unsigned int cp)
{
    return ((cp & 0xfc00) == 0xd800);
}

static inline
bool lo_surrogate(unsigned int cp)
{
    return ((cp & 0xfc00) == 0xdc00);
}

static
unsigned int utf8_decode(const uint8_t *s)
{
    unsigned int cp;

    if (!(s[0] & 0x80))
    {
        /* no continuation */
        return s[0];
    }
    else if ((s[0] & 0xe0) == 0xc0)
    {
        /* one more byte */
        cp = ((s[0] & 0x1f) << 6) | (s[1] & 0x3f);

        if (cp >= 0x80)
            return cp;
    }
    else if ((s[0] & 0xf0) == 0xe0)
    {
        /* two more bytes */
        cp = ((s[0] & 0xf) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f);

        if (cp >= 0x800 && (cp < 0xd800 || cp > 0xdfff))
            return cp;
    }
    else if ((s[0] & 0xf8) == 0xf0)
    {
        /* three more bytes */
        cp = (((s[0] & 0x7) << 18) | ((s[1] & 0x3f) << 12)
              | ((s[2] & 0x3f) << 6) | (s[3] & 0x3f));

        if (cp >= 0x10000 && cp <= 0x10ffff)
            return cp;
    }

    return 0xffffffff;
}

static
const char *esc_list[][2] =
{
    { "/",  "\\/"  },
    { "\\", "\\\\" },
    { "\"", "\\\"" },
    { "\t", "\\t"  },
    { "\r", "\\r"  },
    { "\n", "\\n"  },
    { "\f", "\\f"  },
    { "\b", "\\b"  },
};

START_TEST(escape_sequences)
{
    /* test single-character sequences */
    for (size_t i = 0; i < ASC_ARRAY_SIZE(esc_list); i++)
    {
        char buf[16] = { 0 };

        /* encode */
        push_encode(L);
        lua_pushstring(L, esc_list[i][0]);
        lua_call(L, 1, 1);
        ck_assert(lua_gettop(L) == 1);
        ck_assert(lua_isstring(L, -1));
        snprintf(buf, sizeof(buf), "\"%s\"", esc_list[i][1]);
        ck_assert(!strcmp(buf, lua_tostring(L, -1)));
        lua_pop(L, 1);

        /* decode */
        push_decode(L);
        lua_pushstring(L, buf);
        lua_call(L, 1, 1);
        ck_assert(lua_gettop(L) == 1);
        ck_assert(lua_isstring(L, -1));
        ck_assert(!strcmp(lua_tostring(L, -1), esc_list[i][0]));
        lua_pop(L, 1);
    }

    /* test \uXXXX sequences */
    for (unsigned int hi = 0x0000; hi <= 0xffff; hi++)
    {
        char buf[16] = { 0 };
        snprintf(buf, sizeof(buf), "\"\\u%04x\"\n", hi);

        /* decode \u{hi}, store result */
        push_decode(L);
        lua_pushstring(L, buf);
        int ret = lua_pcall(L, 1, 1, 0);

        if (hi_surrogate(hi))
        {
            /* expect it to fail because no low surrogate */
            ck_assert(ret != 0);

            for (unsigned int lo = 0xdb00; lo <= 0xe0ff; lo += (rand () % 16))
            {
                /* decode surrogate pair, store result */
                snprintf(buf, sizeof(buf), "\"\\u%04x\\u%04x\"\n", hi, lo);
                push_decode(L);
                lua_pushstring(L, buf);
                ret = lua_pcall(L, 1, 1, 0);

                if (lo_surrogate(lo))
                {
                    /* decode UTF-8 output, compare codepoints */
                    ck_assert(ret == 0);
                    const unsigned int in_cp = decode_surrogate(hi, lo);
                    const char *const out_u8 = lua_tostring(L, -1);
                    const unsigned int out_cp = utf8_decode((uint8_t *)out_u8);
                    ck_assert(out_cp != 0xffffffff);
                    ck_assert(out_cp == in_cp);
                }
                else
                {
                    /* bad low surrogate */
                    ck_assert(ret != 0);
                }

                lua_pop(L, 1);
            }
        }
        else if (hi < 0xd800 || hi > 0xdfff)
        {
            /* decode UTF-8 output, compare codepoints */
            ck_assert(ret == 0);
            const char *const out_u8 = lua_tostring(L, -1);
            const unsigned int out_cp = utf8_decode((uint8_t *)out_u8);
            ck_assert(out_cp != 0xffffffff);
            ck_assert(out_cp == hi);
        }

        lua_pop(L, 1);
    }
}
END_TEST

/* save and load from file */
START_TEST(load_save)
{
    /*
     * json.save()
     */

    /* no first argument */
    push_save(L);
    ck_assert(lua_pcall(L, 0, 0, 0) != 0);
    asc_log_debug("json.save: no_filename: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* first argument is not a string */
    push_save(L);
    lua_pushboolean(L, 1);
    ck_assert(lua_pcall(L, 1, 0, 0) != 0);
    asc_log_debug("json.save: non_string_fn: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* no second argument */
    push_save(L);
    lua_pushliteral(L, JSON_FILE);
    ck_assert(lua_pcall(L, 1, 0, 0) != 0);
    asc_log_debug("json.save: no_second_arg: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* second argument is userdata */
    push_save(L);
    lua_pushliteral(L, JSON_FILE);
    lua_pushlightuserdata(L, NULL);
    ck_assert(lua_pcall(L, 2, 0, 0) != 0);
    asc_log_debug("json.save: encode_userdata: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* more than two arguments */
    push_save(L);
    lua_pushliteral(L, JSON_FILE);
    lua_pushlightuserdata(L, NULL);
    lua_pushlightuserdata(L, NULL);
    lua_pushlightuserdata(L, NULL);
    lua_pushlightuserdata(L, NULL);
    lua_pushboolean(L, 1); /* this one gets used */
    ck_assert(lua_pcall(L, 6, 0, 0) == 0);
    ck_assert(unlink(JSON_FILE) == 0);

    /* save to non-existent directory */
    push_save(L);
    lua_pushliteral(L, "./doesnotexist/test.json");
    lua_pushboolean(L, 1);
    ck_assert(lua_pcall(L, 2, 0, 0) != 0);
    asc_log_debug("json.save: no_dir: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* supply a directory as argument */
    push_save(L);
    lua_pushliteral(L, "..");
    lua_pushboolean(L, 1);
    ck_assert(lua_pcall(L, 2, 0, 0) != 0);
    asc_log_debug("json.save: to_dir: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* pass NaN so encode fails */
    push_save(L);
    lua_pushliteral(L, JSON_FILE);
    lua_pushnumber(L, (double)(NAN));
    ck_assert(lua_pcall(L, 2, 0, 0) != 0);
    asc_log_debug("json.save: encode_nan: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

#ifndef _WIN32
    /* save to /dev/full */
    struct stat sb;
    if (stat("/dev/null", &sb) == 0)
    {
        push_save(L);
        lua_pushliteral(L, "/dev/full");
        lua_pushboolean(L, 1);
        ck_assert(lua_pcall(L, 2, 0, 0) != 0);
        asc_log_debug("json.save: dev_full: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
#endif

    /* successful run */
    push_save(L);
    lua_pushliteral(L, JSON_FILE);
    lua_newtable(L);
    lua_pushnumber(L, 500);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, 300);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, 100);
    lua_rawseti(L, -2, 3);
    ck_assert(lua_pcall(L, 2, 0, 0) == 0);

    /*
     * json.load()
     */

    /* no first argument */
    push_load(L);
    ck_assert(lua_pcall(L, 0, 1, 0) != 0);
    asc_log_debug("json.load: no_filename: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* first argument is not a string */
    push_load(L);
    lua_pushboolean(L, 1);
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    asc_log_debug("json.load: non_string_fn: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* load from non-existent file */
    push_load(L);
    lua_pushliteral(L, "./doesnotexist/test.json");
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    asc_log_debug("json.load: no_file: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* supply a directory as argument */
    push_load(L);
    lua_pushliteral(L, "..");
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    asc_log_debug("json.load: from_dir: %s", lua_tostring(L, -1));
    lua_pop(L, 1);

    /* bad decode */
    FILE *const f = fopen("./invalid.json", "w");
    ck_assert(f != NULL);
    ck_assert(fwrite("invalid\n", sizeof("invalid\n"), 1, f) == 1);
    ck_assert(fclose(f) == 0);
    push_load(L);
    lua_pushliteral(L, "./invalid.json");
    ck_assert(lua_pcall(L, 1, 1, 0) != 0);
    asc_log_debug("json.load: invalid: %s", lua_tostring(L, -1));
    ck_assert(unlink("./invalid.json") == 0);
    lua_pop(L, 1);

    /* successful run */
    push_load(L);
    lua_pushliteral(L, JSON_FILE);
    ck_assert(lua_pcall(L, 1, 1, 0) == 0);
    ck_assert(unlink(JSON_FILE) == 0);
    lua_pop(L, 1);
}
END_TEST

/* call from Lua */
START_TEST(from_lua)
{
    static const char *const script =
        "local test = {" "\n"
        "    { 1, 2, 3 }," "\n"
        "    true," "\n"
        "    \"test\"," "\n"
        "}" "\n"
        "local test_json = json.encode(test)" "\n"
        "assert(test_json == \"[[1,2,3],true,\\\"test\\\"]\")" "\n"
        "local out = json.decode(test_json)" "\n"
        "assert(#out[1] == 3)" "\n"
        "assert(out[2] == true)" "\n"
        "assert(out[3] == \"test\")" "\n"
        "out = nil" "\n"
        "collectgarbage()" "\n"
        "json.save(\"./libastra.json\", test)" "\n"
        "local out = json.load(\"./libastra.json\")" "\n"
        "assert(#out[1] == 3)" "\n"
        "assert(out[2] == true)" "\n"
        "assert(out[3] == \"test\")" "\n"
        "os.remove(\"./libastra.json\")" "\n";

    ck_assert_msg(luaL_dostring(L, script) == 0, lua_tostring(L, -1));
}
END_TEST

Suite *luaapi_lib_json(void)
{
    Suite *const s = suite_create("luaapi/lib/json");

    TCase *const tc = tcase_create("default");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_vectors);
    tcase_add_test(tc, lonely_values);
    tcase_add_test(tc, nesting_depth);
    tcase_add_test(tc, escape_sequences);
    tcase_add_test(tc, load_save);
    tcase_add_test(tc, from_lua);

    suite_add_tcase(s, tc);

    return s;
}
