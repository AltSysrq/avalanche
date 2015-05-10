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

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/defs.h"
#include "runtime/-bxlist.h"

#define BXLIST_ORDER AVA_BXLIST_ORDER
#define MULTILEVEL_SZ (BXLIST_ORDER*BXLIST_ORDER + 5)

defsuite(bxlist);

static size_t weight_function(const void* v) {
  return 1;
}

static ava_bxlist create(void) {
  return ava_bxlist_new(sizeof(unsigned), weight_function);
}

static unsigned get_at(ava_bxlist list, size_t ix) {
  size_t avail;
  const unsigned*restrict ptr = ava_bxlist_access(list, ix, &avail);

  ck_assert_int_lt(0, avail);
  return *ptr;
}

static ava_bxlist append(ava_bxlist list, unsigned val) {
  return ava_bxlist_append(list, &val, 1);
}

static ava_bxlist set_at(ava_bxlist list, size_t ix, unsigned val) {
  return ava_bxlist_replace(list, ix, &val, 1);
}

static ava_bxlist insert_at(ava_bxlist list, size_t ix, unsigned val) {
  return ava_bxlist_insert(list, ix, &val, 1);
}

deftest(new_bxlist_is_empty) {
  ck_assert_int_eq(0, ava_bxlist_length(create()));
}

deftest(append_one_and_read) {
  ava_bxlist list = create();
  list = append(list, 42);
  ck_assert_int_eq(1, ava_bxlist_length(list));
  ck_assert_int_eq(42, get_at(list, 0));
}

deftest(single_append_is_persistent) {
  ava_bxlist empty = create();
  ava_bxlist left = append(empty, 42);
  ava_bxlist right = append(empty, 56);

  ck_assert_int_eq(0, ava_bxlist_length(empty));
  ck_assert_int_eq(1, ava_bxlist_length(left));
  ck_assert_int_eq(42, get_at(left, 0));
  ck_assert_int_eq(56, get_at(right, 0));
}

deftest(full_leaf_is_accessible) {
  ava_bxlist list = create();
  unsigned i;

  for (i = 0; i < BXLIST_ORDER; ++i)
    list = append(list, i + 42);

  ck_assert_int_eq(BXLIST_ORDER, ava_bxlist_length(list));
  for (i = 0; i < BXLIST_ORDER; ++i)
    ck_assert_int_eq(i + 42, get_at(list, i));
}

deftest(can_grow_beyond_leaf_via_append) {
  ava_bxlist list = create();
  unsigned i;

  for (i = 0; i <= BXLIST_ORDER; ++i)
    list = append(list, i + 42);

  ck_assert_int_eq(BXLIST_ORDER+1, ava_bxlist_length(list));
  for (i = 0; i <= BXLIST_ORDER; ++i)
    ck_assert_int_eq(i + 42, get_at(list, i));
}

deftest(very_large_list_via_append_fully_accessible) {
  ava_bxlist list = create();
  unsigned i;

  for (i = 0; i < 65537; ++i)
    list = append(list, i + 42);

  ck_assert_int_eq(65537, ava_bxlist_length(list));
  for (i = 0; i < 65537; ++i)
    ck_assert_int_eq(i + 42, get_at(list, i));
}

deftest(append_multinode_conflict) {
  ava_bxlist base = create(), left, right;
  unsigned i;

  for (i = 0; i < 3 * BXLIST_ORDER - 5; ++i)
    base = append(base, i + 42);

  left = append(base, 999);
  right = append(base, 1024);

  ck_assert_int_eq(3 * BXLIST_ORDER - 5, ava_bxlist_length(base));
  ck_assert_int_eq(3 * BXLIST_ORDER - 4, ava_bxlist_length(left));
  ck_assert_int_eq(3 * BXLIST_ORDER - 4, ava_bxlist_length(right));
  for (i = 0; i < 3 * BXLIST_ORDER - 5; ++i) {
    ck_assert_int_eq(i + 42, get_at(base, i));
    ck_assert_int_eq(i + 42, get_at(left, i));
    ck_assert_int_eq(i + 42, get_at(right, i));
  }
  ck_assert_int_eq(999, get_at(left, i));
  ck_assert_int_eq(1024, get_at(right, i));
}

deftest(basic_replace) {
  ava_bxlist base = create(), r;

  base = append(base, 5);
  r = set_at(base, 0, 6);

  ck_assert_int_eq(1, ava_bxlist_length(base));
  ck_assert_int_eq(1, ava_bxlist_length(r));
  ck_assert_int_eq(5, get_at(base, 0));
  ck_assert_int_eq(6, get_at(r, 0));
}

deftest(conflicting_replace) {
  ava_bxlist base = create(), left, right;

  base = append(base, 5);
  left = set_at(base, 0, 6);
  right = set_at(base, 0, 7);

  ck_assert_int_eq(1, ava_bxlist_length(base));
  ck_assert_int_eq(1, ava_bxlist_length(left));
  ck_assert_int_eq(1, ava_bxlist_length(right));
  ck_assert_int_eq(5, get_at(base, 0));
  ck_assert_int_eq(6, get_at(left, 0));
  ck_assert_int_eq(7, get_at(right, 0));
}

deftest(replace_overflowing_patch) {
  ava_bxlist base = create(), first, second;

  base = append(base, 5);
  first = set_at(base, 0, 6);
  second = set_at(first, 0, 7);

  ck_assert_int_eq(1, ava_bxlist_length(base));
  ck_assert_int_eq(1, ava_bxlist_length(first));
  ck_assert_int_eq(1, ava_bxlist_length(second));
  ck_assert_int_eq(5, get_at(base, 0));
  ck_assert_int_eq(6, get_at(first, 0));
  ck_assert_int_eq(7, get_at(second, 0));
}

deftest(multilevel_replace) {
  ava_bxlist numbers = create(), odd_doubled, even_doubled;
  unsigned i;

  for (i = 0; i < MULTILEVEL_SZ; ++i)
    numbers = append(numbers, i);

  odd_doubled = even_doubled = numbers;
  for (i = 0; i < MULTILEVEL_SZ; ++i)
    if (i & 1)
      odd_doubled = set_at(odd_doubled, i, get_at(odd_doubled, i) * 2);
    else
      even_doubled = set_at(even_doubled, i, get_at(even_doubled, i) * 2);

  ck_assert_int_eq(MULTILEVEL_SZ, ava_bxlist_length(numbers));
  ck_assert_int_eq(MULTILEVEL_SZ, ava_bxlist_length(odd_doubled));
  ck_assert_int_eq(MULTILEVEL_SZ, ava_bxlist_length(even_doubled));
  for (i = 0; i < MULTILEVEL_SZ; ++i) {
    ck_assert_int_eq(i, get_at(numbers, i));
    if (i & 1) {
      ck_assert_int_eq(i, get_at(even_doubled, i));
      ck_assert_int_eq(i*2, get_at(odd_doubled, i));
    } else {
      ck_assert_int_eq(i*2, get_at(even_doubled, i));
      ck_assert_int_eq(i, get_at(odd_doubled, i));
    }
  }
}

deftest(multilevel_multireplace) {
  ava_bxlist base = create(), versions[32];
  unsigned i;

  for (i = 0; i < MULTILEVEL_SZ; ++i)
    base = append(base, i);

  versions[0] = base;
  for (i = 1; i < 32; ++i)
    versions[i] = set_at(versions[i-1], 0, i);

  for (i = 0; i < 32; ++i) {
    ck_assert_int_eq(MULTILEVEL_SZ, ava_bxlist_length(versions[i]));
    ck_assert_int_eq(i, get_at(versions[i], 0));
  }
}

deftest(simple_insert_at_middle) {
  ava_bxlist base = create(), sub;

  base = append(base, 0);
  base = append(base, 2);
  sub = insert_at(base, 1, 1);

  ck_assert_int_eq(2, ava_bxlist_length(base));
  ck_assert_int_eq(3, ava_bxlist_length(sub));
  ck_assert_int_eq(0, get_at(base, 0));
  ck_assert_int_eq(2, get_at(base, 1));
  ck_assert_int_eq(0, get_at(sub, 0));
  ck_assert_int_eq(1, get_at(sub, 1));
  ck_assert_int_eq(2, get_at(sub, 2));
}

deftest(simple_insert_at_begin) {
  ava_bxlist base = create(), sub;

  base = append(base, 1);
  base = append(base, 2);
  sub = insert_at(base, 0, 0);

  ck_assert_int_eq(2, ava_bxlist_length(base));
  ck_assert_int_eq(3, ava_bxlist_length(sub));
  ck_assert_int_eq(1, get_at(base, 0));
  ck_assert_int_eq(2, get_at(base, 1));
  ck_assert_int_eq(0, get_at(sub, 0));
  ck_assert_int_eq(1, get_at(sub, 1));
  ck_assert_int_eq(2, get_at(sub, 2));
}

deftest(simple_insert_at_end) {
  ava_bxlist base = create(), sub;

  base = append(base, 0);
  base = append(base, 1);
  sub = insert_at(base, 2, 2);

  ck_assert_int_eq(2, ava_bxlist_length(base));
  ck_assert_int_eq(3, ava_bxlist_length(sub));
  ck_assert_int_eq(0, get_at(base, 0));
  ck_assert_int_eq(1, get_at(base, 1));
  ck_assert_int_eq(0, get_at(sub, 0));
  ck_assert_int_eq(1, get_at(sub, 1));
  ck_assert_int_eq(2, get_at(sub, 2));
}

deftest(single_level_insert_split) {
  ava_bxlist base = create(), splits[BXLIST_ORDER+1];
  unsigned i, j;

  for (i = 0; i < BXLIST_ORDER; ++i)
    base = append(base, i);

  for (i = 0; i <= BXLIST_ORDER; ++i)
    splits[i] = insert_at(base, i, 999);

  ck_assert_int_eq(BXLIST_ORDER, ava_bxlist_length(base));
  for (i = 0; i <= BXLIST_ORDER; ++i)
    ck_assert_int_eq(BXLIST_ORDER+1, ava_bxlist_length(splits[i]));

  for (i = 0; i < BXLIST_ORDER; ++i)
    ck_assert_int_eq(i, get_at(base, i));

  for (i = 0; i <= BXLIST_ORDER; ++i)
    for (j = 0; j <= BXLIST_ORDER; ++j)
      ck_assert_int_eq(j == i? 999 : j < i? j : j-1,
                       get_at(splits[i], j));
}

deftest(multi_level_insert_split) {
  ava_bxlist base = create(), splits[BXLIST_ORDER+1];
  unsigned i, j;

  for (i = 0; i < MULTILEVEL_SZ; ++i)
    base = append(base, i);

  for (i = 0; i <= BXLIST_ORDER; ++i)
    splits[i] = insert_at(base, i, 999);

  ck_assert_int_eq(MULTILEVEL_SZ, ava_bxlist_length(base));
  for (i = 0; i <= BXLIST_ORDER; ++i)
    ck_assert_int_eq(MULTILEVEL_SZ+1, ava_bxlist_length(splits[i]));

  for (i = 0; i < MULTILEVEL_SZ; ++i)
    ck_assert_int_eq(i, get_at(base, i));

  for (i = 0; i <= BXLIST_ORDER; ++i)
    for (j = 0; j < MULTILEVEL_SZ; ++j)
      ck_assert_int_eq(j == i? 999 : j < i? j : j-1,
                       get_at(splits[i], j));
}

deftest(multi_level_insert_at_end) {
  ava_bxlist base = create(), new;
  unsigned i;

  for (i = 0; i < MULTILEVEL_SZ; ++i)
    base = append(base, i);

  new = insert_at(base, MULTILEVEL_SZ, MULTILEVEL_SZ);

  ck_assert_int_eq(MULTILEVEL_SZ, ava_bxlist_length(base));
  ck_assert_int_eq(MULTILEVEL_SZ+1, ava_bxlist_length(new));

  for (i = 0; i < MULTILEVEL_SZ; ++i)
    ck_assert_int_eq(i, get_at(base, i));
  for (i = 0; i <= MULTILEVEL_SZ; ++i)
    ck_assert_int_eq(i, get_at(new, i));
}

deftest(multi_level_multi_insert) {
  ava_bxlist base = create(), versions[32];
  unsigned i, j;

  for (i = 0; i < MULTILEVEL_SZ; ++i)
    base = append(base, i);

  versions[0] = base;
  for (i = 1; i < 32; ++i)
    versions[i] = insert_at(versions[i-1], 0, 1000 + i);

  for (i = 0; i < 32; ++i) {
    ck_assert_int_eq(MULTILEVEL_SZ + i, ava_bxlist_length(versions[i]));
    for (j = 0; j < MULTILEVEL_SZ + i; ++j) {
      ck_assert_int_eq(j < i? 1000 + i - j : j - i,
                       get_at(versions[i], j));
    }
  }
}
