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
#include "runtime/avalanche/alloc.h"
#include "runtime/-esba.h"

defsuite(esba);

static size_t weight_function(const void*restrict userdata,
                              const void*restrict data, size_t count) {
  return sizeof(ava_ulong) * count;
}

static ava_esba new(void) {
  return ava_esba_new(sizeof(ava_ulong), 8, weight_function, ava_alloc_atomic,
                      NULL);
}

static ava_ulong get_at(ava_esba e, size_t ix) {
  ava_esba_tx tx;
  ava_ulong ret;
  const ava_ulong* data = ava_esba_access(e, &tx);

  ret = data[ix];

  ck_assert(ava_esba_check_access(e, data, tx));

  return ret;
}

static ava_esba set_at(ava_esba e, size_t ix, ava_ulong value) {
  return ava_esba_set(e, ix, &value);
}

deftest(new_esba_is_empty) {
  ava_esba e = new();
  ck_assert_int_eq(0, ava_esba_length(e));
}

deftest(append_and_read_one) {
  ava_esba e = new();
  e = ava_esba_append(e, (ava_ulong[]) { 42 }, 1);

  ck_assert_int_eq(1, ava_esba_length(e));
  ck_assert_int_eq(42, get_at(e, 0));
}

deftest(conflicting_single_append) {
  ava_esba empty = new();
  ava_esba left = ava_esba_append(empty, (ava_ulong[]) { 42 }, 1);
  ava_esba right = ava_esba_append(empty, (ava_ulong[]) { 56 }, 1);

  ck_assert_int_eq(0, ava_esba_length(empty));
  ck_assert_int_eq(1, ava_esba_length(left));
  ck_assert_int_eq(1, ava_esba_length(right));
  ck_assert_int_eq(42, get_at(left, 0));
  ck_assert_int_eq(56, get_at(right, 0));
}

deftest(multi_single_append) {
  ava_esba arrays[256];
  ava_ulong i, j;

  arrays[0] = new();
  for (i = 1; i < 256; ++i)
    arrays[i] = ava_esba_append(arrays[i-1], &i, 1);

  for (i = 0; i < 256; ++i) {
    ck_assert_int_eq(i, ava_esba_length(arrays[i]));
    for (j = 0; j < i; ++j) {
      ck_assert_int_eq(j+1, get_at(arrays[i], j));
    }
  }
}

deftest(multiword_append) {
  ava_ulong data[5] = { 42, 56, 72, 88, 101 };
  unsigned i;

  ava_esba empty = new();
  ava_esba one = ava_esba_append(empty, (ava_ulong[]) { 4 }, 1);
  ava_esba six = ava_esba_append(one, data, 5);

  ck_assert_int_eq(0, ava_esba_length(empty));
  ck_assert_int_eq(1, ava_esba_length(one));
  ck_assert_int_eq(6, ava_esba_length(six));
  ck_assert_int_eq(4, get_at(one, 0));
  ck_assert_int_eq(4, get_at(six, 0));

  for (i = 0; i < 5; ++i)
    ck_assert_int_eq(data[i], get_at(six, i+1));
}

deftest(large_append) {
  ava_ulong data[256];
  unsigned i;
  ava_esba e;

  for (i = 0; i < 256; ++i)
    data[i] = rand();

  e = new();
  e = ava_esba_append(e, data, 256);

  ck_assert_int_eq(256, ava_esba_length(e));
  for (i = 0; i < 256; ++i)
    ck_assert_int_eq(data[i], get_at(e, i));
}

deftest(simple_set) {
  ava_esba empty = new();
  ava_esba old = ava_esba_append(empty, (ava_ulong[]) { 42 }, 1);
  ava_esba set = set_at(old, 0, 56);

  ck_assert_int_eq(0, ava_esba_length(empty));
  ck_assert_int_eq(1, ava_esba_length(old));
  ck_assert_int_eq(1, ava_esba_length(set));

  ck_assert_int_eq(56, get_at(set, 0));
  ck_assert_int_eq(42, get_at(old, 0));
}

deftest(conflicting_set) {
  ava_esba base = ava_esba_append(new(), (ava_ulong[]) { 0 }, 1);
  ava_esba left = set_at(base, 0, 42);
  ava_esba right = set_at(base, 0, 56);

  ck_assert_int_eq(1, ava_esba_length(base));
  ck_assert_int_eq(1, ava_esba_length(left));
  ck_assert_int_eq(1, ava_esba_length(right));

  ck_assert_int_eq(56, get_at(right, 0));
  ck_assert_int_eq(42, get_at(left, 0));
  ck_assert_int_eq(0, get_at(base, 0));
}

deftest(chained_set) { /* like above, but right is built from left instead */
  ava_esba base = ava_esba_append(new(), (ava_ulong[]) { 0 }, 1);
  ava_esba left = set_at(base, 0, 42);
  ava_esba right = set_at(left, 0, 56);

  ck_assert_int_eq(1, ava_esba_length(base));
  ck_assert_int_eq(1, ava_esba_length(left));
  ck_assert_int_eq(1, ava_esba_length(right));

  ck_assert_int_eq(56, get_at(right, 0));
  ck_assert_int_eq(42, get_at(left, 0));
  ck_assert_int_eq(0, get_at(base, 0));
}

deftest(multiple_set) {
  ava_esba base = ava_esba_append(
    new(), (ava_ulong[]) { 1, 2, 3, 4 }, 4);
  ava_esba result = base;
  unsigned i;

  for (i = 0; i < 4; ++i)
    result = set_at(result, i, get_at(result, i) + 1);

  for (i = 0; i < 4; ++i) {
    ck_assert_int_eq(i+2, get_at(result, i));
    ck_assert_int_eq(i+1, get_at(base, i));
  }
}

deftest(multiple_overwrite) {
  ava_esba versions[5];
  unsigned i;

  versions[0] = ava_esba_append(new(), (ava_ulong[]) { 0 }, 1);
  for (i = 1; i < 5; ++i)
    versions[i] = set_at(versions[i-1], 0, i);

  for (i = 0; i < 5; ++i)
    ck_assert_int_eq(i, get_at(versions[i], 0));
}

deftest(overflowing_overwrite) {
  ava_esba versions[32];
  unsigned i;

  versions[0] = ava_esba_append(new(), (ava_ulong[]) { 0 }, 1);
  for (i = 1; i < 32; ++i)
    versions[i] = set_at(versions[i-1], 0, i);

  for (i = 0; i < 32; ++i)
    ck_assert_int_eq(i, get_at(versions[i], 0));
}

deftest(append_after_set) {
  ava_esba empty = new();
  ava_esba one = ava_esba_append(empty, (ava_ulong[]) { 1 }, 1);
  ava_esba mod = set_at(one, 0, 4);
  ava_esba two = ava_esba_append(mod, (ava_ulong[]) { 2 }, 1);

  ck_assert_int_eq(0, ava_esba_length(empty));
  ck_assert_int_eq(1, ava_esba_length(one));
  ck_assert_int_eq(1, ava_esba_length(mod));
  ck_assert_int_eq(2, ava_esba_length(two));

  ck_assert_int_eq(1, get_at(one, 0));
  ck_assert_int_eq(4, get_at(mod, 0));
  ck_assert_int_eq(4, get_at(two, 0));
  ck_assert_int_eq(2, get_at(two, 1));
}

deftest(conflicting_append_after_set) {
  ava_esba empty = new();
  ava_esba one = ava_esba_append(empty, (ava_ulong[]) { 1 }, 1);
  ava_esba mod = set_at(one, 0, 4);
  ava_esba two = ava_esba_append(one, (ava_ulong[]) { 2 }, 1);

  ck_assert_int_eq(0, ava_esba_length(empty));
  ck_assert_int_eq(1, ava_esba_length(one));
  ck_assert_int_eq(1, ava_esba_length(mod));
  ck_assert_int_eq(2, ava_esba_length(two));

  ck_assert_int_eq(1, get_at(one, 0));
  ck_assert_int_eq(4, get_at(mod, 0));
  ck_assert_int_eq(1, get_at(two, 0));
  ck_assert_int_eq(2, get_at(two, 1));
}

deftest(conflicting_set_after_append) {
  ava_esba empty = new();
  ava_esba one = ava_esba_append(empty, (ava_ulong[]) { 1 }, 1);
  ava_esba two = ava_esba_append(one, (ava_ulong[]) { 2 }, 1);
  ava_esba mod = set_at(two, 0, 4);

  ck_assert_int_eq(0, ava_esba_length(empty));
  ck_assert_int_eq(1, ava_esba_length(one));
  ck_assert_int_eq(2, ava_esba_length(mod));
  ck_assert_int_eq(2, ava_esba_length(two));

  ck_assert_int_eq(1, get_at(one, 0));
  ck_assert_int_eq(1, get_at(two, 0));
  ck_assert_int_eq(2, get_at(two, 1));
  ck_assert_int_eq(4, get_at(mod, 0));
  ck_assert_int_eq(2, get_at(mod, 1));
}

deftest(check_access_fails_on_conflicting_set) {
  ava_esba old;
  ava_esba_tx tx;
  const void* data;

  old = ava_esba_append(new(), (ava_ulong[]) { 1 }, 1);
  data = ava_esba_access(old, &tx);
  (void)set_at(old, 0, 42);

  ck_assert(!ava_esba_check_access(old, data, tx));
}

deftest(check_access_passes_after_concurrent_append) {
  ava_esba old;
  ava_esba_tx tx;
  const void* data;

  old = ava_esba_append(new(), (ava_ulong[]) { 1 }, 1);
  data = ava_esba_access(old, &tx);
  (void)ava_esba_append(old, (ava_ulong[]) { 42 }, 1);

  ck_assert(ava_esba_check_access(old, data, tx));
}

deftest(weight_calculated_correctly) {
  ava_esba e = new();

  e = ava_esba_append(e, (ava_ulong[]) { 1, 2, 3, 4 }, 4);
  e = set_at(e, 0, 42);

  ck_assert_int_eq(5 * sizeof(ava_ulong), ava_esba_weight(e));
}

deftest(two_part_append) {
  ava_esba base = ava_esba_append(new(), (ava_ulong[]) { 42 }, 1);
  ava_esba left = base, right;

  ava_ulong* left_dst = ava_esba_start_append(&left, 3);
  left_dst[0] = 1;
  left_dst[1] = 2;
  left_dst[2] = 3;
  /* Conflicting append before append finishes */
  right = ava_esba_append(base, (ava_ulong[]) { 56 }, 1);
  ava_esba_finish_append(left, 3);

  ck_assert_int_eq(1, ava_esba_length(base));
  ck_assert_int_eq(42, get_at(base, 0));
  ck_assert_int_eq(4, ava_esba_length(left));
  ck_assert_int_eq(42, get_at(left, 0));
  ck_assert_int_eq(1, get_at(left, 1));
  ck_assert_int_eq(2, get_at(left, 2));
  ck_assert_int_eq(3, get_at(left, 3));
  ck_assert_int_eq(2, ava_esba_length(right));
  ck_assert_int_eq(42, get_at(right, 0));
  ck_assert_int_eq(56, get_at(right, 1));
}
