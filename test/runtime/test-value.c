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

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/integer.h"

defsuite(value);

/* For testing, our type represents a string with some integer number of
 * characters, starting from NUL at the very end of the string, and
 * incrementing for each preceding bytes. It stores this count in the ulong
 * field of the value.
 *
 * Chunk iterators store the number of characters left; each chunk contains one
 * character.
 */
static ava_datum xn_string_chunk_iterator(ava_value value) {
  return (ava_datum) { .ulong = ava_value_ulong(value) };
}

static ava_string xn_iterate_string_chunk(ava_datum*restrict it,
                                          ava_value value) {
  if (it->ulong > 0) {
    --it->ulong;
    return ava_string_of_char(it->ulong);
  } else {
    return AVA_ABSENT_STRING;
  }
}

static const ava_value_trait xn_type = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "xn",
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = xn_string_chunk_iterator,
  .iterate_string_chunk = xn_iterate_string_chunk,
};

static ava_value xn_of(ava_ulong val) {
  return ava_value_with_ulong(&xn_type, val);
}

deftest(string_of_chunk_iterator_empty) {
  ava_value x0 = xn_of(0);
  ava_string str = ava_to_string(x0);

  ck_assert_int_eq(0, ava_strlen(str));
}

deftest(string_of_chunk_iterator_power_of_two) {
  char bytes[65536];
  ava_value x65536 = xn_of(sizeof(bytes));
  ava_string str = ava_to_string(x65536);
  unsigned i;

  ck_assert_int_eq(sizeof(bytes), ava_strlen(str));

  ava_string_to_bytes(bytes, str, 0, sizeof(bytes));
  for (i = 0; i < sizeof(bytes); ++i)
    ck_assert_int_eq((sizeof(bytes) - i - 1) & 0xFF, bytes[i] & 0xFF);
}

deftest(string_of_chunk_iterator_power_of_two_minus_one) {
  char bytes[65535];
  ava_value x65535 = xn_of(sizeof(bytes));
  ava_string str = ava_to_string(x65535);
  unsigned i;

  ck_assert_int_eq(sizeof(bytes), ava_strlen(str));

  ava_string_to_bytes(bytes, str, 0, sizeof(bytes));
  for (i = 0; i < sizeof(bytes); ++i)
    ck_assert_int_eq((sizeof(bytes) - i - 1) & 0xFF, bytes[i] & 0xFF);
}

deftest(string_of_chunk_iterator_two) {
  ava_value x2 = xn_of(2);
  ava_string str = ava_to_string(x2);

  ck_assert_int_eq(2, ava_strlen(str));
  ck_assert_int_eq(1, ava_string_index(str, 0));
  ck_assert_int_eq(0, ava_string_index(str, 1));
}

deftest(singleton_chunk_iterator) {
  AVA_STATIC_STRING(str, "avalanches");
  ava_string accum = AVA_EMPTY_STRING, chunk;
  ava_value val = ava_value_of_string(str);
  ava_datum iterator;

  iterator = ava_string_chunk_iterator(val);
  while (ava_string_is_present(
           (chunk = ava_iterate_string_chunk(&iterator, val))))
    accum = ava_strcat(accum, chunk);

  ck_assert_str_eq("avalanches", ava_string_to_cstring(accum));
}

deftest(identical_string_values_equal) {
  AVA_STATIC_STRING(sfoo, "foo");
  ck_assert(ava_value_equal(ava_value_of_string(AVA_ASCII9_STRING("foo")),
                            ava_value_of_string(sfoo)));
  ck_assert_int_eq(0, ava_value_strcmp(
                     ava_value_of_string(AVA_ASCII9_STRING("foo")),
                     ava_value_of_string(sfoo)));
}

deftest(values_of_different_type_but_same_string_rep_equal) {
  ck_assert(ava_value_equal(ava_value_of_integer(42),
                            ava_value_of_string(AVA_ASCII9_STRING("42"))));
  ck_assert_int_eq(0, ava_value_strcmp(
                     ava_value_of_integer(42),
                     ava_value_of_string(AVA_ASCII9_STRING("42"))));
}

deftest(nonequal_values_ordered_lexicographically) {
  ck_assert_int_lt(0, ava_value_strcmp(ava_value_of_cstring("foo"),
                                       ava_value_of_cstring("bar")));
  ck_assert_int_gt(0, ava_value_strcmp(ava_value_of_cstring("bar"),
                                       ava_value_of_cstring("foo")));
  ck_assert_int_gt(0, ava_value_strcmp(ava_value_of_cstring("fo"),
                                       ava_value_of_cstring("foo")));
  ck_assert_int_lt(0, ava_value_strcmp(ava_value_of_cstring("foo"),
                                       ava_value_of_cstring("fo")));
}

deftest(string_chars_considered_unsigned) {
  ck_assert_int_gt(0, ava_value_strcmp(ava_value_of_cstring("x"),
                                       ava_value_of_cstring("\xC0")));
}

deftest(strcmp_on_strings_of_different_chunks) {
  char cstr[4] = "\2\1\0";

  ck_assert(ava_value_equal(xn_of(3),
                            ava_value_of_string(ava_string_of_bytes(cstr, 3))));
  ck_assert_int_gt(0, ava_value_strcmp(xn_of(3),
                                       ava_value_of_string(
                                         ava_string_of_bytes(cstr, 4))));
  ck_assert_int_lt(0, ava_value_strcmp(xn_of(3),
                                       ava_value_of_string(
                                         ava_string_of_bytes(cstr, 2))));
}

deftest(equal_on_different_types) {
  ck_assert(ava_value_equal(ava_value_of_cstring("42"),
                            ava_value_of_integer(42)));
}

deftest(hash_basically_works) {
  ava_value a = ava_value_of_cstring("hello world");
  ava_value b = ava_value_of_cstring("hello worle");

  ck_assert_int_eq(ava_value_hash(a), ava_value_hash(a));
  /* This has a 1 in 2**64 chance of failing randomly */
  ck_assert_int_ne(ava_value_hash(a), ava_value_hash(b));
}

deftest(hash_crosses_rope_boundaries_correctly) {
  char buf[246];
  ava_string base = ava_to_string(xn_of(123));
  ava_string rope = ava_strcat(base, base);
  ava_string_to_bytes(buf, rope, 0, 246);
  ava_string flat = ava_string_of_bytes(buf, 246);

  ck_assert_int_eq(ava_value_hash(ava_value_of_string(rope)),
                   ava_value_hash(ava_value_of_string(flat)));
}
