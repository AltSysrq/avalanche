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
#ifndef AVA_RUNTIME__LIST_MAP_H_
#define AVA_RUNTIME__LIST_MAP_H_

#include "avalanche/map.h"

/**
 * @file
 *
 * Provides a simple list-based map.
 *
 * A list-map simply implements the map trait directly atop an underlying
 * ava_list_value. This makes it very light-weight, but it does not scale to
 * larger lists. List-maps should not be created with more than
 * AVA_LIST_MAP_THRESH elements, and they automatically promote themselves to
 * other map implementations when they exceed this limit.
 */

/**
 * The threshold, in number of elements, beyond which list-maps are considered
 * too inefficient.
 */
#define AVA_LIST_MAP_THRESH 4

/**
 * Constructs a new list-map atop the given list.
 *
 * The list MUST have an even number of elements, and SHOULD be less than or
 * equal to 2*AVA_LIST_MAP_THRESH in length.
 */
ava_map_value ava_list_map_of_list(ava_list_value list) AVA_PURE;

#endif /* AVA_RUNTIME__LIST_MAP_H_ */
