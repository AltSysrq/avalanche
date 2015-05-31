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

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/list-proj.h"

typedef struct {
  size_t num_lists;
  ava_fat_list_value lists[];
} ava_list_proj_interleave_list;

typedef struct {
  ava_fat_list_value delegate;
  size_t offset, stride;
} ava_list_proj_demux_list;

typedef struct {
  ava_fat_list_value delegate;
  size_t group_size, num_groups;
  /* Groups are calculated once then cached here */
  AO_t /* const ava_list_value*restrict */ groups[];
} ava_list_proj_group_list;

static size_t ava_list_proj_interleave_list_length(ava_list_value list);
static ava_value ava_list_proj_interleave_list_index(ava_list_value list, size_t ix);

static size_t ava_list_proj_demux_list_length(ava_list_value list);
static ava_value ava_list_proj_demux_list_index(ava_list_value list, size_t ix);

static size_t ava_list_proj_group_list_length(ava_list_value list);
static ava_value ava_list_proj_group_list_index(ava_list_value list, size_t ix);

static const ava_value_trait ava_list_proj_interleave_generic_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "interleave-list-proj",
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = ava_list_string_chunk_iterator,
  .iterate_string_chunk = ava_list_iterate_string_chunk,
  .hash = ava_value_default_hash,
};

static const ava_list_trait ava_list_proj_interleave_list_impl = {
  .header = {
    .tag = &ava_list_trait_tag,
    .next = (const ava_attribute*)&ava_list_proj_interleave_generic_impl
  },
  .length = ava_list_proj_interleave_list_length,
  .index = ava_list_proj_interleave_list_index,
  .slice = ava_list_copy_slice,
  .append = ava_list_copy_append,
  .concat = ava_list_copy_concat,
  .delete = ava_list_copy_delete,
  .set = ava_list_copy_set,
};

static const ava_value_trait ava_list_proj_demux_generic_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "demux-list-proj",
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = ava_list_string_chunk_iterator,
  .iterate_string_chunk = ava_list_iterate_string_chunk,
  .hash = ava_value_default_hash,
};

static const ava_list_trait ava_list_proj_demux_list_impl = {
  .header = {
    .tag = &ava_list_trait_tag,
    .next = (const ava_attribute*)&ava_list_proj_demux_generic_impl
  },
  .length = ava_list_proj_demux_list_length,
  .index = ava_list_proj_demux_list_index,
  .slice = ava_list_copy_slice,
  .append = ava_list_copy_append,
  .concat = ava_list_copy_concat,
  .delete = ava_list_copy_delete,
  .set = ava_list_copy_set,
};

static const ava_value_trait ava_list_proj_group_generic_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "group-list-proj",
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = ava_list_string_chunk_iterator,
  .iterate_string_chunk = ava_list_iterate_string_chunk,
  .hash = ava_value_default_hash,
};

static const ava_list_trait ava_list_proj_group_list_impl = {
  .header = {
    .tag = &ava_list_trait_tag,
    .next = (const ava_attribute*)&ava_list_proj_group_generic_impl
  },
  .length = ava_list_proj_group_list_length,
  .index = ava_list_proj_group_list_index,
  .slice = ava_list_copy_slice,
  .append = ava_list_copy_append,
  .concat = ava_list_copy_concat,
  .delete = ava_list_copy_delete,
  .set = ava_list_copy_set,
};

ava_list_value ava_list_proj_interleave(const ava_list_value*restrict lists,
                                        size_t num_lists) {
  size_t i;
#ifndef NDEBUG
  size_t first_list_length;
#endif

  assert(num_lists > 0);
#ifndef NDEBUG
  first_list_length = ava_list_length(lists[0].v);
  for (i = 1; i < num_lists; ++i)
    assert(first_list_length == ava_list_length(lists[i].v));
#endif

  if (1 == num_lists)
    return lists[0];

  /* See if we can invert a demux */
  const ava_list_proj_demux_list*restrict demux0 = ava_value_ptr(lists[0].v);
  for (i = 0; i < num_lists; ++i) {
    if (&ava_list_proj_demux_list_impl !=
        ava_get_attribute(lists[i].v, &ava_list_trait_tag))
      goto project;

    const ava_list_proj_demux_list*restrict demux = ava_value_ptr(lists[i].v);

    if (i != demux->offset || num_lists != demux->stride)
      goto project;

    if (memcmp(&demux->delegate, &demux0->delegate,
               sizeof(ava_fat_list_value)))
      goto project;
  }

  /* All inputs are demuxes to the same delegate, with offset/stride pairs
   * compatible with this interleaving.
   */
  return demux0->delegate.c;

  project:;
  ava_list_proj_interleave_list* dst = ava_alloc(
    sizeof(ava_list_proj_interleave_list) +
    sizeof(ava_fat_list_value) * num_lists);
  dst->num_lists = num_lists;

  for (i = 0; i < num_lists; ++i)
    dst->lists[i] = ava_fat_list_value_of(lists[i].v);

  return (ava_list_value) {
    ava_value_with_ptr(&ava_list_proj_interleave_list_impl, dst)
  };
}

static size_t ava_list_proj_interleave_list_length(ava_list_value list) {
  const ava_list_proj_interleave_list*restrict this = ava_value_ptr(list.v);

  return this->num_lists * this->lists[0].v->length(this->lists[0].c);
}

static ava_value ava_list_proj_interleave_list_index(
  ava_list_value list, size_t ix
) {
  const ava_list_proj_interleave_list*restrict this = ava_value_ptr(list.v);
  size_t which;

  which = ix % this->num_lists;
  ix /= this->num_lists;

  return this->lists[which].v->index(this->lists[which].c, ix);
}

ava_list_value ava_list_proj_demux(ava_list_value delegate,
                                   size_t offset, size_t stride) {
  ava_list_proj_demux_list*restrict this;

  assert(offset < stride);

  if (1 == stride) return delegate;

  if (&ava_list_proj_interleave_list_impl ==
      ava_get_attribute(delegate.v, &ava_list_trait_tag)) {
    const ava_list_proj_interleave_list*restrict il = ava_value_ptr(delegate.v);
    if (stride == il->num_lists) {
      return il->lists[offset].c;
    }
  }

  this = AVA_NEW(ava_list_proj_demux_list);
  this->delegate = ava_fat_list_value_of(delegate.v);
  this->offset = offset;
  this->stride = stride;

  return (ava_list_value) {
    ava_value_with_ptr(&ava_list_proj_demux_list_impl, this)
  };
}

static size_t ava_list_proj_demux_list_length(ava_list_value list) {
  const ava_list_proj_demux_list*restrict this = ava_value_ptr(list.v);
  return (this->delegate.v->length(this->delegate.c) -
          this->offset + this->stride-1) / this->stride;
}

static ava_value ava_list_proj_demux_list_index(
  ava_list_value list, size_t ix
) {
  const ava_list_proj_demux_list*restrict this = ava_value_ptr(list.v);
  return this->delegate.v->index(this->delegate.c,
                                 this->offset + ix * this->stride);
}

ava_list_value ava_list_proj_group(ava_list_value delegate, size_t group_size) {
  ava_list_proj_group_list* this;
  size_t num_groups =
    (ava_list_length(delegate.v) + group_size-1) / group_size;

  this = ava_alloc(sizeof(ava_list_proj_group_list) +
                   sizeof(AO_t) * num_groups);
  this->delegate = ava_fat_list_value_of(delegate.v);
  this->group_size = group_size;
  this->num_groups = num_groups;

  return (ava_list_value) {
    ava_value_with_ptr(&ava_list_proj_group_list_impl, this)
  };
}

static size_t ava_list_proj_group_list_length(ava_list_value list) {
  const ava_list_proj_group_list*restrict this = ava_value_ptr(list.v);
  return this->num_groups;
}

static ava_value ava_list_proj_group_list_index(
  ava_list_value list, size_t ix
) {
  ava_list_proj_group_list*restrict this = (void*)ava_value_ptr(list.v);
  const ava_list_value*restrict ret;
  ava_list_value tmp;
  size_t begin, end, delegate_length;

  assert(ix < this->num_groups);

  ret = (const ava_list_value*restrict)AO_load_acquire_read(this->groups + ix);
  if (ret) return ret->v;

  /* Group not yet cached, create it */
  begin = ix * this->group_size;
  end = begin + this->group_size;
  delegate_length = this->delegate.v->length(this->delegate.c);
  if (end > delegate_length) end = delegate_length;

  tmp = this->delegate.v->slice(this->delegate.c, begin, end);
  ret = ava_clone(&tmp, sizeof(tmp));
  /* Save in cache. If multiple threads hit this index simultaneously, they may
   * produce physically different results and write the cache multiple times.
   * That's ok.
   */
  AO_store_release_write(this->groups + ix, (AO_t)ret);

  return ret->v;
}

ava_list_value ava_list_proj_flatten(ava_list_value list) {
  ava_list_value accum;
  size_t i, n;

  if (&ava_list_proj_group_list_impl ==
      ava_get_attribute(list.v, &ava_list_trait_tag)) {
    const ava_list_proj_group_list*restrict group = ava_value_ptr(list.v);
    return group->delegate.c;
  }

  n = ava_list_length(list);
  if (0 == n)
    return ava_empty_list();

  accum = ava_list_value_of(ava_list_index(list, 0));

  for (i = 1; i < n; ++i)
    accum = ava_list_concat(accum, ava_list_value_of(ava_list_index(list, i)));

  return accum;
}
