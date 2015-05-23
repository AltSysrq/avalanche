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
#ifndef AVA_RUNTIME__ESBA_LIST_H_
#define AVA_RUNTIME__ESBA_LIST_H_

#include "avalanche/list.h"

/**
 * @file
 *
 * Provides facilities for creating lists built on top of ESBAs.
 *
 * ESBAs attempt to optimise away redundancy in the elements they contain; for
 * example, if every value is a string, each element will be only 8 bytes wide
 * instead of 24.
 *
 * ESBA lists can*not* be empty. All operations on an ESBA list that produce
 * new lists also return an ESBA list, except for slice() and operations which
 * result in an empty list.
 */

/**
 * Copies elements in the given range of the given list into a new ESBA list.
 *
 * end may not be equal to begin.
 */
ava_list_value ava_esba_list_copy_of(ava_list_value list,
                                     size_t begin, size_t end);

/**
 * Creates a new ESBA list whose contents are the given array of the given
 * length.
 *
 * The array is copied rather than referenced.
 *
 * length must not be zero.
 */
ava_list_value ava_esba_list_of_raw(const ava_value*restrict array,
                                    size_t length);

/**
 * This is only for testing.
 *
 * Returns the size of elements being used by the given list, which is assumed
 * to be an ESBA list.
 */
size_t ava_esba_list_element_size(ava_value list);

#endif /* AVA_RUNTIME__ESBA_LIST_H_ */
