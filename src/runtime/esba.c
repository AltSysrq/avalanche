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
#include <stdlib.h>
#include <string.h>

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "-esba.h"

/*

  The Eager Shallow-Binding Array was largely inspired by "Fully Persistent
  Arrays for Efficient Incremental Updates and Voluminous Reads"
  (http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.34.1317); however,
  it is not an implementation of that. Familiarity with the original paper only
  helps by virtue of being a similar topic. The documentation below assumes no
  familiarity, except with the principle of persistent data structures
  themselves.

  Most persistent data structures are "deep-binding", which means that writers
  operate by allocating new pieces of a data structure to represent the write,
  copying part of the original structure when reorganisation is necessary. This
  means that readers are always completely undisturbed. However, in the common
  case of only the latest version of the structure ever being used, deep
  binding brings substantial performance penalties, especially since every
  write results in a heap allocation.

  In "shallow binding", there is a concept of a "current context", which is the
  only efficiently usable part of the data structure, and represents exactly
  one version of the structure. Other versions are generally stored as linear
  change-logs that can be used to mutate the context in-place into another
  version. The ESBA is "eager" in that it *immediately* updates the current
  context, pushing readers out of the way (as opposed to recording a change and
  forcing it when necessary). In the very common case of there being only one
  live reference to the structure, this allows all operations to act as if the
  ESBA is a simple dynamic array.

  Somewhat more like a deep-binding structure, the ESBA does *not* revert to
  old versions in-place; rather, readers use the reverse-change-log left by
  writers to reconstruct a fresh array representing their version. This avoids
  shimmering issues that could arise, especially in concurrent environments,
  with simple shallow binding.

  ---

  An ESBA is allocated with a fixed size, divided into three segments. When the
  size becomes insufficient, a fresh ESBA is allocated, with a capacity equal
  to twice the length of the live segment.

       Grows up -->                             <-- Grows down
    ___Live_Segment___  ___Dead_Segment___  _______Undead_Segment_______
   /                  \/                  \/                            \
  [LLLLLLLLLLLLLLLLLLLL--------------------UUUUUUUUUUUUUUUUUUUUUUUUUUUUUU]
  |\________/|         ^ true length       ^ true version      |
  |  access  |                             \__________________/|
  |----------+----\                            rollback data   | version
  | head          |                                            |
  ava_esba_handle-|--------------------------------------------/
  |               |
  | handle        | length
  |               |
  ava_esba--------/

  The segments are used as follows:

  - The Live Segment is a flat array of elements, with no intervening data.

  - The Dead Segment is unused and uninitialised; its only purpose is for the
    other segments to grow into it.

  - The Undead Segment stores a reverse change-log which allows readers of
    older versions. Each element in the undead section contains both an array
    element and the index where that element is found.

  A handle stores three pieces of information:

  - A pointer to the array itself.

  - The offset of the handle's version within the undead segment.

  - The maximum length of any reference to the handle.

  A reference (ava_esba) holds a pointer to a handle and a length.

  A handle is "stale" if it does not point to the latest version of the array.
  Stale handles cannot be used to perform set or read operations, but may still
  append. A handle or reference is "truncating" if its length is less than the
  true length of the array; a truncating reference cannot be used for appending
  data, but may be used for set operations. While theoretically neither
  truncating nor stale implies the other, here we forbid all writes to stale or
  truncating handles, so that the max_length field need to be treated
  specially. (Ie, a conflicting reader and appender would both first clone the
  array with the same max_length, whereas allowing a stale append could cause a
  reader to clone with an out-of-date max_length.)

  An array may not have more than one concurrent writer; if a second writer
  comes along while a first is already operating, the second must resort to
  creating a private copy of the array first.

  Before a writer can modify an array, it must first validate that it is
  possible for it to apply such modifications (eg, if it wants to append, it
  must not have a truncating reference). It then does an atomic CaS on the size
  of the dead segment to reserve the space it needs, only succeeding if the old
  value of the dead segment size was what the true length and true version of
  the array implied, and obviously only if there is actually space for the
  operation.

  Append operations simply write elements to the end of the live segment, then
  do a write-release to the true length and the max length of their handle.

  Set operations first copy the old value of the element to set into the undead
  section, then update the true version with a write release. After that the
  writer may freely modify the element in the live segment without
  synchronisation. Sets always require the allocation of a new handle, since
  the version changes.

  Readers initially simply perform a read-acquire on their handle's head
  reference, and then procede under the optimistic assumption that their handle
  is not stale; it is not worth checking, since it could spontaneously become
  stale without notice. When a reader has completed reading what it wants, it
  checks that the version on the handle (read-acquire) matches the true version
  of the array; if it does not, the reader discards its results and procedes to
  the rollback path.

  For rollback, a reader first allocates a new array, sized for a capacity
  equal to twice the maximum length on its handle. It then uses the following
  procedure to reproduce the desired version of the array:

  - The reader spin-waits until the handle's head and version both point into
    the same array.

  - The current true version of the source is read-acquired.

  - The accessible part of the live segment of the source is copied into the
    live segment of the new array.

  - The true version is read-acquired again; if it differs from the first step,
    go back to the copy step. (This indicates that a writer mutated the live
    segment while it was being copied.)

  - Every element in the source undead segment between the true version read
    above and the handle version are iterated, starting at the true version.
    For each element whose index is less than the maximum length of the handle,
    the corresponding element in the live segment is overwritten with the
    undead segment's element.

  - The handle's head pointer is CaSed (full barrier) with the new array. If
    this fails, the reader assumes that another reader has already completed
    rollback and returns to the normal read process.

  - The handle's version pointer is write-released to the end of the undead
    segment of the new array.

  - The reader goes back to the normal read process.

  Note that readers that try to read the same stale handle simultaneously may
  waste some work both performing the rollback, but the end result will be
  nonetheless correct.

  There is also the possibility of a reader seeing a handle with head and
  version pointers pointing into different arrays; this will cause the reader
  to make a needless rollback, but is otherwise harmless. The only possible
  point of issue is the rollback stage, which would traverse arbitrarily large
  sections of memory in such cases; this, this part of the process must
  spinwait in the extremely rare case where this occurs.

 */

static inline void static_assert_ao_t_can_hold_pointer(void) AVA_UNUSED;
static inline void static_assert_ao_t_can_hold_pointer(void) {
  switch (0) {
  case 0:
  case sizeof(AO_t) >= sizeof(void*):;
  }
}

/**
 * Data within an ESBA is managed in terms of pointer-sized blocks rather than
 * bytes.
 *
 * This definition is for clarity, since having void* everywhere would make it
 * difficult to tell what is actually supposed to be dereferenced and what is
 * just there to stand for a pointer-sized block.
 */
typedef struct { void* p; } pointer;

#define sizeof_ptr(x) (sizeof(x) / sizeof(pointer))

typedef struct {
  /* Constant configuration */
  /**
   * The size of each element, in terms of pointers.
   */
  size_t element_size;
  /**
   * The weight function for the elements in this array.
   */
  size_t (*weight_function)(const void*restrict, size_t);
  /**
   * The allocator used to allocate this array, and copies derived therefrom.
   *
   * Note that while this structure does have pointers, they always point into
   * the same allocation in which they reside, so it is safe for the array to
   * be allocated with ava_alloc_atomic() if the elements are atomic.
   */
  void* (*allocator)(size_t);

  /**
   * The size of the dead segment, in pointers.
   *
   * Ordinarily, this is equal to (true_version - live_tail). However, writers
   * first reduce this value atomically, before updating the others, so that
   * they can obtain write access atomically. (Otherwise we'd need to somehow
   * atomically work with both live_tail and true_version).
   */
  AO_t dead_segment_size;
  /**
   * A pointer to the first element beyond the live segment.
   *
   * The true length is equal to (live_tail - data) / element_size.
   *
   * This field is only conceptual; nothing ever uses it.
   */
  /* AO_t (* pointer* *) live_tail; */
  /**
   * A pointer to the most recent version written.
   *
   * Initially, this points to the end of the array itself, as there are no
   * rollback operations.
   */
  AO_t /* pointer* */ true_version;
  /**
   * A pointer to one past the end of the data array.
   *
   * This permits readers to tell whether their version pointer actually lies
   * within this allocation.
   *
   * This is written once when the array is allocated.
   */
  const pointer* end;

  /**
   * The cumulative weight of this array.
   */
  size_t weight;

  /**
   * The data in this array. The live segment begins here and runs to
   * live_tail. The undead segment begins at true_version and runs to the end
   * of the allocation.
   */
  pointer data[];
} ava_esba_array;

struct ava_esba_handle_s {
  /**
   * The userdata on the ESBA. This needs to be the first element in the
   * structure.
   */
  void* userdata;
  /**
   * A pointer to the array backing this handle.
   *
   * This may only be altered when the handle is stale.
   */
  AO_t /* ava_esba_array* */ head;
  /**
   * A pointer to the undead segment element representing the version of this
   * handle.
   *
   * In the case of rollback, this is the final element to apply before
   * rollback is complete.
   *
   * In certain race conditions, this may not actually point into the array to
   * which head points.
   *
   * This may only be altered when the handle is stale.
   */
  AO_t /* const pointer*restrict */ version;
  /**
   * The maximum length of any reference to this handle.
   *
   * Only appenders may alter this value, and it may only be read if the handle
   * is stale or if the owner has write access to the array (since either
   * implies there are no in-flight appenders that might mutate it).
   */
  AO_t max_length;
};

/**
 * Element type stored in the undead segment.
 */
typedef struct {
  /**
   * The array index of this element.
   */
  size_t index;
  /**
   * The data which used to be at index within the array but were previously
   * overwritten.
   */
  pointer data[];
} ava_esba_undead_element;

/**
 * The value of the volatile fields in an ava_esba_handle.
 *
 * max_length is excluded, since it is only read in very particular
 * circumstances.
 *
 * @see ava_esba_handle_read()
 * @see ava_esba_handle_read_consistent()
 */
typedef struct {
  ava_esba_array* head;
  const ava_esba_undead_element*restrict version;
} ava_esba_handle_value;

static ava_esba_array* ava_esba_array_new(
  size_t element_size,
  size_t initial_capacity,
  size_t (*weight_function)(
    const void*restrict, size_t),
  void* (*allocator)(size_t));

static ava_esba_handle* ava_esba_handle_new(
  void* userdata,
  ava_esba_array* array, AO_t version, size_t max_length);

/**
 * Reads the head and version fields from the given handle into a local value.
 *
 * The returned handle value is not guaranteed to be consistent; ie, it could
 * have head and version pointing into different arrays.
 *
 * @see ava_esba_handle_read_consistent()
 */
static ava_esba_handle_value ava_esba_handle_read(
  const ava_esba_handle* handle);
/**
 * Like ava_esba_handle_read(), but spins until the result is consistent.
 */
static ava_esba_handle_value ava_esba_handle_read_consistent(
  const ava_esba_handle* handle);
/**
 * Returns whether the given handle value is consistent, ie, whether the
 * version is actually within the array.
 */
static inline ava_bool ava_esba_handle_is_consistent(
  ava_esba_handle_value val);
/**
 * Returns whether the given handle value is stale.
 *
 * The returned value is only useful for optimistic reads; writers must test
 * staleness by the atomic CaS to gain write access to the array.
 */
static inline ava_bool ava_esba_handle_is_stale(
  ava_esba_handle_value val);

/**
 * Creates an ava_esba_array holding the version represented by the given
 * handle into a new, fresh array.
 *
 * @param consistent_handle The handle whose logical value to copy; may be
 * stale.
 * @param length The number of pointers to copy out of the original array.
 * @param capacity The number of pointers for which to allocate space.
 */
static ava_esba_array* ava_esba_handle_copy_out(
  const ava_esba_handle* handle, size_t length, size_t capacity);

/**
 * Builds a new array to use as a backing for the given handle, which is
 * assumed to be stale or truncating.
 *
 * When the function returns, handle is no longer stale.
 *
 * @param handle The handle whose contents are to be rewritten to a fresh
 * array.
 * @return The value of the freshened handle.
 */
static ava_esba_handle_value ava_esba_handle_make_fresh(
  ava_esba_handle* handle);

/**
 * Mutates *esba as necessary to permit mutation of the ESBA. *val is set to
 * a consistent read of the result.
 *
 * Note that it is the responsibility of the caller to adjust the live/undead
 * segment boundaries to permit future writes.
 *
 * @param esba The ESBA to make mutable.
 * @param val On return, set to a consistent read of esba->handle.
 * @param required_space The amount of space, in pointers to reserve by
 * removing it from the dead segment.
 * @param capacity_growth Should the array need to be copied, the number of
 * extra elements to ensure fit within the capacity of the new array.
 */
static void ava_esba_make_mutable(
  ava_esba*restrict esba,
  ava_esba_handle_value*restrict val,
  size_t required_space,
  size_t capacity_growth);

ava_esba ava_esba_new(size_t element_size,
                      size_t initial_capacity,
                      size_t (*weight_function)(
                        const void*restrict, size_t),
                      void* (*allocator)(size_t),
                      void* userdata) {
  assert(0 == element_size % sizeof(pointer));

  element_size /= sizeof(pointer);
  ava_esba_array* array = ava_esba_array_new(
    element_size, initial_capacity * element_size,
    weight_function, allocator);

  ava_esba_handle* handle = ava_esba_handle_new(
    userdata, array, array->true_version, 0);

  return (ava_esba) { .handle = handle, .length = 0 };
}

static ava_esba_array* ava_esba_array_new(
  size_t element_size, size_t initial_capacity,
  size_t (*weight_function)(const void*restrict, size_t),
  void* (*allocator)(size_t)
) {
  ava_esba_array* array = (*allocator)(
    sizeof(ava_esba_array) +
    initial_capacity * element_size * sizeof(pointer) +
    /* Always ensure there's space for at least one undead element */
    sizeof_ptr(ava_esba_undead_element));

  array->element_size = element_size;
  array->weight_function = weight_function;
  array->allocator = allocator;
  array->dead_segment_size = initial_capacity * element_size +
    sizeof_ptr(ava_esba_undead_element);
  array->end = array->data + initial_capacity * element_size +
    sizeof_ptr(ava_esba_undead_element);
  array->true_version = (AO_t)array->end;
  array->weight = 0;

  return array;
}

static ava_esba_handle* ava_esba_handle_new(
  void* userdata,
  ava_esba_array* array, AO_t version, size_t max_length
) {
  ava_esba_handle* handle = AVA_NEW(ava_esba_handle);
  handle->userdata = userdata;
  handle->head = (AO_t)array;
  handle->version = version;
  handle->max_length = max_length;
  return handle;
}

const void* ava_esba_access(ava_esba esba, ava_esba_tx* tx) {
  /* Read the handle without ensuring it is consistent. If it is inconsistent,
   * it will be detected as stale anyway.
   */
  ava_esba_handle_value val = ava_esba_handle_read(esba.handle);

  /* If stale, rebuild the array to make it accessible. */
  if (AVA_UNLIKELY(ava_esba_handle_is_stale(val)))
    val = ava_esba_handle_make_fresh(esba.handle);

  /* The handle wasn't stale in the above check. It might be now, or at any
   * time later, which is why ava_esba_check_access() exists, so we don't need
   * to worry about that here. Just give the caller their pointer and
   * transaction context now.
   */

  tx->ptr = val.version;
  return val.head->data;
}

ava_bool ava_esba_check_access(ava_esba esba, const void* accessed,
                               ava_esba_tx tx) {
  /* Read the current handle. We don't need it to be consistent; even if we
   * waited for it to become consistent, the below check would fail anyway
   * since that it had been in an inconsintent state guarantees that at least
   * one of the fields on the handles changed.
   */
  ava_esba_handle_value val = ava_esba_handle_read(esba.handle);

  /* The transaction fails if:
   *
   * - The array head changed. This indicates that something changed the
   *   current version of the array and rendered the handle stale, then another
   *   reader freshened the handle to a new array.
   *
   * - The version on the handle has changed. This happens for the same reason
   *   as the array head changing. This check is necessary because it is
   *   possible for a read of the handle to read the head pointer before a
   *   writer changes both fields, and then read the version pointer after.
   *
   * - The handle is stale. This indicates that something has modified the
   *   current version of the array since the call to access(), and nothing has
   *   come along since to freshen the handle.
   *
   * In other words, the transaction passes if:
   *
   * - The handle is completely unchanged, except possibly max_length, which we
   *   don't care about.
   *
   * - The true version of the array has not changed. This is implied by the
   *   stale test and the above condition; if the true version did change,
   *   either the handle was unchanged and thus became stale, or something
   *   freshened the then-stale handle and thus changed it.
   */
  return val.head->data == accessed && val.version == tx.ptr &&
    !ava_esba_handle_is_stale(val);
}

static ava_esba_handle_value ava_esba_handle_read(
  const ava_esba_handle* handle
) {
  ava_esba_handle_value val;

  /* Read both fields atomically. Order doesn't matter, because the two reads
   * as a whole don't take place atomically either way.
   *
   * The second read is a read-acquire so that both pointers can safely be
   * traversed afterward.
   */
  val.head = (ava_esba_array*)(AO_load(&handle->head));
  val.version = (const ava_esba_undead_element*)(
    AO_load_acquire_read(&handle->version));

  return val;
}

static inline ava_bool ava_esba_handle_is_consistent(
  ava_esba_handle_value val
) {
  return (void*)val.version >= (void*)val.head &&
    (void*)val.version <= (void*)val.head->end;
}

static inline ava_bool ava_esba_handle_is_stale(
  ava_esba_handle_value val
) {
  /* A handle is stale simply if its version pointer does not match the true
   * version of the array.
   *
   * A simple non-barrier load is sufficient here since the caller is a
   * reader using optimistic concurrency control (so it won't behave
   * incorrectly if given an incorrect result). Writers don't test for
   * staleness this way.
   */
  return (void*)val.version != (void*)AO_load(&val.head->true_version);
}

static ava_esba_handle_value ava_esba_handle_read_consistent(
  const ava_esba_handle* handle
) {
  ava_esba_handle_value val;

  val = ava_esba_handle_read(handle);

  while (AVA_UNLIKELY(!ava_esba_handle_is_consistent(val))) {
#if defined(__GNUC__) && defined(__SSE2__)
    __builtin_ia32_pause();
#endif
    val = ava_esba_handle_read(handle);
  }

  return val;
}

static ava_esba_handle_value ava_esba_handle_make_fresh(
  ava_esba_handle* handle
) {
  for (;;) {
    /* Read a consistent view of the handle, and stop if it's already fresh
     * meanwhile. This happens when the loop wraps, or if someone else won the
     * race to this point.
     */
    ava_esba_handle_value old = ava_esba_handle_read_consistent(handle);
    if (!ava_esba_handle_is_stale(old))
      return old;

    /* Make a copy reflecting this handle's version.
     *
     * We can load the length without a memory barrier, since either (a) there
     * are no writers, because the handle is still stale, or (b) there is a
     * writer, implying the handle is no longer stale, and the CaS below will
     * fail, so it doesn't matter what we copy. (The only possibility is that
     * we read too small a value, which is safe.)
     */
    size_t length = AO_load(&handle->max_length);
    ava_esba_array* copy = ava_esba_handle_copy_out(
      handle, length * old.head->element_size,
      length*2 * old.head->element_size);

    /* Successfully built an array with the desired version, install it into
     * the handle.
     *
     * If the CaS fails, it means someone else beat us to this point and
     * already put a fresh array there.
     *
     * The CaS is a write-release barrier since readers may immediately begin
     * traversing that pointer into the new array. This may happen before we
     * update the version; such readers will see the handle as stale, and will
     * attempt to freshen it themselves, but then find it is actually fresh
     * after spinning until the version is updated.
     *
     * The version update is a write-release barrier so that it follows the
     * head update.
     */
    if (AO_compare_and_swap_release_write(
          &handle->head, (AO_t)old.head, (AO_t)copy)) {
      AO_store_release_write(&handle->version, (AO_t)copy->end);
    }
  }
}

static ava_esba_array* ava_esba_handle_copy_out(
  const ava_esba_handle* handle, size_t length, size_t capacity
) {
  /* Get a current snapshot of the handle, since we need to read from both the
   * head and the version.
   */
  ava_esba_handle_value old = ava_esba_handle_read_consistent(handle);

  /* Allocate a new, private array to use as a destination. */
  ava_esba_array* dst = ava_esba_array_new(
    old.head->element_size, capacity,
    old.head->weight_function, old.head->allocator);
  dst->dead_segment_size = capacity - length;

  /* Do a trivial copy of the main data.
   *
   * This will copy some incorrect values, as well as possibly garbage if their
   * are concurrent writers. The rollback loop below will correct these
   * anomalies.
   */
  memcpy(dst->data, old.head->data, sizeof(pointer) * length);

  /* Get the current true version of the array.
   *
   * We don't need to worry about new versions being added after this read,
   * since they won't affect the copy we made above; versions are always
   * created *before* the data array is mutated, so any mutations to the data
   * array will already have undo entries available.
   *
   * Load-acquire to ensure we see the data within.
   */
  const ava_esba_undead_element* start =
    (const ava_esba_undead_element*)AO_load_acquire_read(
      &old.head->true_version);
  const ava_esba_undead_element* end = old.version;

  /* Replay the undo log from newest to oldest, since older entries take
   * precedence.
   *
   * Conceptually, this is for (cur = start; cur != end; ++cur), but the
   * pointer update is complicated by the variable-sized structure.
   */
  const ava_esba_undead_element* cur;
  for (cur = start; cur != end;
       cur = (const ava_esba_undead_element*)(
         ((pointer*)cur) + old.head->element_size +
         sizeof_ptr(ava_esba_undead_element))) {
    /* The undo log may have entries from beyond the length our version knows
     * about; ignore them.
     */
    if (cur->index < length) {
      /* Copy the undo data into the new array. Older undo entries overwrite
       * newer undo entries, which is what we want.
       *
       * No barrier required since we already load-acquired the pointer to this
       * data, and the destination is still unique to the current thread.
       */
      memcpy(dst->data + cur->index * dst->element_size,
             cur->data,
             dst->element_size * sizeof(pointer));
    }
  }

  /* The destination array is now fully consistent; calculate it's weight and
   * we're done.
   */
  dst->weight = (*dst->weight_function)(dst->data, length);
  return dst;
}

static void* ava_esba_start_append_impl(ava_esba* esba, size_t num_elements,
                                        ava_esba_handle_value* val) {
  size_t element_size = ava_esba_handle_read(esba->handle).head->element_size;
  size_t old_length = esba->length;

  ava_esba_make_mutable(esba, val, element_size * num_elements, num_elements);
  esba->length += num_elements;
  return val->head->data + old_length * element_size;
}

void* ava_esba_start_append(ava_esba* esba, size_t num_elements) {
  ava_esba_handle_value val;
  return ava_esba_start_append_impl(esba, num_elements, &val);
}

static void ava_esba_finish_append_impl(ava_esba esba, size_t num_elements,
                                        ava_esba_handle_value val) {
  size_t old_length = esba.length - num_elements;
  size_t new_length = esba.length;
  size_t element_size = val.head->element_size;

  /* Update the handle with the new maximum length.
   *
   * Write-release since a reader on another thread could act on the larger
   * length if the handle goes stale.
   */
  AO_store_release_write(&esba.handle->max_length, new_length);
  /* Update the weight; this can be incremental and non-atomic since we're the
   * only writer.
   */
  val.head->weight += (*val.head->weight_function)(
    val.head->data + old_length * element_size, num_elements);
}


void ava_esba_finish_append(ava_esba esba, size_t num_elements) {
  ava_esba_handle_value val = ava_esba_handle_read(esba.handle);
  ava_esba_finish_append_impl(esba, num_elements, val);
}

ava_esba ava_esba_append(ava_esba esba, const void*restrict data,
                         size_t num_elements) {
  ava_esba_handle_value val;
  size_t element_size = ava_esba_handle_read(esba.handle).head->element_size;

  void* dst = ava_esba_start_append_impl(&esba, num_elements, &val);

  /* Write directly into the newly live area; nothing will be looking at it for
   * now.
   */
  memcpy(dst, data, num_elements * element_size * sizeof(pointer));

  ava_esba_finish_append_impl(esba, num_elements, val);

  return esba;
}

ava_esba ava_esba_set(ava_esba esba, size_t index, const void*restrict data) {
  ava_esba_handle_value val;
  size_t element_size = ava_esba_handle_read(esba.handle).head->element_size;

  ava_esba_make_mutable(&esba, &val,
                        element_size + sizeof_ptr(ava_esba_undead_element),
                        element_size + sizeof_ptr(ava_esba_undead_element));

  /* Obtain the undo element. This is basically val.version-1, but is a but
   * obtuse due to the variable-sized structure.
   */
  ava_esba_undead_element* undo = (ava_esba_undead_element*)(
    ((pointer*)val.version - element_size -
     sizeof_ptr(ava_esba_undead_element)));
  /* Write the undo entry */
  undo->index = index;
  memcpy(undo->data, val.head->data + index * element_size,
         element_size * sizeof(pointer));
  /* Update the true version.
   *
   * This will begin directing readers to the undo entry we created above, so
   * it needs to be write-release so they can see its contents.
   */
  AO_store_release_write(&val.head->true_version, (AO_t)undo);
  /* Readers at this point will will evacuate their data to new arrays, and can
   * correctly reconstruct the element we want to overwrite using the undo
   * element created above. Thus, we can simply overwrite the element without
   * any barrier or atomicity.
   */
  memcpy(val.head->data + index * element_size, data,
         element_size * sizeof(pointer));
  /* Update the weight. Note that we still pay for the weight of the old datum
   * since it lives in the undo log.
   *
   * No atomicity or barrier needed since we're the only writer.
   */
  val.head->weight += (*val.head->weight_function)(data, 1);

  /* Create a new handle for the new version. */
  esba.handle = ava_esba_handle_new(
    esba.handle->userdata, val.head, (AO_t)undo, esba.length);

  return esba;
}

static void ava_esba_make_mutable(
  ava_esba*restrict esba,
  ava_esba_handle_value*restrict val,
  size_t required_space,
  size_t capacity_growth
) {
  *val = ava_esba_handle_read_consistent(esba->handle);

  /* Calculate what the reference thinks the dead segment size is and what the
   * new size will be, then try to take ownership.
   *
   * This implicitly checks for stale and truncating references, since they
   * will caculate the incorrect current dead segment size.
   */
  size_t current_capacity = val->head->end - val->head->data;
  size_t believed_live_size = esba->length * val->head->element_size;
  size_t believed_undead_size = val->head->end - (pointer*)val->version;
  size_t believed_dead_size =
    current_capacity - believed_live_size - believed_undead_size;
  size_t new_dead_size = believed_dead_size - required_space;
  size_t new_capacity = esba->length + capacity_growth;

  /* Atomically ensure that there is sufficient space and that this reference
   * refers to the latest version of the array.
   *
   * No barrier is needed on the CaS because (a) if it succeeds, that means we
   * already have the latest version, so the entire structure has already been
   * made visible, and (b) if it fails, ava_esba_handle_copy_out() performs the
   * appropriate barriers itself.
   */
  if (AVA_UNLIKELY(required_space > believed_dead_size) ||
      AVA_UNLIKELY(!AO_compare_and_swap(
                     &val->head->dead_segment_size,
                     believed_dead_size, new_dead_size))) {
    /* Either not enough space, stale or truncating reference, or conflicting
     * write; need to copy.
     */
    val->head = ava_esba_handle_copy_out(
      esba->handle, believed_live_size,
      new_capacity * 2 * val->head->element_size);
    val->version = (const ava_esba_undead_element*)val->head->end;
    esba->handle = ava_esba_handle_new(
      esba->handle->userdata,
      val->head, (AO_t)val->version, esba->length);

    /* Set the correct dead size */
    val->head->dead_segment_size -= required_space;
  }
}

size_t ava_esba_weight(ava_esba esba) {
  return ava_esba_handle_read(esba.handle).head->weight;
}
