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

#define INTERLEAVE_LISTS r1.ptr
#define INTERLEAVE_LISTS_C(v) ((const ava_list_value*restrict)(v).INTERLEAVE_LISTS)
#define INTERLEAVE_NLISTS r2.ulong
#define DEMUX_LIST r1.ptr
#define DEMUX_LIST_C(v) ((const ava_list_proj_demux_list*restrict)(v).DEMUX_LIST)
#define GROUP_LIST r1.ptr
#define GROUP_LIST_C(v) ((ava_list_proj_group_list*restrict)(v).GROUP_LIST)

typedef struct {
  ava_list_value delegate;
  size_t offset, stride;
} ava_list_proj_demux_list;

typedef struct {
  ava_list_value delegate;
  size_t group_size, num_groups;
  /* Groups are calculated once then cached here */
  AO_t /* const ava_list_value*restrict */ groups[];
} ava_list_proj_group_list;

static size_t ava_list_proj_interleave_value_value_weight(ava_value val);
static size_t ava_list_proj_interleave_list_length(ava_value list);
static ava_value ava_list_proj_interleave_list_index(ava_value list, size_t ix);

static size_t ava_list_proj_demux_value_value_weight(ava_value val);
static size_t ava_list_proj_demux_list_length(ava_value list);
static ava_value ava_list_proj_demux_list_index(ava_value list, size_t ix);

static size_t ava_list_proj_group_value_value_weight(ava_value val);
static size_t ava_list_proj_group_list_length(ava_value list);
static ava_value ava_list_proj_group_list_index(ava_value list, size_t ix);

static const ava_value_trait ava_list_proj_interleave_generic_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "interleave-list-proj",
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = ava_list_string_chunk_iterator,
  .iterate_string_chunk = ava_list_iterate_string_chunk,
  .value_weight = ava_list_proj_interleave_value_value_weight,
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
  .value_weight = ava_list_proj_demux_value_value_weight,
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
  .value_weight = ava_list_proj_group_value_value_weight,
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
  ava_list_value ret;

  assert(num_lists > 0);
#ifndef NDEBUG
  first_list_length = lists[0].v->length(ava_list_value_to_value(lists[0]));
  for (i = 1; i < num_lists; ++i)
    assert(first_list_length == lists[i].v->length(ava_list_value_to_value(lists[i])));
#endif

  if (1 == num_lists)
    return lists[0];

  /* See if we can invert a demux */
  for (i = 0; i < num_lists; ++i) {
    if (&ava_list_proj_demux_list_impl != lists[i].v)
      goto project;

    if (i != DEMUX_LIST_C(lists[i])->offset ||
        num_lists != DEMUX_LIST_C(lists[i])->stride)
      goto project;

    if (memcmp(&DEMUX_LIST_C(lists[0])->delegate,
               &DEMUX_LIST_C(lists[i])->delegate,
               sizeof(ava_list_value)))
      goto project;
  }

  /* All inputs are demuxes to the same delegate, with offset/stride pairs
   * compatible with this interleaving.
   */
  return DEMUX_LIST_C(lists[0])->delegate;

  project:
  ret.v = &ava_list_proj_interleave_list_impl;
  ret.INTERLEAVE_LISTS = ava_clone(lists, sizeof(ava_list_value) * num_lists);
  ret.INTERLEAVE_NLISTS = num_lists;
  return ret;
}

static size_t ava_list_proj_interleave_value_value_weight(ava_value val) {
  const ava_list_value*restrict sublist;
  size_t i, sum = 0;

  for (i = 0; i < val.INTERLEAVE_NLISTS; ++i) {
    sublist = INTERLEAVE_LISTS_C(val) + i;
    sum += ava_value_weight(ava_list_value_to_value(*sublist));
  }

  return sum;
}

static size_t ava_list_proj_interleave_list_length(ava_value list) {
  const ava_list_value*restrict delegate;

  delegate = INTERLEAVE_LISTS_C(list);
  return list.INTERLEAVE_NLISTS * delegate->v->length(ava_list_value_to_value(*delegate));
}

static ava_value ava_list_proj_interleave_list_index(
  ava_value list, size_t ix
) {
  size_t which;
  const ava_list_value*restrict delegate;

  which = ix % list.INTERLEAVE_NLISTS;
  ix /= list.INTERLEAVE_NLISTS;

  delegate = INTERLEAVE_LISTS_C(list) + which;
  return delegate->v->index(ava_list_value_to_value(*delegate), ix);
}

ava_list_value ava_list_proj_demux(ava_list_value delegate,
                                   size_t offset, size_t stride) {
  ava_list_proj_demux_list*restrict this;
  ava_list_value ret;

  assert(offset < stride);

  if (1 == stride) return delegate;

  if (&ava_list_proj_interleave_list_impl == delegate.v) {
    if (stride == delegate.INTERLEAVE_NLISTS) {
      return INTERLEAVE_LISTS_C(delegate)[offset];
    }
  }

  this = AVA_NEW(ava_list_proj_demux_list);
  this->delegate = delegate;
  this->offset = offset;
  this->stride = stride;

  ret.v = &ava_list_proj_demux_list_impl;
  ret.DEMUX_LIST = this;
  ret.r2.ulong = 0;
  return ret;
}

static size_t ava_list_proj_demux_value_value_weight(ava_value val) {
  const ava_list_value*restrict delegate = &DEMUX_LIST_C(val)->delegate;
  return ava_value_weight(ava_list_value_to_value(*delegate));
}

static size_t ava_list_proj_demux_list_length(ava_value list) {
  const ava_list_value*restrict delegate = &DEMUX_LIST_C(list)->delegate;
  return (delegate->v->length(ava_list_value_to_value(*delegate)) -
          DEMUX_LIST_C(list)->offset +
          DEMUX_LIST_C(list)->stride-1) / DEMUX_LIST_C(list)->stride;
}

static ava_value ava_list_proj_demux_list_index(
  ava_value list, size_t ix
) {
  const ava_list_value*restrict delegate = &DEMUX_LIST_C(list)->delegate;
  return delegate->v->index(ava_list_value_to_value(*delegate),
                            DEMUX_LIST_C(list)->offset +
                            ix * DEMUX_LIST_C(list)->stride);
}

ava_value ava_list_proj_group(ava_value delegate, size_t group_size) {
  ava_list_proj_group_list* this;
  size_t num_groups =
    (ava_list_length(delegate) + group_size-1) / group_size;

  this = ava_alloc(sizeof(ava_list_proj_group_list) +
                   sizeof(AO_t) * num_groups);
  this->delegate = ava_list_value_of(delegate);
  this->group_size = group_size;
  this->num_groups = num_groups;

  return (ava_value) {
    .attr = (const ava_attribute*)&ava_list_proj_group_list_impl,
    .r1 = { .ptr = this },
    .r2 = { .ulong = NULL }
  };
}

static size_t ava_list_proj_group_value_value_weight(ava_value val) {
  const ava_list_value*restrict delegate = &GROUP_LIST_C(val)->delegate;

  return ava_value_weight(ava_list_value_to_value(*delegate));
}

static size_t ava_list_proj_group_list_length(ava_value list) {
  return GROUP_LIST_C(list)->num_groups;
}

static ava_value ava_list_proj_group_list_index(
  ava_value list, size_t ix
) {
  ava_list_proj_group_list*restrict this = GROUP_LIST_C(list);
  const ava_list_value*restrict ret;
  ava_list_value tmp;
  size_t begin, end, delegate_length;

  assert(ix < this->num_groups);

  ret = (const ava_list_value*restrict)AO_load_acquire_read(this->groups + ix);
  if (ret) return ava_list_value_to_value(*ret);

  /* Group not yet cached, create it */
  begin = ix * this->group_size;
  end = begin + this->group_size;
  delegate_length = this->delegate.v->length(ava_list_value_to_value(this->delegate));
  if (end > delegate_length) end = delegate_length;

  tmp = ava_list_value_of(
    this->delegate.v->slice(ava_list_value_to_value(this->delegate), begin, end));
  ret = ava_clone(&tmp, sizeof(tmp));
  /* Save in cache. If multiple threads hit this index simultaneously, they may
   * produce physically different results and write the cache multiple times.
   * That's ok.
   */
  AO_store_release_write(this->groups + ix, (AO_t)ret);

  return ava_list_value_to_value(*ret);
}

ava_value ava_list_proj_flatten(ava_value list) {
  ava_value accum;
  size_t i, n;

  if ((const ava_attribute*)&ava_list_proj_group_list_impl == list.attr)
    return ava_list_value_to_value(GROUP_LIST_C(list)->delegate);

  n = ava_list_length(list);
  if (0 == n)
    return ava_empty_list;

  accum = ava_list_index(list, 0);

  for (i = 1; i < n; ++i)
    accum = ava_list_concat(accum, ava_list_index(list, i));

  return accum;
}
