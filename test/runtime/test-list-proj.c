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

#include <stdlib.h>
#include <string.h>

#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/integer.h"
#include "runtime/avalanche/list.h"
#include "runtime/avalanche/list-proj.h"

defsuite(list_proj);

static ava_list_value range(unsigned low, unsigned high) {
  unsigned i;
  ava_value accum = ava_list_value_to_value(ava_empty_list);

  for (i = low; i < high; ++i)
    accum = ava_list_append(accum, ava_value_of_integer(i));

  return ava_list_value_of(accum);
}

static void assert_looks_like(const char* expected, ava_value actual) {
  ck_assert_str_eq(expected, ava_string_to_cstring(
                     ava_to_string(actual)));
}

static size_t list_weight(ava_list_value list) {
  return ava_value_weight(ava_list_value_to_value(list));
}

deftest(basic_interleave) {
  ava_list_value input[3] = { range(0, 3), range(3, 6), range(6, 9) };
  ava_value result = ava_list_value_to_value(
    ava_list_proj_interleave(input, 3));

  ck_assert_int_eq(9, ava_list_length(result));
  ck_assert_int_eq(3 * list_weight(input[0]), ava_value_weight(result));
  assert_looks_like("0 3 6 1 4 7 2 5 8", result);
}

deftest(singlular_interleave) {
  ava_list_value input = range(0, 3);
  ava_list_value result = ava_list_proj_interleave(&input, 1);

  ck_assert_int_eq(0, memcmp(&input, &result, sizeof(input)));
}

deftest(basic_demux) {
  ava_list_value input = range(0, 5);
  ava_value result[3] = {
    ava_list_value_to_value(ava_list_proj_demux(input, 0, 3)),
    ava_list_value_to_value(ava_list_proj_demux(input, 1, 3)),
    ava_list_value_to_value(ava_list_proj_demux(input, 2, 3)),
  };

  ck_assert_int_eq(2, ava_list_length(result[0]));
  ck_assert_int_eq(2, ava_list_length(result[1]));
  ck_assert_int_eq(1, ava_list_length(result[2]));
  ck_assert_int_eq(list_weight(input), ava_value_weight(result[0]));
  ck_assert_int_eq(list_weight(input), ava_value_weight(result[1]));
  ck_assert_int_eq(list_weight(input), ava_value_weight(result[2]));
  assert_looks_like("0 3", result[0]);
  assert_looks_like("1 4", result[1]);
  assert_looks_like("2", result[2]);
}

deftest(noop_demux) {
  ava_list_value input = range(0, 5);
  ava_list_value result = ava_list_proj_demux(input, 0, 1);

  ck_assert_int_eq(0, memcmp(&input, &result, sizeof(input)));
}

deftest(empty_demux) {
  ava_value result = ava_list_value_to_value(
    ava_list_proj_demux(ava_empty_list, 0, 2));
  ck_assert_int_eq(0, ava_list_length(result));
}

deftest(interleave_inverts_demux) {
  ava_list_value input = range(0, 6);
  ava_list_value demuxed[3] = {
    ava_list_proj_demux(input, 0, 3),
    ava_list_proj_demux(input, 1, 3),
    ava_list_proj_demux(input, 2, 3),
  };
  ava_list_value result = ava_list_proj_interleave(demuxed, 3);

  ck_assert_int_eq(0, memcmp(&input, &result, sizeof(input)));
}

deftest(interleave_doesnt_invert_misordered_demux) {
  ava_list_value input = range(0, 6);
  ava_list_value demuxed[3] = {
    ava_list_proj_demux(input, 0, 3),
    ava_list_proj_demux(input, 2, 3),
    ava_list_proj_demux(input, 1, 3),
  };
  ava_list_value result = ava_list_proj_interleave(demuxed, 3);

  ck_assert_int_ne(0, memcmp(&input, &result, sizeof(input)));
}

deftest(interleave_doesnt_invert_misstrided_demux) {
  ava_list_value input = range(0, 6);
  ava_list_value demuxed[3] = {
    ava_list_proj_demux(input, 0, 3),
    ava_list_proj_demux(input, 2, 3),
    ava_list_proj_demux(input, 1, 3),
  };
  ava_list_value result = ava_list_proj_interleave(demuxed, 2);

  ck_assert_int_ne(0, memcmp(&input, &result, sizeof(input)));
}

deftest(interleave_doesnt_invert_mismatched_demux) {
  ava_list_value input = range(0, 6);
  ava_list_value other_input = range(10, 16);
  ava_list_value demuxed[3] = {
    ava_list_proj_demux(input, 0, 3),
    ava_list_proj_demux(input, 1, 3),
    ava_list_proj_demux(other_input, 2, 3),
  };
  ava_list_value result = ava_list_proj_interleave(demuxed, 3);

  ck_assert_int_ne(0, memcmp(&input, &result, sizeof(input)));
}

deftest(demux_inverts_interleave) {
  ava_list_value input[3] = { range(0, 3), range(3, 6), range(6, 9) };
  ava_list_value muxed = ava_list_proj_interleave(input, 3);
  ava_list_value result[3] = {
    ava_list_proj_demux(muxed, 0, 3),
    ava_list_proj_demux(muxed, 1, 3),
    ava_list_proj_demux(muxed, 2, 3),
  };

  ck_assert_int_eq(0, memcmp(input, result, sizeof(input)));
}

deftest(demux_doesnt_invert_misstrided_interleave) {
  ava_list_value input[3] = { range(0, 3), range(3, 6), range(6, 9) };
  ava_list_value muxed = ava_list_proj_interleave(input, 3);
  ava_list_value result[3] = {
    ava_list_proj_demux(muxed, 0, 4),
    ava_list_proj_demux(muxed, 1, 4),
    ava_list_proj_demux(muxed, 2, 4),
  };

  ck_assert_int_ne(0, memcmp(input, result, sizeof(input)));
}

deftest(basic_group) {
  ava_list_value input = range(0, 8);
  ava_value result = ava_list_value_to_value(
    ava_list_proj_group(input, 3));

  ck_assert_int_eq(3, ava_list_length(result));
  ck_assert_int_eq(list_weight(input), ava_value_weight(result));
  assert_looks_like("\"0 1 2\" \"3 4 5\" \"6 7\"", result);
}

deftest(group_caches_members) {
  ava_list_value input = range(0, 8);
  ava_value result = ava_list_value_to_value(
    ava_list_proj_group(input, 3));
  ava_value r0 = ava_list_index(result, 0);
  ava_value r1 = ava_list_index(result, 0);

  ck_assert_int_eq(0, memcmp(&r0, &r1, sizeof(r0)));
}

deftest(basic_flatten) {
  ava_value values[3] = {
    ava_value_of_cstring("hello world"),
    ava_value_of_cstring("1 2 3 4"),
    ava_value_of_cstring(""),
  };
  ava_list_value input = ava_list_of_values(values, 3);
  ava_value result = ava_list_value_to_value(
    ava_list_proj_flatten(input));

  ck_assert_int_eq(6, ava_list_length(result));
  assert_looks_like("hello world 1 2 3 4", result);
}

deftest(flatten_inverts_group) {
  ava_list_value input = range(0, 10);
  ava_list_value grouped = ava_list_proj_group(input, 4);
  ava_list_value result = ava_list_proj_flatten(grouped);

  ck_assert_int_eq(0, memcmp(&input, &result, sizeof(input)));
}

deftest(empty_flatten) {
  ava_list_value result = ava_list_proj_flatten(ava_empty_list);

  ck_assert_int_eq(0, memcmp(&ava_empty_list, &result, sizeof(result)));
}
