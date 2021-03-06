/*-
 * Copyright (c) 2015 Jason Lingle
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
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/list.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/integer.h"
#include "runtime/-array-list.h"
#include "runtime/-esba-list.h"

defsuite(esba_list);

deftest(single_element_list) {
  ava_value fourty_two = ava_value_of_integer(42);
  ava_list_value list = ava_esba_list_of_raw(&fourty_two, 1);

  ck_assert_int_eq(1, ava_list_length(list));
  assert_values_equal(fourty_two, ava_list_index(list, 0));
  ck_assert_int_eq(0, ava_esba_list_element_size(list.v));
}

/* The following couple of tests specifically test handling of zero-sized
 * values, since identical values get optimised to that.
 */
deftest(identical_append) {
  ava_value fourty_two = ava_value_of_integer(42);
  ava_list_value list = ava_esba_list_of_raw(&fourty_two, 1);
  unsigned i;

  for (i = 1; i < 256; ++i)
    list = ava_list_append(list, fourty_two);

  ck_assert_int_eq(256, ava_list_length(list));
  ck_assert_int_eq(0, ava_esba_list_element_size(list.v));
  for (i = 0; i < 256; ++i)
    assert_values_equal(fourty_two, ava_list_index(list, i));
}

deftest(identical_set) {
  ava_value fourty_two = ava_value_of_integer(42);
  ava_list_value list = ava_esba_list_of_raw(&fourty_two, 1);
  unsigned i;

  for (i = 1; i < 256; ++i)
    list = ava_list_set(list, 0, fourty_two);

  ck_assert_int_eq(1, ava_list_length(list));
  ck_assert_int_eq(0, ava_esba_list_element_size(list.v));
  assert_values_equal(fourty_two, ava_list_index(list, 0));
}

deftest(polymorphic_value_append) {
  ava_value zero = ava_value_of_integer(0);
  ava_list_value list = ava_esba_list_of_raw(&zero, 1);
  unsigned i;

  for (i = 1; i < 256; ++i)
    list = ava_list_append(list, ava_value_of_integer(i));

  ck_assert_int_eq(256, ava_list_length(list));
  ck_assert_int_eq(sizeof(ava_ulong), ava_esba_list_element_size(list.v));
  for (i = 0; i < 256; ++i)
    ck_assert_int_eq(i, ava_integer_of_value(ava_list_index(list, i), -1));
}

deftest(polymorphic_value_and_type_append) {
  ava_value fourty_two = ava_value_of_integer(42);
  ava_value string = ava_value_of_string(ava_string_of_cstring("hello world"));
  ava_list_value list = ava_esba_list_of_raw(&fourty_two, 1);
  list = ava_list_append(list, string);

  ck_assert_int_eq(2, ava_list_length(list));
  ck_assert_int_eq(sizeof(ava_value), ava_esba_list_element_size(list.v));
  assert_values_equal(fourty_two, ava_list_index(list, 0));
  assert_values_equal(string, ava_list_index(list, 1));
}

deftest(fully_polymorphic_append) {
  ava_value fourty_two = ava_value_of_integer(42);
  ava_value string = ava_value_of_string(ava_string_of_cstring("hello world"));
  ava_list_value list = ava_esba_list_of_raw(&fourty_two, 1);
  list = ava_list_append(list, string);

  ava_value list_value = list.v;
  list = ava_list_append(list, list_value);

  ck_assert_int_eq(3, ava_list_length(list));
  ck_assert_int_eq(sizeof(ava_value), ava_esba_list_element_size(list.v));
  assert_values_equal(fourty_two, ava_list_index(list, 0));
  assert_values_equal(string, ava_list_index(list, 1));
  assert_values_equal(list_value, ava_list_index(list, 2));
}

deftest(polymorphic_create_from_array) {
  ava_value values[2] = {
    ava_value_of_integer(42),
    ava_value_of_string(ava_string_of_cstring("hello world")),
  };
  ava_list_value list = ava_esba_list_of_raw(values, 2);

  ck_assert_int_eq(2, ava_list_length(list));
  assert_values_equal(values[0], ava_list_index(list, 0));
  assert_values_equal(values[1], ava_list_index(list, 1));
}

deftest(polymorphic_create_from_list) {
  ava_value values[2] = {
    ava_value_of_integer(42),
    ava_value_of_string(ava_string_of_cstring("hello world")),
  };
  ava_list_value array_list = ava_array_list_of_raw(values, 2);
  ava_list_value list = ava_esba_list_copy_of(array_list, 0, 2);

  ck_assert_int_eq(2, ava_list_length(list));
  assert_values_equal(values[0], ava_list_index(list, 0));
  assert_values_equal(values[1], ava_list_index(list, 1));
}

deftest(strided_create_from_array) {
  ava_value values[3] = {
    ava_value_of_integer(42),
    ava_value_of_cstring("hello world"),
    ava_value_of_integer(56),
  };
  ava_list_value first = ava_esba_list_of_raw_strided(
    values, 2, 2);
  ava_list_value second = ava_esba_list_of_raw_strided(
    values + 1, 1, 2);

  ck_assert_int_eq(2, ava_list_length(first));
  ck_assert_str_eq("42 56", ava_string_to_cstring(
                     ava_to_string(first.v)));
  ck_assert_str_eq("[hello world]", ava_string_to_cstring(
                     ava_to_string(second.v)));
}

deftest(slice_to_empty_list) {
  ava_value values[2] = {
    ava_value_of_integer(1),
    ava_value_of_integer(2),
  };
  ava_list_value list = ava_esba_list_of_raw(values, 2);
  ava_list_value empty = ava_list_slice(list, 1, 1);

  assert_values_equal(empty.v, ava_empty_list().v);
}

deftest(slice_to_array_list) {
  ava_value zero = ava_value_of_integer(0);
  ava_list_value list = ava_esba_list_of_raw(&zero, 1);
  ava_list_value result;
  unsigned i;

  for (i = 1; i < 64; ++i)
    list = ava_list_append(list, ava_value_of_integer(i));

  result = ava_list_slice(list, 5, 8);

  ck_assert_int_eq(3, ava_array_list_used(result.v));
  ck_assert_int_eq(3, ava_list_length(result.v));
  ck_assert_int_eq(5, ava_integer_of_value(
                     ava_list_index(result, 0), -1));
  ck_assert_int_eq(6, ava_integer_of_value(
                     ava_list_index(result, 1), -1));
  ck_assert_int_eq(7, ava_integer_of_value(
                     ava_list_index(result, 2), -1));
}

deftest(slice_to_esba_list) {
  ava_value zero = ava_value_of_integer(0);
  ava_list_value list = ava_esba_list_of_raw(&zero, 1);
  ava_list_value result;
  unsigned i;

  for (i = 1; i < 64; ++i)
    list = ava_list_append(list, ava_value_of_integer(i));

  result = ava_list_slice(list, 5, 58);
  ck_assert_int_eq(53, ava_list_length(result));
  for (i = 0; i < 53; ++i)
    ck_assert_int_eq(5+i, ava_integer_of_value(
                       ava_list_index(result, i), -1));
}

deftest(noop_slice) {
  ava_value zero = ava_value_of_integer(0);
  ava_list_value list = ava_esba_list_of_raw(&zero, 1);
  ava_list_value result;

  result = ava_list_slice(list, 0, 1);
  ck_assert_int_eq(0, memcmp(&list, &result, sizeof(list)));
}

deftest(concat_with_compatible_esba_list) {
  ava_value values[4] = {
    ava_value_of_integer(0),
    ava_value_of_integer(1),
    ava_value_of_integer(2),
    ava_value_of_integer(3),
  };
  ava_list_value left, right, result;
  unsigned i;

  left = ava_esba_list_of_raw(values, 2);
  right = ava_esba_list_of_raw(values + 2, 2);
  result = ava_list_concat(left, right);

  ck_assert_int_eq(4, ava_list_length(result));
  for (i = 0; i < 4; ++i)
    assert_values_equal(values[i], ava_list_index(result, i));
}

deftest(concat_with_incompatible_esba_list) {
  ava_value values[4] = {
    ava_value_of_integer(0),
    ava_value_of_integer(1),
    ava_value_of_string(ava_string_of_cstring("foo")),
    ava_value_of_string(ava_string_of_cstring("bar")),
  };
  ava_list_value left, right, result;
  unsigned i;

  left = ava_esba_list_of_raw(values, 2);
  right = ava_esba_list_of_raw(values + 2, 2);
  result = ava_list_concat(left, right);

  ck_assert_int_eq(4, ava_list_length(result));
  for (i = 0; i < 4; ++i)
    assert_values_equal(values[i], ava_list_index(result, i));
}

deftest(concat_with_compatible_other_list) {
  ava_value values[4] = {
    ava_value_of_integer(0),
    ava_value_of_integer(1),
    ava_value_of_integer(2),
    ava_value_of_integer(3),
  };
  ava_list_value left, right, result;
  unsigned i;

  left = ava_esba_list_of_raw(values, 2);
  right = ava_array_list_of_raw(values + 2, 2);
  result = ava_list_concat(left, right);

  ck_assert_int_eq(4, ava_list_length(result));
  for (i = 0; i < 4; ++i)
    assert_values_equal(values[i], ava_list_index(result, i));
}

deftest(concat_with_incompatible_other_list) {
  ava_value values[4] = {
    ava_value_of_integer(0),
    ava_value_of_integer(1),
    ava_value_of_string(ava_string_of_cstring("foo")),
    ava_value_of_string(ava_string_of_cstring("bar")),
  };
  ava_list_value left, right, result;
  unsigned i;

  left = ava_esba_list_of_raw(values, 2);
  right = ava_array_list_of_raw(values + 2, 2);
  result = ava_list_concat(left, right);

  ck_assert_int_eq(4, ava_list_length(result));
  for (i = 0; i < 4; ++i)
    assert_values_equal(values[i], ava_list_index(result, i));
}

deftest(noop_delete) {
  ava_value zero = ava_value_of_integer(0);
  ava_list_value list = ava_esba_list_of_raw(&zero, 1);
  ava_list_value result = ava_list_remove(list, 1, 1);

  ck_assert_int_eq(0, memcmp(&list, &result, sizeof(list)));
}

deftest(delete_to_empty_list) {
  ava_value values[2] = {
    ava_value_of_integer(42),
    ava_value_of_integer(56),
  };
  ava_list_value list = ava_esba_list_of_raw(values, 2);
  ava_list_value result = ava_list_remove(list, 0, 2);
  ava_list_value empty = ava_empty_list();

  ck_assert_int_eq(0, memcmp(&empty, &result, sizeof(result)));
}

deftest(delete_from_begin) {
  ava_value values[5] = {
    ava_value_of_integer(0),
    ava_value_of_integer(1),
    ava_value_of_integer(2),
    ava_value_of_integer(3),
    ava_value_of_integer(4),
  };
  ava_list_value list = ava_esba_list_of_raw(values, 5);
  ava_list_value result = ava_list_remove(list, 0, 2);

  ck_assert_int_eq(3, ava_list_length(result));
  assert_values_equal(values[2], ava_list_index(result, 0));
  assert_values_equal(values[3], ava_list_index(result, 1));
  assert_values_equal(values[4], ava_list_index(result, 2));
}

deftest(delete_from_middle) {
  ava_value values[5] = {
    ava_value_of_integer(0),
    ava_value_of_integer(1),
    ava_value_of_integer(2),
    ava_value_of_integer(3),
    ava_value_of_integer(4),
  };
  ava_list_value list = ava_esba_list_of_raw(values, 5);
  ava_list_value result = ava_list_remove(list, 2, 4);

  ck_assert_int_eq(3, ava_list_length(result));
  assert_values_equal(values[0], ava_list_index(result, 0));
  assert_values_equal(values[1], ava_list_index(result, 1));
  assert_values_equal(values[4], ava_list_index(result, 2));
}

deftest(delete_from_end) {
  ava_value values[5] = {
    ava_value_of_integer(0),
    ava_value_of_integer(1),
    ava_value_of_integer(2),
    ava_value_of_integer(3),
    ava_value_of_integer(4),
  };
  ava_list_value list = ava_esba_list_of_raw(values, 5);
  ava_list_value result = ava_list_remove(list, 3, 5);

  ck_assert_int_eq(3, ava_list_length(result));
  assert_values_equal(values[0], ava_list_index(result, 0));
  assert_values_equal(values[1], ava_list_index(result, 1));
  assert_values_equal(values[2], ava_list_index(result, 2));
}

deftest(compatible_set) {
  ava_value values[3] = {
    ava_value_of_integer(0),
    ava_value_of_integer(1),
    ava_value_of_integer(2),
  };
  ava_list_value list = ava_esba_list_of_raw(values, 3);
  ava_list_value result = ava_list_set(list, 1, ava_value_of_integer(42));

  ck_assert_int_eq(3, ava_list_length(result));
  assert_values_equal(values[0], ava_list_index(result, 0));
  assert_values_equal(ava_value_of_integer(42),
                      ava_list_index(result, 1));
  assert_values_equal(values[2], ava_list_index(result, 2));
}

deftest(incompatible_set) {
  ava_value values[3] = {
    ava_value_of_integer(0),
    ava_value_of_integer(1),
    ava_value_of_integer(2),
  };
  ava_value str = ava_value_of_string(ava_string_of_cstring("foo"));
  ava_list_value list = ava_esba_list_of_raw(values, 3);
  ava_list_value result = ava_list_set(list, 1, str);

  ck_assert_int_eq(3, ava_list_length(result));
  assert_values_equal(values[0], ava_list_index(result, 0));
  assert_values_equal(str, ava_list_index(result, 1));
  assert_values_equal(values[2], ava_list_index(result, 2));
}
