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

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "-array-list.h"

static ava_string ava_empty_list_value_to_string(ava_value);
static ava_datum ava_empty_list_value_string_chunk_iterator(ava_value);
static ava_string ava_empty_list_value_iterate_string_chunk(
  ava_datum*restrict, ava_value);
static size_t ava_empty_list_value_value_weight(ava_value);

static size_t ava_empty_list_list_length(ava_list_value);
static ava_value ava_empty_list_list_index(ava_list_value, size_t);
static ava_list_value ava_empty_list_list_slice(ava_list_value, size_t, size_t);
static ava_list_value ava_empty_list_list_append(ava_list_value, ava_value);
static ava_value ava_empty_list_list_concat(ava_value, ava_value);
static ava_value ava_empty_list_list_delete(ava_value, size_t, size_t);
static ava_value ava_empty_list_list_set(ava_value, size_t, ava_value);

static const ava_value_trait ava_empty_list_generic_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "empty-list",
  .to_string = ava_empty_list_value_to_string,
  .string_chunk_iterator = ava_empty_list_value_string_chunk_iterator,
  .iterate_string_chunk = ava_empty_list_value_iterate_string_chunk,
  .value_weight = ava_empty_list_value_value_weight,
};

static const ava_list_trait ava_empty_list_list_impl = {
  .header = { .tag = &ava_list_trait_tag,
              .next = (const ava_attribute*)&ava_empty_list_generic_impl },
  .length = ava_empty_list_list_length,
  .index = ava_empty_list_list_index,
  .slice = ava_empty_list_list_slice,
  .append = ava_empty_list_list_append,
  .concat = ava_empty_list_list_concat,
  .delete = ava_empty_list_list_delete,
  .set = ava_empty_list_list_set,
};

const ava_list_value ava_empty_list = {
  .r1 = { .ptr = NULL },
  .r2 = { .ptr = NULL },
  .v = &ava_empty_list_list_impl
};

static ava_string ava_empty_list_value_to_string(ava_value el) {
  return AVA_EMPTY_STRING;
}

static ava_datum ava_empty_list_value_string_chunk_iterator(ava_value el) {
  return (ava_datum) { .ptr = NULL };
}

static ava_string ava_empty_list_value_iterate_string_chunk(
  ava_datum*restrict it, ava_value el
) {
  return AVA_ABSENT_STRING;
}

static size_t ava_empty_list_value_value_weight(ava_value el) {
  return 0;
}

static size_t ava_empty_list_list_length(ava_list_value el) {
  return 0;
}

static ava_value ava_empty_list_list_index(ava_list_value el, size_t ix) {
  abort();
}

static ava_list_value ava_empty_list_list_slice(ava_list_value el,
                                                size_t begin,
                                                size_t end) {
  if (begin || end) abort();

  return el;
}

static ava_list_value ava_empty_list_list_append(ava_list_value el,
                                                 ava_value elt) {
  return ava_array_list_of_raw(&elt, 1);
}

static ava_value ava_empty_list_list_concat(ava_value el, ava_value other) {
  return other;
}

static ava_value ava_empty_list_list_delete(
  ava_value el, size_t begin, size_t end
) {
  if (begin || end) abort();

  return el;
}

static ava_value ava_empty_list_list_set(
  ava_value el, size_t index, ava_value value
) {
  abort();
}
