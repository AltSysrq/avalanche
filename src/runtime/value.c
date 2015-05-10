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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/value.h"

ava_string ava_string_of_chunk_iterator(ava_value value) {
  /* Produce a near-optimal string by concatting the chunks in a perfect binary
   * tree instead of left-to-right.
   *
   * (This is only near-optimal since it assumes no coalescing occurs.)
   */
  ava_string accum[AVA_MAX_ROPE_DEPTH];
  unsigned height[AVA_MAX_ROPE_DEPTH];
  unsigned count = 0;
  ava_string chunk;
  ava_datum iterator;

  iterator = ava_string_chunk_iterator(value);

  while (ava_string_is_present(
           (chunk = ava_iterate_string_chunk(&iterator, value)))) {
    accum[count] = chunk;
    height[count] = 0;
    ++count;

    while (count > 1 && height[count-2] == height[count-1]) {
      accum[count-2] = ava_string_concat(accum[count-2], accum[count-1]);
      ++height[count-2];
      --count;
    }
  }

  if (0 == count)
    return AVA_EMPTY_STRING;

  /* Concat whatever remains right-to-left, since the right-hand-side currently
   * has smaller trees.
   */
  while (count > 1) {
    accum[count-2] = ava_string_concat(accum[count-2], accum[count-1]);
    --count;
  }

  return accum[0];
}

ava_datum ava_singleton_string_chunk_iterator(ava_value value) {
  return (ava_datum) { .str = ava_to_string(value) };
}

ava_string ava_iterate_singleton_string_chunk(ava_datum*restrict it,
                                              ava_value value) {
  ava_string ret = it->str;
  it->str = AVA_ABSENT_STRING;
  return ret;
}

const void* ava_noop_query_accelerator(const ava_accelerator* accel,
                                       const void* dfault) {
  return dfault;
}

static ava_string ava_string_type_to_string(ava_value value) AVA_CONSTFUN;
static ava_string ava_string_type_to_string(ava_value value) {
  return value.r1.str;
}

static size_t ava_string_type_value_weight(ava_value value) {
  return ava_string_length(value.r1.str);
}

const ava_value_type ava_string_type = {
  .size = sizeof(ava_value_type),
  .name = "string",
  .to_string = ava_string_type_to_string,
  .string_chunk_iterator = ava_singleton_string_chunk_iterator,
  .iterate_string_chunk = ava_iterate_singleton_string_chunk,
  .query_accelerator  = ava_noop_query_accelerator,
  .value_weight = ava_string_type_value_weight,
};

/* The string representation will become a bit more interesting once we have
 * caching for large values.
 */
ava_value ava_value_of_string(ava_string str) {
  return (ava_value) {
    .r1 = { .str = str },
    .r2 = { .ptr = NULL },
    .type = &ava_string_type,
  };
}

ava_value ava_string_imbue(ava_value value) {
  if (&ava_string_type == value.type)
    return value;
  else
    return ava_value_of_string(ava_to_string(value));
}
