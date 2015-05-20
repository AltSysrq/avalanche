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

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/list.h"

defsuite(empty_list);

deftest(empty_list_stringifies_to_empty_string) {
  ck_assert_str_eq("", ava_string_to_cstring(
                     ava_to_string(ava_empty_list)));
}

deftest(empty_list_string_chunk_iterator_is_empty) {
  ava_datum it;

  it = ava_string_chunk_iterator(ava_empty_list);
  ck_assert(!ava_string_is_present(ava_iterate_string_chunk(
                                     &it, ava_empty_list)));
}

deftest(empty_list_is_weightless) {
  ck_assert_int_eq(0, ava_value_weight(ava_empty_list));
}

deftest(empty_list_has_length_zero) {
  ck_assert_int_eq(0, ava_list_length(ava_empty_list));
}

deftest(empty_list_permits_slice_zero_to_zero) {
  ava_value result = ava_list_slice(ava_empty_list, 0, 0);
  ck_assert_int_eq(0, memcmp(&result, &ava_empty_list, sizeof(result)));
}

deftest_signal(empty_list_refuses_nonzero_slice, SIGABRT) {
  ava_list_slice(ava_empty_list, 1, 1);
}

deftest_signal(empty_list_refuses_index, SIGABRT) {
  ava_list_index(ava_empty_list, 0);
}

deftest(empty_list_appends_to_singleton_array_list) {
  ava_value result = ava_list_append(ava_empty_list, ava_empty_list);
  ck_assert_int_eq(1, ava_list_length(result));
}

deftest(empty_list_concats_to_other_list) {
  ava_value garbage = {
    .attr = (void*)0x100,
    .r1 = { .ulong = 42 },
  };
  ava_value result = ava_list_concat(ava_empty_list, garbage);

  ck_assert_int_eq(0, memcmp(&garbage, &result, sizeof(result)));
}

deftest(empty_list_permits_zero_to_zero_delete) {
  ava_value result = ava_list_delete(ava_empty_list, 0, 0);

  ck_assert_int_eq(0, memcmp(&result, &ava_empty_list, sizeof(result)));
}

deftest_signal(empty_list_refuses_nonzero_delete, SIGABRT) {
  ava_list_delete(ava_empty_list, 1, 1);
}

deftest_signal(empty_list_refuses_set, SIGABRT) {
  ava_list_set(ava_empty_list, 0, ava_empty_list);
}
