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
#include "-esba-list.h"

#define AVA_ARRAY_LIST_MIN_CAPACITY 8

static const ava_attribute_tag ava_array_list_data_tag = {
  .name = "array-list-data"
};

/**
 * The body of an array list value.
 *
 * This is stored as the first attribute on the value. The length of the array
 * is stored in the value ulong. Initially, array lists are allocated to be
 * just big enough for their contents, since many lists are constructed then
 * never modified. Upon a reallocation, the new list will have a capacity of
 * AVA_ARRAY_LIST_MIN_CAPACITY or twice what is needed, whichever is larger.
 *
 * When an item is appended, we first check whether the length is less than the
 * capacity. If it is, we attempt a compare-and-swap to change the used field
 * from length to length+1. If this succeeds, the new value can be added to the
 * end of the values array instead of allocating a new array list. This is safe
 * without special reader support since concurrent readers each have their own
 * length which will be less than the new value of used, and thus no other
 * thread will ever attempt to read into the modified portion of values unless
 * it gets its hands on a new ava_value with the larger length, which by nature
 * already involves the necessary memory barrier.
 */
typedef struct {
  ava_attribute header;
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

static ava_array_list* ava_array_list_of_array(
  const ava_value*restrict, size_t, size_t);
static size_t ava_array_list_growing_capacity(size_t);
static size_t ava_array_list_value_value_weight(ava_value);

static size_t ava_array_list_list_length(ava_list_value);
static ava_value ava_array_list_list_index(ava_list_value, size_t);
static ava_list_value ava_array_list_list_slice(ava_list_value, size_t, size_t);
static ava_list_value ava_array_list_list_append(ava_list_value, ava_value);
static ava_list_value ava_array_list_list_concat(ava_list_value, ava_list_value);
static ava_list_value ava_array_list_list_delete(ava_list_value, size_t, size_t);
static ava_list_value ava_array_list_list_set(ava_list_value, size_t, ava_value);

static const ava_value_trait ava_array_list_generic_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = ava_list_string_chunk_iterator,
  .iterate_string_chunk = ava_list_iterate_string_chunk,
  .value_weight = ava_array_list_value_value_weight,
};

static const ava_list_trait ava_array_list_list_impl = {
  .header = { .tag = &ava_list_trait_tag,
              .next = (const ava_attribute*)&ava_array_list_generic_impl },
  .length = ava_array_list_list_length,
  .index = ava_array_list_list_index,
  .slice = ava_array_list_list_slice,
  .append = ava_array_list_list_append,
  .concat = ava_array_list_list_concat,
  .delete = ava_array_list_list_delete,
  .set = ava_array_list_list_set,
};

unsigned ava_array_list_used(ava_value list) {
  const ava_array_list*restrict al = ava_value_attr(list);
  return al->used;
}

ava_list_value ava_array_list_copy_of(
  ava_list_value list, size_t begin, size_t end
) {
  ava_array_list* al = ava_alloc(sizeof(ava_array_list) +
                                 sizeof(ava_value) * (end - begin));
  size_t weight = 0, i;

  al->header.tag = &ava_array_list_data_tag;
  al->header.next = (const ava_attribute*)&ava_array_list_list_impl;
  al->capacity = end - begin;
  al->used = end - begin;

  for (i = begin; i < end; ++i) {
    al->values[i-begin] = ava_list_index(list, i);
    weight += ava_value_weight(al->values[i-begin]);
  }

  al->weight = weight + sizeof(ava_value) * (end - begin);

  return (ava_list_value) { ava_value_with_ulong(al, end - begin) };
}

static ava_array_list* ava_array_list_of_array(
  const ava_value*restrict array,
  size_t length,
  size_t cap
) {
  ava_array_list* al = ava_alloc(sizeof(ava_array_list) +
                                 sizeof(ava_value) * cap);
  size_t weight = 0, i;

  al->header.tag = &ava_array_list_data_tag;
  al->header.next = (const ava_attribute*)&ava_array_list_list_impl;
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

  return (ava_list_value) { ava_value_with_ulong(al, length) };
}

static size_t ava_array_list_value_value_weight(ava_value list) {
  const ava_array_list*restrict al = ava_value_attr(list);
  return al->weight;
}

static size_t ava_array_list_list_length(ava_list_value list) {
  return ava_value_ulong(list.v);
}

static ava_value ava_array_list_list_index(ava_list_value list, size_t ix) {
  const ava_array_list*restrict al = ava_value_attr(list.v);

  assert(ix < ava_value_ulong(list.v));
  return al->values[ix];
}

static ava_list_value ava_array_list_list_slice(ava_list_value list,
                                                size_t begin, size_t end) {
  const ava_array_list*restrict al = ava_value_attr(list.v);

  assert(begin <= end);
  assert(end <= ava_value_ulong(list.v));

  if (begin == end)
    return ava_empty_list();

  if (0 == begin && end * 2 >= al->capacity)
    return (ava_list_value) { ava_value_with_ulong(al, end) };

  return ava_array_list_of_raw(al->values + begin, end - begin);
}

static size_t ava_array_list_growing_capacity(size_t length) {
  return length * 2 < AVA_ARRAY_LIST_MIN_CAPACITY?
    AVA_ARRAY_LIST_MIN_CAPACITY : length * 2;
}

static ava_list_value ava_array_list_list_append(ava_list_value list,
                                                 ava_value elt) {
  ava_array_list*restrict al = (ava_array_list*)ava_value_attr(list.v);
  size_t length = ava_value_ulong(list.v);

  if (length < al->capacity) {
    /* Try to append in-place */
    if (AO_compare_and_swap(&al->used, length, length + 1)) {
      /* Success */
      al->values[length] = elt;
      AO_fetch_and_add(&al->weight, ava_value_weight(elt));

      return (ava_list_value) { ava_value_with_ulong(al, length + 1) };
    }
  }

  /* Failed, create a new allocation */
  if (length + 1 <= AVA_ARRAY_LIST_THRESH) {
    al = ava_array_list_of_array(al->values, length,
                                 ava_array_list_growing_capacity(
                                   length + 1));
    al->values[length] = elt;
    ++al->used;
    al->weight += ava_value_weight(elt);

    return (ava_list_value) { ava_value_with_ulong(al, length + 1) };
  } else {
    list = ava_esba_list_of_raw(al->values, length);
    return ava_list_append(list, elt);
  }
}

static ava_list_value ava_array_list_list_concat(ava_list_value list,
                                                 ava_list_value other) {
  ava_array_list*restrict al = (ava_array_list*)ava_value_attr(list.v);
  size_t this_length = ava_value_ulong(list.v);
  size_t other_length = ava_list_length(other);

  if (0 == other_length) return list;

  if (this_length + other_length <= al->capacity) {
    /* Try to append in-place */
    if (AO_compare_and_swap(&al->used, this_length,
                            this_length + other_length)) {
      /* Success, skip reallocation */
      goto mutate_al;
    }
  }

  /* Can't concat in-place, create a new allocation */
  if (this_length + other_length <= AVA_ARRAY_LIST_THRESH) {
    al = ava_array_list_of_array(al->values, this_length,
                                 ava_array_list_growing_capacity(
                                   this_length + other_length));
    al->used += other_length;

    mutate_al:;
    size_t added_weight = 0;
    size_t i;
    for (i = 0; i < other_length; ++i) {
      al->values[this_length + i] = ava_list_index(other, i);
      added_weight += ava_value_weight(al->values[this_length + i]);
    }

    al->weight += added_weight;
    return (ava_list_value) {
      ava_value_with_ulong(al, this_length + other_length)
    };
  } else {
    list = ava_esba_list_of_raw(al->values, this_length);
    return ava_list_concat(list, other);
  }
}

static ava_list_value ava_array_list_list_delete(
  ava_list_value list, size_t begin, size_t end
) {
  const ava_array_list*restrict al = ava_value_attr(list.v);
  size_t length = ava_value_ulong(list.v);

  assert(begin <= end);
  assert(end <= length);

  if (begin == end)
    return list;

  if (0 == begin)
    return ava_array_list_list_slice(list, end, length);

  if (length == end)
    return ava_array_list_list_slice(list, 0, begin);

  ava_array_list*restrict nal = ava_array_list_of_array(
    al->values, begin, length - (end - begin));
  size_t i;
  for (i = end; i < length; ++i) {
    nal->values[i - (end - begin)] = al->values[i];
    nal->weight += ava_value_weight(al->values[i]);
  }
  nal->used += length - end;

  return (ava_list_value) { ava_value_with_ulong(nal, length - (end - begin)) };
}

static ava_list_value ava_array_list_list_set(
  ava_list_value list, size_t index, ava_value value
) {
  const ava_array_list*restrict al = ava_value_attr(list.v);
  size_t length = ava_value_ulong(list.v);

  assert(index < length);

  ava_array_list*restrict mal = ava_array_list_of_array(
    al->values, length, length);
  mal->weight -= ava_value_weight(mal->values[index]);
  mal->weight += ava_value_weight(value);
  mal->values[index] = value;

  return (ava_list_value) { ava_value_with_ulong(mal, length) };
}
