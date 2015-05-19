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

static ava_value empty_list_value;

defsuite(empty_list);

defsetup {
  empty_list_value = ava_list_value_to_value(ava_empty_list);
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
  ava_value result = ava_list_delete(empty_list_value, 0, 0);

  ck_assert_int_eq(0, memcmp(&result, &empty_list_value, sizeof(result)));
}

deftest_signal(empty_list_refuses_nonzero_delete, SIGABRT) {
  ava_list_delete(empty_list_value, 1, 1);
}

deftest_signal(empty_list_refuses_set, SIGABRT) {
  ava_list_set(empty_list_value, 0, empty_list_value);
}
