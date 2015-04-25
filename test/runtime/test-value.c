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

#include "runtime/avalanche.h"

defsuite(value);

/* For testing, our type represents a string with some integer number of
 * characters, starting from NUL at the very end of the string, and
 * incrementing for each preceding bytes. It stores this count in the ulong
 * field of r1, leaving r2 untouched.
 *
 * Chunk iterators store the number of characters left; each chunk contains one
 * character.
 */
static ava_datum xn_string_chunk_iterator(ava_value value) {
  return value.r1;
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

static const ava_value_type xn_type = {
  .size = sizeof(ava_value_type),
  .name = "xn",
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = xn_string_chunk_iterator,
  .iterate_string_chunk = xn_iterate_string_chunk,
  .query_accelerator = ava_noop_query_accelerator
};

static ava_value xn_of(unsigned long long val) {
  return (ava_value) {
    .r1 = { .ulong = val },
    .r2 = { .ulong = 0 },
    .type = &xn_type
  };
}

deftest(string_of_chunk_iterator_empty) {
  ava_value x0 = xn_of(0);
  ava_string str = ava_to_string(x0);

  ck_assert_int_eq(0, ava_string_length(str));
}

deftest(string_of_chunk_iterator_power_of_two) {
  char bytes[65536];
  ava_value x65536 = xn_of(sizeof(bytes));
  ava_string str = ava_to_string(x65536);
  unsigned i;

  ck_assert_int_eq(sizeof(bytes), ava_string_length(str));

  ava_string_to_bytes(bytes, str, 0, sizeof(bytes));
  for (i = 0; i < sizeof(bytes); ++i)
    ck_assert_int_eq((sizeof(bytes) - i - 1) & 0xFF, bytes[i] & 0xFF);
}

deftest(string_of_chunk_iterator_power_of_two_minus_one) {
  char bytes[65535];
  ava_value x65535 = xn_of(sizeof(bytes));
  ava_string str = ava_to_string(x65535);
  unsigned i;

  ck_assert_int_eq(sizeof(bytes), ava_string_length(str));

  ava_string_to_bytes(bytes, str, 0, sizeof(bytes));
  for (i = 0; i < sizeof(bytes); ++i)
    ck_assert_int_eq((sizeof(bytes) - i - 1) & 0xFF, bytes[i] & 0xFF);
}

deftest(string_of_chunk_iterator_two) {
  ava_value x2 = xn_of(2);
  ava_string str = ava_to_string(x2);

  ck_assert_int_eq(2, ava_string_length(str));
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
    accum = ava_string_concat(accum, chunk);

  ck_assert_str_eq("avalanches", ava_string_to_cstring(accum));
}

AVA_DECLARE_ACCELERATOR(foo_accelerator);
AVA_DEFINE_ACCELERATOR(foo_accelerator);

deftest(noop_query_accelerator) {
  ava_value val = ava_value_of_string(AVA_EMPTY_STRING);

  ck_assert_ptr_eq(&val, ava_query_accelerator(val, &foo_accelerator, &val));
  ck_assert_ptr_eq(NULL, ava_query_accelerator(val, &foo_accelerator, NULL));
}

deftest(string_imbue_of_string_is_noop) {
  ava_value orig, new;

  orig = ava_value_of_string(AVA_EMPTY_STRING);
  /* Put something in r2 to make sure it gets preserved. */
  orig.r2.ptr = &orig;

  new = ava_string_imbue(orig);
  ck_assert_int_eq(orig.r1.str.ascii9, new.r1.str.ascii9);
  ck_assert_ptr_eq(&orig, new.r2.ptr);
  ck_assert_ptr_eq(&ava_string_type, new.type);
}

deftest(string_imbue_stringifies_other_types) {
  ava_value orig, new;

  orig = xn_of(5);

  new = ava_string_imbue(orig);
  ck_assert_str_eq("\4\3\2\1", ava_string_to_cstring(new.r1.str));
  ck_assert_ptr_eq(NULL, new.r2.ptr);
  ck_assert_ptr_eq(&ava_string_type, new.type);
}

deftest(weight_of_string_is_its_length) {
  ava_value value = ava_value_of_string(AVA_ASCII9_STRING("avalanche"));

  ck_assert_int_eq(9, ava_value_weight(value));
}
