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
#include "runtime/avalanche/integer.h"
#include "runtime/avalanche/list.h"
#include "runtime/avalanche/map.h"
#include "runtime/-hash-map.h"

defsuite(hash_map);

deftest(array_construction) {
  ava_value keys[] = { WORD(foo), WORD(bar) };
  ava_value values[] = { WORD(plugh), WORD(xyzzy) };

  ava_map_value map = ava_hash_map_of_raw(
    keys, 1, values, 1, 2);
  ava_map_cursor cursor;

  ck_assert_int_eq(2, ava_map_npairs(map));
  ck_assert_int_eq(4, ava_list_length(map.v));

  assert_values_equal(ava_value_of_cstring("foo plugh bar xyzzy"),
                      map.v);

  cursor = ava_map_find(map, keys[0]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(keys[0], ava_map_get_key(map, cursor));
  assert_values_equal(values[0], ava_map_get(map, cursor));

  cursor = ava_map_find(map, keys[1]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(keys[1], ava_map_get_key(map, cursor));
  assert_values_equal(values[1], ava_map_get(map, cursor));

  ck_assert_str_eq("ascii9", ava_hash_map_get_hash_function(map));
}

deftest(list_construction) {
  ava_value values[] = {
    WORD(foo), WORD(bar),
    WORD(baz), WORD(fum),
  };
  ava_list_value list = ava_list_of_values(values, 4);
  ava_map_value map = ava_hash_map_of_list(list);
  ava_map_cursor cursor;

  ck_assert_int_eq(2, ava_map_npairs(map));
  ck_assert_int_eq(4, ava_list_length(map.v));

  assert_values_equal(ava_value_of_cstring("foo bar baz fum"),
                      map.v);

  cursor = ava_map_find(map, values[0]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(values[0], ava_map_get_key(map, cursor));
  assert_values_equal(values[1], ava_map_get(map, cursor));

  cursor = ava_map_find(map, values[2]);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(values[2], ava_map_get_key(map, cursor));
  assert_values_equal(values[3], ava_map_get(map, cursor));

  ck_assert_str_eq("ascii9", ava_hash_map_get_hash_function(map));
}

deftest(access_nonexistent_element) {
  ava_map_value map = ava_hash_map_of_list(
    ava_list_of_values((ava_value[]) {
      WORD(foo), WORD(bar),
    }, 2));

  ava_map_cursor cursor;

  cursor = ava_map_find(map, WORD(blah));
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);
}

deftest(ascii9_hashed_finds_non_ascii9_keys) {
  AVA_STATIC_STRING(foo_str, "foo");
  ava_value values[] = {
    WORD(foo), WORD(bar),
    WORD(42), WORD(baz),
  };
  ava_map_value map = ava_hash_map_of_list(
    ava_list_of_values(values, 4));
  ava_map_cursor cursor;

  ck_assert_str_eq("ascii9", ava_hash_map_get_hash_function(map));
  assert_value_equals_str("foo bar 42 baz", map.v);

  cursor = ava_map_find(map, ava_value_of_string(foo_str));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(bar), ava_map_get(map, cursor));

  cursor = ava_map_find(map, INT(42));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(baz), ava_map_get(map, cursor));
}

deftest(create_list_non_ascii9) {
  ava_value values[] = {
    WORD(foo), WORD(bar),
    INT(42), INT(56),
  };
  ava_map_value map = ava_hash_map_of_list(
    ava_list_of_values(values, 4));
  ava_map_cursor cursor;

  ck_assert_str_eq("value", ava_hash_map_get_hash_function(map));
  assert_value_equals_str("foo bar 42 56", map.v);

  cursor = ava_map_find(map, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(bar), ava_map_get(map, cursor));

  cursor = ava_map_find(map, WORD(42));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(INT(56), ava_map_get(map, cursor));

  cursor = ava_map_find(map, INT(42));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(INT(56), ava_map_get(map, cursor));
}

deftest(multimap_access) {
  ava_value values[] = {
    WORD(foo), WORD(bar),
    WORD(baz), WORD(quux),
    WORD(foo), WORD(xyzzy),
  };
  ava_map_value map = ava_hash_map_of_raw(values, 2, values + 1, 2, 3);
  ava_map_cursor cursor;

  assert_value_equals_str("foo bar baz quux foo xyzzy", map.v);

  cursor = ava_map_find(map, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(bar), ava_map_get(map, cursor));

  cursor = ava_map_next(map, cursor);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(xyzzy), ava_map_get(map, cursor));

  cursor = ava_map_next(map, cursor);
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);

  cursor = ava_map_find(map, WORD(baz));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(quux), ava_map_get(map, cursor));

  cursor = ava_map_next(map, cursor);
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);
}

deftest(basic_add) {
  ava_map_value orig = ava_hash_map_of_list(
    ava_list_of_values((ava_value[]) {
      WORD(foo), WORD(bar),
    }, 2));
  ava_map_value map;
  ava_map_cursor cursor;

  map = ava_map_add(orig, WORD(plugh), WORD(xyzzy));

  ck_assert_int_eq(1, ava_map_npairs(orig));
  ck_assert_int_eq(2, ava_list_length(orig.v));
  ck_assert_int_eq(2, ava_map_npairs(map));
  ck_assert_int_eq(4, ava_list_length(map.v));
  assert_values_equal(ava_value_of_cstring("foo bar"), orig.v);
  assert_values_equal(ava_value_of_cstring("foo bar plugh xyzzy"),
                      map.v);

  cursor = ava_map_find(map, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(foo), ava_map_get_key(map, cursor));
  assert_values_equal(WORD(bar), ava_map_get(map, cursor));

  cursor = ava_map_find(orig, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(foo), ava_map_get_key(map, cursor));
  assert_values_equal(WORD(bar), ava_map_get(map, cursor));

  cursor = ava_map_find(map, WORD(plugh));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(plugh), ava_map_get_key(map, cursor));
  assert_values_equal(WORD(xyzzy), ava_map_get(map, cursor));

  cursor = ava_map_find(orig, WORD(plugh));
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);

  ck_assert_str_eq("ascii9", ava_hash_map_get_hash_function(map));
}

deftest(conflicting_add) {
  ava_map_value base = ava_hash_map_of_list(
    ava_list_of_values(
      (ava_value[]) { WORD(foo), WORD(bar) }, 2));
  ava_map_value left, right;
  ava_map_cursor cursor;

  left = ava_map_add(base, WORD(plugh), WORD(xyzzy));
  left = ava_map_add(left, WORD(fee), WORD(foe));
  right = ava_map_add(base, WORD(fee), WORD(foo));
  right = ava_map_add(right, WORD(plugh), WORD(42));

  ck_assert_int_eq(3, ava_map_npairs(left));
  ck_assert_int_eq(3, ava_map_npairs(right));
  assert_value_equals_str("foo bar plugh xyzzy fee foe", left.v);
  assert_value_equals_str("foo bar fee foo plugh 42", right.v);

  cursor = ava_map_find(left, WORD(plugh));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(xyzzy), ava_map_get(left, cursor));

  cursor = ava_map_find(left, WORD(fee));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(foe), ava_map_get(left, cursor));

  cursor = ava_map_find(right, WORD(plugh));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(42), ava_map_get(right, cursor));

  cursor = ava_map_find(right, WORD(fee));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(foo), ava_map_get(right, cursor));
}

deftest(add_non_ascii9_to_ascii9_hashed) {
  ava_map_value map = ava_hash_map_of_list(
    ava_list_of_values(
      (ava_value[]) { WORD(foo), WORD(bar) }, 2));
  ava_map_cursor cursor;

  ck_assert_str_eq("ascii9", ava_hash_map_get_hash_function(map));

  map = ava_map_add(map, INT(42), INT(56));
  ck_assert_str_eq("value", ava_hash_map_get_hash_function(map));

  assert_value_equals_str("foo bar 42 56", map.v);

  cursor = ava_map_find(map, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(bar), ava_map_get(map, cursor));

  cursor = ava_map_find(map, INT(42));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(INT(56), ava_map_get(map, cursor));
}

deftest(add_many_times) {
  ava_map_value map = ava_hash_map_of_list(
    ava_list_of_values(
      (ava_value[]) { INT(0), INT(1) }, 2));
  unsigned i;
  ava_map_cursor cursor;

  for (i = 1; i < 4096; ++i)
    map = ava_map_add(map, INT(i), INT(i+1));

  ck_assert_int_eq(4096, ava_map_npairs(map));
  ck_assert_int_eq(8192, ava_list_length(map.v));

  for (i = 0; i < 4096; ++i) {
    cursor = ava_map_find(map, INT(i));
    ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
    assert_values_equal(INT(i+1), ava_map_get(map, cursor));
  }
}

deftest(ascii9_hashing_switches_to_value_hashing_on_too_many_collisions) {
  ava_map_value map = ava_hash_map_of_list(
    ava_list_of_values(
      (ava_value[]) { WORD(foo), INT(0) }, 2));
  ava_map_cursor cursor;
  unsigned i;

  ck_assert_str_eq("ascii9", ava_hash_map_get_hash_function(map));

  /* Add a bunch of duplicates to cause collisions.
   *
   * This will provoke the map to switch to value hashing. Not that it will
   * help in this case, but testing it like this means we don't need to waste
   * time finding hash collisions / writing code to find hash collisions.
   */
  for (i = 1; i < 16; ++i)
    map = ava_map_add(map, WORD(foo), INT(i));

  ck_assert_str_eq("value", ava_hash_map_get_hash_function(map));

  cursor = ava_map_find(map, WORD(foo));
  for (i = 0; i < 16; ++i) {
    ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
    assert_values_equal(INT(i), ava_map_get(map, cursor));
    cursor = ava_map_next(map, cursor);
  }

  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);
}

deftest(multimap_add) {
  ava_value values[] = { WORD(foo), WORD(bar) };
  ava_map_value orig = ava_hash_map_of_raw(values, 2, values+1, 2, 1);
  ava_map_value map = orig;
  ava_map_cursor cursor;

  map = ava_map_add(map, WORD(foo), WORD(xyzzy));

  cursor = ava_map_find(orig, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(bar), ava_map_get(orig, cursor));

  cursor = ava_map_next(orig, cursor);
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);

  cursor = ava_map_find(map, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(bar), ava_map_get(map, cursor));

  cursor = ava_map_next(map, cursor);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(xyzzy), ava_map_get(map, cursor));

  cursor = ava_map_next(map, cursor);
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);
}

deftest(basic_set) {
  ava_value values[] = {
    WORD(foo), WORD(bar),
    WORD(baz), WORD(quux),
    WORD(foo), WORD(plugh),
    WORD(foo), WORD(42),
  };
  ava_map_value orig = ava_hash_map_of_raw(values, 2, values+1, 2, 4);
  ava_map_value map = orig;
  ava_map_cursor cursor;

  cursor = ava_map_find(map, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  cursor = ava_map_next(map, cursor);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(plugh), ava_map_get(map, cursor));

  map = ava_map_set(map, cursor, WORD(xyzzy));

  assert_value_equals_str("foo bar baz quux foo plugh foo 42", orig.v);
  assert_value_equals_str("foo bar baz quux foo xyzzy foo 42", map.v);

  cursor = ava_map_next(orig, ava_map_find(orig, WORD(foo)));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(plugh), ava_map_get(orig, cursor));

  cursor = ava_map_next(map, ava_map_find(map, WORD(foo)));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(xyzzy), ava_map_get(map, cursor));
}

deftest(conflicting_set) {
  ava_value values[] = { WORD(foo), WORD(bar) };
  ava_map_value base = ava_hash_map_of_raw(values, 2, values+1, 2, 1);
  ava_map_value left, right;
  ava_map_cursor cursor;

  cursor = ava_map_find(base, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);

  left = ava_map_set(base, cursor, WORD(plugh));
  right = ava_map_set(base, cursor, WORD(xyzzy));

  assert_value_equals_str("foo bar", base.v);
  assert_value_equals_str("foo plugh", left.v);
  assert_value_equals_str("foo xyzzy", right.v);
}

deftest(basic_delete) {
  ava_value values[] = {
    WORD(foo), WORD(bar),
    WORD(baz), WORD(quux),
    WORD(foo), WORD(plugh),
    WORD(foo), WORD(42),
  };
  ava_map_value orig = ava_hash_map_of_raw(values, 2, values+1, 2, 4);
  ava_map_value map = orig;
  ava_map_cursor cursor;

  cursor = ava_map_find(map, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  cursor = ava_map_next(map, cursor);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(plugh), ava_map_get(map, cursor));

  map = ava_map_delete(map, cursor);

  ck_assert_int_eq(8, ava_list_length(orig.v));
  ck_assert_int_eq(4, ava_map_npairs(orig.v));
  ck_assert_int_eq(6, ava_list_length(map.v));
  ck_assert_int_eq(3, ava_map_npairs(map.v));

  cursor = ava_map_find(orig, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(bar), ava_map_get(orig, cursor));

  cursor = ava_map_next(orig, cursor);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(plugh), ava_map_get(orig, cursor));

  cursor = ava_map_next(orig, cursor);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(42), ava_map_get(orig, cursor));

  cursor = ava_map_next(orig, cursor);
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);

  cursor = ava_map_find(map, WORD(foo));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(bar), ava_map_get(map, cursor));

  cursor = ava_map_next(map, cursor);
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  assert_values_equal(WORD(42), ava_map_get(map, cursor));

  cursor = ava_map_next(map, cursor);
  ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);

  assert_value_equals_str("foo bar baz quux foo plugh foo 42", orig.v);
  assert_value_equals_str("foo bar baz quux foo 42", map.v);
}

deftest(delete_vacuum) {
  ava_value values[] = {
    INT(0), INT(0),
  };
  ava_map_value map = ava_hash_map_of_raw(values, 2, values+1, 2, 1);
  ava_map_cursor cursor;
  unsigned i;

  for (i = 1; i < 64; ++i)
    map = ava_map_add(map, INT(i), INT(i));

  /* Delete all even keys but 42.
   *
   * This test deletes even and odd keys in separate passes so that vacuum is
   * not presented with a trivial case (eg, all deleted elements at the
   * beginning or end).
   */
  for (i = 0; i < 64; i += 2) {
    if (42 == i) continue;

    cursor = ava_map_find(map, INT(i));
    ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
    map = ava_map_delete(map, cursor);

    cursor = ava_map_find(map, INT(i));
    ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);

    cursor = ava_map_find(map, INT(42));
    ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
    assert_values_equal(INT(42), ava_map_get(map, cursor));
  }
  /* Delete all odd keys */
  for (i = 1; i < 64; i += 2) {
    cursor = ava_map_find(map, INT(i));
    ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
    map = ava_map_delete(map, cursor);

    cursor = ava_map_find(map, INT(i));
    ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);

    cursor = ava_map_find(map, INT(42));
    ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
    assert_values_equal(INT(42), ava_map_get(map, cursor));
  }

  assert_value_equals_str("42 42", map.v);
  for (i = 0; i < 64; ++i) {
    cursor = ava_map_find(map, INT(i));
    if (42 == i) {
      ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
      assert_values_equal(INT(42), ava_map_get(map, cursor));
      cursor = ava_map_next(map, cursor);
      ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);
    } else {
      ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);
    }
  }
}

deftest(delete_to_empty) {
  ava_value values[] = { INT(0), INT(0), INT(1), INT(1) };
  ava_map_value map = ava_hash_map_of_raw(values, 2, values+1, 2, 2);
  ava_map_cursor cursor;

  cursor = ava_map_find(map, INT(1));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  map = ava_map_delete(map, cursor);

  cursor = ava_map_find(map, INT(0));
  ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
  map = ava_map_delete(map, cursor);

  assert_values_same(ava_empty_map().v, map.v);
}

deftest(extend_after_delete) {
  ava_value values[] = { INT(0), INT(0) };
  ava_map_value map = ava_hash_map_of_raw(values, 2, values+1, 2, 1);
  ava_map_cursor cursor;
  unsigned i, j;

  /* Expand the map to fill a full 64-bit deletion bitmap entry */
  for (i = 1; i < 64; ++i)
    map = ava_map_add(map, INT(i), INT(i));

  /* Delete an element from the middle */
  map = ava_map_delete(map, ava_map_find(map, INT(42)));

  /* Add another element. This will spill over the end of the existing deletion
   * bitmap without provoking a rehash/vacuum.
   */
  map = ava_map_add(map, INT(64), INT(64));

  /* Ensure that map reading correctly handles the undersized bitmap */
  ck_assert_int_eq(64, ava_map_npairs(map));
  for (i = 0; i < 65; ++i) {
    cursor = ava_map_find(map, INT(i));
    if (42 == i) {
      ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);
    } else {
      ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
      assert_values_equal(INT(i), ava_map_get(map, cursor));
    }
  }

  /* Ensure that list access correctly handles the undersized bitmap */
  ck_assert_int_eq(64*2, ava_list_length(map.v));
  for (i = 0; i < 64; ++i) {
    j = i < 42? i : i+1;
    assert_values_equal(INT(j), ava_list_index(map.v, i*2+0));
    assert_values_equal(INT(j), ava_list_index(map.v, i*2+1));
  }

  /* Ensure that vacuuming correctly handles the undersized bitmap */
  for (i = 0; i < 42; ++i)
    map = ava_map_delete(map, ava_map_find(map, INT(i)));

  ck_assert_int_eq(22, ava_map_npairs(map));
  for (i = 0; i < 65; ++i) {
    cursor = ava_map_find(map, INT(i));
    if (i <= 42) {
      ck_assert_int_eq(AVA_MAP_CURSOR_NONE, cursor);
    } else {
      ck_assert_int_ne(AVA_MAP_CURSOR_NONE, cursor);
      assert_values_equal(INT(i), ava_map_get(map, cursor));
    }
  }
}

deftest(concat_with_self) {
  ava_value values[] = { WORD(foo), WORD(bar) };
  ava_value map = ava_hash_map_of_raw(values, 2, values+1, 2, 1).v;

  map = ava_list_concat(map, map);
  /* Ensure it really is still a map */
  ck_assert_ptr_ne(NULL, AVA_GET_ATTRIBUTE(map, ava_map_trait));

  assert_value_equals_str("foo bar foo bar", map);
}

deftest(concat_with_even_list) {
  ava_value values[] = { WORD(foo), WORD(bar), WORD(baz) };
  ava_value map = ava_hash_map_of_raw(values, 2, values+1, 2, 1).v;
  ava_value list = ava_list_of_values(values+1, 2).v;

  map = ava_list_concat(map, list);
  ck_assert_ptr_ne(NULL, AVA_GET_ATTRIBUTE(map, ava_map_trait));

  assert_value_equals_str("foo bar bar baz", map);
}

deftest(concat_with_odd_list) {
  ava_value values[] = { WORD(foo), WORD(bar), WORD(baz) };
  ava_value map = ava_hash_map_of_raw(values, 2, values+1, 2, 1).v;
  ava_value list = ava_list_of_values(values, 3).v;

  map = ava_list_concat(map, list);
  /* It can't be a map any more */
  ck_assert_ptr_eq(NULL, AVA_GET_ATTRIBUTE(map, ava_map_trait));

  assert_value_equals_str("foo bar foo bar baz", map);
}

deftest(list_slice) {
  ava_value values[] = { INT(0), INT(1), INT(2), INT(3) };
  ava_value map = ava_hash_map_of_raw(values, 2, values+1, 2, 2).v;
  ava_value slice;

  slice = ava_list_slice(map, 1, 3);
  assert_value_equals_str("1 2", slice);
}

deftest(list_append) {
  ava_value values[] = { INT(0), INT(1), INT(2), INT(3) };
  ava_value map = ava_hash_map_of_raw(values, 2, values+1, 2, 2).v;
  ava_value result;

  result = ava_list_append(map, WORD(plugh));
  assert_value_equals_str("0 1 2 3 plugh", result);
}

deftest(list_delete) {
  ava_value values[] = { INT(0), INT(1), INT(2), INT(3) };
  ava_value map = ava_hash_map_of_raw(values, 2, values+1, 2, 2).v;
  ava_value result;

  result = ava_list_delete(map, 1, 3);
  assert_value_equals_str("0 3", result);
}

deftest(list_set) {
  ava_value values[] = { INT(0), INT(1), INT(2), INT(3) };
  ava_value map = ava_hash_map_of_raw(values, 2, values+1, 2, 2).v;
  ava_value result;

  result = ava_list_set(map, 1, WORD(xyzzy));
  assert_value_equals_str("0 xyzzy 2 3", result);
}
