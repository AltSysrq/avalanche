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

#include "runtime/avalanche.h"

static char large_string[65536];

defsuite(string);

defsetup {
  unsigned i;

  srand(0);
  for (i = 0; i < sizeof(large_string); ++i)
    large_string[i] = rand() % 64 + ' ';
}

defteardown {
}

static void assert_matches_large_string(
  ava_string str, unsigned begin, unsigned end
) {
  char dat[end - begin];

  ck_assert_int_eq(end - begin, ava_string_length(str));
  ava_string_to_bytes(dat, str, 0, end - begin);
  ck_assert_int_eq(0, memcmp(dat, large_string + begin, end - begin));
}

deftest(length_of_empty_string_is_zero) {
  ck_assert_int_eq(0, ava_string_length(AVA_EMPTY_STRING));
}

deftest(length_of_ascii9_hello_is_5) {
  ck_assert_int_eq(5, ava_string_length(AVA_ASCII9_STRING("hello")));
}

deftest(length_of_ascii9_avalanche_is_9) {
  ck_assert_int_eq(9, ava_string_length(AVA_ASCII9_STRING("avalanche")));
}

deftest(length_of_flat_helloworld_is_10) {
  AVA_STATIC_STRING(hello_world, "helloworld");
  ck_assert_int_eq(10, ava_string_length(hello_world));
}

deftest(string_of_hello_produces_ascii9_string) {
  ava_string str = ava_string_of_shared_cstring("hello");
  ck_assert(str.ascii9 & 1);
  ck_assert_int_eq(5, ava_string_length(str));
}

deftest(indexing_works_for_ascii9_string) {
  ava_string str = AVA_ASCII9_STRING("123456789");
  unsigned i;

  for (i = 0; i < 9; ++i)
    ck_assert_int_eq(i + '1', ava_string_index(str, i));
}

deftest(string_of_hello_world_produces_heap_string) {
  ava_string str = ava_string_of_shared_cstring("hello world");
  ck_assert(!(str.ascii9 & 1));
  ck_assert_int_eq(11, ava_string_length(str));
}

deftest(string_of_cstring_produces_nonshared_string) {
  char dat[] = "fooooooooooooooooooooooooooooooo";
  ava_string str = ava_string_of_cstring(dat);

  dat[0] = 'g';
  ck_assert_int_eq('f', ava_string_index(str, 0));
}

deftest(string_of_bytes_accepts_nuls) {
  const char dat[] = "hello\0world";
  ava_string str = ava_string_of_shared_bytes(dat, sizeof(dat));
  unsigned i;

  ck_assert_int_eq(sizeof(dat), ava_string_length(str));

  for (i = 0; i < sizeof(dat); ++i)
    ck_assert_int_eq(dat[i], ava_string_index(str, i));
}

deftest(string_of_bytes_produces_nonshared_string) {
  char dat[] = "hello\0world";
  ava_string str = ava_string_of_bytes(dat, sizeof(dat));

  dat[0] = 0;
  ck_assert_int_eq('h', ava_string_index(str, 0));
}

deftest(string_of_char_produces_single_character_string) {
  ava_string str;

  str = ava_string_of_char(0);
  ck_assert_int_eq(1, ava_string_length(str));
  ck_assert_int_eq(0, ava_string_index(str, 0));

  str = ava_string_of_char('x');
  ck_assert_int_eq(1, ava_string_length(str));
  ck_assert_int_eq('x', ava_string_index(str, 0));

  str = ava_string_of_char(255);
  ck_assert_int_eq(1, ava_string_length(str));
  ck_assert_int_eq(255, ava_string_index(str, 0) & 0xFF);
}

deftest(ascii9_string_to_cstring) {
  ck_assert_str_eq("hello", ava_string_to_cstring(AVA_ASCII9_STRING("hello")));
}

deftest(ascci9_string_to_cstring_buff_fit) {
  char buf[6];
  char* ret;

  ret = ava_string_to_cstring_buff(buf, sizeof(buf),
                                   AVA_ASCII9_STRING("hello"));
  ck_assert_ptr_eq(buf, ret);
  ck_assert_str_eq("hello", buf);
}

deftest(ascii9_string_to_cstring_buff_overflow) {
  char buf[5];
  char* ret;

  ret = ava_string_to_cstring_buff(buf, sizeof(buf),
                                   AVA_ASCII9_STRING("hello"));

  ck_assert_ptr_ne(buf, ret);
  ck_assert_str_eq("hello", ret);
}

deftest(ascii9_ascii9_to_ascii9_concat) {
  ava_string str = ava_string_concat(
    AVA_ASCII9_STRING("foo"),
    AVA_ASCII9_STRING("bar"));

  ck_assert(str.ascii9 & 1);
  ck_assert_int_eq(6, ava_string_length(str));
  ck_assert_str_eq("foobar", ava_string_to_cstring(str));
}

deftest(empty_ascii9_to_ascii9_concat) {
  ava_string str = ava_string_concat(
    AVA_EMPTY_STRING,
    AVA_ASCII9_STRING("foo"));

  ck_assert(str.ascii9 & 1);
  ck_assert_int_eq(3, ava_string_length(str));
  ck_assert_str_eq("foo", ava_string_to_cstring(str));
}

deftest(ascii9_empty_to_ascii9_concat) {
  ava_string str = ava_string_concat(
    AVA_ASCII9_STRING("foo"),
    AVA_EMPTY_STRING);

  ck_assert(str.ascii9 & 1);
  ck_assert_int_eq(3, ava_string_length(str));
  ck_assert_str_eq("foo", ava_string_to_cstring(str));
}

deftest(ascii9_ascii9_to_flat_concat) {
  ava_string str = ava_string_concat(
    AVA_ASCII9_STRING("avalanche"),
    AVA_ASCII9_STRING("foobar"));

  ck_assert_int_eq(15, ava_string_length(str));
  ck_assert_str_eq("avalanchefoobar", ava_string_to_cstring(str));
}

deftest(ascii9_flat_to_flat_concat) {
  AVA_STATIC_STRING(flat, "\303\237");
  ava_string str = ava_string_concat(
    AVA_ASCII9_STRING("foo"), flat);

  ck_assert_int_eq(5, ava_string_length(str));
  ck_assert_str_eq("foo\303\237", ava_string_to_cstring(str));
}

deftest(flat_ascii9_to_flat_concat) {
  AVA_STATIC_STRING(flat, "\303\237");
  ava_string str = ava_string_concat(
    flat, AVA_ASCII9_STRING("foo"));

  ck_assert_int_eq(5, ava_string_length(str));
  ck_assert_str_eq("\303\237foo", ava_string_to_cstring(str));
}

deftest(flat_flat_to_flat_concat) {
  AVA_STATIC_STRING(flat, "\303\237");
  ava_string str = ava_string_concat(flat, flat);

  ck_assert_int_eq(4, ava_string_length(str));
  ck_assert_str_eq("\303\237\303\237", ava_string_to_cstring(str));
}

deftest(flat_flat_to_rope_concat) {
  ava_string str = ava_string_concat(
    ava_string_of_shared_bytes(large_string + 0, 256),
    ava_string_of_shared_bytes(large_string + 256, 256));

  assert_matches_large_string(str, 0, 512);
}

deftest(left_to_right_rope_build_chars) {
  ava_string str = AVA_EMPTY_STRING;
  unsigned i;

  for (i = 0; i < 1024; ++i)
    str = ava_string_concat(str, ava_string_of_char(large_string[i]));

  assert_matches_large_string(str, 0, 1024);
}

deftest(right_to_left_rope_build_chars) {
  ava_string str = AVA_EMPTY_STRING;
  unsigned i;

  for (i = 0; i < 1024; ++i)
    str = ava_string_concat(ava_string_of_char(large_string[1023-i]), str);

  assert_matches_large_string(str, 0, 1024);
}

deftest(left_to_right_rope_build_slices) {
  ava_string str = AVA_EMPTY_STRING;
  unsigned i;

  for (i = 0; i < sizeof(large_string) / 128; ++i)
    str = ava_string_concat(str, ava_string_of_shared_bytes(
                              large_string + i * 128, 128));

  assert_matches_large_string(str, 0, sizeof(large_string));
}

deftest(right_to_left_rope_build_slices) {
  ava_string str = AVA_EMPTY_STRING;
  unsigned i;

  for (i = 0; i < sizeof(large_string) / 128; ++i)
    str = ava_string_concat(
      ava_string_of_shared_bytes(
        large_string + sizeof(large_string) - (i+1) * 128, 128),
      str);

  assert_matches_large_string(str, 0, sizeof(large_string));
}

deftest(ascii9_slice_middle) {
  ava_string str = ava_string_slice(
    AVA_ASCII9_STRING("foobar"), 1, 4);

  ck_assert_str_eq("oob", ava_string_to_cstring(str));
}

deftest(ascii9_slice_whole) {
  ava_string str = ava_string_slice(
    AVA_ASCII9_STRING("avalanche"), 0, 9);

  ck_assert_str_eq("avalanche", ava_string_to_cstring(str));
}

deftest(ascii9_slice_empty) {
  ava_string str = ava_string_slice(
    AVA_ASCII9_STRING("avalanche"), 2, 2);

  ck_assert_int_eq(1, str.ascii9);
}

deftest(flat_slice_to_ascii9) {
  AVA_STATIC_STRING(orig, "hello world");
  ava_string str = ava_string_slice(orig, 3, 7);

  ck_assert(str.ascii9 & 1);
  ck_assert_str_eq("lo w", ava_string_to_cstring(str));
}

deftest(flat_slice_to_short_flat) {
  AVA_STATIC_STRING(orig, "hello\303\237world");
  ava_string str = ava_string_slice(orig, 3, 8);

  ck_assert_str_eq("lo\303\237w", ava_string_to_cstring(str));
}

deftest(flat_slice_to_long_flat) {
  ava_string orig = ava_string_of_shared_bytes(
    large_string, sizeof(large_string));
  ava_string str = ava_string_slice(orig, 32, 42);

  assert_matches_large_string(str, 32, 42);
}

deftest(flat_slice_to_unshared_long_flat) {
  ava_string orig = ava_string_of_bytes(
    large_string, sizeof(large_string));
  ava_string str = ava_string_slice(orig, 32, 42);

  ck_assert_int_eq(10, str.rope->external_size);
  assert_matches_large_string(str, 32, 42);
}

deftest(flat_slice_to_empty) {
  AVA_STATIC_STRING(orig, "hello world");
  ava_string str = ava_string_slice(orig, 2, 2);

  ck_assert_int_eq(1, str.ascii9);
}

deftest(rope_slice_to_empty) {
  ava_string orig = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 256),
    ava_string_of_shared_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 42, 42);

  ck_assert_int_eq(1, str.ascii9);
}

deftest(rope_slice_to_ascii9) {
  ava_string orig = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 256),
    ava_string_of_shared_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 42, 48);

  ck_assert(str.ascii9 & 1);
  assert_matches_large_string(str, 42, 48);
}

deftest(rope_slice_to_ascii9_across_boundary) {
  ava_string orig = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 256),
    ava_string_of_shared_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 254, 259);

  ck_assert(str.ascii9 & 1);
  assert_matches_large_string(str, 254, 259);
}

deftest(rope_slice_to_flat_across_boundary) {
  ava_string orig = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 256),
    ava_string_of_shared_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 250, 265);

  assert_matches_large_string(str, 250, 265);
}

deftest(rope_slice_to_rope_across_boundary) {
  ava_string orig = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 256),
    ava_string_of_shared_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 128, 300);

  assert_matches_large_string(str, 128, 300);
}
