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

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/list.h"
#include "runtime/avalanche/map.h"
#include "runtime/avalanche/integer.h"

defsuite(empty_list);

static void force(ava_value list) {
  ava_string_chunk_iterator(list);
}

deftest(stringifies_to_empty_string) {
  ck_assert_str_eq("", ava_string_to_cstring(
                     ava_to_string(ava_empty_list().v)));
}

deftest(string_chunk_iterator_is_empty) {
  ava_datum it;

  it = ava_string_chunk_iterator(ava_empty_list().v);
  ck_assert(!ava_string_is_present(ava_iterate_string_chunk(
                                     &it, ava_empty_list().v)));
}

deftest(has_length_zero) {
  ck_assert_int_eq(0, ava_list_length(ava_empty_list().v));
}

deftest(permits_slice_zero_to_zero) {
  ava_value empty = ava_empty_list().v;
  ava_value result = ava_list_slice(ava_empty_list().v, 0, 0);
  ck_assert_int_eq(0, memcmp(&result, &empty, sizeof(result)));
}

deftest_signal(refuses_nonzero_slice, SIGABRT) {
  force(ava_list_slice(ava_empty_list().v, 1, 1));
}

deftest_signal(refuses_index, SIGABRT) {
  force(ava_list_index(ava_empty_list().v, 0));
}

deftest(appends_to_singleton_array_list) {
  ava_value result = ava_list_append(ava_empty_list().v, ava_empty_list().v);
  ck_assert_int_eq(1, ava_list_length(result));
}

deftest(concats_to_other_list) {
  ava_value empty = ava_empty_list().v;
  ava_value other = ava_list_of_values(&empty, 1).v;
  ava_value result = ava_list_concat(ava_empty_list().v, other);

  ck_assert_int_eq(0, memcmp(&other, &result, sizeof(result)));
}

deftest(permits_zero_to_zero_delete) {
  ava_value result = ava_list_delete(ava_empty_list().v, 0, 0);
  ava_value empty = ava_empty_list().v;

  ck_assert_int_eq(0, memcmp(&result, &empty, sizeof(result)));
}

deftest_signal(refuses_nonzero_delete, SIGABRT) {
  force(ava_list_delete(ava_empty_list().v, 1, 1));
}

deftest_signal(refuses_set, SIGABRT) {
  force(ava_list_set(ava_empty_list().v, 0, ava_empty_list().v));
}

deftest(is_empty_map) {
  ck_assert_int_eq(0, ava_map_npairs(ava_empty_map()));
}

deftest(contains_no_map_elements) {
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE,
                   ava_map_find(ava_empty_map(), WORD(foo)));
}

deftest_signal(refuses_map_next, SIGABRT) {
  force(INT(ava_map_next(ava_empty_map(), 0)));
}

deftest_signal(refuses_map_get, SIGABRT) {
  force(ava_map_get(ava_empty_map(), 0));
}

deftest_signal(refuses_map_get_key, SIGABRT) {
  force(ava_map_get_key(ava_empty_map(), 0));
}

deftest_signal(refuses_map_set, SIGABRT) {
  force(ava_map_set(ava_empty_map(), 0, INT(42)).v);
}

deftest_signal(refuses_map_delete, SIGABRT) {
  force(ava_map_delete(ava_empty_map(), 0).v);
}

deftest(produces_singleton_map_on_map_add) {
  ava_map_value result = ava_map_add(
    ava_empty_map(), WORD(foo), WORD(bar));
  ava_map_cursor cursor;

  cursor = ava_map_find(result, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(bar), ava_map_get(result, cursor));
}
