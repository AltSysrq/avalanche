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

#define AVA__INTERNAL_INCLUDE 1
#define AVA__IN_INTERVAL_C
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/integer.h"
#include "avalanche/interval.h"

static ava_string ava_compact_interval_to_string(ava_value value);
static ava_string ava_wide_interval_to_string(ava_value value);

const ava_value_trait ava_compact_interval_type = {
  .header = { .tag = &ava_value_trait_tag },
  .name = "compact-interval",
  .to_string = ava_compact_interval_to_string,
  .string_chunk_iterator = ava_singleton_string_chunk_iterator,
  .iterate_string_chunk = ava_iterate_singleton_string_chunk,
};

const ava_value_trait ava_wide_interval_type = {
  .header = { .tag = &ava_value_trait_tag },
  .name = "wide-interval",
  .to_string = ava_wide_interval_to_string,
  .string_chunk_iterator = ava_singleton_string_chunk_iterator,
  .iterate_string_chunk = ava_iterate_singleton_string_chunk,
};

ava_interval_value ava_interval_value_of_other(ava_value val) {
  ava_string whole, prefix, suffix;
  ssize_t tilde;

  whole = ava_to_string(val);
  tilde = ava_strchr_ascii(whole, '~');
  if (-1 == tilde) {
    return ava_interval_value_of_singular(
      ava_integer_of_value(val, AVA_INTEGER_END));
  } else {
    prefix = ava_string_trunc(whole, tilde);
    suffix = ava_string_behead(whole, tilde+1);

    return ava_interval_value_of_range(
      ava_integer_of_value(ava_value_of_string(prefix), 0),
      ava_integer_of_value(ava_value_of_string(suffix), AVA_INTEGER_END));
  }
}

const ava_wide_interval* ava_wide_interval_new(
  ava_integer begin, ava_integer end
) {
  ava_wide_interval* i;

  i = ava_alloc_atomic(sizeof(ava_wide_interval));
  i->begin = begin;
  i->end = end;
  return i;
}

static ava_string ava_compact_interval_to_string(ava_value value) {
  ava_sint begin, end;

  begin = ava_value_slong(value);
  end = ava_value_slong(value) >> 32;

  return ava_strcat(
    ava_strcat(
      (ava_sint)0x80000000 == begin? AVA_ASCII9_STRING("end") :
      ava_to_string(ava_value_of_integer(begin)),
      AVA_ASCII9_STRING("~")),
    (ava_sint)0x80000000 == end? AVA_ASCII9_STRING("end") :
    ava_to_string(ava_value_of_integer(end)));
}

static ava_string ava_wide_interval_to_string(ava_value value) {
  const ava_wide_interval* i = ava_value_ptr(value);

  return ava_strcat(
    ava_strcat(
      AVA_INTEGER_END == i->begin? AVA_ASCII9_STRING("end") :
      ava_to_string(ava_value_of_integer(i->begin)),
      AVA_ASCII9_STRING("~")),
    AVA_INTEGER_END == i->end? AVA_ASCII9_STRING("end") :
    ava_to_string(ava_value_of_integer(i->end)));
}
