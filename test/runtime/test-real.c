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
#include <math.h>

#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/integer.h"
#include "runtime/avalanche/real.h"
#include "runtime/avalanche/exception.h"

defsuite(real);

#define assert_real_eq(a, b) do {                                       \
    ava_real va = (a), vb = (b);                                        \
    ck_assert_msg(va == vb, "Assertion '%s' failed: %s==%f, %s==%f",    \
                  #a "==" #b, #a, va, #b, vb);                          \
  } while (0)

static ava_real of_cstring(const char* str, ava_real dfault) {
  return ava_real_of_value(ava_value_of_cstring(str), dfault);
}

/* These tests generally assume that the contrib implementation of strtod() is
 * correct, so they only test the interface with it and any modifications
 * particular to Avalanche.
 */

deftest(simple_parse) {
  assert_real_eq(1.0, of_cstring("1.0", 0));
}

deftest(parse_ignores_whitespace) {
  assert_real_eq(1.1, of_cstring(" \t\r\n1.1\n\r\t ", 0));
}

deftest(parse_defaults_on_empty_string) {
  assert_real_eq(3.14, of_cstring("", 3.14));
}

deftest(parse_defaults_on_whitespace_string) {
  assert_real_eq(3.14, of_cstring("\t \r\n", 3.14));
}

deftest(integer_literals_accepted) {
  assert_real_eq(1.0, of_cstring(" on ", 3.14));
}

static void do_throws_on_illegal_string(void* ignored) {
  ava_real r = of_cstring("3.14 foo", 0);
  ck_abort_msg("unexpectedly parsed %f", r);
}

deftest(throws_on_illegal_string) {
  ava_exception ex;

  if (ava_catch(&ex, do_throws_on_illegal_string, NULL))
    ck_assert_ptr_eq(&ava_format_exception, ex.type);
  else
    ck_abort_msg("no exception thrown");
}

deftest(accepts_nan) {
  ava_real r = of_cstring("NaN", 0);
  ck_assert(isnan(r));
}

deftest(accepts_infinity) {
  ava_real r = of_cstring("infinity", 0);
  ck_assert(isinf(r));
  ck_assert(r > 0.0);
}

deftest(accepts_neg_infinity) {
  ava_real r = of_cstring("-infinity", 0);
  ck_assert(isinf(r));
  ck_assert(r < 0.0);
}

deftest(accepts_comma_as_decimal_at_start) {
  assert_real_eq(0.1, of_cstring(",1", 0));
}

deftest(accepts_comma_as_decimal_at_end) {
  assert_real_eq(1.0, of_cstring("1,", 0));
}

deftest(accepts_comma_as_decimal_in_middle) {
  assert_real_eq(3.14, of_cstring("3,14", 0));
}

deftest(accepts_comma_as_decimal_with_exponentiation) {
  assert_real_eq(314.0, of_cstring("3,14e2", 0));
}

deftest(real_of_real) {
  assert_real_eq(3.14, ava_real_of_value(ava_value_of_real(3.14), 0));
}

deftest(real_of_integer) {
  assert_real_eq(3.0, ava_real_of_value(ava_value_of_integer(3), 0));
}

deftest(stringify) {
  assert_value_equals_str("1", ava_value_of_real(1.0));
  assert_value_equals_str("1.1", ava_value_of_real(1.1));
  assert_value_equals_str("-1.1", ava_value_of_real(-1.1));
  assert_value_equals_str("NaN", ava_value_of_real(of_cstring("NaN", 0)));
}
