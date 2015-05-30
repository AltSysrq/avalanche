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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/list-proj.h"
#include "avalanche/map.h"
#include "-esba.h"
#include "-esba-list.h"
#include "-hash-map.h"

#define ASCII9_COLLISION_THRESH 8
#define ASCII9_SIZE_THRESH (1<<24)
#define COLLISION_SHIFT_AMT 4
#define BME_BITS (sizeof(ava_ulong) * 8)
#define MIN_CAPACITY 8

/*

  The hash-map is essentially composed of six parts:

  - The keys array, an ESBA list containing every even-indexed list element.

  - The values array, an ESBA list containing every odd-indexed list element.

  - The index array, which is the hash table proper.

  - The hash cache, which is an array of hash values parallel to the keys
    array, to speed rehashing and collision handling.

  - The deletion bitmap, which tracks soft-deletions of elements.

  - The list-index table, which corrects list indices in the presence of soft
    deletions.

  KEYS and VALUES
  ---------------

  The keys and values are stored in parallel ESBA lists, ordered according to
  the order of the map. The use of ESBA lists has several advantages:

  - Appending of new elements can be done in constant time, usually with no
    heap allocations.

  - Replacement of elements (in the values array) are fairly cheap, though they
    do require heap allocations.

  - The ESBA takes care of all concurrency for us; there's no need to worry
    about races surrounding the hash map's content.

  - The ESBA list optimises away redundant values; eg, if all keys are strings,
    each key will only occupy an ava_datum's worth of space rather than a full
    ava_value. Another use case is mapping to the empty string to emulate a
    set, in which case values consume no space at all.

  The final point is the primary reason the keys and values are kept in
  separate lists --- frequently the keys or values are monomorphic, but not
  both.

  The keys array is append-only. Thus the split between the arrays also reduces
  copying costs when a stale ESBA needs to be evacuated.

  Cursors are simply indices into each of these ESBAs.

  INDEX ARRAY and HASH CACHE
  --------------------------

  The index array is the hash-table proper, which allows quickly locating
  values corresponding to particular keys.

  The index array is always a power of two in size so that a simple bitwise AND
  can be used to reduce a hash value to an index into the index array. Each
  element in the index array indicates a cursor into the keys and values lists,
  or else holds an indeterminate value greater than the observer's array
  length.

  Appends (ie, initialising elements which were previously unused) is performed
  without any synchronisation on the part of the readers. A writer must begin
  by atomically CaSing the entry count on the index array itself from the
  length the writer thinks it should be to the new value. If this succeeds, the
  writer knows there are no other writers, and may proceed. Otherwise, the
  writer must copy the index array, and reset any values greater than its own
  array length back to AVA_MAP_CURSOR_NONE before proceeding with the write
  against the copy. Writers may only edit entries whose value is
  AVA_MAP_CURSOR_NONE.

  Because readers and writers may operate on the same index array
  simultaneously, entries read by readers may be indeterminate; however, in
  such cases, the reader will still see _some_ value greater than their own
  array length, and can thus treat them as not-present.

  Collisions are handled via open addressing: If a slot is taken, the original
  hash (before the AND) is shifted right and another attempt is made. On each
  attempt, the number of prior attempts is added to the hash before the AND, so
  eventually the scheme reduces to simple linear probing.

  There are two hash functions used with the index array:

  ASCII9 hashing is used for the very common case of all keys being relatively
  small, printable, ASCII strings. It uses ava_ascii9_hash(), and is thus
  susceptable to malicious collisions, and has limited entropy with which to
  derive indices. Use thus is restricted to the following conditions:

  - All keys have ava_string_type as their attribute, and an ASCII9 string on
    their value.

  - There are fewer than ASCII9_SIZE_THRESH entries.

  - No key collides more than ASCII9_COLLISION_THRESH times.

  When any of the above conditions cease to apply, the table is rehashed using
  ava_value_hash(), which has none of these limitations.

  The index array is doubled in size whenever it reaches 75% capacity.

  The index array shares its structure with the hash cache, so that the two can
  share the same concurrency control.

  DELETION BITMAP
  ---------------

  Since deletion of one element in an ESBA is relatively expensive, deletions
  are implemented by keeping a bitmap of cursors which have been deleted, as
  well as a running count of the number of deletions.

  The bitmap itself is stored in an ESBA of ava_ulongs, and only expanded as
  far as necessary to record deleted elements.

  If the number of deletions exceeds half the length of the keys/values arrays,
  the hash-map is rebuilt to only contain live elements. Any rehashing event
  also does a full rebuild if any deleted elements exist, since deleted
  elements do contribute to the load factor on the index array.

  LIST-INDEX TABLE
  ----------------

  Efficient list indexing of the hash-map is difficult due to the soft
  deletions. Therefore, each hash-map with at least one deleted elements has a
  list-index table allocated as necessary, which translates from list indices
  to cursors.

  The list-index table is shared when possible; however, it never undergoes
  incremental updates. If a read finds that the list-index table is unsuitable,
  it creates a brand new one. Similarly, new delete operations result in a
  hash-map with no list-index table at all.

 */

/**
 * Identifies the hash function used in an index array.
 */
typedef enum {
  /**
   * Indicates that the hash function in use is simply ava_value_hash().
   */
  ava_hmhf_value,
  /**
   * Indicates that the hash function in use is ava_string_hash(), and thus
   * that all keys are ASCII9 strings.
   */
  ava_hmhf_ascii9
} ava_hash_map_hash_function;

static const ava_attribute_tag ava_hash_map_tag = {
  .name = "hash-map"
};

typedef struct {
  /**
   * The bitmask to map hash values to indices.
   */
  size_t mask;
  /**
   * The current hash function in use.
   */
  ava_hash_map_hash_function hash_function;

  /**
   * The actual number of elements present in this array.
   *
   * Readers: This value is indeterminate.
   *
   * Writers: Writers take ownership of the array by CaSing it from the length
   * on their ava_value to some greater value, according to the number of
   * elements they are going to insert.
   */
  AO_t num_elements;
  /**
   * Array of hash values parallel to the keys array.
   *
   * Values beyond the visible length of keys have undefined content.
   *
   * This is part of the same allocation as the ava_hash_map_index structure,
   * immediately following indices. Its length is 3/4 the value of (mask+1).
   */
  ava_ulong*restrict hash_cache;
  /**
   * Map from reduced hash values to indices in the keys/values arrays.
   *
   * Readers: Values may be either some value strictly less than their own
   * length (on the ava_value), which is stable, or some indeterminate value
   * greater than or equal to their own length.
   *
   * Writers: Values which are still AVA_MAP_CURSOR_NONE may be reduced to
   * values greater than or equal to the original length of num_elements
   * (before it was CaSed to a greater value).
   */
  ava_map_cursor indices[];
} ava_hash_map_index;

/**
 * Structure used to map logical list indices to physical map cursors.
 */
typedef struct {
  /**
   * The number of indices covered by this table, ie, the length of indices.
   */
  size_t n;
  /**
   * A table mapping list indices to map cursors.
   *
   * Given a list index ix, the equivalent cursor can be found at indices[ix].
   */
  ava_map_cursor indices[];
} ava_hash_map_list_indices;

typedef struct {
  ava_attribute header;

  /**
   * The list trait on both keys and values.
   */
  const ava_list_trait*restrict esba_trait;
  /**
   * The parallel key and value ESBA lists.
   *
   * They both always have the same length, which is stored on the ulong on the
   * ava_map_value itself, which allows appends to operate without extra heap
   * allocations.
   */
  const ava_attribute*restrict keys;
  const ava_attribute*restrict values;
  /**
   * The hash table proper.
   */
  ava_hash_map_index*restrict index;

  /**
   * The deleted entry bitmap.
   *
   * This value is not initialised if num_deleted_entries is zero.
   */
  ava_esba deleted_entries;
  /**
   * The number of 1 bits in deleted_entries.
   */
  size_t num_deleted_entries;

  /**
   * If non-NULL, a mapping from list indices 0..n (where n is some integer not
   * necessarily related to anything a reader can see) to cursor values.
   *
   * Writers do not directly interact with this value, except to propagate it
   * across realocations, and to null it in the new value after a deletion.
   *
   * Readers read-acquire this pointer, which points to frozen memory. If the
   * length of the list-index table is less or equal to the index they wish to
   * map, they build a replacement table that is large enough and write-release
   * it here. This means concurrent readers can waste some work, but it does
   * not affect correctness.
   */
  AO_t /* const ava_hash_map_list_indices*restrict */ effective_indices;
} ava_hash_map;

static const ava_value_trait ava_hash_map_value_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = ava_list_string_chunk_iterator,
  .iterate_string_chunk = ava_list_iterate_string_chunk,
};

AVA_LIST_DEFIMPL(ava_hash_map, &ava_hash_map_value_impl)
AVA_MAP_DEFIMPL(ava_hash_map, &ava_hash_map_list_impl)

static ava_bool is_ascii9_string(ava_value value) {
  return &ava_string_type == ava_value_attr(value) &&
    (ava_value_ulong(value) & 1);
}

static inline ava_list_value get_keys(ava_value this) {
  const ava_hash_map*restrict data = ava_value_attr(this);
  return (ava_list_value) {
    ava_value_with_ulong(data->keys, ava_value_ulong(this))
  };
}

static inline ava_list_value get_values(ava_value this) {
  const ava_hash_map*restrict data = ava_value_attr(this);
  return (ava_list_value) {
    ava_value_with_ulong(data->values, ava_value_ulong(this))
  };
}

static inline const ava_list_trait* get_esba_v(ava_value this) {
  const ava_hash_map*restrict data = ava_value_attr(this);
  return data->esba_trait;
}

static inline size_t desired_capacity(size_t num_elements) {
  /* Maximum load factor of 67% */
  return num_elements * 3 / 2 > MIN_CAPACITY?
    num_elements * 3 / 2 : MIN_CAPACITY;
}

#define INVOKE_LIST(this, member, method, ...)                  \
  (get_esba_v(this)->method(get_##member(this) __VA_ARGS__))

/**
 * Allocates a new index.
 *
 * Neither num_elements, hash_function, nor indices are initialised.
 *
 * @param capacity The desired capacity. The lowest power of two greater than
 * or equal to this value is used.
 */
static ava_hash_map_index* ava_hash_map_index_new(size_t capacity);

/**
 * Takes ownership of the given map, changing the index length from
 * expected_length to expected_length+1.
 *
 * The index is forked if necessary.
 */
static void ava_hash_map_make_index_writable(ava_hash_map*restrict map,
                                             size_t expected_length);

/**
 * Puts the given key/value pair into the given hash map, overwriting the map
 * in-place.
 *
 * This does _not_ add the elements to the keys/values lists; it merely
 * manipulates the hash table and adds the element to the hash cache. The
 * elements must already be present within the lists. The caller must have
 * already called ava_hash_map_make_index_writable() on the map.
 *
 * Rehashes the table if necessary.
 *
 * @param map The map to mutate.
 * @param expected_length The expected value for
 * ava_hash_map_index.num_elements, and the assumed index of the new element.
 * @param key The key being inserted.
 * @param value The value being inserted.
 * @return The new length of the hash map (including deleted elements).
 */
static size_t ava_hash_map_put(ava_hash_map*restrict map,
                               size_t expected_length,
                               ava_value key);

/**
 * Maps the given hash code to the given index.
 *
 * This assumes that the caller already has right access to the index table.
 *
 * @param map The map to mutate.
 * @param index The index to map.
 * @param hash The hash to map to the index.
 * @return Whether a rehash is suggested.
 */
static ava_bool ava_hash_map_put_direct(ava_hash_map*restrict map,
                                        size_t index,
                                        ava_ulong hash);

/**
 * Builds a new hash table for the given hash map.
 *
 * A call to ava_hash_map_vacuum() is implied.
 *
 * @param map The map whose hash table is to be rebuilt.
 * @param num_elements The number of elements in the final result.
 * @param permit_ascii9 Whether the new table may continue using ASCII9 hashing
 * if the old one was using ASCII9 hashing.
 * @return The new length of the hash map (including deleted elements).
 */
static size_t ava_hash_map_rehash(ava_hash_map*restrict map,
                                  size_t num_elements,
                                  ava_bool permit_ascii9);

/**
 * If the given hash map has any deleted elements, the keys and values arrays
 * are rebuilt to have no deleted elements, and the deleted bitmap is nulled.
 *
 * This destroys the hash table proper, so it should only be called from
 * ava_hash_map_rehash().
 *
 * @param map The map to vacuum.
 * @param num_elements The number of elements in each array to examine,
 * including deleted elements.
 * @return The number of elements in the vacuumed hash map.
 */
static size_t ava_hash_map_vacuum(ava_hash_map*restrict map,
                                  size_t num_elements);

/**
 * Copies the given index to a freshly-allocated index.
 *
 * Indices at or beyond the limit are reset to AVA_MAP_CURSOR_NONE.
 *
 * @param index The index to fork.
 * @param limit The first cursor which is considered beyond the known indices;
 * any elements greater than or equal to it are reset to AVA_MAP_CURSOR_NONE.
 * @return The new index.
 */
static ava_hash_map_index* ava_hash_map_fork_index(
  const ava_hash_map_index*restrict index,
  ava_map_cursor limit);

/**
 * Prepares an ava_map_value with the given data.
 *
 * @param old The original ava_map_value, ie, that was passed into the method.
 * @param data A stack-allocated hash map that contains the new data for the
 * value. This will be copied to a heap-allocated object if the hash map on old
 * does not already contain the same values.
 * @param length The length of the hash map.
 */
static ava_map_value ava_hash_map_combine(ava_map_value old,
                                          const ava_hash_map*restrict data,
                                          size_t length);

static ava_bool ava_hash_map_is_deleted(const ava_hash_map*restrict this,
                                        ava_map_cursor element);

static ava_map_cursor ava_hash_map_search(ava_map_value map,
                                          ava_value key,
                                          ava_map_cursor start);

static ava_bool ava_hash_map_to_ascii9(ava_value*restrict value);

static const ava_hash_map_list_indices* ava_hash_map_build_effective_indices(
  const ava_hash_map*restrict this, size_t length);

static ava_map_value ava_hash_map_of_esbas(ava_list_value keys_list,
                                           ava_list_value values_list);

ava_map_value ava_hash_map_of_raw(const ava_value*restrict keys,
                                  size_t key_stride,
                                  const ava_value*restrict values,
                                  size_t value_stride,
                                  size_t count) {
  ava_list_value keys_list, values_list;

  keys_list = ava_esba_list_of_raw_strided(keys, count, key_stride);
  values_list = ava_esba_list_of_raw_strided(values, count, value_stride);

  return ava_hash_map_of_esbas(keys_list, values_list);
}

ava_map_value ava_hash_map_of_list(ava_list_value list) {
  ava_list_value keys_list, values_list;

  assert(0 == ava_list_length(list) % 2);

  keys_list = ava_list_proj_demux(list, 0, 2);
  values_list = ava_list_proj_demux(list, 1, 2);

  keys_list = ava_esba_list_copy_of(
    keys_list, 0, ava_list_length(keys_list));
  values_list = ava_esba_list_copy_of(
    values_list, 0, ava_list_length(values_list));

  return ava_hash_map_of_esbas(keys_list, values_list);
}

static ava_map_value ava_hash_map_of_esbas(ava_list_value keys_list,
                                           ava_list_value values_list) {
  ava_hash_map*restrict this = AVA_NEW(ava_hash_map);
  size_t length;

  this->header.tag = &ava_hash_map_tag;
  this->header.next = (const ava_attribute*)&ava_hash_map_map_impl;
  this->esba_trait = ava_get_attribute(keys_list.v, &ava_list_trait_tag);
  this->keys = ava_value_attr(keys_list.v);
  this->values = ava_value_attr(values_list.v);
  length = ava_value_ulong(keys_list.v);
  assert(length == ava_value_ulong(values_list.v));
  length = ava_hash_map_rehash(this, length, ava_true);

  return (ava_map_value) {
    ava_value_with_ulong(this, length)
  };
}

static ava_hash_map_index* ava_hash_map_index_new(size_t base_cap) {
  size_t cap = 1;
  ava_hash_map_index*restrict index;

  while (cap < base_cap) cap <<= 1;

  index = ava_alloc_atomic_precise(sizeof(ava_hash_map_index) +
                                   sizeof(index->indices[0]) * cap +
                                   sizeof(ava_ulong) * (cap * 3/4));
  index->hash_cache = (ava_ulong*)(index->indices + cap);
  index->mask = cap-1;
  return index;
}

static ava_map_value ava_hash_map_combine(ava_map_value old,
                                          const ava_hash_map*restrict data,
                                          size_t length) {
  const ava_hash_map*restrict old_data = ava_value_attr(old.v);

  if (0 == memcmp(data, old_data, sizeof(ava_hash_map))) {
    return (ava_map_value) {
      ava_value_with_ulong(old_data, length)
    };
  } else {
    return (ava_map_value) {
      ava_value_with_ulong(ava_clone(data, sizeof(ava_hash_map)), length)
    };
  }
}

static size_t ava_hash_map_map_npairs(ava_map_value map) {
  const ava_hash_map*restrict this = ava_value_attr(map.v);

  return ava_value_ulong(map.v) - this->num_deleted_entries;
}

static ava_map_cursor ava_hash_map_map_find(ava_map_value map, ava_value key) {
  return ava_hash_map_search(map, key, 0);
}

static ava_map_cursor ava_hash_map_map_next(ava_map_value map,
                                            ava_map_cursor cursor) {
  return ava_hash_map_search(
    map, INVOKE_LIST(map.v, keys, index,, cursor), cursor + 1);
}

static ava_bool ava_hash_map_to_ascii9(ava_value*restrict value) {
  ava_string str;
  char buf[9];
  size_t strlen;

  if (is_ascii9_string(*value))
    return ava_true;

  str = ava_to_string(*value);
  strlen = ava_string_length(str);
  if (strlen > 9)
    return ava_false;

  /* Force the string to reencode to see if it can be represented as ASCII9.
   *
   * It is possible for string values to exist in the rope representation which
   * could actually be ASCII9, but this difference must be transparent to
   * clients.
   */
  ava_string_to_bytes(buf, str, 0, strlen);
  str = ava_string_of_bytes(buf, strlen);

  if (str.ascii9 & 1) {
    *value = ava_value_of_string(str);
    return ava_true;
  } else {
    return ava_false;
  }
}

static ava_map_cursor ava_hash_map_search(ava_map_value map,
                                          ava_value key,
                                          ava_map_cursor start) {
  const ava_hash_map*restrict this = ava_value_attr(map.v);

  ava_ulong hash, orig_hash, length = ava_value_ulong(map.v);
  unsigned tries = 0;
  size_t ix;
  ava_map_cursor cursor;
  ava_value other_key;
  ava_bool equal;

  switch (this->index->hash_function) {
  case ava_hmhf_value:
    hash = ava_value_hash(key);
    break;

  case ava_hmhf_ascii9:
    if (!ava_hash_map_to_ascii9(&key)) {
      /* It can't be represented as ASCII9, so it's guaranteed to not be in the
       * hash table.
       */
      return AVA_MAP_CURSOR_NONE;
    }

    hash = ava_ascii9_hash(ava_value_ulong(key));
    break;

  default:
    /* unreachable */
    abort();
  }

  orig_hash = hash;

  for (;;) {
    ix = (tries + hash) & this->index->mask;
    cursor = this->index->indices[ix];

    if (cursor >= length) {
      /* Empty slot, definitely not in this hash map */
      return AVA_MAP_CURSOR_NONE;
    }

    /* Only consider items later than the start point.
     *
     * We are guaranteed to always encounter elements in insertion order,
     * except that an earlier element may be re-encountered later.
     *
     * Deleted elements must also be excluded.
     *
     * There's no point in fetching the key and comparing them if their full
     * hashes aren't equal.
     */
    if (cursor >= start && !ava_hash_map_is_deleted(this, cursor) &&
        orig_hash == this->index->hash_cache[cursor]) {
      /* Check whether it actually corresponds to the query key */
      other_key = INVOKE_LIST(map.v, keys, index,, cursor);
      switch (this->index->hash_function) {
      case ava_hmhf_value:
        equal = ava_value_equal(key, other_key);
        break;

      case ava_hmhf_ascii9:
        equal = ava_value_ulong(key) == ava_value_ulong(other_key);
        break;
      }

      if (equal)
        return cursor;
    }

    /* No matching element found, shift and try again until we do find one or
     * encounter an empty slot.
     */
    hash >>= COLLISION_SHIFT_AMT;
    ++tries;
  }
}

static void ava_hash_map_make_index_writable(ava_hash_map*restrict map,
                                             size_t expected_length) {
  if (!AO_compare_and_swap(&map->index->num_elements, expected_length,
                           expected_length + 1)) {
    /* Unable to get write access.
     *
     * We don't need a full rehash; simply cloning the hash table will do.
     *
     * Note that the number of elements in the clone is expected_length, not
     * expected_length+1, because anything at the latter index actually belongs
     * to a different hash table.
     */
    map->index = ava_hash_map_fork_index(map->index, expected_length);
    ++map->index->num_elements;
  }
}

static size_t ava_hash_map_put(ava_hash_map*restrict map,
                               size_t expected_length,
                               ava_value key) {
  ava_bool can_use_ascii9_hash_function;
  ava_ulong hash;

  can_use_ascii9_hash_function =
    expected_length <= ASCII9_SIZE_THRESH &&
    is_ascii9_string(key);

  /* Check for conditions that require rehashing.
   *
   * Note that rehashing will add the pair to the newly-built table, since the
   * elements are already present in the table when it returns.
   */

  if (!can_use_ascii9_hash_function &&
      ava_hmhf_ascii9 == map->index->hash_function) {
    /* New key isn't compatible with existing table.
     *
     * We can't add the element to the hash cache, but that's fine since the
     * hash cache isn't usable for this rehash.
     */
    return ava_hash_map_rehash(map, expected_length+1,
                               can_use_ascii9_hash_function);
  }

  switch (map->index->hash_function) {
  case ava_hmhf_value:
    hash = ava_value_hash(key);
    break;

  case ava_hmhf_ascii9:
    hash = ava_ascii9_hash(ava_value_ulong(key));
    break;

  default:
    /* unreachable */
    abort();
  }

  /* Add to hash cache, needed for any rehashes from hereonout */
  map->index->hash_cache[expected_length] = hash;

  if (desired_capacity(expected_length+1) > map->index->mask+1) {
    /* Load factor exceeded */
    return ava_hash_map_rehash(map, expected_length+1,
                               can_use_ascii9_hash_function);
  }

  if (ava_hash_map_put_direct(map, expected_length, hash))
    return ava_hash_map_rehash(map, expected_length+1, 0);
  else
    return expected_length + 1;
}

static ava_bool ava_hash_map_put_direct(ava_hash_map*restrict map,
                                        size_t index,
                                        ava_ulong orig_hash) {
  ava_ulong hash = orig_hash;
  unsigned tries = 0;
  size_t ix;
  ava_bool suggest_rehash = ava_false;

  /* Find the first free slot */
  for (;;) {
    ix = (tries + hash) & map->index->mask;
    if (AVA_LIKELY(AVA_MAP_CURSOR_NONE == map->index->indices[ix])) break;

    /* Not free, move onto the next */
    ++tries;
    hash >>= COLLISION_SHIFT_AMT;

    /* If using the weaker ASCII9 hash and there've been too many collisions,
     * give up and rehash with the stronger value hash.
     */
    if (AVA_UNLIKELY(tries > ASCII9_COLLISION_THRESH) &&
        ava_hmhf_ascii9 == map->index->hash_function) {
      suggest_rehash = 1;
    }
  }

  /* Free slot at ix */
  map->index->indices[ix] = index;
  return suggest_rehash;
}

static ava_hash_map_index* ava_hash_map_fork_index(
  const ava_hash_map_index*restrict src,
  ava_map_cursor limit
) {
  ava_hash_map_index*restrict dst = ava_hash_map_index_new(src->mask + 1);
  size_t i;

  dst->num_elements = limit;
  dst->hash_function = src->hash_function;
  memcpy(dst->indices, src->indices, sizeof(dst->indices[0]) * (dst->mask+1));
  for (i = 0; i <= dst->mask; ++i) {
    if (dst->indices[i] >= limit)
      dst->indices[i] = AVA_MAP_CURSOR_NONE;
  }
  memcpy(dst->hash_cache, src->hash_cache, sizeof(ava_ulong) * limit);

  return dst;
}

static size_t ava_hash_map_rehash(ava_hash_map*restrict map,
                                  size_t num_elements,
                                  ava_bool permit_ascii9) {
  size_t i, orig_num_elements;
  size_t new_size AVA_UNUSED;
  ava_list_value keys;
  ava_hash_map_hash_function preferred_hash_function;
  ava_bool vacuumed;
  const ava_hash_map_index*restrict old_index = map->index;

  orig_num_elements = num_elements;
  num_elements = ava_hash_map_vacuum(map, num_elements);
  vacuumed = orig_num_elements != num_elements;

  keys = (ava_list_value) {
    ava_value_with_ulong(map->keys, num_elements)
  };

  if (NULL != map->index) {
    preferred_hash_function = map->index->hash_function;
  } else {
    preferred_hash_function = ava_hmhf_ascii9;
    for (i = 0; i < num_elements &&
           preferred_hash_function != ava_hmhf_value; ++i) {
      if (!is_ascii9_string(map->esba_trait->index(keys, i))) {
        preferred_hash_function = ava_hmhf_value;
      }
    }
  }

  map->index = ava_hash_map_index_new(desired_capacity(num_elements));
  map->index->num_elements = 0;
  map->index->hash_function = permit_ascii9?
    preferred_hash_function : ava_hmhf_value;
  memset(map->index->indices, -1,
         sizeof(map->index->indices[0]) * (map->index->mask+1));

  if (old_index && !vacuumed &&
      map->index->hash_function == old_index->hash_function) {
    /* If there is an existing index using the same hash function and having
     * compatible indices (ie, no vacuuming has occurred to invalidate them),
     * use the hash cache to quickly rebuild the index instead of needing to
     * fetch and hash values from the ESBA list.
     */
    for (i = 0; i < num_elements; ++i) {
      map->index->hash_cache[i] = old_index->hash_cache[i];
      ava_hash_map_put_direct(map, i, old_index->hash_cache[i]);
    }
    map->index->num_elements = num_elements;
  } else {
    for (i = 0; i < num_elements; ++i) {
      new_size = ava_hash_map_put(map, i, map->esba_trait->index(keys, i));
      assert(i+1 == new_size);
      ++map->index->num_elements;
    }
  }

  return num_elements;
}

static size_t ava_hash_map_vacuum(ava_hash_map*restrict src,
                                  size_t num_elements) {
  ava_hash_map dst;
  size_t i, bitmap_nelt, new_num_elements;
  ava_list_value src_keys, src_values;
  ava_list_value dst_keys, dst_values;
  ava_value key, value;
  const ava_ulong*restrict bitmap;
  ava_esba_tx tx;

  if (0 == src->num_deleted_entries) return num_elements;

  new_num_elements = num_elements - src->num_deleted_entries;

  dst = *src;
  /* Clear the deleted entries on the new value, as well as the effective
   * indices, since the result will be linear.
   */
  dst.deleted_entries = (ava_esba) { 0, 0 };
  dst.num_deleted_entries = 0;
  dst.effective_indices = 0;

  src_keys = (ava_list_value) {
    ava_value_with_ulong(src->keys, num_elements)
  };
  src_values = (ava_list_value) {
    ava_value_with_ulong(src->values, num_elements)
  };

  bitmap_nelt = ava_esba_length(src->deleted_entries);
  do {
    bitmap = ava_esba_access(src->deleted_entries, &tx);

    /* Find the first non-deleted index to initialise the ESBA lists (since
     * they can't be empty).
     */
    for (i = 0; i/BME_BITS < bitmap_nelt &&
           ((bitmap[i/BME_BITS] >> i % BME_BITS) & 1); ++i)
      assert(i < num_elements);

    key = src->esba_trait->index(src_keys, i);
    value = src->esba_trait->index(src_keys, i);
    dst_keys = ava_esba_list_of_raw(&key, 1);
    dst_values = ava_esba_list_of_raw(&value, 1);

    /* Add the rest of the non-deleted elements. */
    for (++i; i < num_elements; ++i) {
      if (i/BME_BITS >= bitmap_nelt ||
          0 == ((bitmap[i/BME_BITS] >> i % BME_BITS) & 1)) {
        dst_keys = dst.esba_trait->append(
          dst_keys, src->esba_trait->index(src_keys, i));
        dst_values = dst.esba_trait->append(
          dst_values, src->esba_trait->index(src_values, i));
      }
    }
  } while (AVA_UNLIKELY(!ava_esba_check_access(
                          src->deleted_entries, bitmap, tx)));

  dst.keys = ava_value_attr(dst_keys.v);
  dst.values = ava_value_attr(dst_values.v);

  *src = dst;
  return new_num_elements;
}

static ava_bool ava_hash_map_is_deleted(const ava_hash_map*restrict this,
                                        ava_map_cursor element) {
  size_t esba_ix = element / BME_BITS;
  size_t shift = element % BME_BITS;
  const ava_ulong*restrict bits;
  ava_esba_tx tx;
  ava_bool ret;

  if (0 == this->num_deleted_entries) return ava_false;
  if (ava_esba_length(this->deleted_entries) <= esba_ix) return ava_false;

  do {
    bits = ava_esba_access(this->deleted_entries, &tx);
    ret = (bits[esba_ix] >> shift) & 1;
  } while (AVA_UNLIKELY(!ava_esba_check_access(this->deleted_entries, bits, tx)));

  return ret;
}

static ava_value ava_hash_map_map_get_key(ava_map_value map,
                                          ava_map_cursor cursor) {
  return INVOKE_LIST(map.v, keys, index,, cursor);
}

static ava_value ava_hash_map_map_get(ava_map_value map,
                                      ava_map_cursor cursor) {
  return INVOKE_LIST(map.v, values, index,, cursor);
}

static ava_map_value ava_hash_map_map_set(ava_map_value map,
                                          ava_map_cursor cursor,
                                          ava_value value) {
  ava_hash_map this = *(const ava_hash_map*)ava_value_attr(map.v);
  this.values = ava_value_attr(
    INVOKE_LIST(map.v, values, set,, cursor, value).v);
  return ava_hash_map_combine(map, &this, ava_value_ulong(map.v));
}

static ava_map_value ava_hash_map_map_add(ava_map_value map,
                                          ava_value key,
                                          ava_value value) {
  ava_hash_map this = *(const ava_hash_map*)ava_value_attr(map.v);
  ava_ulong length = ava_value_ulong(map.v);

  this.keys = ava_value_attr(INVOKE_LIST(map.v, keys, append,, key).v);
  this.values = ava_value_attr(INVOKE_LIST(map.v, values, append,, value).v);
  ava_hash_map_make_index_writable(&this, length);
  length = ava_hash_map_put(&this, length, key);

  return ava_hash_map_combine(map, &this, length);
}

static ava_map_value ava_hash_map_map_delete(ava_map_value map,
                                             ava_map_cursor cursor) {
  ava_hash_map this = *(const ava_hash_map*)ava_value_attr(map.v);
  ava_ulong length = ava_value_ulong(map.v);
  size_t required_bitset_elements = 1 + cursor / BME_BITS;
  const ava_ulong*restrict bitmap;
  ava_ulong bitmap_element;
  ava_esba_tx tx;

  /* If this is removing the last element, degenerate into an empty map, since
   * the ESBA lists can't become empty.
   */
  if (length == this.num_deleted_entries + 1)
    return ava_empty_map();

  /* Create a deletion bitmap if needed */
  if (0 == this.num_deleted_entries) {
    this.deleted_entries = ava_esba_new(
      sizeof(ava_ulong), required_bitset_elements,
      ava_alloc_atomic_precise, NULL);
  }

  /* Extend the bitmap to be sufficient to hold this bit */
  if (ava_esba_length(this.deleted_entries) < required_bitset_elements) {
    size_t num_to_append =
      required_bitset_elements - ava_esba_length(this.deleted_entries);
    ava_ulong*restrict new_elements = ava_esba_start_append(
      &this.deleted_entries, num_to_append);
    memset(new_elements, 0, sizeof(ava_ulong) * num_to_append);
    ava_esba_finish_append(this.deleted_entries, num_to_append);
  }

  /* Soft-delete the element by marking it in the bitmap */
  do {
    bitmap = ava_esba_access(this.deleted_entries, &tx);
    bitmap_element = bitmap[cursor/BME_BITS];
  } while (AVA_UNLIKELY(!ava_esba_check_access(
                          this.deleted_entries, bitmap, tx)));
  bitmap_element |= 1ULL << (ava_ulong)(cursor % BME_BITS);
  this.deleted_entries = ava_esba_set(this.deleted_entries,
                                      cursor / BME_BITS, &bitmap_element);
  ++this.num_deleted_entries;

  /* If more than half the map is deleted entries, vacuum and rehash */
  if (this.num_deleted_entries > length / 2) {
    length = ava_hash_map_rehash(&this, length, ava_true);
  }

  /* The list-index table is wrong now, if it had been present */
  this.effective_indices = 0;

  return ava_hash_map_combine(map, &this, length);
}

static size_t ava_hash_map_list_length(ava_list_value map) {
  const ava_hash_map*restrict this = ava_value_attr(map.v);

  return ava_value_ulong(map.v) * 2 - this->num_deleted_entries * 2;
}

static ava_value ava_hash_map_list_index(ava_list_value map, size_t index) {
  const ava_hash_map*restrict this = ava_value_attr(map.v);
  const ava_hash_map_list_indices*restrict indices =
    (const ava_hash_map_list_indices*restrict)
    AO_load_acquire_read(&this->effective_indices);
  ava_bool is_value = index & 1;

  index /= 2;

  if (this->num_deleted_entries > 0) {
    if (!indices || index >= indices->n) {
      indices = ava_hash_map_build_effective_indices(
        this, ava_value_ulong(map.v));
      /* Write back so later reads can take advantage of it.
       *
       * Note that multiple readers here will race and an arbitrary one will
       * win; that's fine, each will still use the result it calculated above.
       */
      AO_store_release_write((AO_t*)&this->effective_indices, (AO_t)indices);
    }

    index = indices->indices[index];
  }

  if (is_value)
    return INVOKE_LIST(map.v, values, index,, index);
  else
    return INVOKE_LIST(map.v, keys, index,, index);
}

static const ava_hash_map_list_indices* ava_hash_map_build_effective_indices(
  const ava_hash_map*restrict this, size_t length
) {
  size_t num_inputs = length - this->num_deleted_entries;
  size_t logical, physical, bitmap_nelt;
  const ava_ulong*restrict bitmap;
  ava_esba_tx tx;
  ava_hash_map_list_indices*restrict dst;

  dst = ava_alloc_atomic_precise(sizeof(ava_hash_map_list_indices) +
                                 sizeof(ava_map_cursor) * num_inputs);
  dst->n = num_inputs;
  bitmap_nelt = ava_esba_length(this->deleted_entries);

  do {
    bitmap = ava_esba_access(this->deleted_entries, &tx);
    for (logical = physical = 0; physical < length; ++physical) {
      if (physical/BME_BITS >= bitmap_nelt ||
          0 == ((bitmap[physical/BME_BITS] >> physical % BME_BITS) & 1))
        dst->indices[logical++] = physical;
    }
  } while (AVA_UNLIKELY(!ava_esba_check_access(this->deleted_entries,
                                               bitmap, tx)));

  return dst;
}

static ava_list_value ava_hash_map_list_slice(ava_list_value map,
                                              size_t begin, size_t end) {
  return ava_list_copy_slice(map, begin, end);
}

static ava_list_value ava_hash_map_list_append(ava_list_value map,
                                               ava_value element) {
  return ava_list_copy_append(map, element);
}

static ava_list_value ava_hash_map_list_concat(ava_list_value map,
                                               ava_list_value other) {
  size_t other_length = ava_list_length(other), i;

  /* If other contains an even number of elements, it's a valid map, so just
   * add all its elements to this map.
   */
  if (0 == (other_length & 1)) {
    ava_map_value mmap = (ava_map_value) { map.v };
    for (i = 0; i < other_length; i += 2)
      mmap = ava_hash_map_map_add(
        mmap, ava_list_index(other, i), ava_list_index(other, i+1));
    return (ava_list_value) { mmap.v };
  }

  return ava_list_copy_concat(map, other);
}

static ava_list_value ava_hash_map_list_delete(ava_list_value map,
                                               size_t begin, size_t end) {
  return ava_list_copy_delete(map, begin, end);
}

static ava_list_value ava_hash_map_list_set(ava_list_value map,
                                            size_t index,
                                            ava_value value) {
  return ava_list_copy_set(map, index, value);
}

const char* ava_hash_map_get_hash_function(ava_map_value map) {
  const ava_hash_map*restrict this = ava_value_attr(map.v);

  switch (this->index->hash_function) {
  case ava_hmhf_ascii9: return "ascii9";
  case ava_hmhf_value:  return "value";
  default:
    /* unreachable */
    abort();
  }
}
