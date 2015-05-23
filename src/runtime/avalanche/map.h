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
#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/map.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_MAP_H_
#define AVA_RUNTIME_MAP_H_

#include "defs.h"
#include "value.h"
#include "list.h"

/**
 * A cursor into a map.
 *
 * The values stored in this type are up to each map implementation; there are
 * no defined semantics, except as follows:
 *
 * - The special value AVA_MAP_CURSOR_NONE never refers to an element.
 *
 * - Two cursors into the same map which compare equal refer to the same
 *   element.
 *
 * - Two cursors into the same map which are inequal refer to different
 *   elements.
 *
 * Implementations which use the ava_map_copy_*() functions to implement
 * add/set/delete must use cursors which indicate element indices, in terms of
 * pairs. For example, the map "foo bar baz qux" would use cursor 0 to refer to
 * the "foo bar" pair, and cursor 1 to refer to "baz qux". Clients of such
 * implementations MUST NOT make assumptions about such usage.
 */
typedef ava_ulong ava_map_cursor;

/**
 * Sentinal cursor indicating the absence of an element.
 */
#define AVA_MAP_CURSOR_NONE (~(ava_map_cursor)0)

#include "map-trait.h"

/**
 * Constructs a map from the given keys and values.
 *
 * This call supports both parallel arrays of keys and values as well as single
 * interleaved arrays.
 *
 * @param keys Array of keys. This array is copied.
 * @param key_stride Stride of the keys array. For every n from 0 to count-1, a
 * key is taken from keys[n*key_stride].
 * @param values Array of values. This array is copied.
 * @param value_stride Stride of the values array. For every n from 0 to
 * count-1, a value is taken from values[n*value_stride].
 * @param count The number of key/value pairs to produce.
 * @return A map containing count elements; for every n from 0 to count-1,
 * keys[n*key_stride] is mapped to values[n*value_stride]. Elements occur in
 * the order presented in the arrays.
 */
ava_map_value ava_map_of_values(const ava_value*restrict keys,
                                size_t key_stride,
                                const ava_value*restrict values,
                                size_t value_stride,
                                size_t count);

/**
 * Convenience implementation of ava_map_trait.next() which always returns
 * AVA_MAP_CURSOR_NONE.
 */
ava_map_cursor ava_map_unique_next(
  ava_map_value map, ava_map_cursor cursor) AVA_PURE;

/**
 * Implementation of ava_map_trait.set() which copies the source map into a new
 * map, for implementations that do not directly implement this operation.
 *
 * This requires that the underlying implementation use element indices for
 * cursors, as documented in ava_map_cursor.
 */
ava_map_value ava_map_copy_set(
  ava_map_value this, ava_map_cursor cursor, ava_value value) AVA_PURE;

/**
 * Implementation of ava_map_trait.add() which copies the source map into a new
 * map, for implementations that do not directly implement this operation.
 *
 * This requires that the underlying implementation use element indices for
 * cursors, as documented in ava_map_cursor.
 */
ava_map_value ava_map_copy_add(
  ava_map_value this, ava_value key, ava_value value) AVA_PURE;

/**
 * Implementation of ava_map_trait.delete() which copies the source map into a
 * new map, for implementations that do not directly implement this operation.
 *
 * This requires that the underlying implementation use element indices for
 * cursors, as documented in ava_map_cursor.
 */
ava_map_value ava_map_copy_delete(
  ava_map_value this, ava_map_cursor cursor, ava_value value) AVA_PURE;

/**
 * The empty map.
 */
ava_map_value ava_empty_map(void) AVA_CONSTFUN;

#endif /* AVA_RUNTIME_MAP_H_ */
