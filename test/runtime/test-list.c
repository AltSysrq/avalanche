/*-
 * Copyright (c) 2015, Jason Lingle
 *
 * Permission to  use, copy,  modify, and/or distribute  this software  for any
 * purpose  with or  without fee  is hereby  granted, provided  that the  above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE  IS PROVIDED "AS  IS" AND  THE AUTHOR DISCLAIMS  ALL WARRANTIES
 * WITH  REGARD   TO  THIS  SOFTWARE   INCLUDING  ALL  IMPLIED   WARRANTIES  OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT  SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL,  DIRECT,   INDIRECT,  OR  CONSEQUENTIAL  DAMAGES   OR  ANY  DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF  CONTRACT, NEGLIGENCE  OR OTHER  TORTIOUS ACTION,  ARISING OUT  OF OR  IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "test.c"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/list.h"
#include "runtime/avalanche/exception.h"

defsuite(list);

static ava_list_value list_of_cstring(const char* str) {
  return
    ava_fat_list_value_of(ava_value_of_string(ava_string_of_cstring(str))).c;
}

static ava_value value_of_cstring(const char* str) {
  return list_of_cstring(str).v;
}

deftest(empty_string_converted_to_empty_list) {
  ava_value list = value_of_cstring("");
  ava_value empty = ava_empty_list().v;

  ck_assert_int_eq(0, memcmp(&empty, &list, sizeof(list)));
}

deftest(whitespace_string_converted_to_empty_list) {
  ava_value list = value_of_cstring("  \t\r\n\t");
  ava_value empty = ava_empty_list().v;

  ck_assert_int_eq(0, memcmp(&empty, &list, sizeof(list)));
}

deftest(normal_tokens_parsed_as_list_items) {
  ava_value list = value_of_cstring(
    "  foo \"bar baz\"\n \\{fum\\\\}  ");

  ck_assert_int_eq(3, ava_list_length(list));
  ck_assert_str_eq("foo", ava_string_to_cstring(
                     ava_to_string(ava_list_index(list, 0))));
  ck_assert_str_eq("bar baz", ava_string_to_cstring(
                     ava_to_string(ava_list_index(list, 1))));
  ck_assert_str_eq("fum\\", ava_string_to_cstring(
                     ava_to_string(ava_list_index(list, 2))));
}

deftest(simple_sublists) {
  ava_value list = value_of_cstring("a [b   c] [d  e f]");

  ck_assert_int_eq(3, ava_list_length(list));
  assert_value_equals_str("a", ava_list_index(list, 0));
  assert_value_equals_str("b c", ava_list_index(list, 1));
  assert_value_equals_str("d e f", ava_list_index(list, 2));
}

deftest(nested_sublists) {
  ava_value list = value_of_cstring("a [b [c d] e [[f g]]] h");
  ava_value sub;

  ck_assert_int_eq(3, ava_list_length(list));
  assert_value_equals_str("a", ava_list_index(list, 0));
  sub = ava_list_index(list, 1);
  ck_assert_int_eq(4, ava_list_length(sub));
  assert_value_equals_str("b", ava_list_index(sub, 0));
  assert_value_equals_str("e", ava_list_index(sub, 2));
  sub = ava_list_index(sub, 1);
  ck_assert_int_eq(2, ava_list_length(sub));
  assert_value_equals_str("c", ava_list_index(sub, 0));
  assert_value_equals_str("d", ava_list_index(sub, 1));
  sub = ava_list_index(ava_list_index(list, 1), 3);
  ck_assert_int_eq(1, ava_list_length(sub));
  sub = ava_list_index(sub, 0);
  ck_assert_int_eq(2, ava_list_length(sub));
  assert_value_equals_str("f", ava_list_index(sub, 0));
  assert_value_equals_str("g", ava_list_index(sub, 1));
  assert_value_equals_str("h", ava_list_index(list, 2));
}

deftest(empty_sublist) {
  ava_value list = value_of_cstring("[[]]");

  ck_assert_int_eq(1, ava_list_length(list));
  ck_assert_int_eq(1, ava_list_length(ava_list_index(list, 0)));
  ck_assert_int_eq(0, ava_list_length(ava_list_index(ava_list_index(list, 0), 0)));
}

#define assert_format_exception(expr)                   \
  do {                                                  \
    ava_exception_handler handler;                      \
    ava_try (handler) {                                 \
      /* Need to do something with expr so it is not */ \
      /* optimised away */                              \
      ck_assert_ptr_ne(NULL, ava_value_attr(expr));     \
      ck_abort_msg("no exception thrown");              \
    } ava_catch (handler, ava_format_exception) {       \
      /* ok */                                          \
    } ava_catch_all {                                   \
      ava_rethrow(&handler);                            \
    }                                                   \
  } while (0)

deftest(lexer_errors_propagated) {
  assert_format_exception(value_of_cstring("\""));
}

deftest(non_astrings_rejected) {
  assert_format_exception(value_of_cstring("`lr`"));
  assert_format_exception(value_of_cstring("`l\""));
  assert_format_exception(value_of_cstring("\"r`"));
}

deftest(enclosures_rejected) {
  assert_format_exception(value_of_cstring("(a)"));
  assert_format_exception(value_of_cstring("a()"));
  assert_format_exception(value_of_cstring("b[]"));
  assert_format_exception(value_of_cstring("{c}"));
}

deftest(error_on_unbalanced_brackets) {
  assert_format_exception(value_of_cstring("[foo"));
  assert_format_exception(value_of_cstring("foo]"));
  assert_format_exception(value_of_cstring("[[][]]]"));
}

deftest(error_on_tagged_brackets) {
  assert_format_exception(value_of_cstring("[foo]bar"));
}

const char* escape(const char* str) {
  return ava_string_to_cstring(ava_list_escape(ava_string_of_cstring(str)));
}

deftest(simple_words_not_modified_by_escape) {
  ck_assert_str_eq("foo", escape("foo"));
  ck_assert_str_eq("Stra\303\237e", escape("Stra\303\237e"));
}

deftest(strings_containing_quotables_escaped_by_quotes) {
  ck_assert_str_eq("\"foo bar\"", escape("foo bar"));
  ck_assert_str_eq("\"foo;bar\"", escape("foo;bar"));
  ck_assert_str_eq("\"foo(\"", escape("foo("));
  ck_assert_str_eq("\")foo\"", escape(")foo"));
  ck_assert_str_eq("\"bar[\"", escape("bar["));
  ck_assert_str_eq("\"]bar\"", escape("]bar"));
  ck_assert_str_eq("\"baz{\"", escape("baz{"));
  ck_assert_str_eq("\"}baz\"", escape("}baz"));
}

deftest(strings_containing_quotes_escaped_by_verbatim) {
  ck_assert_str_eq("\\{x\"y\\}", escape("x\"y"));
  ck_assert_str_eq("\\{x`y\\}", escape("x`y"));
}

deftest(strings_contining_nl_ht_escaped_by_verbatim_literal) {
  ck_assert_str_eq("\\{a\nb\\}", escape("a\nb"));
  ck_assert_str_eq("\\{a\tb\\}", escape("a\tb"));
}

deftest(isolated_bs_escaped_by_verbatim_literal) {
  ck_assert_str_eq("\\{a\\b\\}", escape("a\\b"));
}

deftest(control_chars_escaped_by_verbatim_hex_escape) {
  ck_assert_str_eq("\\{a\\;x0Db\\}", escape("a\rb"));
  ck_assert_str_eq("\\{a\\;x0D\nb\\}", escape("a\r\nb"));
  ck_assert_str_eq("\\{a\\;x7Fq\\}", escape("a\x7Fq"));
}

deftest(verbatim_escapes_escaped_by_verbamit_backslash_escape) {
  ck_assert_str_eq("\\{\\;\\{\\}", escape("\\{"));
  ck_assert_str_eq("\\{\\;\\}\\}", escape("\\}"));
  ck_assert_str_eq("\\{\\;\\;n\\}", escape("\\;n"));
}

deftest(all_two_char_strings_escaped_reversibly) {
  char in[2], out[2];
  ava_string in_str;
  ava_value parsed_list;
  ava_string out_str;
  unsigned i, j;

  for (i = 0; i < 256; ++i) {
    for (j = 0; j < 256; ++j) {
      in[0] = i;
      in[1] = j;

      in_str = ava_string_of_bytes(in, 2);
      parsed_list =
        ava_fat_list_value_of(ava_value_of_string(ava_list_escape(in_str))).c.v;
      ck_assert_int_eq(1, ava_list_length(parsed_list));
      out_str = ava_to_string(ava_list_index(parsed_list, 0));
      ck_assert_int_eq(2, ava_string_length(out_str));
      ava_string_to_bytes(out, out_str, 0, 2);

      ck_assert_int_eq(in[0], out[0]);
      ck_assert_int_eq(in[1], out[1]);
    }
  }
}

deftest(list_stringified_correctly) {
  ava_value values[2] = {
    ava_value_of_string(ava_string_of_cstring("foo bar")),
    ava_value_of_string(ava_string_of_cstring("xy\"zzy")),
  };
  ava_value list = ava_list_of_values(values, 2).v;

  ava_string str = ava_to_string(list);

  ck_assert_str_eq("\"foo bar\" \\{xy\"zzy\\}",
                   ava_string_to_cstring(str));
}

deftest(empty_string_is_quoted) {
  ava_value values[2] = {
    ava_value_of_string(AVA_EMPTY_STRING),
    ava_value_of_string(AVA_EMPTY_STRING),
  };
  ava_value list = ava_list_of_values(values, 2).v;

  ava_string str = ava_to_string(list);

  ck_assert_str_eq("\"\" \"\"", ava_string_to_cstring(str));
}
