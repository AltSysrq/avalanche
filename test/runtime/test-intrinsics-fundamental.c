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

defsuite(intrinsics_fundamental);

deftest(empty_substitution) {
  test_macsub("seq(void) { seq(last) { } }", "()");
}

deftest(nonempty_substitution) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  test_macsub("seq(void) { seq(last) { "
              "+ { left = bareword:1; right = bareword:2; } "
              "} }",
              "(1 + 2)");
}

deftest(lstring) {
  defun("#string-concat#");
  test_macsub("seq(void) { seq(last) { "
              "#string-concat# { "
              "seq(last) { bareword:foo }; string:bar; } } }",
              "foo `bar\"");
}

deftest(rstring) {
  defun("#string-concat#");
  test_macsub("seq(void) { seq(last) { "
              "#string-concat# { "
              "string:foo; seq(last) { bareword:bar }; } } }",
              "\"foo` bar");
}

deftest(lrstring) {
  defun("#string-concat#");
  test_macsub("seq(void) { seq(last) { "
              "#string-concat# { "
              "seq(last) { "
              "#string-concat# { "
              "seq(last) { bareword:foo }; "
              "string:bar; } }; "
              "seq(last) { bareword:quux }; } } }",
              "foo `bar` quux");
}

deftest(isolated_lstring) {
  test_macsub_fail("seq(void) { <error> }",
                   "expression before",
                   "`foo\"");
}

deftest(lstring_at_beginning) {
  test_macsub_fail("seq(void) { <error> }",
                   "expression before",
                   "`foo\" bar");
}

deftest(isolated_rstring) {
  test_macsub_fail("seq(void) { <error> }",
                   "expression after",
                   "\"foo`");
}

deftest(rstring_at_end) {
  test_macsub_fail("seq(void) { <error> }",
                   "expression after",
                   "foo \"bar`");
}
