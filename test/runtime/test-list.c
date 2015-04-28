/*-
 * Copyright (c) 2015 Jason Lingle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
  return ava_list_value_of(ava_value_of_string(ava_string_of_cstring(str)));
}

deftest(empty_string_converted_to_empty_list) {
  ava_list_value list = list_of_cstring("");

  ck_assert_int_eq(0, memcmp(&ava_empty_list, &list, sizeof(list)));
}

deftest(whitespace_string_converted_to_empty_list) {
  ava_list_value list = list_of_cstring("  \t\r\n\t");

  ck_assert_int_eq(0, memcmp(&ava_empty_list, &list, sizeof(list)));
}

deftest(normal_tokens_parsed_as_list_items) {
  ava_list_value list = list_of_cstring(
    "  foo \"bar baz\"\n \\{fum\\\\}  ");

  ck_assert_int_eq(3, list.v->length(list));
  ck_assert_str_eq("foo", ava_string_to_cstring(
                     ava_to_string(list.v->index(list, 0))));
  ck_assert_str_eq("bar baz", ava_string_to_cstring(
                     ava_to_string(list.v->index(list, 1))));
  ck_assert_str_eq("fum\\", ava_string_to_cstring(
                     ava_to_string(list.v->index(list, 2))));
}

#define assert_format_exception(expr)                   \
  do {                                                  \
    ava_exception_handler handler;                      \
    ava_try (handler) {                                 \
      expr;                                             \
      ck_abort_msg("no exception thrown");              \
    } ava_catch (handler, ava_format_exception) {       \
      /* ok */                                          \
    } ava_catch_all {                                   \
      ava_rethrow(&handler);                            \
    }                                                   \
  } while (0)

deftest(lexer_errors_propagated) {
  assert_format_exception(list_of_cstring("\""));
}

deftest(non_astrings_rejected) {
  assert_format_exception(list_of_cstring("`lr`"));
  assert_format_exception(list_of_cstring("`l\""));
  assert_format_exception(list_of_cstring("\"r`"));
}

deftest(enclosures_rejected) {
  assert_format_exception(list_of_cstring("(a)"));
  assert_format_exception(list_of_cstring("a()"));
  assert_format_exception(list_of_cstring("[b]"));
  assert_format_exception(list_of_cstring("b[]"));
  assert_format_exception(list_of_cstring("{c}"));
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
  ava_list_value parsed_list;
  ava_string out_str;
  unsigned i, j;

  for (i = 0; i < 256; ++i) {
    for (j = 0; j < 256; ++j) {
      in[0] = i;
      in[1] = j;
      printf("%02X%02X...\n", in[0] & 0xFF, in[1] & 0xFF);

      in_str = ava_string_of_bytes(in, 2);
      parsed_list = ava_list_value_of(
        ava_value_of_string(
          ava_list_escape(in_str)));
      ck_assert_int_eq(1, parsed_list.v->length(parsed_list));
      out_str = ava_to_string(parsed_list.v->index(parsed_list, 0));
      ck_assert_int_eq(2, ava_string_length(out_str));
      ava_string_to_bytes(out, out_str, 0, 2);

      ck_assert_int_eq(in[0], out[0]);
      ck_assert_int_eq(in[1], out[1]);
    }
  }
}
