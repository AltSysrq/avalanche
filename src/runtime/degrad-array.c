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
#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "-degrad-array.h"

/*
  The degradation array is a semi-persistent data structure which provides
  high-performance implementations of the most common list operations, in
  particular append, indexing, and in-place replace.

  This array is decomposed as follows:

  - elements. An element is the minimum number of bytes guaranteed to be
    contiguous in any access.

  - zones. A whole number of elements, used for mutation tracking.

  - pages. A whole number of zones, used for managing allocation.

  All mutations on a degradation array increment a timestamp, t_A. This
  timestamp is used to provide persistence; every reference to a degradation
  array carries its own timestamp t^R. Writers may only mutate a degradation
  array if their t^R == t_A, *and* they succeed on an atomic compare-and-swap
  of t_A = t^R -> t^R+1; writers which fail this check must first copy part of
  the array structure in order to mutate it.

  At least 50% of the elements in a degradation array are reachable from the
  "primary backing", an append-only flat array of elements. (It is only flat in
  concept; the implementation here splits it into pages to limit the
  reallocation costs in copy-the-whole-array cases, and to permit the pages to
  be allocated as "atomic" objects.)

  The capacity of the primary backing is termed c_A; the used length is l_A.
  Each reference additionally caries the number of elements it considers
  initialised within the primary backing, l^R. Note that even if t_A==t^R, l_A
  may be greater than l^R in some cases, as described below.

  The degradation array is logically split into a number of degradation zones.
  Initially, every zone is inactive. When an in-place mutation is to occur, the
  zone which contains that index becomes active at the timestamp of the
  mutation, if it was inactive. An active degradition zone references a hash
  table of edits, each edit being a triple of (index,timestamp,data) where
  index is the offset of the element within the zone; timestamp is the
  timestamp of the edit; and data is the new data of the full element. The hash
  of a triple is simply the index; collisions are handled via linear probing.

  When the hash table of a degradation zone becomes full, it is rehashed to
  contain only the latest value of each index and the new table written back to
  the degradation zone. For this to be efficient, the table of hash table
  pointers for the degradation zones is itself stored in a degradation array.

  Every time an element is mutated by writing it into a degradation zone, a
  dead element counter on the array is incremented. If it exceeds the number of
  elements in the array, the array is completely rebuilt with all degration
  zones flattened into a new primary backing.

  Appending is done simply by increasing l_A and writing data directly into the
  primary backing after resizing it if necessary. Appends which precede l_A are
  turned into replacements.

  Reading is performed by first checking whether t^R is greater than or equal
  to the activation time of the zone containing the selected element. If not,
  the degradation zone is inactive for that reference, and the primary backing
  is accessed directly. Otherwise, the degradation hash table table is
  consulted to locate the hash table for that timestamp, and the hash table is
  searched for the entry matching the selected element whose timestamp is less
  than or equal to t^R. If one is found, the element within the hash table is
  returned; otherwise, the primary backing is accessed for the element data.

  Readers do not use atomic operations or memory barriers. This is accomplished
  by initialising all stored timestamps to "infinity"; if a writer overwrites a
  timestamp, it is still guaranteed to be greater than the t^R used by any
  reference, so the reader makes the same decesion regardless of which value it
  sees, and in all cases only traverses memory already initialised at or before
  its own t^R.

  As already mentioned, this implementation splits the array into pages. Each
  page actually manages its own degradation tables, but shares a t_A with its
  original parent structure.

  ----------------

  Complexity analysis:

  In all figures below, n refers to the number of items in the array before or
  after the operation, whichever is greater. m refers to the number of items
  being mutated. z is the zone size.

  Append. In common cases, the array behaves like a traditional dynamic array,
  providing amortised O(m) append time (O(1) per element). In certain cases, it
  may need to copy its backing more frequently, for O(n+m) worst case. Note
  that the coefficient on n is small since the array is broken into pieces, but
  as a fixed-height tree, it cannot reduce n to log(n).

  Index. If all elements have been mutated, each access has O(1) constant
  overhead since there is only one level in the degradation array. (Any more
  and the array would be rebuilt.) On the other side, with n mutations on one
  element, accessing most elements has no overhead (O(1)); accessing those in
  the mutated zone traverses log(n) degradation levels. Thus there is an
  average access time of ((n-z) + z/n * log(n))/n for each element. The limit
  of this as n approaches infinity is (n + 0 * log(n))/n = n/n = 1, so index
  time is always amortised O(1). Non-amortised, indexing is O(1) in the average
  case and O(log(n)) in the worst case for an individual index.

  Replace. Replace involves the same operations as an index, except that every
  n replacements requires an O(n) rebuild of the array. Thus amortised
  complexity is O(m). Non-amortised complexity is O(m) average-case, with
  O(m*log(n)) worst-case when no rebuilding occurs, and O(n+m*log(n)) when any
  mutation triggers a rebuild. In the case of mutating an older version of a
  degradation array, every mutation could rebuild the array (ie, the previous
  version is one mutation away from a rebuild), which results in O(n+m*log(n))
  complexity in pathological cases.
 */

#define T_INFINITY (~(ava_darray_timestamp)0)

typedef AO_t ava_darray_timestamp_atomic;
typedef struct ava_darray_root_s ava_darray_root;
typedef struct ava_darray_page_s ava_darray_page;

/**
 * An extra level of indirection into the root.
 *
 * This serves two purposes:
 * - It expands the 32-bit length from the reference to 64 bits on systems with
 *   a 64-bit length.
 * - It permits efficient slicing by permitting either not rebuilding the root
 *   at all, or by accounting for page misalignment so that the pages
 *   themselves need not be rebuilt.
 */
struct ava_darray_handle_s {
  /**
   * The root for the degradation array.
   */
  const ava_darray_root*restrict root;
  /**
   * The index to subtract from all incoming indices and from the reported
   * length.
   */
  size_t base_index;
  /**
   * The length to add to the 32-bit length on the external reference.
   */
  size_t base_length;
};

/**
 * A reference from a root to a page.
 *
 * This is necessary since pages might not have the same timestamp as the root
 * that references them.
 */
typedef struct {
  /**
   * The page itself.
   */
  const ava_darray_page*restrict page;
  /**
   * If not T_INFINITY, the page has a timestamp independent of the table.
   */
  ava_darray_timestamp foreign_timestamp;
} ava_darray_page_ref;

/**
 * The root of the fixed-height tree which holds all the data in the
 * degradation array.
 *
 * Conventions: All simple pointers to an ava_darray are const restrict.
 * Mutable instances are described by the ava_darray_*_root wrappers.
 */
struct ava_darray_root_s {
  /**
   * Details on the type of data this degradation array contains.
   */
  const ava_darray_spec* spec;

  /**
   * The timestamp used for obtaining exclusive write access.
   *
   * This is a heap-allocated pointer since pages also need to reference it to
   * indicate whether ownership of the root implies ownership of the page.
   */
  ava_darray_timestamp_atomic* timestamp;
  /**
   * The number of elements in the degradation array.
   */
  size_t length;
  /**
   * The length of the pages array.
   */
  size_t capacity_pages;

  /**
   * The pages in this array.
   *
   * If capacity_pages == 1, the only page might not be fully allocated (ie,
   * allocated_length < AVA_DARRAY_ELTS_PER_PAGE); otherwise, all pages are
   * fully allocated. Pages are allocated on-demand; readers must therefore
   * consider pages beyond the last implied to exist by their length to be
   * indeterminate. All pages but the last used are full; the final may have
   * unused space.
   */
  ava_darray_page_ref pages[];
};

/**
 * A wrapper to make semantics around ownership more type-safe.
 *
 * A ava_darray_appendable_root*restrict implies that there may be concurrent
 * readers, but that the owner of the pointer is the only writer. This permits
 * the writer to update the length field, and to perform appending operations
 * on the non-foreign pages.
 *
 * A const ava_darray_root*restrict can be promoted to an
 * ava_darray_appendable_root*restirct via a successful incrementing CaS of the
 * timestamp.
 */
typedef union {
  const ava_darray_root*restrict s;
  ava_darray_root* a;
} ava_darray_appendable_root;

/**
 * A wrapper to make semantics around ownership more type-safe.
 *
 * An ava_darray_unique_root*restrict implies that there is no concurrent
 * access to the array, nor to any non-foreign pages it references.
 *
 * A ava_darray_appendable_root*restrict can be promoted to an
 * ava_darray_unique_root*restrict if the timestamp is zero. Note that this
 * implies that a const ava_darray_root*restrict can *not* be promoted to an
 * ava_darray_unique_root*restrict without first copying the root (to reset its
 * timestamp to zero).
 */
typedef union {
  const ava_darray_root*restrict s;
  ava_darray_appendable_root a;
  ava_darray_root* u;
} ava_darray_unique_root;

/**
 * A single page in a degradation array.
 *
 * Conventions for page pointers are the same as those for roots.
 */
struct ava_darray_page_s {
  /**
   * The timestamp controlling write access to this page.
   */
  const ava_darray_timestamp_atomic* timestamp;

  /**
   * Indicates the times at which zones became degraded.
   *
   * Initialised to T_INFINITY.
   *
   * The only meaningful thing readers can do with values in this array is test
   * whether any given value is less than or equal to their own t^R; values
   * read are otherwise indeterminate.
   */
  ava_darray_timestamp degraded_zones[AVA_DARRAY_ZONES_PER_PAGE];
  /**
   * If there are any degraded zones, the degraded array used to store the hash
   * tables.
   *
   * If there are no degraded zones, the value of this field is indeterminate
   * to readers. Writers will observe it to be NULL and will initialise it
   * on-demand.
   */
  const ava_darray_root*restrict zones;
  /**
   * The number of elements within this page which are dead in the latest
   * version.
   */
  unsigned short dead_elts;
  /**
   * The number of elements the data array can hold.
   *
   * A single-page degradation array is permitted to have a page with a size
   * smaller than implied by AVA_DARRAY_ELTS_PER_PAGE; this handles that case,
   * indicating how large this page actually is.
   *
   * This is always a multiple of AVA_DARRAY_ELTS_PER_ZONE.
   */
  unsigned short capacity;
  /**
   * The approximate weight of the values in this page.
   */
  size_t weight;

  /**
   * The data associated with this page.
   */
  char*restrict data;
};

/**
 * Like ava_darray_appendable_root, but for pages.
 */
typedef struct {
  ava_darray_page a;
} ava_darray_appendable_page;

/**
 * Like ava_darray_unique_root, but for pages.
 */
typedef struct {
  ava_darray_page a;
} ava_darray_unique_page;

/**
 * An entry in a degraded zone hash table.
 */
typedef struct {
  /**
   * The timestamp at which this element was initialised. Initially T_INFINITY.
   *
   * Readers can do nothing with this value but check whether it is less than
   * or equal to their t^R. Its value is otherwise indeterminate. If it is not
   * less than or equal to t^R, the values of other fields in this struct are
   * indeterminate.
   */
  ava_darray_timestamp timestamp;
  /**
   * The index, relative to the degraded zone, of this element.
   */
  ava_uint index;
  /**
   * The data of the element stored here.
   *
   * This is declared as a pointer only to force proper alignment.
   */
  void* data[];
} ava_darray_ht_entry;

ava_darray_unique_root* ava_darray_root_new(
  const ava_darray_spec* spec,
  ava_darray_timestamp_atomic* timestamp,
  size_t capacity_pages);

ava_darray_unique_page* ava_darray_page_new(
  const ava_darray_timestamp_atomic* timestamp,
  unsigned short capacity_elts,
  size_t capacity_bytes);

ava_darray_appendable_root* ava_darray_root_promote_appendable(
  const ava_darray_root*restrict);
ava_darray_unique_root* ava_darray_root_promote_unique(
  ava_darray_appendable_root*restirct);

ava_darray ava_darray_new(const ava_darray_spec* spec,
                          size_t initial_capacity) {
  ava_darray_unique_root*restrict root = ava_darray_root_new(
    spec, ava_alloc_atomic(sizeof(ava_darray_timestamp_atomic)),
    (initial_capacity + AVA_DARRAY_ELTS_PER_PAGE - 1) /
    AVA_DARRAY_ELTS_PER_PAGE);

  /* If the requested capacity is less than a page, allocate the first one with
   * the requested capacity now.
   */
  if (initial_capacity < AVA_DARRAY_ELTS_PER_PAGE)
    root->u.pages[0] = &ava_darray_page_new(
      root->u.timestamp, initial_capacity,
      spec->elt_size * initial_capacity)->u;

  return (ava_darray) {
    .root = &root->u, .loff = 0, .toff = 0
  };
}
