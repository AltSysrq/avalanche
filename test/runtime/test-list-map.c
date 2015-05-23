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

#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/list.h"
#include "runtime/avalanche/map.h"
#include "runtime/-list-map.h"

defsuite(list_map);

static void assert_values_equal(ava_value a, ava_value b) {
  ck_assert_int_eq(0, ava_value_strcmp(a, b));
}

deftest(basic_construct) {
  ava_value values[] = {
    ava_value_of_cstring("foo"), ava_value_of_cstring("bar"),
    ava_value_of_cstring("baz"), ava_value_of_cstring("quux"),
  };
  ava_map_value map = ava_list_map_of_list(
    ava_list_of_values(values, 4));

  ck_assert_int_eq(2, ava_map_npairs(map));
  ck_assert_int_eq(4, ava_list_length(map.v));
}

deftest(simple_access) {
  ava_value values[] = {
    ava_value_of_cstring("foo"), ava_value_of_cstring("bar"),
    ava_value_of_cstring("baz"), ava_value_of_cstring("quux"),
  };
  ava_map_value map = ava_list_map_of_list(
    ava_list_of_values(values, 4));
  ava_map_cursor cursor;

  cursor = ava_map_find(map, values[0]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(values[0], ava_map_get_key(map, cursor));
  assert_values_equal(values[1], ava_map_get(map, cursor));

  cursor = ava_map_find(map, values[2]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(values[2], ava_map_get_key(map, cursor));
  assert_values_equal(values[3], ava_map_get(map, cursor));
}

deftest(nonexistent_access) {
  ava_value values[] = {
    ava_value_of_cstring("foo"), ava_value_of_cstring("bar"),
    ava_value_of_cstring("baz"), ava_value_of_cstring("quux"),
  };
  ava_map_value map = ava_list_map_of_list(
    ava_list_of_values(values, 4));
  ava_map_cursor cursor;

  cursor = ava_map_find(map, ava_value_of_cstring("xyzzy"));
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);
}

deftest(multimap_access) {
  ava_value values[] = {
    ava_value_of_cstring("foo"), ava_value_of_cstring("bar"),
    ava_value_of_cstring("baz"), ava_value_of_cstring("quux"),
    ava_value_of_cstring("foo"), ava_value_of_cstring("plugh"),
  };
  ava_map_value map = ava_list_map_of_list(
    ava_list_of_values(values, 6));
  ava_map_cursor cursor;

  cursor = ava_map_find(map, values[0]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(values[0], ava_map_get_key(map, cursor));
  assert_values_equal(values[1], ava_map_get(map, cursor));
  cursor = ava_map_next(map, cursor);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(values[4], ava_map_get_key(map, cursor));
  assert_values_equal(values[5], ava_map_get(map, cursor));
  cursor = ava_map_next(map, cursor);
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);

  cursor = ava_map_find(map, values[2]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(values[2], ava_map_get_key(map, cursor));
  assert_values_equal(values[3], ava_map_get(map, cursor));
  cursor = ava_map_next(map, cursor);
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);
}

deftest(set_value) {
  ava_value values[] = {
    ava_value_of_cstring("foo"), ava_value_of_cstring("bar"),
    ava_value_of_cstring("baz"), ava_value_of_cstring("quux"),
    ava_value_of_cstring("foo"), ava_value_of_cstring("plugh"),
  };
  ava_map_value map = ava_list_map_of_list(
    ava_list_of_values(values, 6));
  ava_map_value orig = map;
  ava_map_cursor cursor;

  cursor = ava_map_find(map, values[0]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  map = ava_map_set(map, cursor, ava_value_of_cstring("xyzzy"));

  cursor = ava_map_find(map, values[2]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  map = ava_map_set(map, cursor, ava_value_of_cstring("fum"));

  assert_values_equal(ava_value_of_cstring("xyzzy"),
                      ava_map_get(map, ava_map_find(map, values[0])));
  assert_values_equal(ava_value_of_cstring("fum"),
                      ava_map_get(map, ava_map_find(map, values[2])));
  assert_values_equal(
    ava_value_of_cstring("foo bar baz quux foo plugh"), orig.v);
  assert_values_equal(
    ava_value_of_cstring("foo xyzzy baz fum foo plugh"), map.v);
}

deftest(delete_pair_from_nonend) {
  ava_value values[] = {
    ava_value_of_cstring("foo"), ava_value_of_cstring("bar"),
    ava_value_of_cstring("baz"), ava_value_of_cstring("quux"),
    ava_value_of_cstring("foo"), ava_value_of_cstring("plugh"),
  };
  ava_map_value map = ava_list_map_of_list(
    ava_list_of_values(values, 6));
  ava_map_value orig = map;
  ava_map_cursor cursor;

  cursor = ava_map_find(map, values[2]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  map = ava_map_delete(map, cursor);

  ck_assert_int_eq(2, ava_map_npairs(map));
  ck_assert_int_eq(4, ava_list_length(map.v));
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, ava_map_find(map, values[2]));
  assert_values_equal(
    ava_value_of_cstring("foo bar baz quux foo plugh"), orig.v);
  assert_values_equal(ava_value_of_cstring("foo bar foo plugh"), map.v);
}

/* Because array-list behaves differently when deleting from the end */
deftest(delete_pair_from_end) {
  ava_value values[] = {
    ava_value_of_cstring("foo"), ava_value_of_cstring("bar"),
    ava_value_of_cstring("baz"), ava_value_of_cstring("quux"),
    ava_value_of_cstring("foo"), ava_value_of_cstring("plugh"),
  };
  ava_map_value map = ava_list_map_of_list(
    ava_list_of_values(values, 6));
  ava_map_value orig = map;
  ava_map_cursor cursor;

  cursor = ava_map_find(map, values[0]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  cursor = ava_map_next(map, cursor);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  map = ava_map_delete(map, cursor);

  ck_assert_int_eq(2, ava_map_npairs(map));
  ck_assert_int_eq(4, ava_list_length(map.v));
  cursor = ava_map_find(map, values[0]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, ava_map_next(map, cursor));
  assert_values_equal(
    ava_value_of_cstring("foo bar baz quux foo plugh"), orig.v);
  assert_values_equal(
    ava_value_of_cstring("foo bar baz quux"), map.v);
}

deftest(append_single_pair) {
  ava_value values[] = {
    ava_value_of_cstring("foo"), ava_value_of_cstring("bar"),
    ava_value_of_cstring("baz"), ava_value_of_cstring("quux"),
  };
  ava_map_value map = ava_list_map_of_list(
    ava_list_of_values(values, 4));
  ava_map_value orig = map;
  ava_map_cursor cursor;

  map = ava_map_add(map, ava_value_of_cstring("foo"),
                    ava_value_of_cstring("plugh"));
  ck_assert_int_eq(3, ava_map_npairs(map));
  ck_assert_int_eq(6, ava_list_length(map.v));

  cursor = ava_map_find(map, values[0]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  cursor = ava_map_next(map, cursor);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  cursor = ava_map_next(map, cursor);
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);

  assert_values_equal(
    ava_value_of_cstring("foo bar baz quux"), orig.v);
  assert_values_equal(
    ava_value_of_cstring("foo bar baz quux foo plugh"), map.v);
}

/* Because array-list will do an in-place append on the second one */
deftest(append_two_pairs) {
  ava_value values[] = {
    ava_value_of_cstring("foo"), ava_value_of_cstring("bar"),
    ava_value_of_cstring("baz"), ava_value_of_cstring("quux"),
  };
  ava_map_value map = ava_list_map_of_list(
    ava_list_of_values(values, 4));
  ava_map_value orig = map, one;

  map = ava_map_add(map, ava_value_of_cstring("foo"),
                    ava_value_of_cstring("plugh"));
  ck_assert_int_eq(3, ava_map_npairs(map));
  ck_assert_int_eq(6, ava_list_length(map.v));
  one = map;

  map = ava_map_add(map, ava_value_of_cstring("fum"),
                    ava_value_of_cstring("x"));
  ck_assert_int_eq(4, ava_map_npairs(map));
  ck_assert_int_eq(8, ava_list_length(map.v));

  assert_values_equal(
    ava_value_of_cstring("foo bar baz quux"), orig.v);
  assert_values_equal(
    ava_value_of_cstring("foo bar baz quux foo plugh"), one.v);
  assert_values_equal(
    ava_value_of_cstring("foo bar baz quux foo plugh fum x"), map.v);
}

deftest(list_operations) {
  ava_value values[] = {
    ava_value_of_cstring("foo"), ava_value_of_cstring("bar"),
    ava_value_of_cstring("baz"), ava_value_of_cstring("quux"),
  };
  ava_value map = ava_list_map_of_list(
    ava_list_of_values(values, 4)).v;

  assert_values_equal(values[1], ava_list_index(map, 1));
  assert_values_equal(ava_value_of_cstring("bar baz"),
                      ava_list_slice(map, 1, 3));
  assert_values_equal(ava_value_of_cstring("foo bar baz quux xyzzy"),
                      ava_list_append(map, ava_value_of_cstring("xyzzy")));
  assert_values_equal(ava_value_of_cstring("foo bar baz quux xyzzy plugh"),
                      ava_list_concat(
                        map, ava_value_of_cstring("xyzzy\nplugh")));
  assert_values_equal(ava_value_of_cstring("foo quux"),
                      ava_list_delete(map, 1, 3));
  assert_values_equal(ava_value_of_cstring("foo bar plugh quux"),
                      ava_list_set(map, 2, ava_value_of_cstring("plugh")));
}
