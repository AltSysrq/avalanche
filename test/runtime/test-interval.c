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

#include "runtime/avalanche/string.h"
#include "runtime/avalanche/integer.h"
#include "runtime/avalanche/interval.h"

defsuite(interval);

#define IV(str) ava_interval_value_of(ava_value_of_cstring(str))
#define SG(str,len) ava_interval_get_singular(  \
    ava_interval_value_of(                      \
      ava_value_of_cstring(str)), len)
#define BEG(str,len) ava_interval_get_begin(    \
    ava_interval_value_of(                      \
      ava_value_of_cstring(str)), len)
#define END(str,len) ava_interval_get_end(      \
    ava_interval_value_of(                      \
      ava_value_of_cstring(str)), len)

deftest(empty_is_end) {
  ck_assert_int_eq(42, SG(" ", 42));
}

deftest(positive_singular_is_absolute) {
  ck_assert_int_eq(42, SG("042", 36));
}

deftest(negative_singular_is_end_relative) {
  ck_assert_int_eq(40, SG("-2", 42));
}

deftest(end_is_length) {
  ck_assert_int_eq(42, SG("end", 42));
}

deftest(begin_defaults_to_0) {
  ck_assert_int_eq(0, BEG("~42", 66));
}

deftest(end_defaults_to_end) {
  ck_assert_int_eq(66, END("6~", 66));
}

deftest(small_compact_absolute) {
  ck_assert_int_eq(6, BEG("6~42", 66));
  ck_assert_int_eq(42, END("6~42", 66));
}

deftest(small_compact_relative) {
  ck_assert_int_eq(30, BEG("-12~-2", 42));
  ck_assert_int_eq(40, END("-42~-2", 42));
}

deftest(min_min_compact) {
  ck_assert_ptr_eq(
    &ava_compact_interval_type,
    ava_value_attr(IV("-0x7FFFFFFF~-0x7FFFFFFF").v));
  ck_assert_int_eq(0, BEG("-0x7FFFFFFF~-0x7FFFFFFF", 0x7FFFFFFF));
  ck_assert_int_eq(0, END("-0x7FFFFFFF~-0x7FFFFFFF", 0x7FFFFFFF));
}

deftest(min_max_compact) {
  ck_assert_int_eq(0, BEG("-0x7FFFFFFF~0x7FFFFFFF", 0x7FFFFFFF));
  ck_assert_int_eq(0x7FFFFFFF, END("-0x7FFFFFFF~0x7FFFFFFF",
                                   0x7FFFFFFF));
}

deftest(max_min_compact) {
  ck_assert_int_eq(0x7FFFFFFF, BEG("0x7FFFFFFF~-0x7FFFFFFF", 0x7FFFFFFF));
  ck_assert_int_eq(0, END("0x7FFFFFFF~-0x7FFFFFFF", 0x7FFFFFFF));
}

deftest(max_max_compact) {
  ck_assert_ptr_eq(
    &ava_compact_interval_type,
    ava_value_attr(IV("0x7FFFFFFF~0x7FFFFFFF").v));
  ck_assert_int_eq(0x7FFFFFFF, BEG("0x7FFFFFFF~0x7FFFFFFF", 0x7FFFFFFF));
  ck_assert_int_eq(0x7FFFFFFF, END("0x7FFFFFFF~0x7FFFFFFF", 0x7FFFFFFF));
}

deftest(compact_end_begin) {
  ck_assert_ptr_eq(
    &ava_compact_interval_type,
    ava_value_attr(IV("end~42").v));
  ck_assert_int_eq(66, BEG("end~42", 66));
  ck_assert_int_eq(42, END("end~42", 66));
}

deftest(compact_end_end) {
  ck_assert_ptr_eq(
    &ava_compact_interval_type,
    ava_value_attr(IV("42~end").v));
  ck_assert_int_eq(42, BEG("42~end", 66));
  ck_assert_int_eq(66, END("42~end", 66));
}

deftest(wide_min_begin) {
  ck_assert_ptr_eq(
    &ava_wide_interval_type,
    ava_value_attr(IV("-0x80000000~42").v));
  ck_assert_int_eq(-0x80000000LL+66LL, BEG("-0x80000000~42", 66));
  ck_assert_int_eq(42, END("-0x80000000~42", 66));
}

deftest(wide_max_begin) {
  ck_assert_ptr_eq(
    &ava_wide_interval_type,
    ava_value_attr(IV("+0x80000000~42").v));
  ck_assert_int_eq(0x80000000LL, BEG("+0x80000000~42", 66));
  ck_assert_int_eq(42, END("+0x80000000~42", 66));
}

deftest(wide_min_end) {
  ck_assert_ptr_eq(
    &ava_wide_interval_type,
    ava_value_attr(IV("42~-0x80000000").v));
  ck_assert_int_eq(42, BEG("42~-0x80000000", 66));
  ck_assert_int_eq(66LL-0x80000000LL, END("42~-0x80000000", 66));
}

deftest(wide_max_end) {
  ck_assert_ptr_eq(
    &ava_wide_interval_type,
    ava_value_attr(IV("42~0x80000000").v));

  ck_assert_int_eq(42, BEG("42~0x80000000", 66));
  ck_assert_int_eq(0x80000000LL, END("42~0x80000000", 66));
}

deftest(wide_end_begin) {
  ck_assert_ptr_eq(
    &ava_wide_interval_type,
    ava_value_attr(IV("end~0x80000000").v));

  ck_assert_int_eq(66, BEG("end~0x80000000", 66));
  ck_assert_int_eq(0x80000000LL, END("end~0x80000000", 66));
}

deftest(wide_end_end) {
  ck_assert_ptr_eq(
    &ava_wide_interval_type,
    ava_value_attr(IV("0x80000000~end").v));

  ck_assert_int_eq(0x80000000LL, BEG("0x80000000~end", 66));
  ck_assert_int_eq(66, END("0x80000000~end", 66));
}
