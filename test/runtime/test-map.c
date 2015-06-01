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

defsuite(map);

static const char* map_type(ava_map_value map) {
  if (0 == memcmp(&map, (ava_map_value[]) { ava_empty_map() },
                  sizeof(ava_map_value))) {
    return "empty-map";
  } else {
    return ((const ava_attribute*)ava_value_attr(map.v))->tag->name;
  }
}

deftest(empty_map_of_values) {
  ava_map_value map = ava_map_of_values(NULL, 0, NULL, 0, 0);
  ck_assert_str_eq("empty-map", map_type(map));
}

deftest(list_map_of_few_noninterleaved_values) {
  ava_value keys[] =   { WORD(foo),   WORD(bar) };
  ava_value values[] = { WORD(plugh), WORD(xyzzy) };
  ava_map_value map = ava_map_of_values(keys, 1, values, 1, 2);

  ck_assert_int_eq(2, ava_map_npairs(map));
  assert_values_equal(WORD(plugh),
                      ava_map_get(map, ava_map_find(map, WORD(foo))));
  assert_values_equal(WORD(xyzzy),
                      ava_map_get(map, ava_map_find(map, WORD(bar))));
  ck_assert_str_eq("list-map", map_type(map));
}

deftest(list_map_of_few_interleaved_values) {
  ava_value values[] = { WORD(foo), WORD(plugh),
                         WORD(bar), WORD(xyzzy) };
  ava_map_value map = ava_map_of_values(values, 2, values+1, 2, 2);

  ck_assert_int_eq(2, ava_map_npairs(map));
  assert_values_equal(WORD(plugh),
                      ava_map_get(map, ava_map_find(map, WORD(foo))));
  assert_values_equal(WORD(xyzzy),
                      ava_map_get(map, ava_map_find(map, WORD(bar))));
  ck_assert_str_eq("list-map", map_type(map));
}

deftest(hash_map_from_many_values) {
  ava_value value = WORD(foo);
  ava_map_value map = ava_map_of_values(&value, 0, &value, 0, 32);

  ck_assert_int_eq(32, ava_map_npairs(map));
  ck_assert_str_eq("hash-map-ava_ushort", map_type(map));
}

deftest(string_to_empty_map) {
  ava_map_value map = ava_map_value_of(
    ava_value_of_cstring("   \t\n"));
  assert_values_same(ava_empty_map().v, map.v);
}

deftest(string_to_list_map) {
  ava_map_value map = ava_map_value_of(
    ava_value_of_cstring("foo bar\nbaz quux"));

  ck_assert_str_eq("list-map", map_type(map));
  ck_assert_int_eq(2, ava_map_npairs(map));
  assert_values_equal(WORD(bar),
                      ava_map_get(map, ava_map_find(map, WORD(foo))));
  assert_values_equal(WORD(quux),
                      ava_map_get(map, ava_map_find(map, WORD(baz))));
}

deftest(string_to_hash_map) {
  ava_map_value map = ava_map_value_of(
    ava_value_of_cstring(
      "0 0 1 1 2 2 3 3 4 4 5 5 6 6 7 7 8 8"));

  ck_assert_str_eq("hash-map-ava_ushort", map_type(map));
  ck_assert_int_eq(9, ava_map_npairs(map));
}
