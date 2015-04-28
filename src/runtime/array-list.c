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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "-array-list.h"

#define AVA_ARRAY_LIST_MIN_CAPACITY 8

/**
 * The body of an array list value.
 *
 * This is stored in r1.ptr. The length of the array is stored in r2.ulong.
 * Initially, array lists are allocated to be just big enough for their
 * contents, since many lists are constructed then never modified. Upon a
 * reallocation, the new list will have a capacity of
 * AVA_ARRAY_LIST_MIN_CAPACITY or twice what is needed, whichever is larger.
 *
 * When an item is appended, we first check whether the length is less than the
 * capacity. If it is, we attempt a compare-and-swap to change the used field
 * from length to length+1. If this succeeds, the new value can be added to the
 * end of the values array instead of allocating a new array list. This is safe
 * without special reader support since concurrent readers each have their own
 * length which will be less than the new value of used, and thus no other
 * thread will ever attempt to read into the modified portion of values unless
 * it gets its hands on a new ava_list_value with the larger length, which
 * by nature already involves the necessary memory barrier.
 */
typedef struct {
  /**
   * The actual length of values.
   */
  size_t capacity;
  /**
   * The largest index in values that has actually been populated.
   */
  AO_t used;
  /**
   * The sum of weight of the values.
   *
   * Readers access this without a memory barrier, and so may see inconsistent
   * results; since this is only used for performance advising, that's ok.
   */
  AO_t weight;

  /**
   * The contents of this array list.
   */
  ava_value values[];
} ava_array_list;

#define LIST r1.ptr
#define LENGTH r2.ulong

static ava_array_list* ava_array_list_of_array(
  const ava_value*restrict, size_t, size_t);
static size_t ava_array_list_growing_capacity(size_t);
static ava_datum ava_array_list_value_string_chunk_iterator(ava_value);
static ava_string ava_array_list_value_iterate_string_chunk(
  ava_datum*restrict, ava_value);
static const void* ava_array_list_value_query_accelerator(
  const ava_accelerator* accel, const void* dfault);
static size_t ava_array_list_value_value_weight(ava_value);

static ava_value ava_array_list_list_to_value(ava_list_value);
static size_t ava_array_list_list_length(ava_list_value);
static ava_value ava_array_list_list_index(ava_list_value, size_t);
static ava_list_value ava_array_list_list_slice(ava_list_value, size_t, size_t);
static ava_list_value ava_array_list_list_append(ava_list_value, ava_value);
static ava_list_value ava_array_list_list_concat(
  ava_list_value, ava_list_value);
static ava_list_value ava_array_list_list_delete(
  ava_list_value, size_t, size_t);
static size_t ava_array_list_list_iterator_size(ava_list_value);
static void ava_array_list_list_iterator_place(
  ava_list_value, void*restrict, size_t);
static ava_value ava_array_list_list_iterator_get(
  ava_list_value, const void*restrict);
static void ava_array_list_list_iterator_move(
  ava_list_value, void*restrict, ssize_t);
static size_t ava_array_list_list_iterator_index(
  ava_list_value, const void*restrict);

static const ava_value_type ava_array_list_type = {
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = ava_array_list_value_string_chunk_iterator,
  .iterate_string_chunk = ava_array_list_value_iterate_string_chunk,
  .query_accelerator = ava_array_list_value_query_accelerator,
  .value_weight = ava_array_list_value_value_weight,
};

static const ava_list_iface ava_array_list_iface = {
  .to_value = ava_array_list_list_to_value,
  .length = ava_array_list_list_length,
  .index = ava_array_list_list_index,
  .slice = ava_array_list_list_slice,
  .append = ava_array_list_list_append,
  .concat = ava_array_list_list_concat,
  .delete = ava_array_list_list_delete,
  .iterator_size = ava_array_list_list_iterator_size,
  .iterator_place = ava_array_list_list_iterator_place,
  .iterator_get = ava_array_list_list_iterator_get,
  .iterator_move = ava_array_list_list_iterator_move,
  .iterator_index = ava_array_list_list_iterator_index,
};

unsigned ava_array_list_used(ava_list_value list) {
  const ava_array_list*restrict al = list.LIST;
  return al->used;
}

ava_list_value ava_array_list_copy_of(
  ava_list_value list, size_t begin, size_t end
) {
  ava_array_list* al = ava_alloc(sizeof(ava_array_list) +
                                 sizeof(ava_value) * (end - begin));
  size_t weight = 0, i;
  AVA_LIST_ITERATOR(list, it);

  al->capacity = end - begin;
  al->used = end - begin;

  i = 0;
  for (list.v->iterator_place(list, it, begin);
       i < end - begin;
       list.v->iterator_move(list, it, +1)) {
    al->values[i] = list.v->iterator_get(list, it);
    weight += ava_value_weight(al->values[i]);
  }

  al->weight = weight + sizeof(ava_value) * (end - begin);

  return (ava_list_value) {
    .r1 = { .ptr = al },
    .r2 = { .ulong = end - begin },
    .v = &ava_array_list_iface,
  };
}

static ava_array_list* ava_array_list_of_array(
  const ava_value*restrict array,
  size_t length,
  size_t cap
) {
  ava_array_list* al = ava_alloc(sizeof(ava_array_list) +
                                 sizeof(ava_value) * cap);
  size_t weight = 0, i;

  al->capacity = cap;
  al->used = length;

  for (i = 0; i < length; ++i) {
    al->values[i] = array[i];
    weight += ava_value_weight(al->values[i]);
  }

  al->weight = weight + sizeof(ava_value) * cap;
  return al;
}

ava_list_value ava_array_list_of_raw(
  const ava_value*restrict array,
  size_t length
) {
  ava_array_list* al = ava_array_list_of_array(array, length, length);

  return (ava_list_value) {
    .r1 = { .ptr = al },
    .r2 = { .ulong = length },
    .v = &ava_array_list_iface
  };
}

static ava_datum ava_array_list_value_string_chunk_iterator(ava_value list) {
  return (ava_datum) { .ulong = 0 };
}

static ava_string ava_array_list_value_iterate_string_chunk(
  ava_datum*restrict it, ava_value list
) {
  const ava_array_list*restrict al = list.LIST;
  ava_string elt;

  if (it->ulong >= list.LENGTH)
    return AVA_ABSENT_STRING;

  elt = ava_to_string(al->values[it->ulong++]);

  if (it->ulong > 1)
    return ava_string_concat(AVA_ASCII9_STRING(" "),
                             elt);
  else
    return elt;
}

static const void* ava_array_list_value_query_accelerator(
  const ava_accelerator* accel,
  const void* dfault
) {
  return &ava_list_accelerator == accel? &ava_array_list_iface : dfault;
}

static size_t ava_array_list_value_value_weight(ava_value list) {
  const ava_array_list*restrict al = list.LIST;
  return al->weight;
}

static ava_value ava_array_list_list_to_value(ava_list_value list) {
  ava_value v = {
    .r1 = list.r1,
    .r2 = list.r2,
    .type = &ava_array_list_type
  };
  return v;
}

static size_t ava_array_list_list_length(ava_list_value list) {
  return list.LENGTH;
}

static ava_value ava_array_list_list_index(ava_list_value list, size_t ix) {
  const ava_array_list*restrict al = list.LIST;

  assert(ix < list.LENGTH);
  return al->values[ix];
}

static ava_list_value ava_array_list_list_slice(ava_list_value list,
                                                size_t begin,
                                                size_t end) {
  const ava_array_list*restrict al = list.LIST;

  assert(begin <= end);
  assert(end <= list.LENGTH);

  if (begin == end)
    return ava_empty_list;

  if (0 == begin && end * 2 >= al->capacity) {
    list.LENGTH = end;
    return list;
  }

  return ava_array_list_of_raw(
    al->values + begin, end - begin);
}

static size_t ava_array_list_growing_capacity(size_t length) {
  return length * 2 < AVA_ARRAY_LIST_MIN_CAPACITY?
    AVA_ARRAY_LIST_MIN_CAPACITY : length * 2;
}

static ava_list_value ava_array_list_list_append(ava_list_value list,
                                                 ava_value elt) {
  ava_array_list*restrict al = (ava_array_list*restrict)list.LIST;

  if (list.LENGTH < al->capacity) {
    /* Try to append in-place */
    if (AO_compare_and_swap(&al->used, list.LENGTH, list.LENGTH + 1)) {
      /* Success */
      al->values[list.LENGTH] = elt;
      AO_fetch_and_add(&al->weight, ava_value_weight(elt));
      ++list.LENGTH;
      return list;
    }
  }

  /* Failed, create a new allocation */
  /* TODO: Switch to B+Tree if too large */
  al = ava_array_list_of_array(al->values, list.LENGTH,
                               ava_array_list_growing_capacity(
                                 list.LENGTH + 1));
  al->values[list.LENGTH] = elt;
  ++al->used;
  al->weight += ava_value_weight(elt);

  list.LIST = al;
  ++list.LENGTH;

  return list;
}

static ava_list_value ava_array_list_list_concat(ava_list_value list,
                                                 ava_list_value other) {
  ava_array_list*restrict al = (ava_array_list*restrict)list.LIST;
  size_t other_length = other.v->length(other);

  if (0 == other_length) return list;

  if (list.LENGTH + other_length <= al->capacity) {
    /* Try to append in-place */
    if (AO_compare_and_swap(&al->used, list.LENGTH,
                            list.LENGTH + other_length)) {
      /* Success, skip reallocation */
      goto mutate_al;
    }
  }

  /* Can't concat in-place, create a new allocation */
  /* TODO: Switch to B+Tree if too large */
  al = ava_array_list_of_array(al->values, list.LENGTH,
                               ava_array_list_growing_capacity(
                                 list.LENGTH + other_length));
  al->used += other_length;
  list.LIST = al;

  mutate_al:;
  size_t added_weight = 0;
  size_t i = 0;
  AVA_LIST_ITERATOR(other, it);
  for (other.v->iterator_place(other, it, 0);
       i < other_length;
       other.v->iterator_move(other, it, 1)) {
    al->values[list.LENGTH + i] = other.v->iterator_get(other, it);
    added_weight += ava_value_weight(al->values[list.LENGTH + i]);
    ++i;
  }

  AO_fetch_and_add(&al->weight, added_weight);
  list.LENGTH += other_length;
  return list;
}

static ava_list_value ava_array_list_list_delete(
  ava_list_value list, size_t begin, size_t end
) {
  const ava_array_list*restrict al = list.LIST;

  assert(begin <= end);
  assert(end <= list.LENGTH);

  if (begin == end)
    return list;

  if (0 == begin)
    return ava_array_list_list_slice(list, end, list.LENGTH);

  if (list.LENGTH == end)
    return ava_array_list_list_slice(list, 0, begin);

  ava_array_list*restrict nal = ava_array_list_of_array(
    al->values, begin, list.LENGTH - (end - begin));
  size_t i;
  for (i = end; i < list.LENGTH; ++i) {
    nal->values[i - (end - begin)] = al->values[i];
    nal->weight += ava_value_weight(al->values[i]);
  }
  nal->used += list.LENGTH - end;

  list.LIST = nal;
  list.LENGTH -= end - begin;
  return list;
}

static size_t ava_array_list_list_iterator_size(ava_list_value list) {
  return sizeof(size_t);
}

static void ava_array_list_list_iterator_place(
  ava_list_value list, void*restrict iterator, size_t ix
) {
  *(size_t*restrict)iterator = ix;
}

static ava_value ava_array_list_list_iterator_get(
  ava_list_value list, const void*restrict iterator
) {
  const ava_array_list*restrict al = list.LIST;
  size_t ix = *(const size_t*restrict)iterator;

  assert(ix <= list.LENGTH);
  return al->values[ix];
}

static void ava_array_list_list_iterator_move(
  ava_list_value list, void*restrict iterator, ssize_t off
) {
  *(size_t*restrict)iterator += off;
}

static size_t ava_array_list_list_iterator_index(
  ava_list_value el, const void*restrict iterator
) {
  return *(const size_t*restrict)iterator;
}
