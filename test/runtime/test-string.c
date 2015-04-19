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

deftest(ascii9_string_index) {
  ava_string str = AVA_ASCII9_STRING("123456789");
  unsigned i;

  for (i = 0; i < 9; ++i)
    ck_assert_int_eq(i + '1', ava_string_index(str, i));
}

deftest(flat_string_index) {
  AVA_STATIC_STRING(str, "hello world");
  unsigned i;

  for (i = 0; i < 11; ++i)
    ck_assert_int_eq("hello world"[i], ava_string_index(str, i));
}

deftest(rope_of_flat_index) {
  ava_string str = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 128),
    ava_string_of_shared_bytes(large_string + 128, 128));
  unsigned i;

  for (i = 0; i < 256; ++i)
    ck_assert_int_eq(large_string[i], ava_string_index(str, i));
}

deftest(rope_of_flat_and_ascii9_index) {
  ava_string left = ava_string_of_shared_bytes(large_string, 128);
  ava_string right = ava_string_of_shared_bytes(large_string + 128, 5);
  ava_string str = ava_string_concat(left, right);
  unsigned i;

  ck_assert(right.ascii9 & 1);

  for (i = 0; i < 128 + 5; ++i)
    ck_assert_int_eq(large_string[i], ava_string_index(str, i));
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

deftest(ascii9_string_to_cstring_tmpbuff_null) {
  char* buf = NULL, * ret;
  size_t sz;

  ret = ava_string_to_cstring_tmpbuff(&buf, &sz, AVA_ASCII9_STRING("hello"));
  ck_assert_str_eq("hello", ret);
  ck_assert_ptr_eq(buf, ret);
  ck_assert_int_lt(5, sz);
}

deftest(ascii9_string_to_cstring_tmpbuff_fit) {
  char init_buf[16];
  char* buf = init_buf, * ret;
  size_t sz = sizeof(init_buf);

  ret = ava_string_to_cstring_tmpbuff(&buf, &sz, AVA_ASCII9_STRING("hello"));
  ck_assert_str_eq("hello", ret);
  ck_assert_ptr_eq(init_buf, buf);
  ck_assert_ptr_eq(init_buf, ret);
  ck_assert_int_eq(sizeof(init_buf), sz);
}

deftest(ascii9_string_to_cstring_tmpbuff_overflow) {
  char init_buf[4];
  char* buf = init_buf, * ret;
  size_t sz = sizeof(init_buf);

  ret = ava_string_to_cstring_tmpbuff(&buf, &sz, AVA_ASCII9_STRING("hello"));
  ck_assert_str_eq("hello", ret);
  ck_assert_ptr_ne(init_buf, ret);
  ck_assert_ptr_eq(ret, buf);
  ck_assert_int_lt(5, sz);
}

deftest(flat_string_to_cstring) {
  AVA_STATIC_STRING(orig, "hello");

  ck_assert_str_eq("hello", ava_string_to_cstring(orig));
}

deftest(rope_of_flat_and_ascii9_to_cstring) {
  ava_string orig = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 256),
    AVA_ASCII9_STRING("foo"));
  char expected[256+3+1];

  memcpy(expected, large_string, 256);
  memcpy(expected + 256, "foo", sizeof("foo"));

  ck_assert_str_eq(expected, ava_string_to_cstring(orig));
}

deftest(rope_of_flats_to_cstring) {
  ava_string orig = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 256),
    ava_string_of_shared_bytes(large_string + 256, 256));
  char expected[513];

  memcpy(expected, large_string, 512);
  expected[512] = 0;

  ck_assert_str_eq(expected, ava_string_to_cstring(orig));
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

deftest(rope_slice_to_rope_before_boundary) {
  ava_string orig = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 256),
    ava_string_of_shared_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 128, 256);

  assert_matches_large_string(str, 128, 256);
  ck_assert_ptr_eq(NULL, str.rope->concat_right);
}

deftest(rope_slice_to_rope_after_boundary) {
  ava_string orig = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 256),
    ava_string_of_shared_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 256, 384);

  assert_matches_large_string(str, 256, 384);
  ck_assert_ptr_eq(NULL, str.rope->concat_right);
}

deftest(rope_slice_to_rope_whole) {
  ava_string orig = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 256),
    ava_string_of_shared_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 0, 512);

  ck_assert_ptr_eq(orig.rope, str.rope);
}

deftest(rope_slice_to_flat_ascii9_pair) {
  ava_string left_v = ava_string_of_shared_bytes(large_string, 256);
  ava_string mid_9 = ava_string_of_shared_bytes(large_string + 256, 5);
  ava_string right_v = ava_string_of_shared_bytes(large_string + 256 + 5, 256);
  ava_string orig = ava_string_concat(
    left_v, ava_string_concat(mid_9, right_v));
  ava_string str = ava_string_slice(orig, 255, 265);

  assert_matches_large_string(str, 255, 265);
}

deftest(rope_slice_across_ascii9) {
  ava_string left_9 = ava_string_of_shared_bytes(large_string, 5);
  ava_string right_v = ava_string_of_shared_bytes(large_string + 5, 256);
  ava_string orig = ava_string_concat(left_9, right_v);
  ava_string str = ava_string_slice(orig, 1, 255);

  assert_matches_large_string(str, 1, 255);
}

deftest(ascii9_to_bytes_whole) {
  char buf[9];
  ava_string str = AVA_ASCII9_STRING("avalanche");

  ava_string_to_bytes(buf, str, 0, sizeof(buf));
  ck_assert_int_eq(0, memcmp("avalanche", buf, sizeof(buf)));
}

deftest(ascii9_to_bytes_slice) {
  char buf[3];
  ava_string str = AVA_ASCII9_STRING("avalanche");

  ava_string_to_bytes(buf, str, 1, 1 + sizeof(buf));
  ck_assert_int_eq(0, memcmp("val", buf, sizeof(buf)));
}

deftest(flat_to_bytes_whole) {
  AVA_STATIC_STRING(str, "avalanche\303\237");
  char buf[11];

  ava_string_to_bytes(buf, str, 0, sizeof(buf));
  ck_assert_int_eq(0, memcmp("avalanche\303\237", buf, sizeof(buf)));
}

deftest(flat_to_bytes_slice) {
  AVA_STATIC_STRING(str, "avalanche\303\237");
  char buf[4];

  ava_string_to_bytes(buf, str, 1, 1 + sizeof(buf));
  ck_assert_int_eq(0, memcmp("vala", buf, sizeof(buf)));
}

deftest(rope_to_bytes_whole) {
  char buf[512];
  ava_string str = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 256),
    ava_string_of_shared_bytes(large_string + 256, 256));

  ava_string_to_bytes(buf, str, 0, 512);
  ck_assert_int_eq(0, memcmp(large_string, buf, sizeof(buf)));
}

deftest(rope_to_bytes_slice_before_boundary) {
  char buf[128];
  ava_string str = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 128),
    ava_string_of_shared_bytes(large_string + 128, 256));

  ava_string_to_bytes(buf, str, 0, 128);
  ck_assert_int_eq(0, memcmp(large_string, buf, sizeof(buf)));
}

deftest(rope_to_bytes_slice_after_boundary) {
  char buf[256];
  ava_string str = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 64),
    ava_string_of_shared_bytes(large_string + 64, 256));

  ava_string_to_bytes(buf, str, 64, 64 + 256);
  ck_assert_int_eq(0, memcmp(large_string + 64, buf, sizeof(buf)));
}

deftest(rope_to_bytes_slice_across_boundary) {
  char buf[128];
  ava_string str = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 128),
    ava_string_of_shared_bytes(large_string + 128, 256));

  ava_string_to_bytes(buf, str, 128, 256);
  ck_assert_int_eq(0, memcmp(large_string + 128, buf, sizeof(buf)));
}

deftest(rope_rebalance) {
  /* Concat a bunch of poorly-balanced strings to force a rebalance. */
  ava_string str = AVA_EMPTY_STRING;
  unsigned i, j;

  for (i = 0; i < 100; ++i) {
    ava_string sub = AVA_EMPTY_STRING;
    for (j = 0; j < 7; ++j) {
      sub = ava_string_concat(
        sub, ava_string_of_shared_bytes(
          large_string + i*70+j*10, 10));
    }

    str = ava_string_concat(str, sub);
  }

  /* XXX The rebalance path is really hard to trigger with concats */
  str = ava_string_optimise(str);

  assert_matches_large_string(str, 0, 7000);
}

deftest(ascii9_iterator_place_and_get) {
  ava_string str = AVA_ASCII9_STRING("avalanche");
  ava_string_iterator it;

  ava_string_iterator_place(&it, str, 3);
  ck_assert_int_eq('l', ava_string_iterator_get(&it));
}

deftest(ascii9_iterator_move_and_get) {
  ava_string str = AVA_ASCII9_STRING("avalanche");
  ava_string_iterator it;

  ava_string_iterator_place(&it, str, 3);
  ck_assert(ava_string_iterator_move(&it, +3));
  ck_assert_int_eq('c', ava_string_iterator_get(&it));
  ck_assert(ava_string_iterator_move(&it, -3));
  ck_assert_int_eq('l', ava_string_iterator_get(&it));
}

deftest(ascii9_iterator_bounds_handling) {
  ava_string str = AVA_ASCII9_STRING("avalanche");
  ava_string_iterator it;

  ava_string_iterator_place(&it, str, 3);
  ck_assert_int_eq(3, ava_string_iterator_index(&it));
  ck_assert(ava_string_iterator_valid(&it));
  ck_assert(!ava_string_iterator_move(&it, -10));
  ck_assert(!ava_string_iterator_valid(&it));
  ck_assert_int_eq(0, ava_string_iterator_index(&it));
  ck_assert(!ava_string_iterator_move(&it, +5));
  ck_assert(ava_string_iterator_move(&it, +5));
  ck_assert(ava_string_iterator_valid(&it));
  ck_assert_int_eq('l', ava_string_iterator_get(&it));
  ck_assert(!ava_string_iterator_move(&it, +10));
  ck_assert(!ava_string_iterator_valid(&it));
  ck_assert_int_eq(9, ava_string_iterator_index(&it));
  ck_assert(!ava_string_iterator_move(&it, -1));
  ck_assert(!ava_string_iterator_valid(&it));
  ck_assert(ava_string_iterator_move(&it, -9));
  ck_assert(ava_string_iterator_valid(&it));
  ck_assert_int_eq('l', ava_string_iterator_get(&it));
}

deftest(ascii9_iterator_read_underflow) {
  char buf[10];
  ava_string str = AVA_ASCII9_STRING("avalanche");
  ava_string_iterator it;

  ava_string_iterator_place(&it, str, 3);
  ck_assert_int_eq(6, ava_string_iterator_read(buf, sizeof(buf), &it));
  ck_assert_int_eq(0, memcmp(buf, "lanche", 6));
  ck_assert(!ava_string_iterator_valid(&it));
  ck_assert_int_eq(9, ava_string_iterator_index(&it));

  ck_assert_int_eq(0, ava_string_iterator_read(buf, sizeof(buf), &it));
  ck_assert(!ava_string_iterator_valid(&it));
  ck_assert_int_eq(9, ava_string_iterator_index(&it));
}

deftest(ascii9_iterator_read_overflow) {
  char buf[3];
  ava_string str = AVA_ASCII9_STRING("avalanche");
  ava_string_iterator it;

  ava_string_iterator_place(&it, str, 4);
  ck_assert_int_eq(3, ava_string_iterator_read(buf, sizeof(buf), &it));
  ck_assert_int_eq(0, memcmp(buf, "anc", 3));
  ck_assert(ava_string_iterator_valid(&it));
  ck_assert_int_eq(7, ava_string_iterator_index(&it));

  ck_assert_int_eq(2, ava_string_iterator_read(buf, sizeof(buf), &it));
  ck_assert_int_eq(0, memcmp(buf, "he", 2));
  ck_assert(!ava_string_iterator_valid(&it));
  ck_assert_int_eq(9, ava_string_iterator_index(&it));
}

deftest(rope_iterator_movement) {
  ava_string str = AVA_EMPTY_STRING;
  ava_string_iterator it;
  ssize_t max = 0, ix, nix;
  unsigned i, n;

  for (i = 0; i < 100; ++i) {
    n = 5 + rand() % 100;
    str = ava_string_concat(str, ava_string_of_shared_bytes(
                              large_string + max, n));
    max += n;
  }

  ix = rand() % max;
  ava_string_iterator_place(&it, str, ix);
  ck_assert(ava_string_iterator_valid(&it));
  ck_assert_int_eq(large_string[ix], ava_string_iterator_get(&it));

  for (i = 0; i < 100; ++i) {
    nix = rand() % max;
    ck_assert(ava_string_iterator_move(&it, nix - ix));
    ix = nix;

    ck_assert(ava_string_iterator_valid(&it));
    ck_assert_int_eq(ix, ava_string_iterator_index(&it));
    ck_assert_int_eq(large_string[ix], ava_string_iterator_get(&it));
  }
}

deftest(rope_iterator_get_read_ascii9) {
  ava_string str = ava_string_concat(
    ava_string_of_shared_bytes(large_string, 5),
    ava_string_of_shared_bytes(large_string + 5, 128));
  ava_string_iterator it;
  char buf[3];

  ava_string_iterator_place(&it, str, 1);
  ck_assert_int_eq(large_string[1], ava_string_iterator_get(&it));
  ck_assert_int_eq(sizeof(buf), ava_string_iterator_read(
                     buf, sizeof(buf), &it));
  ck_assert_int_eq(0, memcmp(large_string + 1, buf, sizeof(buf)));
}

deftest(rope_iterator_get_read_flat) {
  AVA_STATIC_STRING(str, "foobar\303\237");
  ava_string_iterator it;
  char buf[3];

  ava_string_iterator_place(&it, str, 1);
  ck_assert_int_eq('o', ava_string_iterator_get(&it));
  ck_assert_int_eq(sizeof(buf), ava_string_iterator_read(
                     buf, sizeof(buf), &it));
  ck_assert_int_eq(0, memcmp("oob", buf, sizeof(buf)));
}
