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
#ifndef AVA_RUNTIME__ARRAY_LIST_H_
#define AVA_RUNTIME__ARRAY_LIST_H_

#include "avalanche/list.h"

/**
 * @file
 *
 * Provides facilities for creating array lists, the simplest non-empty list
 * implementation.
 *
 * Array lists are, as their name suggests, backed by simple arrays. This makes
 * them well-suited to lists of small size. Because they are normally quite
 * small, no attempt is made to optimise for particular types of elements.
 */

/**
 * The threshold above which esba lists should be preferred over array lists.
 * Mutating operations on an array list automatically switch if the result size
 * is greater than this threshold and a reallocation is required anyway.
 */
#define AVA_ARRAY_LIST_THRESH 16

/**
 * Copies elements in the given range of the given list into a new array list.
 */
ava_list_value ava_array_list_copy_of(
  ava_list_value list, size_t begin, size_t end);

/**
 * Creates a new array list whose contents are the given array of the given
 * length.
 *
 * The array is copied, not referenced.
 */
ava_list_value ava_array_list_of_raw(
  const ava_value*restrict array,
  size_t length);

/**
 * This is for testing.
 *
 * It returns the number of used elements in the backing of the list. This is
 * not thread-safe and violates the generally-expected behaviour of values.
 */
unsigned ava_array_list_used(ava_list_value list);

#endif /* AVA_RUNTIME__ARRAY_LIST_H_ */
