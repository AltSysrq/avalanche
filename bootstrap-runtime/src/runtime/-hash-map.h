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
#ifndef AVA_RUNTIME__HASH_MAP_H_
#define AVA_RUNTIME__HASH_MAP_H_

#include "avalanche/map.h"

/**
 * @file
 *
 * Provides an efficient hash-table-based map.
 *
 * A hash-map implements the map trait via parallel ESBA lists and a hash table
 * index, and is thus able to perform all map and list operations relatively
 * quickly, even for large numbers of values. However, this makes it much more
 * expensive than a list-map to construct.
 *
 * Since they are based on ESBA lists, a hash-map can*not* be empty.
 */

/**
 * Constructs a new hash-map from parallel arrays of keys and values.
 *
 * This behaves exactly like ava_map_of_values(), except that count may not be
 * zero, and it always produces a hash-map.
 *
 * @see ava_map_of_values()
 */
ava_map_value ava_hash_map_of_raw(const ava_value*restrict keys,
                                  size_t key_stride,
                                  const ava_value*restrict values,
                                  size_t value_stride,
                                  size_t count);

/**
 * Constructs a new hash-map from the given non-empty list of even length.
 */
ava_map_value ava_hash_map_of_list(ava_list_value list) AVA_PURE;

/**
 * Returns the name of the hash function being used by the given hash-map.
 *
 * This is only useful for tests or diagnostics.
 */
const char* ava_hash_map_get_hash_function(ava_map_value map);

/* Specialised versions of the below. Generally they should not be used
 * directly.
 */
ava_map_value ava_hash_map_of_raw_ava_ushort(
  const ava_value*restrict keys,
  size_t key_stride,
  const ava_value*restrict values,
  size_t value_stride,
  size_t count);
ava_map_value ava_hash_map_of_list_ava_ushort(ava_list_value list) AVA_PURE;
const char* ava_hash_map_get_hash_function_ava_ushort(ava_map_value map);
ava_map_value ava_hash_map_of_raw_ava_uint(
  const ava_value*restrict keys,
  size_t key_stride,
  const ava_value*restrict values,
  size_t value_stride,
  size_t count);
ava_map_value ava_hash_map_of_list_ava_uint(ava_list_value list) AVA_PURE;
const char* ava_hash_map_get_hash_function_ava_uint(ava_map_value map);
ava_map_value ava_hash_map_of_raw_ava_ulong(
  const ava_value*restrict keys,
  size_t key_stride,
  const ava_value*restrict values,
  size_t value_stride,
  size_t count);
ava_map_value ava_hash_map_of_list_ava_ulong(ava_list_value list) AVA_PURE;
const char* ava_hash_map_get_hash_function_ava_ulong(ava_map_value map);

#endif /* AVA_RUNTIME__HASH_MAP_H_ */
