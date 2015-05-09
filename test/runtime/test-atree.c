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

#include <string.h>
#include <stdlib.h>

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/alloc.h"
#include "runtime/-atree.h"

defsuite(atree);

static size_t weight_function(const void*restrict data, size_t nelt) {
  return nelt * sizeof(unsigned);
}

static const ava_atree_spec spec = {
  .elt_size = sizeof(unsigned),
  .weight_function = weight_function,
};

static unsigned get_at(ava_atree tree, size_t ix) {
  size_t avail;
  const unsigned* ret = ava_atree_get(tree, ix, &spec, &avail);
  ck_assert_int_lt(0, avail);

  return *ret;
}

deftest(empty_atree_is_empty) {
  ava_atree tree = ava_atree_new(&spec);

  ck_assert_int_eq(0, ava_atree_length(tree));
}

deftest(append_and_read_one_elt) {
  ava_atree empty = ava_atree_new(&spec);
  ava_atree single = ava_atree_append(empty, (unsigned[]) { 42 }, 1, &spec);

  ck_assert_int_eq(0, ava_atree_length(empty));
  ck_assert_int_eq(1, ava_atree_length(single));
  ck_assert_int_eq(42, get_at(single, 0));
}

deftest(conflicting_singluar_append) {
  ava_atree empty = ava_atree_new(&spec);
  ava_atree left = ava_atree_append(empty, (unsigned[]) { 42 }, 1, &spec);
  ava_atree right = ava_atree_append(empty, (unsigned[]) { 56 }, 1, &spec);

  ck_assert_int_eq(0, ava_atree_length(empty));
  ck_assert_int_eq(1, ava_atree_length(left));
  ck_assert_int_eq(1, ava_atree_length(right));
  ck_assert_int_eq(42, get_at(left, 0));
  ck_assert_int_eq(56, get_at(right, 0));
}

deftest(single_append_single_read_70k) {
  unsigned i;
  static ava_atree trees[70000];

  trees[0] = ava_atree_new(&spec);
  for (i = 1; i < 70000; ++i)
    trees[i] = ava_atree_append(trees[i-1], &i, 1, &spec);

  for (i = 0; i < 70000; ++i) {
    ck_assert_int_eq(i, ava_atree_length(trees[i]));
  }

  ck_assert_int_le(70000 * sizeof(unsigned), ava_atree_weight(trees[69999]));

  for (i = 1; i < 70000; ++i)
    ck_assert_int_eq(i, get_at(trees[69999], i-1));
}

deftest(bulk10k_append_single_read_70k) {
  unsigned i;
  static unsigned values[70000];
  ava_atree tree;

  for (i = 0; i < 70000; ++i)
    values[i] = i;

  tree = ava_atree_new(&spec);
  for (i = 0; i < 7; ++i)
    tree = ava_atree_append(tree, values + i*10000, 10000, &spec);

  ck_assert_int_eq(70000, ava_atree_length(tree));
  for (i = 0; i < 70000; ++i)
    ck_assert_int_eq(i, get_at(tree, i));
}

deftest(bulk70k_append_single_read_70k) {
  unsigned i;
  static unsigned values[70000];
  ava_atree tree;

  for (i = 0; i < 70000; ++i)
    values[i] = i;

  tree = ava_atree_new(&spec);
  tree = ava_atree_append(tree, values, 70000, &spec);

  ck_assert_int_eq(70000, ava_atree_length(tree));
  for (i = 0; i < 70000; ++i)
    ck_assert_int_eq(i, get_at(tree, i));
}

deftest(single_append_bulk_read_70k) {
  unsigned i, j;
  const unsigned* read;
  size_t avail;
  ava_atree tree;

  tree = ava_atree_new(&spec);

  for (i = 0; i < 70000; ++i)
    tree = ava_atree_append(tree, &i, 1, &spec);

  ck_assert_int_eq(70000, ava_atree_length(tree));
  for (i = 0; i < 70000; i += avail) {
    read = ava_atree_get(tree, i, &spec, &avail);
    for (j = 0; j < avail; ++j)
      ck_assert_int_eq(i+j, read[j]);
  }
}

deftest(multilevel_conflicting_append) {
  unsigned i;
  ava_atree base, left, right;

  base = ava_atree_new(&spec);
  for (i = 0; i < 250; ++i)
    base = ava_atree_append(base, &i, 1, &spec);

  left = ava_atree_append(base, (unsigned[]) { 42 }, 1, &spec);
  right = ava_atree_append(base, (unsigned[]) { 56 }, 1, &spec);

  ck_assert_int_eq(251, ava_atree_length(left));
  ck_assert_int_eq(251, ava_atree_length(right));
  ck_assert_int_eq(42, get_at(left, 250));
  ck_assert_int_eq(56, get_at(right, 250));
}
