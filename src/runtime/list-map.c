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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/map.h"
#include "-hash-map.h"
#include "-list-map.h"

/**
 * The head of the attribute chain on a list-map value; allows obtaining the
 * attribute chain belonging to the underlying list. (The data element of the
 * value is still stored directly on the value.)
 */
typedef struct {
  ava_attribute header;

  /**
   * The original attribute chain.
   */
  const ava_attribute*restrict list_attr;

  /**
   * The list trait on the delegate.
   */
  const ava_list_trait*restrict v;
} ava_list_map;

static const ava_attribute_tag ava_list_map_tag = {
  .name = "list-map"
};

static inline ava_list_value ava_list_map_delegate(ava_value map);
static inline const ava_list_trait* ava_list_map_v(ava_value map);
static ava_map_value ava_list_map_redelegate(ava_map_value this,
                                             ava_list_value new_delegate);
static ava_map_cursor ava_list_map_search(
  ava_map_value this, ava_value key, ava_map_cursor start);

static const ava_value_trait ava_list_map_value_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = ava_list_string_chunk_iterator,
  .iterate_string_chunk = ava_list_iterate_string_chunk,
};

AVA_LIST_DEFIMPL(ava_list_map, &ava_list_map_value_impl)
AVA_MAP_DEFIMPL(ava_list_map, &ava_list_map_list_impl)

ava_map_value ava_list_map_of_list(ava_list_value list) {
  const ava_list_trait*restrict trait = ava_get_attribute(
    list.v, &ava_list_trait_tag);
  ava_list_map* this;

  assert(0 == ava_list_length(list) % 2);
  assert(trait);

  this = AVA_NEW(ava_list_map);
  this->header.tag = &ava_list_map_tag;
  this->header.next = (const ava_attribute*)&ava_list_map_map_impl;
  this->list_attr = ava_value_attr(list.v);
  this->v = trait;

  return (ava_map_value) {
    ava_value_with_ulong(this, ava_value_ulong(list.v))
  };
}

static inline ava_list_value ava_list_map_delegate(ava_value map) {
  const ava_list_map*restrict this = (const ava_list_map*)ava_value_attr(map);

  return (ava_list_value) {
    ava_value_with_ulong(this->list_attr, ava_value_ulong(map))
  };
}

static inline const ava_list_trait* ava_list_map_v(ava_value map) {
  const ava_list_map*restrict this = (const ava_list_map*)ava_value_attr(map);

  return this->v;
}

#define DELEGATE(this,method,...)                                       \
  (ava_list_map_v(this.v)->method(ava_list_map_delegate(this.v) __VA_ARGS__))

static size_t ava_list_map_map_npairs(ava_map_value this) {
  return DELEGATE(this, length) / 2;
}

static ava_map_cursor ava_list_map_search(ava_map_value this, ava_value key,
                                          ava_map_cursor start) {
  size_t i, n;

  n = DELEGATE(this, length) / 2;
  for (i = start; i < n; ++i) {
    if (ava_value_equal(key, DELEGATE(this, index,, i*2)))
      return i;
  }

  return AVA_MAP_CURSOR_NONE;
}

static ava_map_cursor ava_list_map_map_find(ava_map_value this, ava_value key) {
  return ava_list_map_search(this, key, 0);
}

static ava_map_cursor ava_list_map_map_next(ava_map_value this,
                                            ava_map_cursor cursor) {
  return ava_list_map_search(this, DELEGATE(this, index,, cursor*2), cursor+1);
}

static ava_value ava_list_map_map_get(ava_map_value this,
                                      ava_map_cursor cursor) {
  return DELEGATE(this, index,, cursor*2 + 1);
}

static ava_value ava_list_map_map_get_key(ava_map_value this,
                                          ava_map_cursor cursor) {
  return DELEGATE(this, index,, cursor*2);
}

static ava_map_value ava_list_map_redelegate(ava_map_value this,
                                             ava_list_value new_delegate) {
  ava_list_value orig_delegate = ava_list_map_delegate(this.v);

  if (ava_value_attr(new_delegate.v) != ava_value_attr(orig_delegate.v)) {
    /* The attribute chain changed, need to create a new map-list */
    return ava_list_map_of_list(new_delegate);
  } else {
    /* The attribute is the same; any changes are reflected in the new ulong */
    return (ava_map_value) {
      ava_value_with_ulong(ava_value_attr(this.v),
                           ava_value_ulong(new_delegate.v))
    };
  }
}

static ava_map_value ava_list_map_map_set(ava_map_value this,
                                          ava_map_cursor cursor,
                                          ava_value value) {
  ava_list_value new_delegate = DELEGATE(this, set,, cursor*2 + 1, value);

  return ava_list_map_redelegate(this, new_delegate);
}

static ava_map_value ava_list_map_map_add(ava_map_value this,
                                          ava_value key,
                                          ava_value value) {
  ava_list_value new_delegate = DELEGATE(this, append,, key);
  new_delegate = ava_list_append(new_delegate, value);

  if (ava_list_length(new_delegate) <= AVA_LIST_MAP_THRESH) {
    return ava_list_map_redelegate(this, new_delegate);
  } else {
    return ava_hash_map_of_list(new_delegate);
  }
}

static ava_map_value ava_list_map_map_delete(ava_map_value this,
                                             ava_map_cursor cursor) {
  if (1 == ava_list_map_map_npairs(this)) {
    return ava_empty_map();
  } else {
    return ava_list_map_redelegate(
      this, DELEGATE(this, delete,, cursor*2, cursor*2+2));
  }
}

static size_t ava_list_map_list_length(ava_list_value this) {
  return DELEGATE(this, length);
}

static ava_value ava_list_map_list_index(ava_list_value this, size_t ix) {
  return DELEGATE(this, index,, ix);
}

static ava_list_value ava_list_map_list_slice(ava_list_value this,
                                              size_t begin, size_t end) {
  return DELEGATE(this, slice,, begin, end);
}

static ava_list_value ava_list_map_list_append(ava_list_value this,
                                               ava_value element) {
  return DELEGATE(this, append,, element);
}

static ava_list_value ava_list_map_list_concat(ava_list_value this,
                                               ava_list_value that) {
  return DELEGATE(this, concat,, that);
}

static ava_list_value ava_list_map_list_delete(ava_list_value this,
                                               size_t begin, size_t end) {
  return DELEGATE(this, delete,, begin, end);
}

static ava_list_value ava_list_map_list_set(ava_list_value this,
                                            size_t ix, ava_value value) {
  return DELEGATE(this, set,, ix, value);
}
