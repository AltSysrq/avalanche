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

deftest(responds_to_query_accelerator) {
  ava_list_value orig = ava_array_list_of_raw(values, 4);
  ava_list_value cycled = ava_list_value_of(orig.v->to_value(orig));

  ck_assert_int_eq(0, memcmp(&orig, &cycled, sizeof(orig)));
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
