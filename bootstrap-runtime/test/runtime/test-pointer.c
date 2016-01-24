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
#include <string.h>
#include <stdio.h>

#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/list.h"
#include "runtime/avalanche/exception.h"
#include "runtime/avalanche/pointer.h"

defsuite(pointer);

static void force(const void* ptr) {
  printf("%p\n", ptr);
}

static void do_assert_format_exception(void* v) {
  ava_pointer_value p = ava_pointer_value_of(
    ava_value_of_cstring(v));
  force(ava_pointer_get_const(p, AVA_EMPTY_STRING));
}

static void assert_format_exception(const char* in) {
  ava_exception ex;

  if (ava_catch(&ex, do_assert_format_exception, (void*)in)) {
    ck_assert_ptr_eq(&ava_format_exception, ex.type);
  } else {
    ck_abort_msg("no exception thrown");
  }
}

static ava_pointer_value of_cstring(const char* str) {
  return ava_pointer_value_of(ava_value_of_cstring(str));
}

deftest(void_null_pointer) {
  ava_pointer_value p = of_cstring("*\n0");

  assert_value_equals_str("* null", p.v);
  ck_assert_ptr_eq(&ava_pointer_proto_mut_void,
                   ava_value_attr(p.v));
  ck_assert_ptr_eq(NULL, ava_pointer_get_const(p, AVA_EMPTY_STRING));
  ck_assert_ptr_eq(NULL, ava_pointer_get_mutable(p, AVA_EMPTY_STRING));
  ck_assert_ptr_eq(NULL, ava_pointer_get_const(p, AVA_ASCII9_STRING("foo")));
  ck_assert_ptr_eq(NULL, ava_pointer_get_mutable(p, AVA_ASCII9_STRING("foo")));
}

deftest(const_void_null_pointer) {
  ava_pointer_value p = of_cstring("&\n0");

  assert_value_equals_str("& null", p.v);
  ck_assert_ptr_eq(&ava_pointer_proto_const_void,
                   ava_value_attr(p.v));
  ck_assert_ptr_eq(NULL, ava_pointer_get_const(p, AVA_EMPTY_STRING));
  ck_assert_ptr_eq(NULL, ava_pointer_get_const(p, AVA_ASCII9_STRING("foo")));
}

static void do_mutable_access_to_const_throws(void* ignore) {
  ava_pointer_value p = ava_pointer_of_proto(
    &ava_pointer_proto_const_void, NULL);
  force(ava_pointer_get_mutable(p, AVA_EMPTY_STRING));
}

deftest(mutable_access_to_const_throws) {
  ava_exception ex;

  if (ava_catch(&ex, do_mutable_access_to_const_throws, NULL))
    ck_assert_ptr_eq(&ava_error_exception, ex.type);
  else
    ck_abort_msg("no exception thrown");
}

deftest(correct_use_of_typed_pointer) {
  ava_pointer_value p = of_cstring("FILE* null");

  assert_value_equals_str("FILE* null", p.v);
  ck_assert_ptr_eq(NULL, ava_pointer_get_const(p, AVA_EMPTY_STRING));
  ck_assert_ptr_eq(NULL, ava_pointer_get_mutable(p, AVA_EMPTY_STRING));
  ck_assert_ptr_eq(NULL, ava_pointer_get_const(
                     p, AVA_ASCII9_STRING("FILE")));
  ck_assert_ptr_eq(NULL, ava_pointer_get_mutable(
                     p, AVA_ASCII9_STRING("FILE")));
}

static void do_incompatible_use_of_typed_pointer_throws(void* ignored) {
  ava_pointer_value p = of_cstring("FILE* null");
  assert_value_equals_str("FILE* null", p.v);
  force(ava_pointer_get_mutable(p, AVA_ASCII9_STRING("bar")));
}

deftest(incompatible_use_of_typed_pointer_throws) {
  ava_exception ex;

  if (ava_catch(&ex, do_incompatible_use_of_typed_pointer_throws, NULL))
    ck_assert_ptr_eq(&ava_error_exception, ex.type);
  else
    ck_abort_msg("no exception thrown");
}

deftest(pointer_value_survives_stringification) {
  int pointee;
  ava_pointer_value p = ava_pointer_of_proto(
    &ava_pointer_proto_const_void, &pointee);
  ava_pointer_value result = ava_pointer_value_of(
    ava_value_of_string(ava_to_string(p.v)));

  ck_assert_ptr_eq(&pointee, ava_pointer_get_const(p, AVA_EMPTY_STRING));
  ck_assert_ptr_eq(&pointee, ava_pointer_get_const(result, AVA_EMPTY_STRING));
  ck_assert_ptr_eq(&ava_pointer_proto_const_void,
                   ava_value_attr(result.v));
}

deftest(throws_on_invalid_list_syntax) {
  assert_format_exception("FILE* \\{");
}

deftest(throws_on_empty_first_token) {
  assert_format_exception("\"\" null");
}

deftest(throws_on_invalid_constness) {
  assert_format_exception("FILE^ null");
}

deftest(throws_on_truncated_list) {
  assert_format_exception("FILE*");
}

deftest(throws_on_oversized_list) {
  assert_format_exception("const FILE* null");
}

deftest(throws_on_invalid_integer) {
  assert_format_exception("FILE* foo");
}

deftest(inspect_prototype) {
  ava_pointer_value p = of_cstring("FILE& 42");
  ck_assert(ava_pointer_is_const(p));
  ck_assert_int_eq(0, ava_strcmp(AVA_ASCII9_STRING("FILE"),
                                 ava_pointer_get_tag(p)));
}

deftest(const_cast_to_self) {
  ava_pointer_value p = of_cstring("FILE* 42");
  ava_pointer_value r = ava_pointer_const_cast_to(p, ava_false);

  ck_assert_ptr_eq(ava_value_attr(p.v), ava_value_attr(r.v));
}

deftest(const_cast_to_mut_void) {
  ava_pointer_value p = of_cstring("& null");
  ava_pointer_value r = ava_pointer_const_cast_to(p, ava_false);

  ck_assert_ptr_eq(&ava_pointer_proto_mut_void, ava_value_attr(r.v));
}

deftest(const_cast_to_const_void) {
  ava_pointer_value p = of_cstring("* null");
  ava_pointer_value r = ava_pointer_const_cast_to(p, ava_true);

  ck_assert_ptr_eq(&ava_pointer_proto_const_void, ava_value_attr(r.v));
}

deftest(const_cast_to_other) {
  ava_pointer_value p = of_cstring("FILE& 42");
  ava_pointer_value r = ava_pointer_const_cast_to(p, ava_false);

  ck_assert_ptr_eq(ava_pointer_get_const(p, AVA_EMPTY_STRING),
                   ava_pointer_get_const(r, AVA_EMPTY_STRING));
  ck_assert(!ava_pointer_is_const(r));
  ck_assert_int_eq(0, ava_strcmp(ava_pointer_get_tag(p),
                                 ava_pointer_get_tag(r)));
}

deftest(reinterpret_cast_to_self) {
  ava_pointer_value p = of_cstring("FILE& 42");
  ava_pointer_value r = ava_pointer_reinterpret_cast_to(
    p, AVA_ASCII9_STRING("FILE"));

  assert_values_same(p.v, r.v);
}

deftest(reinterpret_cast_to_const_void) {
  ava_pointer_value p = of_cstring("FILE& 42");
  ava_pointer_value r = ava_pointer_reinterpret_cast_to(
    p, AVA_EMPTY_STRING);

  ck_assert_ptr_eq(ava_pointer_get_const(p, AVA_EMPTY_STRING),
                   ava_pointer_get_const(r, AVA_EMPTY_STRING));
  ck_assert_ptr_eq(&ava_pointer_proto_const_void, ava_value_attr(r.v));
}

deftest(reinterpret_cast_to_mut_void) {
  ava_pointer_value p = of_cstring("FILE* 42");
  ava_pointer_value r = ava_pointer_reinterpret_cast_to(
    p, AVA_EMPTY_STRING);

  ck_assert_ptr_eq(ava_pointer_get_const(p, AVA_EMPTY_STRING),
                   ava_pointer_get_const(r, AVA_EMPTY_STRING));
  ck_assert_ptr_eq(&ava_pointer_proto_mut_void, ava_value_attr(r.v));
}

deftest(reinterpret_cast_to_other) {
  ava_pointer_value p = of_cstring("FILE* 42");
  ava_pointer_value r = ava_pointer_reinterpret_cast_to(
    p, AVA_ASCII9_STRING("bar"));

  ck_assert_ptr_eq(ava_pointer_get_const(p, AVA_EMPTY_STRING),
                   ava_pointer_get_const(r, AVA_EMPTY_STRING));
  ck_assert_int_eq(0, ava_strcmp(AVA_ASCII9_STRING("bar"),
                                 ava_pointer_get_tag(r)));
}

deftest(list_length_is_2) {
  ava_pointer_value p = of_cstring("FILE* 42");

  ck_assert_int_eq(2, ava_list_length(p.v));
}

deftest(zeroth_list_value_is_tag_and_constness) {
  ava_pointer_value p = of_cstring("FILE* 42");

  assert_value_equals_str("FILE*", ava_list_index(p.v, 0));
}

deftest(first_list_value_is_pointer) {
  char fmt[16], buf[64];
  ava_pointer_value p = of_cstring("FILE* 42");

  snprintf(fmt, sizeof(fmt), "x%%0%dllX", (int)(2 * sizeof(void*)));
  snprintf(buf, sizeof(buf), fmt, 42ULL);
  assert_value_equals_str(buf, ava_list_index(p.v, 1));
}

deftest(null_pointer_stringified_to_null) {
  ava_pointer_value p = of_cstring("FILE* 0");

  assert_value_equals_str("null", ava_list_index(p.v, 1));
}

deftest(other_list_operations) {
  ava_value v = of_cstring("FILE* 0").v;

  assert_value_equals_str("FILE*", ava_list_slice(v, 0, 1));
  assert_value_equals_str("FILE* null foo", ava_list_append(
                            v, ava_value_of_cstring("foo")));
  assert_value_equals_str("FILE* null FILE* null", ava_list_concat(v, v));
  assert_value_equals_str("null", ava_list_remove(v, 0, 1));
  assert_value_equals_str("FILE* foo", ava_list_set(
                            v, 1, ava_value_of_cstring("foo")));
}
