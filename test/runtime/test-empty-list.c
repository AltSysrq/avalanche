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

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/list.h"

static ava_value empty_list_value;

defsuite(empty_list);

defsetup {
  empty_list_value = ava_empty_list.v->to_value(
    ava_empty_list);
}

defteardown { }

deftest(empty_list_stringifies_to_empty_string) {
  ck_assert_str_eq("", ava_string_to_cstring(
                     ava_to_string(empty_list_value)));
}

deftest(empty_list_string_chunk_iterator_is_empty) {
  ava_datum it;

  it = ava_string_chunk_iterator(empty_list_value);
  ck_assert(!ava_string_is_present(ava_iterate_string_chunk(
                                     &it, empty_list_value)));
}

deftest(empty_list_responds_to_query_list_accelerator) {
  ck_assert_ptr_eq(ava_empty_list.v, ava_query_accelerator(
                     empty_list_value, &ava_list_accelerator, NULL));
}

deftest(empty_list_is_weightless) {
  ck_assert_int_eq(0, ava_value_weight(empty_list_value));
}

deftest(empty_list_has_length_zero) {
  ck_assert_int_eq(0, ava_empty_list.v->length(ava_empty_list));
}

deftest(empty_list_permits_slice_zero_to_zero) {
  ava_list_value result = ava_empty_list.v->slice(ava_empty_list, 0, 0);
  ck_assert_int_eq(0, memcmp(&result, &ava_empty_list, sizeof(result)));
}

deftest_signal(empty_list_refuses_nonzero_slice, SIGABRT) {
  ava_empty_list.v->slice(ava_empty_list, 1, 1);
}

deftest_signal(empty_list_refuses_index, SIGABRT) {
  ava_empty_list.v->index(ava_empty_list, 0);
}

deftest(empty_list_appends_to_singleton_array_list) {
  ava_list_value result = ava_empty_list.v->append(
    ava_empty_list, empty_list_value);
  ck_assert_int_eq(1, result.v->length(result));
}

deftest(empty_list_concats_to_other_list) {
  ava_list_value garbage = {
    .v = (void*)0x100,
    .r1 = { .ulong = 42 },
    .r2 = { .ulong = 56 },
  };
  ava_list_value result = ava_empty_list.v->concat(
    ava_empty_list, garbage);

  ck_assert_int_eq(0, memcmp(&garbage, &result, sizeof(result)));
}

deftest(empty_list_permits_zero_to_zero_delete) {
  ava_list_value result = ava_empty_list.v->delete(
    ava_empty_list, 0, 0);

  ck_assert_int_eq(0, memcmp(&result, &ava_empty_list, sizeof(result)));
}

deftest_signal(empty_list_refuses_nonzero_delete, SIGABRT) {
  ava_empty_list.v->delete(ava_empty_list, 1, 1);
}

deftest(empty_list_iterator_retains_position) {
  AVA_LIST_ITERATOR(ava_empty_list, it);

  ava_empty_list.v->iterator_place(ava_empty_list, it, 5);
  ck_assert_int_eq(5, ava_empty_list.v->iterator_index(ava_empty_list, it));
  ava_empty_list.v->iterator_move(ava_empty_list, it, -2);
  ck_assert_int_eq(3, ava_empty_list.v->iterator_index(ava_empty_list, it));
}

deftest_signal(empty_list_iterator_refuses_get, SIGABRT) {
  AVA_LIST_ITERATOR(ava_empty_list, it);

  ava_empty_list.v->iterator_place(ava_empty_list, it, 0);
  ava_empty_list.v->iterator_get(ava_empty_list, it);
}
