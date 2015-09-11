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

#include <stdlib.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "macro-test-common.h"

/* Tests for the macro substitution algorithm itself; intrinsics like function
 * calls are not tested here.
 *
 * The only actual macro invoked is a dummy macro in this file, which doesn't
 * implement any macro primitives except for stringification. Handling of
 * bareword and sequence intrinsics is also necessarily covered by this test,
 * since it is essentially impossible not to.
 */

defsuite(macsub);

deftest(empty_input) {
  test_macsub("seq(void) { }", "");
}

deftest(lone_bareword) {
  test_macsub("seq(void) { bareword:foo }", "foo");
}

deftest(lone_string) {
  test_macsub("seq(void) { string:foo }", "\"foo\"");
  test_macsub("seq(void) { string:foo }", "\\{foo\\}");
}

deftest(simple_multi_statement) {
  test_macsub("seq(void) { bareword:foo; bareword:bar }", "foo \\ bar");
}

deftest(simple_control_macro) {
  defmacro("macro", ava_st_control_macro, 0, ava_false);
  test_macsub("seq(void) { macro { right = bareword:foo; } }",
              "macro foo");
}

deftest(simple_function_macro) {
  defmacro("macro", ava_st_function_macro, 0, ava_false);
  test_macsub("seq(void) { macro { right = bareword:foo; } }",
              "macro foo");
}

deftest(simple_operator_macro_prefix) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  test_macsub("seq(void) { + { right = bareword:2; } }",
              "+ 2");
}

deftest(simple_operator_macro_suffix) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  test_macsub("seq(void) { + { left = bareword:1; } }",
              "1 +");
}

deftest(simple_operator_macro_interfix) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  test_macsub("seq(void) { + { left = bareword:1; "
              "right = bareword:2; } }", "1 + 2");
}

deftest(multi_operator_precedence) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  defmacro("*", ava_st_operator_macro, 20, ava_false);
  test_macsub("seq(void) { + { left = * { "
              "left = bareword:a; right = bareword:b; }; "
              "right = * { "
              "left = bareword:c; right = bareword:d; }; } }",
              "a * b + c * d");
}

deftest(left_to_right_operator_associativity) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  test_macsub("seq(void) { + { left = + { "
              "left = bareword:a; right = bareword:b; }; "
              "right = bareword:c; } }",
              "a + b + c");
}

deftest(right_to_left_operator_associativity) {
  defmacro("**", ava_st_operator_macro, 33, ava_false);
  test_macsub("seq(void) { ** { left = bareword:a; "
              "right = ** { left = bareword:b; "
              "right = bareword:c; }; } }",
              "a ** b ** c");
}

deftest(control_macro_contains_operators) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  defmacro("ret", ava_st_control_macro, 0, ava_false);
  test_macsub("seq(void) { "
              "ret { right = + { "
              "left = bareword:a; right = bareword:b; }; } }",
              "ret a + b");
}

deftest(operator_contains_function_macros) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  defmacro("f", ava_st_function_macro, 0, ava_false);
  test_macsub("seq(void) { "
              "+ { left = f { right = bareword:a; }; "
              "right = f { right = bareword:b; }; } }",
              "f a + f b");
}

deftest(isolated_function_macro_not_invoked) {
  defmacro("foo", ava_st_function_macro, 0, ava_false);
  test_macsub("seq(void) { bareword:foo }", "foo");
}

deftest(isolated_operator_macro_not_invoked) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  test_macsub("seq(void) { bareword:+ }", "+");
}

deftest(isolated_control_macro_not_invoked) {
  defmacro("foo", ava_st_control_macro, 0, ava_false);
  test_macsub("seq(void) { bareword:foo }", "foo");
}

deftest(ambiguous_possible_macro_results_in_error) {
  ava_string abs, amb;

  defmacro("a.foo", ava_st_operator_macro, 10, ava_false);
  defmacro("b.foo", ava_st_operator_macro, 10, ava_false);
  ava_macsub_import(&abs, &amb, context,
                    AVA_ASCII9_STRING("a."), AVA_EMPTY_STRING,
                    ava_true, ava_false);
  ava_macsub_import(&abs, &amb, context,
                    AVA_ASCII9_STRING("b."), AVA_EMPTY_STRING,
                    ava_true, ava_false);

  test_macsub_fail("seq(void) { <error> }",
                   "ambiguous",
                   "a foo bar");
}

deftest(macro_consuming_rest_of_scope) {
  defmacro("macro", ava_st_control_macro, 0, ava_false);
  defmacro("defer", ava_st_control_macro, 0, ava_true);

  test_macsub("seq(void) { defer { "
              "right = string:; "
              "next = seq(void) { "
              "macro { right = bareword:foo; } }; } }",
              "defer \"\"\nmacro foo");
}
