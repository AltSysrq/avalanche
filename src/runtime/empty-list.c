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
static const void* ava_empty_list_value_query_accelerator(
  const ava_accelerator* accel, const void* dfault);
static size_t ava_empty_list_value_value_weight(ava_value);

static ava_value ava_empty_list_list_to_value(ava_list_value);
static size_t ava_empty_list_list_length(ava_list_value);
static ava_value ava_empty_list_list_index(ava_list_value, size_t);
static ava_list_value ava_empty_list_list_slice(ava_list_value, size_t, size_t);
static ava_list_value ava_empty_list_list_append(ava_list_value, ava_value);
static ava_list_value ava_empty_list_list_concat(
  ava_list_value, ava_list_value);
static ava_list_value ava_empty_list_list_delete(
  ava_list_value, size_t, size_t);
static size_t ava_empty_list_list_iterator_size(ava_list_value);
static void ava_empty_list_list_iterator_place(
  ava_list_value, void*restrict, size_t);
static ava_value ava_empty_list_list_iterator_get(
  ava_list_value, const void*restrict);
static void ava_empty_list_list_iterator_move(
  ava_list_value, void*restrict, ssize_t);
static size_t ava_empty_list_list_iterator_index(
  ava_list_value, const void*restrict);

static const ava_value_type ava_empty_list_type = {
  .to_string = ava_empty_list_value_to_string,
  .string_chunk_iterator = ava_empty_list_value_string_chunk_iterator,
  .iterate_string_chunk = ava_empty_list_value_iterate_string_chunk,
  .query_accelerator = ava_empty_list_value_query_accelerator,
  .value_weight = ava_empty_list_value_value_weight,
};

static const ava_list_iface ava_empty_list_iface = {
  .to_value = ava_empty_list_list_to_value,
  .length = ava_empty_list_list_length,
  .index = ava_empty_list_list_index,
  .slice = ava_empty_list_list_slice,
  .append = ava_empty_list_list_append,
  .concat = ava_empty_list_list_concat,
  .delete = ava_empty_list_list_delete,
  .iterator_size = ava_empty_list_list_iterator_size,
  .iterator_place = ava_empty_list_list_iterator_place,
  .iterator_get = ava_empty_list_list_iterator_get,
  .iterator_move = ava_empty_list_list_iterator_move,
  .iterator_index = ava_empty_list_list_iterator_index,
};

const ava_list_value ava_empty_list = {
  .r1 = { .ptr = NULL },
  .r2 = { .ptr = NULL },
  .v = &ava_empty_list_iface
};

static const ava_value ava_empty_list_value = {
  .r1 = { .ptr = NULL },
  .r2 = { .ptr = NULL },
  .type = &ava_empty_list_type
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

static const void* ava_empty_list_value_query_accelerator(
  const ava_accelerator* accel,
  const void* dfault
) {
  return &ava_list_accelerator == accel? &ava_empty_list_iface : dfault;
}

static size_t ava_empty_list_value_value_weight(ava_value el) {
  return 0;
}

static ava_value ava_empty_list_list_to_value(ava_list_value el) {
  return ava_empty_list_value;
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

static ava_list_value ava_empty_list_list_concat(ava_list_value el,
                                                 ava_list_value other) {
  return other;
}

static ava_list_value ava_empty_list_list_delete(
  ava_list_value el, size_t begin, size_t end
) {
  if (begin || end) abort();

  return el;
}

static size_t ava_empty_list_list_iterator_size(ava_list_value el) {
  return sizeof(size_t);
}

static void ava_empty_list_list_iterator_place(
  ava_list_value list, void*restrict iterator, size_t ix
) {
  *(size_t*restrict)iterator = ix;
}

static ava_value ava_empty_list_list_iterator_get(
  ava_list_value el, const void*restrict iterator
) {
  abort();
}

static void ava_empty_list_list_iterator_move(
  ava_list_value el, void*restrict iterator, ssize_t off
) {
  *(size_t*restrict)iterator += off;
}

static size_t ava_empty_list_list_iterator_index(
  ava_list_value el, const void*restrict iterator
) {
  return *(const size_t*restrict)iterator;
}
