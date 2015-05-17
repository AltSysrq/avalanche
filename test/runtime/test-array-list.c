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
  ava_list_value list = ava_array_list_of_raw(values, 4);
  ck_assert_int_eq(4, list.v->length(list));
}

deftest(stringification_produces_normal_form) {
  ava_list_value list = ava_array_list_of_raw(values, 4);
  ck_assert_str_eq("a b c d",
                   ava_string_to_cstring(
                     ava_to_string(list.v->to_value(list))));
}

deftest(value_weight_nonzero) {
  ava_list_value list = ava_array_list_of_raw(values, 4);

  ck_assert_int_lt(0, ava_value_weight(list.v->to_value(list)));
}

deftest(simple_indexing) {
  ava_list_value list = ava_array_list_of_raw(values, 4);
  unsigned i;

  for (i = 0; i < 4; ++i)
    ck_assert_int_eq(values[i].r1.str.ascii9,
                     list.v->index(list, i).r1.str.ascii9);
}

deftest(copying_append) {
  ava_list_value orig = ava_array_list_of_raw(values, 1);
  ava_list_value new = orig.v->append(orig, values[1]);

  ck_assert_int_eq(1, orig.v->length(orig));
  ck_assert(values_equal(values[0], orig.v->index(orig, 0)));
  ck_assert_int_eq(2, new.v->length(new));
  ck_assert(values_equal(values[0], new.v->index(new, 0)));
  ck_assert(values_equal(values[1], new.v->index(new, 1)));
}

deftest(inplace_append) {
  ava_list_value orig = ava_array_list_of_raw(values, 1);
  /* Append so the array is grown large enough for the next append to fit
   * in-place.
   */
  ava_list_value old = orig.v->append(orig, values[1]);
  /* Next one will use the same array */
  ava_list_value new = old.v->append(old, values[2]);
  ck_assert_ptr_eq(old.r1.ptr, new.r1.ptr);

  ck_assert_int_eq(2, old.v->length(old));
  ck_assert_int_eq(3, new.v->length(new));
  ck_assert_int_eq(3, ava_array_list_used(old));
  ck_assert_int_eq(3, ava_array_list_used(new));
}

deftest(conflicting_append) {
  ava_list_value orig = ava_array_list_of_raw(values, 1);
  /* Append so the array is grown large enough for the next append to fit
   * in-place.
   */
  ava_list_value base = orig.v->append(orig, values[1]);
  /* Two lists independently build off base */
  ava_list_value left = base.v->append(base, values[2]);
  ava_list_value right = base.v->append(base, values[3]);

  ck_assert_int_eq(3, left.v->length(left));
  ck_assert_int_eq(3, right.v->length(right));
  ck_assert(values_equal(values[2], left.v->index(left, 2)));
  ck_assert(values_equal(values[3], right.v->index(right, 2)));
  ck_assert_int_eq(3, ava_array_list_used(base));
  ck_assert_int_eq(3, ava_array_list_used(left));
  ck_assert_int_eq(3, ava_array_list_used(right));
}

deftest(copying_concat) {
  ava_list_value orig = ava_array_list_of_raw(values, 2);
  ava_list_value other = ava_array_list_of_raw(values + 2, 2);
  ava_list_value cat = orig.v->concat(orig, other);
  unsigned i;

  ck_assert_int_eq(4, cat.v->length(cat));
  for (i = 0; i < 4; ++i)
    ck_assert(values_equal(values[i], cat.v->index(cat, i)));
}

deftest(inplace_concat) {
  ava_list_value orig = ava_array_list_of_raw(values, 2);
  ava_list_value o23 = ava_array_list_of_raw(values + 2, 2);
  ava_list_value o45 = ava_array_list_of_raw(values + 4, 2);
  ava_list_value old = orig.v->concat(orig, o23);
  ava_list_value new = old.v->concat(old, o45);
  unsigned i;

  ck_assert_ptr_eq(old.r1.ptr, new.r1.ptr);
  ck_assert_int_eq(4, old.v->length(old));
  ck_assert_int_eq(6, new.v->length(new));
  ck_assert_int_eq(6, ava_array_list_used(old));
  ck_assert_int_eq(6, ava_array_list_used(new));
  for (i = 0; i < 6; ++i)
    ck_assert(values_equal(values[i], new.v->index(new, i)));
}

deftest(conflicting_concat) {
  ava_list_value orig = ava_array_list_of_raw(values, 2);
  ava_list_value o23 = ava_array_list_of_raw(values + 2, 2);
  ava_list_value o45 = ava_array_list_of_raw(values + 4, 2);
  ava_list_value o67 = ava_array_list_of_raw(values + 6, 2);
  ava_list_value base = orig.v->concat(orig, o23);
  ava_list_value left = base.v->concat(base, o45);
  ava_list_value right = base.v->concat(base, o67);

  ck_assert_ptr_eq(base.r1.ptr, left.r1.ptr);
  ck_assert_int_eq(4, base.v->length(base));
  ck_assert_int_eq(6, left.v->length(left));
  ck_assert_int_eq(6, right.v->length(right));
  ck_assert_int_eq(6, ava_array_list_used(base));
  ck_assert_int_eq(6, ava_array_list_used(left));
  ck_assert_int_eq(6, ava_array_list_used(right));
  ck_assert(values_equal(values[4], left.v->index(left, 4)));
  ck_assert(values_equal(values[5], left.v->index(left, 5)));
  ck_assert(values_equal(values[6], right.v->index(right, 4)));
  ck_assert(values_equal(values[7], right.v->index(right, 5)));
}

deftest(inplace_self_concat) {
  ava_list_value orig = ava_array_list_of_raw(values, 2);
  ava_list_value o23 = ava_array_list_of_raw(values + 2, 2);
  ava_list_value base = orig.v->concat(orig, o23);
  ava_list_value result = base.v->concat(base, base);
  unsigned i;

  ck_assert_ptr_eq(base.r1.ptr, result.r1.ptr);
  ck_assert_int_eq(4, base.v->length(base));
  ck_assert_int_eq(8, result.v->length(result));
  ck_assert_int_eq(8, ava_array_list_used(result));
  for (i = 0; i < 4; ++i) {
    ck_assert(values_equal(values[i], base.v->index(base, i)));
    ck_assert(values_equal(values[i], result.v->index(result, i)));
    ck_assert(values_equal(values[i], result.v->index(result, i+4)));
  }
}

deftest(empty_list_concat) {
  ava_list_value orig = ava_array_list_of_raw(values, 2);
  ava_list_value result = orig.v->concat(orig, ava_empty_list);

  ck_assert_ptr_eq(orig.r1.ptr, result.r1.ptr);
}

deftest(slice_to_empty) {
  ava_list_value orig = ava_array_list_of_raw(values, 4);
  ava_list_value empty = orig.v->slice(orig, 1, 1);

  ck_assert(values_equal(ava_empty_list.v->to_value(ava_empty_list),
                         empty.v->to_value(empty)));
}

deftest(inplace_slice) {
  ava_list_value orig = ava_array_list_of_raw(values, 8);
  ava_list_value slice = orig.v->slice(orig, 0, 4);
  unsigned i;

  ck_assert_int_eq(8, orig.v->length(orig));
  ck_assert_int_eq(4, slice.v->length(slice));
  ck_assert_ptr_eq(orig.r1.ptr, slice.r1.ptr);
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(8, ava_array_list_used(slice));

  for (i = 0; i < 4; ++i)
    ck_assert(values_equal(values[i], slice.v->index(slice, i)));
}

deftest(copying_slice_due_to_misalignment) {
  ava_list_value orig = ava_array_list_of_raw(values, 8);
  ava_list_value slice = orig.v->slice(orig, 1, 8);
  unsigned i;

  ck_assert_int_eq(8, orig.v->length(orig));
  ck_assert_int_eq(7, slice.v->length(slice));
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(7, ava_array_list_used(slice));

  for (i = 0; i < 7; ++i)
    ck_assert(values_equal(values[i+1], slice.v->index(slice, i)));
}

deftest(copying_slice_due_to_size_reduction) {
  ava_list_value orig = ava_array_list_of_raw(values, 8);
  ava_list_value slice = orig.v->slice(orig, 0, 2);
  unsigned i;

  ck_assert_int_eq(8, orig.v->length(orig));
  ck_assert_int_eq(2, slice.v->length(slice));
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(2, ava_array_list_used(slice));

  for (i = 0; i < 2; ++i)
    ck_assert(values_equal(values[i], slice.v->index(slice, i)));
}

deftest(noop_delete) {
  ava_list_value orig = ava_array_list_of_raw(values, 8);
  ava_list_value new = orig.v->delete(orig, 5, 5);

  ck_assert(values_equal(orig.v->to_value(orig), new.v->to_value(new)));
}

deftest(prefix_delete) {
  ava_list_value orig = ava_array_list_of_raw(values, 8);
  ava_list_value new = orig.v->delete(orig, 0, 2);
  unsigned i;

  ck_assert_int_eq(8, orig.v->length(orig));
  ck_assert_int_eq(6, new.v->length(new));
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(6, ava_array_list_used(new));

  for (i = 0; i < 6; ++i)
    ck_assert(values_equal(values[i + 2], new.v->index(new, i)));
}

deftest(suffix_delete) {
  ava_list_value orig = ava_array_list_of_raw(values, 8);
  ava_list_value new = orig.v->delete(orig, 6, 8);
  unsigned i;

  ck_assert_int_eq(8, orig.v->length(orig));
  ck_assert_int_eq(6, new.v->length(new));
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(8, ava_array_list_used(new));

  for (i = 0; i < 6; ++i)
    ck_assert(values_equal(values[i], new.v->index(new, i)));
}

deftest(internal_delete) {
  ava_list_value orig = ava_array_list_of_raw(values, 8);
  ava_list_value new = orig.v->delete(orig, 4, 6);
  unsigned i;

  ck_assert_int_eq(8, orig.v->length(orig));
  ck_assert_int_eq(6, new.v->length(new));
  ck_assert_int_eq(8, ava_array_list_used(orig));
  ck_assert_int_eq(6, ava_array_list_used(new));

  for (i = 0; i < 4; ++i)
    ck_assert(values_equal(values[i], new.v->index(new, i)));
  for (; i < 6; ++i)
    ck_assert(values_equal(values[i+2], new.v->index(new, i)));
}

deftest(set) {
  ava_value new_value = values[20];
  ava_list_value orig = ava_array_list_of_raw(values, 8);
  ava_list_value new = orig.v->set(orig, 2, new_value);
  unsigned i;

  ck_assert_int_eq(8, orig.v->length(orig));
  ck_assert_int_eq(8, new.v->length(new));

  for (i = 0; i < 8; ++i) {
    ck_assert(values_equal(values[i], orig.v->index(orig, i)));
    ck_assert(values_equal(values[2 == i? 20 : i], new.v->index(new, i)));
  }
}

deftest(iterator_preserves_index) {
  ava_list_value orig = ava_array_list_of_raw(values, 2);
  AVA_LIST_ITERATOR(orig, it);

  orig.v->iterator_place(orig, it, 42);
  ck_assert_int_eq(42, orig.v->iterator_index(orig, it));
  orig.v->iterator_move(orig, it, -40);
  ck_assert_int_eq(2, orig.v->iterator_index(orig, it));
}
