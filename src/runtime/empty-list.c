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
#include "avalanche/map.h"
#include "-array-list.h"

static ava_string ava_empty_list_value_to_string(ava_value);
static ava_datum ava_empty_list_value_string_chunk_iterator(ava_value);
static ava_string ava_empty_list_value_iterate_string_chunk(
  ava_datum*restrict, ava_value);

static const ava_value_trait ava_empty_list_generic_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "empty-list",
  .to_string = ava_empty_list_value_to_string,
  .string_chunk_iterator = ava_empty_list_value_string_chunk_iterator,
  .iterate_string_chunk = ava_empty_list_value_iterate_string_chunk,
  .hash = ava_value_default_hash,
};

AVA_MAP_DEFIMPL(ava_empty_list, &ava_empty_list_generic_impl)
AVA_LIST_DEFIMPL(ava_empty_list, &ava_empty_list_map_impl)

ava_list_value ava_empty_list(void) {
  return (ava_list_value) {
    ava_value_with_ptr(&ava_empty_list_list_impl, NULL)
  };
}

ava_map_value ava_empty_map(void) {
  return (ava_map_value) {
    ava_value_with_ptr(&ava_empty_list_list_impl, NULL)
  };
}

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

static size_t ava_empty_list_list_length(ava_list_value el) {
  return 0;
}

static ava_value ava_empty_list_list_index(ava_list_value el, size_t ix) {
  abort();
}

static ava_list_value ava_empty_list_list_slice(ava_list_value el,
                                                size_t begin, size_t end) {
  if (begin || end) abort();

  return el;
}

static ava_list_value ava_empty_list_list_append(ava_list_value el,
                                                 ava_value elt) {
  return ava_array_list_of_raw(&elt, 1);
}

static ava_list_value ava_empty_list_list_concat(
  ava_list_value el, ava_list_value other
) {
  return other;
}

static ava_list_value ava_empty_list_list_delete(
  ava_list_value el, size_t begin, size_t end
) {
  if (begin || end) abort();

  return el;
}

static ava_list_value ava_empty_list_list_set(
  ava_list_value el, size_t index, ava_value value
) {
  abort();
}

static size_t ava_empty_list_map_npairs(ava_map_value el) {
  return 0;
}

static ava_map_cursor ava_empty_list_map_find(ava_map_value el, ava_value key) {
  return AVA_MAP_CURSOR_NONE;
}

static ava_map_cursor ava_empty_list_map_next(ava_map_value el,
                                              ava_map_cursor cursor) {
  abort();
}

static ava_value ava_empty_list_map_get(ava_map_value el,
                                        ava_map_cursor cursor) {
  abort();
}

static ava_value ava_empty_list_map_get_key(ava_map_value el,
                                            ava_map_cursor cursor) {
  abort();
}

static ava_map_value ava_empty_list_map_set(ava_map_value el,
                                            ava_map_cursor cursor,
                                            ava_value value) {
  abort();
}

static ava_map_value ava_empty_list_map_delete(ava_map_value el,
                                               ava_map_cursor cursor) {
  abort();
}

static ava_map_value ava_empty_list_map_add(ava_map_value el,
                                            ava_value key,
                                            ava_value value) {
  return ava_map_of_values(&key, 0, &value, 0, 1);
}
