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
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/list.h"
#include "runtime/-array-list.h"

defsuite(array_list);

static ava_value values[2 * AVA_ARRAY_LIST_THRESH];

defsetup {
  unsigned i;

  for (i = 0; i < sizeof(values) / sizeof(ava_value); ++i)
    values[i] = ava_value_of_string(ava_string_of_char('a' + i));
}

defteardown { }

static ava_bool values_equal(ava_value a, ava_value b) {
  return 0 == memcmp(&a, &b, sizeof(a));
}

deftest(copied_from_array) {
  ava_value list = ava_array_list_of_raw(values, 4);
  ck_assert_int_eq(4, ava_list_length(list));
}

deftest(stringification_produces_normal_form) {
  ava_value list = ava_array_list_of_raw(values, 4);
  ck_assert_str_eq("a b c d",
                   ava_string_to_cstring(ava_to_string(list)));
}

deftest(value_weight_nonzero) {
  ava_value list = ava_array_list_of_raw(values, 4);

  ck_assert_int_lt(0, ava_value_weight(list));
}

deftest(simple_indexing) {
  ava_value list = ava_array_list_of_raw(values, 4);
  unsigned i;

  for (i = 0; i < 4; ++i)
    ck_assert_int_eq(ava_value_str(values[i]).ascii9,
                     ava_value_str(ava_list_index(list, i)).ascii9);
}

deftest(copying_append) {
  ava_value orig = ava_array_list_of_raw(values, 1);
  ava_value new = ava_list_append(orig, values[1]);

  ck_assert_int_eq(1, ava_list_length(orig));
  ck_assert(values_equal(values[0], ava_list_index(orig, 0)));
  ck_assert_int_eq(2, ava_list_length(new));
  ck_assert(values_equal(values[0], ava_list_index(new, 0)));
  ck_assert(values_equal(values[1], ava_list_index(new, 1)));
}

deftest(inplace_append) {
  ava_value orig = ava_array_list_of_raw(values, 1);
  /* Append so the array is grown large enough for the next append to fit
   * in-place.
   */
  ava_value old = ava_list_append(orig, values[1]);
  /* Next one will use the same array */
  ava_value new = ava_list_append(old, values[2]);
  ck_assert_ptr_eq(ava_value_attr(old), ava_value_attr(new));

  ck_assert_int_eq(2, ava_list_length(old));
  ck_assert_int_eq(3, ava_list_length(new));
  ck_assert_int_eq(3, ava_array_list_used(old));
  ck_assert_int_eq(3, ava_array_list_used(new));
}

deftest(conflicting_append) {
  ava_value orig = ava_array_list_of_raw(values, 1);
  /* Append so the array is grown large enough for the next append to fit
   * in-place.
   */
  ava_value base = ava_list_append(orig, values[1]);
  /* Two lists independently build off base */
  ava_value left = ava_list_append(base, values[2]);
  ava_value right = ava_list_append(base, values[3]);

  ck_assert_int_eq(3, ava_list_length(left));
  ck_assert_int_eq(3, ava_list_length(right));
  ck_assert(values_equal(values[2], ava_list_index(left, 2)));
  ck_assert(values_equal(values[3], ava_list_index(right, 2)));
  ck_assert_int_eq(3, ava_array_list_used(base));
  ck_assert_int_eq(3, ava_array_list_used(left));
  ck_assert_int_eq(3, ava_array_list_used(right));
}

deftest(copying_concat) {
  ava_value orig = ava_array_list_of_raw(values, 2);
  ava_value other = ava_array_list_of_raw(values + 2, 2);
  ava_value cat = ava_list_concat(orig, other);
  unsigned i;

  ck_assert_int_eq(4, ava_list_length(cat));
  for (i = 0; i < 4; ++i)
    ck_assert(values_equal(values[i], ava_list_index(cat, i)));
}

deftest(inplace_concat) {
  ava_value orig = ava_array_list_of_raw(values, 2);
  ava_value o23 = ava_array_list_of_raw(values + 2, 2);
  ava_value o45 = ava_array_list_of_raw(values + 4, 2);
  ava_value old = ava_list_concat(orig, o23);
  ava_value new = ava_list_concat(old, o45);
  unsigned i;

  ck_assert_ptr_eq(ava_value_attr(old), ava_value_attr(new));
  ck_assert_int_eq(4, ava_list_length(old));
  ck_assert_int_eq(6, ava_list_length(new));
  ck_assert_int_eq(6, ava_array_list_used(old));
  ck_assert_int_eq(6, ava_array_list_used(new));
  for (i = 0; i < 6; ++i)
    ck_assert(values_equal(values[i], ava_list_index(new, i)));
}

deftest(conflicting_concat) {
  ava_value orig = ava_array_list_of_raw(values, 2);
  ava_value o23 = ava_array_list_of_raw(values + 2, 2);
  ava_value o45 = ava_array_list_of_raw(values + 4, 2);
  ava_value o67 = ava_array_list_of_raw(values + 6, 2);
  ava_value base = ava_list_concat(orig, o23);
  ava_value left = ava_list_concat(base, o45);
  ava_value right = ava_list_concat(base, o67);

  ck_assert_ptr_eq(ava_value_attr(base), ava_value_attr(left));
  ck_assert_int_eq(4, ava_list_length(base));
  ck_assert_int_eq(6, ava_list_length(left));
  ck_assert_int_eq(6, ava_list_length(right));
  ck_assert_int_eq(6, ava_array_list_used(base));
  ck_assert_int_eq(6, ava_array_list_used(left));
  ck_assert_int_eq(6, ava_array_list_used(right));
  ck_assert(values_equal(values[4], ava_list_index(left, 4)));
  ck_assert(values_equal(values[5], ava_list_index(left, 5)));
  ck_assert(values_equal(values[6], ava_list_index(right, 4)));
  ck_assert(values_equal(values[7], ava_list_index(right, 5)));
}

deftest(inplace_self_concat) {
  ava_value orig = ava_array_list_of_raw(values, 2);
  ava_value o23 = ava_array_list_of_raw(values + 2, 2);
  ava_value base = ava_list_concat(orig, o23);
  ava_value result = ava_list_concat(base, base);
  unsigned i;

  ck_assert_ptr_eq(ava_value_attr(base), ava_value_attr(result));
  ck_assert_int_eq(4, ava_list_length(base));
  ck_assert_int_eq(8, ava_list_length(result));
  ck_assert_int_eq(8, ava_array_list_used(result));
  for (i = 0; i < 4; ++i) {
    ck_assert(values_equal(values[i], ava_list_index(base, i)));
    ck_assert(values_equal(values[i], ava_list_index(result, i)));
    ck_assert(values_equal(values[i], ava_list_index(result, i+4)));
  }
}

deftest(empty_list_concat) {
  ava_value orig = ava_array_list_of_raw(values, 2);
  ava_value result = ava_list_concat(orig, ava_empty_list);

  ck_assert_ptr_eq(ava_value_attr(orig), ava_value_attr(result));
}

deftest(slice_to_empty) {
  ava_value orig = ava_array_list_of_raw(values, 4);
  ava_value empty = ava_list_slice(orig, 1, 1);

  ck_assert(values_equal(ava_empty_list, empty));
}

deftest(inplace_slice) {
  ava_value orig = ava_array_list_of_raw(values, 8);
  ava_value slice = ava_list_slice(orig, 0, 4);
  unsigned i;

  ck_assert_int_eq(8, ava_list_length(orig));
  ck_assert_int_eq(4, ava_list_length(slice));
  ck_assert_ptr_eq(ava_value_attr(orig), ava_value_attr(slice));
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(8, ava_array_list_used(slice));

  for (i = 0; i < 4; ++i)
    ck_assert(values_equal(values[i], ava_list_index(slice, i)));
}

deftest(copying_slice_due_to_misalignment) {
  ava_value orig = ava_array_list_of_raw(values, 8);
  ava_value slice = ava_list_slice(orig, 1, 8);
  unsigned i;

  ck_assert_int_eq(8, ava_list_length(orig));
  ck_assert_int_eq(7, ava_list_length(slice));
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(7, ava_array_list_used(slice));

  for (i = 0; i < 7; ++i)
    ck_assert(values_equal(values[i+1], ava_list_index(slice, i)));
}

deftest(copying_slice_due_to_size_reduction) {
  ava_value orig = ava_array_list_of_raw(values, 8);
  ava_value slice = ava_list_slice(orig, 0, 2);
  unsigned i;

  ck_assert_int_eq(8, ava_list_length(orig));
  ck_assert_int_eq(2, ava_list_length(slice));
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(2, ava_array_list_used(slice));

  for (i = 0; i < 2; ++i)
    ck_assert(values_equal(values[i], ava_list_index(slice, i)));
}

deftest(noop_delete) {
  ava_value orig = ava_array_list_of_raw(values, 8);
  ava_value new = ava_list_delete(orig, 5, 5);

  ck_assert(values_equal(orig, new));
}

deftest(prefix_delete) {
  ava_value orig = ava_array_list_of_raw(values, 8);
  ava_value new = ava_list_delete(orig, 0, 2);
  unsigned i;

  ck_assert_int_eq(8, ava_list_length(orig));
  ck_assert_int_eq(6, ava_list_length(new));
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(6, ava_array_list_used(new));

  for (i = 0; i < 6; ++i)
    ck_assert(values_equal(values[i + 2], ava_list_index(new, i)));
}

deftest(suffix_delete) {
  ava_value orig = ava_array_list_of_raw(values, 8);
  ava_value new = ava_list_delete(orig, 6, 8);
  unsigned i;

  ck_assert_int_eq(8, ava_list_length(orig));
  ck_assert_int_eq(6, ava_list_length(new));
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(8, ava_array_list_used(new));

  for (i = 0; i < 6; ++i)
    ck_assert(values_equal(values[i], ava_list_index(new, i)));
}

deftest(internal_delete) {
  ava_value orig = ava_array_list_of_raw(values, 8);
  ava_value new = ava_list_delete(orig, 4, 6);
  unsigned i;

  ck_assert_int_eq(8, ava_list_length(orig));
  ck_assert_int_eq(6, ava_list_length(new));
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(6, ava_array_list_used(new));

  for (i = 0; i < 4; ++i)
    ck_assert(values_equal(values[i], ava_list_index(new, i)));
  for (; i < 6; ++i)
    ck_assert(values_equal(values[i+2], ava_list_index(new, i)));
}

deftest(set) {
  ava_value new_value = values[20];
  ava_value orig = ava_array_list_of_raw(values, 8);
  ava_value new = ava_list_set(orig, 2, new_value);
  unsigned i;

  ck_assert_int_eq(8, ava_list_length(orig));
  ck_assert_int_eq(8, ava_list_length(new));

  for (i = 0; i < 8; ++i) {
    ck_assert(values_equal(values[i], ava_list_index(orig, i)));
    ck_assert(values_equal(values[2 == i? 20 : i], ava_list_index(new, i)));
  }
}
