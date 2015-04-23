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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "avalanche.h"

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

const ava_value_type ava_string_type = {
  .size = sizeof(ava_value_type),
  .name = "string",
  .to_string = ava_string_type_to_string,
  .string_chunk_iterator = ava_singleton_string_chunk_iterator,
  .iterate_string_chunk = ava_iterate_singleton_string_chunk,
  .query_accelerator  = ava_noop_query_accelerator,
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
