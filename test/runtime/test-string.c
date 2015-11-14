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

#include "runtime/avalanche/string.h"

static char large_string[65536];

/* Note that a lot of the tests in this file refer in name to the older
 * rope-based string design. A "rope" referred to a concatenation node, and
 * "flat" to a node that contained a simple heap-allocated string. Nodes could
 * also contain ASCII9 strings themselves.
 *
 * These tests are still valid, though they don't necessarily test any
 * execution path in particular.
 */

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
  ava_string str = ava_string_of_cstring("hello");
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
    ava_string_of_bytes(large_string, 128),
    ava_string_of_bytes(large_string + 128, 128));
  unsigned i;

  for (i = 0; i < 256; ++i)
    ck_assert_int_eq(large_string[i], ava_string_index(str, i));
}

deftest(rope_of_flat_and_ascii9_index) {
  ava_string left = ava_string_of_bytes(large_string, 128);
  ava_string right = ava_string_of_bytes(large_string + 128, 5);
  ava_string str = ava_string_concat(left, right);
  unsigned i;

  ck_assert(right.ascii9 & 1);

  for (i = 0; i < 128 + 5; ++i)
    ck_assert_int_eq(large_string[i], ava_string_index(str, i));
}

deftest(string_of_hello_world_produces_heap_string) {
  ava_string str = ava_string_of_cstring("hello world");
  ck_assert(!(str.ascii9 & 1));
  ck_assert_int_eq(11, ava_string_length(str));
}

deftest(string_of_cstring_produces_nonstring) {
  char dat[] = "fooooooooooooooooooooooooooooooo";
  ava_string str = ava_string_of_cstring(dat);

  dat[0] = 'g';
  ck_assert_int_eq('f', ava_string_index(str, 0));
}

deftest(string_of_bytes_accepts_nuls) {
  const char dat[] = "hello\0world";
  ava_string str = ava_string_of_bytes(dat, sizeof(dat));
  unsigned i;

  ck_assert_int_eq(sizeof(dat), ava_string_length(str));

  for (i = 0; i < sizeof(dat); ++i)
    ck_assert_int_eq(dat[i], ava_string_index(str, i));
}

deftest(string_of_bytes_produces_nonstring) {
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
  ava_str_tmpbuff buf;
  const char* ret;

  ret = ava_string_to_cstring_buff(buf, AVA_ASCII9_STRING("hello"));
  ck_assert_ptr_eq(buf, ret);
  ck_assert_str_eq("hello", ret);
}

deftest(flat_string_to_cstring) {
  AVA_STATIC_STRING(orig, "hello");

  ck_assert_str_eq("hello", ava_string_to_cstring(orig));
}

deftest(rope_of_flat_and_ascii9_to_cstring) {
  ava_string orig = ava_string_concat(
    ava_string_of_bytes(large_string, 256),
    AVA_ASCII9_STRING("foo"));
  char expected[256+3+1];

  memcpy(expected, large_string, 256);
  memcpy(expected + 256, "foo", sizeof("foo"));

  ck_assert_str_eq(expected, ava_string_to_cstring(orig));
}

deftest(rope_of_flats_to_cstring) {
  ava_string orig = ava_string_concat(
    ava_string_of_bytes(large_string, 256),
    ava_string_of_bytes(large_string + 256, 256));
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
    ava_string_of_bytes(large_string + 0, 256),
    ava_string_of_bytes(large_string + 256, 256));

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
    str = ava_string_concat(str, ava_string_of_bytes(
                              large_string + i * 128, 128));

  assert_matches_large_string(str, 0, sizeof(large_string));
}

deftest(right_to_left_rope_build_slices) {
  ava_string str = AVA_EMPTY_STRING;
  unsigned i;

  for (i = 0; i < sizeof(large_string) / 128; ++i)
    str = ava_string_concat(
      ava_string_of_bytes(
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
  ava_string orig = ava_string_of_bytes(
    large_string, sizeof(large_string));
  ava_string str = ava_string_slice(orig, 32, 42);

  assert_matches_large_string(str, 32, 42);
}

deftest(flat_slice_to_empty) {
  AVA_STATIC_STRING(orig, "hello world");
  ava_string str = ava_string_slice(orig, 2, 2);

  ck_assert_int_eq(1, str.ascii9);
}

deftest(rope_slice_to_empty) {
  ava_string orig = ava_string_concat(
    ava_string_of_bytes(large_string, 256),
    ava_string_of_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 42, 42);

  ck_assert_int_eq(1, str.ascii9);
}

deftest(rope_slice_to_ascii9) {
  ava_string orig = ava_string_concat(
    ava_string_of_bytes(large_string, 256),
    ava_string_of_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 42, 48);

  ck_assert(str.ascii9 & 1);
  assert_matches_large_string(str, 42, 48);
}

deftest(rope_slice_to_ascii9_across_boundary) {
  ava_string orig = ava_string_concat(
    ava_string_of_bytes(large_string, 256),
    ava_string_of_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 254, 259);

  ck_assert(str.ascii9 & 1);
  assert_matches_large_string(str, 254, 259);
}

deftest(rope_slice_to_flat_across_boundary) {
  ava_string orig = ava_string_concat(
    ava_string_of_bytes(large_string, 256),
    ava_string_of_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 250, 265);

  assert_matches_large_string(str, 250, 265);
}

deftest(rope_slice_to_rope_across_boundary) {
  ava_string orig = ava_string_concat(
    ava_string_of_bytes(large_string, 256),
    ava_string_of_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 128, 300);

  assert_matches_large_string(str, 128, 300);
}

deftest(rope_slice_to_rope_whole) {
  ava_string orig = ava_string_concat(
    ava_string_of_bytes(large_string, 256),
    ava_string_of_bytes(large_string + 256, 256));
  ava_string str = ava_string_slice(orig, 0, 512);

  ck_assert_ptr_eq(orig.twine, str.twine);
}

deftest(rope_slice_to_flat_ascii9_pair) {
  ava_string left_v = ava_string_of_bytes(large_string, 256);
  ava_string mid_9 = ava_string_of_bytes(large_string + 256, 5);
  ava_string right_v = ava_string_of_bytes(large_string + 256 + 5, 256);
  ava_string orig = ava_string_concat(
    left_v, ava_string_concat(mid_9, right_v));
  ava_string str = ava_string_slice(orig, 255, 265);

  assert_matches_large_string(str, 255, 265);
}

deftest(rope_slice_across_ascii9) {
  ava_string left_9 = ava_string_of_bytes(large_string, 5);
  ava_string right_v = ava_string_of_bytes(large_string + 5, 256);
  ava_string orig = ava_string_concat(left_9, right_v);
  ava_string str = ava_string_slice(orig, 1, 255);

  assert_matches_large_string(str, 1, 255);
}

deftest(slice_to_concat_left_only) {
  ava_string in = ava_string_concat(
    ava_string_of_bytes(large_string, 256),
    ava_string_of_bytes(large_string + 256, 256));
  ava_string result = ava_string_slice(in, 32, 64);

  assert_matches_large_string(result, 32, 64);
}

deftest(slice_to_concat_ascii9_right_only) {
  ava_string in = ava_string_concat(
    ava_string_of_bytes(large_string, 512),
    AVA_ASCII9_STRING("avalanche"));
  ava_string result = ava_string_slice(in, 513, 516);

  ck_assert_str_eq("val", ava_string_to_cstring(result));
}

deftest(slice_to_concat_twine_right_only) {
  ava_string in = ava_string_concat(
    ava_string_of_bytes(large_string, 256),
    ava_string_of_bytes(large_string + 256, 256));
  ava_string result = ava_string_slice(in, 300, 400);

  assert_matches_large_string(result, 300, 400);
}

deftest(slice_to_concat_ascii9_right_only_complex) {
  ava_string in = ava_string_concat(
    ava_string_concat(
      ava_string_of_bytes(large_string, 250),
      ava_string_of_bytes(large_string + 250, 6)),
    ava_string_of_bytes(large_string + 256, 256));
  ava_string result = ava_string_slice(in, 252, 300);

  assert_matches_large_string(result, 252, 300);
}

deftest(slice_to_tocnac_left_only_complex) {
  ava_string in = ava_string_concat(
      ava_string_of_bytes(large_string, 250),
    ava_string_concat(
      ava_string_of_bytes(large_string + 250, 6),
      ava_string_of_bytes(large_string + 256, 256)));
  ava_string result = ava_string_slice(in, 200, 252);

  assert_matches_large_string(result, 200, 252);
}

#define TBTEST(name, input, op, off, expect)            \
  deftest(name) {                                       \
    ava_string a = AVA_ASCII9_STRING(input);            \
    ck_assert_str_eq(expect, ava_string_to_cstring(     \
                       ava_string_##op(a, off)));       \
  }
TBTEST(trunc_ascii9_0, "avalanche", trunc, 0, "");
TBTEST(trunc_ascii9_1, "avalanche", trunc, 1, "a");
TBTEST(trunc_ascii9_2, "avalanche", trunc, 2, "av");
TBTEST(trunc_ascii9_3, "avalanche", trunc, 3, "ava");
TBTEST(trunc_ascii9_4, "avalanche", trunc, 4, "aval");
TBTEST(trunc_ascii9_5, "avalanche", trunc, 5, "avala");
TBTEST(trunc_ascii9_6, "avalanche", trunc, 6, "avalan");
TBTEST(trunc_ascii9_7, "avalanche", trunc, 7, "avalanc");
TBTEST(trunc_ascii9_8, "avalanche", trunc, 8, "avalanch");
TBTEST(trunc_ascii9_9, "avalanche", trunc, 9, "avalanche");
TBTEST(behead_ascii9_0,"avalanche", behead,0, "avalanche");
TBTEST(behead_ascii9_1,"avalanche", behead,1,  "valanche");
TBTEST(behead_ascii9_2,"avalanche", behead,2,   "alanche");
TBTEST(behead_ascii9_3,"avalanche", behead,3,    "lanche");
TBTEST(behead_ascii9_4,"avalanche", behead,4,     "anche");
TBTEST(behead_ascii9_5,"avalanche", behead,5,      "nche");
TBTEST(behead_ascii9_6,"avalanche", behead,6,       "che");
TBTEST(behead_ascii9_7,"avalanche", behead,7,        "he");
TBTEST(behead_ascii9_8,"avalanche", behead,8,         "e");
TBTEST(behead_ascii9_9,"avalanche", behead,9,          "");
#undef TBTEST

deftest(trunc_twine) {
  AVA_STATIC_STRING(in, "avalanches");

  ck_assert_str_eq("ava", ava_string_to_cstring(
                     ava_string_trunc(in, 3)));
}

deftest(behead_twine) {
  AVA_STATIC_STRING(in, "avalanches");

  ck_assert_str_eq("lanches", ava_string_to_cstring(
                     ava_string_behead(in, 3)));
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
    ava_string_of_bytes(large_string, 256),
    ava_string_of_bytes(large_string + 256, 256));

  ava_string_to_bytes(buf, str, 0, 512);
  ck_assert_int_eq(0, memcmp(large_string, buf, sizeof(buf)));
}

deftest(rope_to_bytes_slice_before_boundary) {
  char buf[128];
  ava_string str = ava_string_concat(
    ava_string_of_bytes(large_string, 128),
    ava_string_of_bytes(large_string + 128, 256));

  ava_string_to_bytes(buf, str, 0, 128);
  ck_assert_int_eq(0, memcmp(large_string, buf, sizeof(buf)));
}

deftest(rope_to_bytes_slice_after_boundary) {
  char buf[256];
  ava_string str = ava_string_concat(
    ava_string_of_bytes(large_string, 64),
    ava_string_of_bytes(large_string + 64, 256));

  ava_string_to_bytes(buf, str, 64, 64 + 256);
  ck_assert_int_eq(0, memcmp(large_string + 64, buf, sizeof(buf)));
}

deftest(rope_to_bytes_slice_across_boundary) {
  char buf[128];
  ava_string str = ava_string_concat(
    ava_string_of_bytes(large_string, 128),
    ava_string_of_bytes(large_string + 128, 256));

  ava_string_to_bytes(buf, str, 128, 256);
  ck_assert_int_eq(0, memcmp(large_string + 128, buf, sizeof(buf)));
}

deftest(ascii9_hash) {
  /* This has a small chance of failing spuriously */
  ck_assert_int_ne(ava_ascii9_hash(AVA_ASCII9_STRING("foo").ascii9),
                   ava_ascii9_hash(AVA_ASCII9_STRING("bar").ascii9));
}

deftest(ascii9_strcmp_equal) {
  ava_string str = AVA_ASCII9_STRING("foo");

  ck_assert_int_eq(0, ava_strcmp(str, str));
}

deftest(ascii9_strcmp_different) {
  ava_string a = AVA_ASCII9_STRING("foo"), b = AVA_ASCII9_STRING("bar");

  ck_assert_int_lt(0, ava_strcmp(a, b));
  ck_assert_int_gt(0, ava_strcmp(b, a));
}

deftest(ascii9_strcmp_prefix) {
  ava_string a = AVA_ASCII9_STRING("foo"), b = AVA_ASCII9_STRING("food");

  ck_assert_int_gt(0, ava_strcmp(a, b));
  ck_assert_int_lt(0, ava_strcmp(b, a));
}

deftest(twine_strcmp_equal) {
  AVA_STATIC_STRING(str, "avalanches");

  ck_assert_int_eq(0, ava_strcmp(str, str));
}

deftest(twine_strcmp_different) {
  AVA_STATIC_STRING(a, "avalanches");
  AVA_STATIC_STRING(b, "landslides");

  ck_assert_int_gt(0, ava_strcmp(a, b));
  ck_assert_int_lt(0, ava_strcmp(b, a));
}

deftest(twine_strcmp_prex) {
  AVA_STATIC_STRING(a, "avalanches");
  AVA_STATIC_STRING(b, "avalanches'");

  ck_assert_int_gt(0, ava_strcmp(a, b));
  ck_assert_int_lt(0, ava_strcmp(b, a));
}

deftest(mixed_strcmp) {
  AVA_STATIC_STRING(a, "avalanche");
  ava_string b = AVA_ASCII9_STRING("avalanche");

  ck_assert_int_eq(0, ava_strcmp(a, b));
}

deftest(ascii9_equal_true) {
  ck_assert(ava_string_equal(AVA_ASCII9_STRING("avalanche"),
                             AVA_ASCII9_STRING("avalanche")));
}

deftest(ascii9_equal_false) {
  ck_assert(!ava_string_equal(AVA_ASCII9_STRING("avalanche"),
                              AVA_ASCII9_STRING("foo")));
}

deftest(twine_equal_true) {
  AVA_STATIC_STRING(a, "avalanches");

  ck_assert(ava_string_equal(a, a));
}

deftest(twine_equal_false_different_length) {
  AVA_STATIC_STRING(a, "avalanches");
  AVA_STATIC_STRING(b, "antidisestablishmentarianism");

  ck_assert(!ava_string_equal(a, b));
}

deftest(twine_equal_false_same_length) {
  AVA_STATIC_STRING(a, "avalanches");
  AVA_STATIC_STRING(b, "comparison");

  ck_assert(!ava_string_equal(a, b));
}

deftest(twine_equal_false_embedded_nul) {
  static const char ab[] = "con\0temporary";
  static const char bb[] = "con\0tinuities";

  ck_assert(!ava_string_equal(
              ava_string_of_bytes(ab, sizeof(ab)),
              ava_string_of_bytes(bb, sizeof(bb))));
}

deftest(mixed_equal_true) {
  AVA_STATIC_STRING(t, "avalanche");

  ck_assert(ava_string_equal(AVA_ASCII9_STRING("avalanche"), t));
  ck_assert(ava_string_equal(t, AVA_ASCII9_STRING("avalanche")));
}

deftest(mixed_equal_false) {
  AVA_STATIC_STRING(t, "comparison");

  ck_assert(!ava_string_equal(AVA_ASCII9_STRING("avalanche"), t));
  ck_assert(!ava_string_equal(t, AVA_ASCII9_STRING("avalanche")));
}

deftest(empty_string_starts_with_empty_string) {
  ck_assert(ava_string_starts_with(AVA_EMPTY_STRING, AVA_EMPTY_STRING));
}

deftest(ascii9_string_starts_with_empty_string) {
  ck_assert(ava_string_starts_with(
              AVA_ASCII9_STRING("avalanche"), AVA_EMPTY_STRING));
}

deftest(twine_starts_with_empty_string) {
  AVA_STATIC_STRING(a, "avalanches");
  ck_assert(ava_string_starts_with(a, AVA_EMPTY_STRING));
}

deftest(empty_string_doesnt_start_with_nonempty) {
  ck_assert(!ava_string_starts_with(
              AVA_EMPTY_STRING, AVA_ASCII9_STRING("foo")));
}

deftest(ascii9_starts_with_self) {
  ck_assert(ava_string_starts_with(AVA_ASCII9_STRING("avalanche"),
                                   AVA_ASCII9_STRING("avalanche")));
}

deftest(twine_starts_with_self) {
  AVA_STATIC_STRING(a, "avalanches");
  ck_assert(ava_string_starts_with(a, a));
}

deftest(ascii9_cant_start_with_needle_longer_than_9) {
  AVA_STATIC_STRING(a, "avalanches");
  ck_assert(!ava_string_starts_with(AVA_ASCII9_STRING("avalanche"), a));
}

deftest(ascii9_starts_with_ascii9_simple_positive) {
  ck_assert(ava_string_starts_with(
              AVA_ASCII9_STRING("foobar"), AVA_ASCII9_STRING("foo")));
}

deftest(ascii9_starts_with_ascii9_simple_negative_lt) {
  ck_assert(!ava_string_starts_with(
              AVA_ASCII9_STRING("foobar"), AVA_ASCII9_STRING("bar")));
}

deftest(ascii9_starts_with_ascii9_simple_negative_gt) {
  ck_assert(!ava_string_starts_with(
              AVA_ASCII9_STRING("foobar"), AVA_ASCII9_STRING("quux")));
}

deftest(ascii9_starts_with_ascii9_negative_extension) {
  ck_assert(!ava_string_starts_with(
              AVA_ASCII9_STRING("foo"), AVA_ASCII9_STRING("foob")));
}

deftest(ascii9_starts_with_ascii9_positive_overflow) {
  ck_assert(ava_string_starts_with(
              AVA_ASCII9_STRING("\x7f\x7f"), AVA_ASCII9_STRING("\x7f")));
}

deftest(ascii9_starts_with_ascii9_positive_underflow) {
  ck_assert(ava_string_starts_with(
              AVA_ASCII9_STRING("\x01\x01"), AVA_ASCII9_STRING("\x01")));
}

deftest(ascii9_starts_with_twine_positive) {
  AVA_STATIC_STRING(a, "ava");
  ck_assert(ava_string_starts_with(AVA_ASCII9_STRING("avalanche"), a));
}

deftest(ascii9_starts_with_twine_negative) {
  AVA_STATIC_STRING(a, "foo");
  ck_assert(!ava_string_starts_with(AVA_ASCII9_STRING("avalanche"), a));
}

deftest(twine_starts_with_twine_positive) {
  AVA_STATIC_STRING(a, "ava");
  AVA_STATIC_STRING(b, "avalanche");
  ck_assert(ava_string_starts_with(b, a));
}

deftest(twine_starts_with_twine_negative) {
  AVA_STATIC_STRING(a, "foo");
  AVA_STATIC_STRING(b, "avalanche");
  ck_assert(!ava_string_starts_with(b, a));
}

deftest(twine_starts_with_twine_negative_extension) {
  AVA_STATIC_STRING(a, "ava");
  AVA_STATIC_STRING(b, "avalanche");
  ck_assert(!ava_string_starts_with(a, b));
}

deftest(is_empty_ascii9_empty) {
  ck_assert(ava_string_is_empty(AVA_EMPTY_STRING));
}

deftest(is_empty_ascii9_nonempty) {
  ck_assert(!ava_string_is_empty(AVA_ASCII9_STRING("foo")));
}

deftest(is_empty_twine_empty) {
  AVA_STATIC_STRING(str, "");
  ck_assert(ava_string_is_empty(str));
}

deftest(is_empty_twine_nonempty) {
  AVA_STATIC_STRING(str, "avalanches");
  ck_assert(!ava_string_is_empty(str));
}

#define A9_IX_OF(name, ix, a, b)                                \
  deftest(a9_ix_##name) {                                       \
    ava_ascii9_string as = _AVA_ASCII9_ENCODE_STR(a);           \
    ava_ascii9_string bs = _AVA_ASCII9_ENCODE_STR(b);           \
    ck_assert_int_eq(ix, ava_ascii9_index_of_match(as, bs));    \
  }

A9_IX_OF(no_match, -1, "avalanche", "mountains");
A9_IX_OF(exact_match, 0, "avalanche", "avalanche");
A9_IX_OF(match_0_ab, 0, "abbbbbbbb", "aaaaaaaaa");
A9_IX_OF(match_0_ba, 0, "baaaaaaaa", "bbbbbbbbb");
A9_IX_OF(match_1_ab, 1, "babbbbbbb", "aaaaaaaaa");
A9_IX_OF(match_1_ba, 1, "abaaaaaaa", "bbbbbbbbb");
A9_IX_OF(match_2_ab, 2, "bbabbbbbb", "aaaaaaaaa");
A9_IX_OF(match_2_ba, 2, "aabaaaaaa", "bbbbbbbbb");
A9_IX_OF(match_3_ab, 3, "bbbabbbbb", "aaaaaaaaa");
A9_IX_OF(match_3_ba, 3, "aaabaaaaa", "bbbbbbbbb");
A9_IX_OF(match_4_ab, 4, "bbbbabbbb", "aaaaaaaaa");
A9_IX_OF(match_4_ba, 4, "aaaabaaaa", "bbbbbbbbb");
A9_IX_OF(match_5_ab, 5, "bbbbbabbb", "aaaaaaaaa");
A9_IX_OF(match_5_ba, 5, "aaaaabaaa", "bbbbbbbbb");
A9_IX_OF(match_6_ab, 6, "bbbbbbabb", "aaaaaaaaa");
A9_IX_OF(match_6_ba, 6, "aaaaaabaa", "bbbbbbbbb");
A9_IX_OF(match_7_ab, 7, "bbbbbbbab", "aaaaaaaaa");
A9_IX_OF(match_7_ba, 7, "aaaaaaaba", "bbbbbbbbb");
A9_IX_OF(match_8_ab, 8, "bbbbbbbba", "aaaaaaaaa");
A9_IX_OF(match_8_ba, 8, "aaaaaaaab", "bbbbbbbbb");
A9_IX_OF(match_eos, 3, "foo", "bar");

A9_IX_OF(match_0203_03, 1, "\x02\x03", "\x03\x03\x03\x03\x03\x03\x03\x03\x03");

#undef A9_IX_OF

deftest(a9_ix_brute_force) {
  unsigned char a, b;
  ava_string str;

  for (a = 1; a <= 127; ++a) {
    for (b = 1; b <= 127; ++b) {
      str.ascii9 = AVA_ASCII9(a, b);
      ck_assert_msg(0 == ava_strchr(str, a),
                    "ava_strchr(%02X%02X, %02X) = %d, expected 0",
                    (unsigned)a, (unsigned)b, (unsigned)a,
                    ava_strchr(str, a));
      if (b != a)
        ck_assert_msg(1 == ava_strchr(str, b),
                      "ava_strchr(%02X%02X, %02X) = %d, expected 1",
                      (unsigned)a, (unsigned)b, (unsigned)b,
                      ava_strchr(str, b));
    }
  }
}

deftest(strchr_ascii_ascii9_hit) {
  ava_string str = AVA_ASCII9_STRING("avalanche");
  ck_assert_int_eq(1, ava_strchr_ascii(str, 'v'));
}

deftest(strchr_ascii_ascii9_miss) {
  ava_string str = AVA_ASCII9_STRING("avalanche");
  ck_assert_int_eq(-1, ava_strchr_ascii(str, 'x'));
}

deftest(strchr_ascii_twine_hit) {
  AVA_STATIC_STRING(str, "avalanches");
  ck_assert_int_eq(9, ava_strchr_ascii(str, 's'));
}

deftest(strchr_ascii_twine_miss) {
  AVA_STATIC_STRING(str, "avalanches");
  ck_assert_int_eq(-1, ava_strchr_ascii(str, 'x'));
}

deftest(strchr_general_ascii9_hit) {
  ava_string str = AVA_ASCII9_STRING("avalanche");
  ck_assert_int_eq(1, ava_strchr(str, 'v'));
}

deftest(strchr_general_ascii9_miss) {
  ava_string str = AVA_ASCII9_STRING("avalanche");
  ck_assert_int_eq(-1, ava_strchr(str, 'x'));
}

deftest(strchr_general_ascii9_nul) {
  ava_string str = AVA_ASCII9_STRING("foo");
  ck_assert_int_eq(-1, ava_strchr(str, 0));
}

deftest(strchr_general_ascii9_nonascii) {
  ava_string str = AVA_ASCII9_STRING("eoo");
  ck_assert_int_eq(-1, ava_strchr(str, 128 | 'o'));
}

deftest(strchr_general_twine_hit) {
  AVA_STATIC_STRING(str, "avalanches");
  ck_assert_int_eq(9, ava_strchr(str, 's'));
}

deftest(strchr_general_twine_miss) {
  AVA_STATIC_STRING(str, "avalanches");
  ck_assert_int_eq(-1, ava_strchr(str, 'x'));
}
