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
#error "Don't include avalanche/list-proj.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_LIST_PROJ_H_
#define AVA_RUNTIME_LIST_PROJ_H_

#include "defs.h"
#include "list.h"

/**
 * @file
 *
 * Provides lists which are projections of other lists.
 *
 * A list projection is not in and of itself a list, but rather transforms
 * another list it holds as needed.
 */

/**
 * Returns a list whose values are alternatively taken from each input list in
 * sequence. For example, the lists [a b c], [d e f], [g h i] interleave to
 * produce [a d g b e h c f i].
 *
 * All lists must be of the same length.
 *
 * This function is aware of values produced by ava_list_proj_demux() and can
 * invert them quickly.
 *
 * @param lists An array of lists to interleave.
 * @param num_lists The number of elements in the lists array.
 * @return An interleaved list projection.
 */
ava_list_value ava_list_proj_interleave(const ava_list_value*restrict lists,
                                        size_t num_lists);

/**
 * Returns a list containing every strideth value, starting at offset.
 *
 * Specifically, the result will containain every element from the input list
 * of index ix, where (offset == ix % stride).
 *
 * For example, demux([a b c d e f g h i], 1, 3) produces [b e h].
 *
 * offset must be strictly less than stride.
 *
 * This function is aware of values produced by ava_list_proj_interleave() and
 * can invert them quickly.
 *
 * This transform assumes that the caller plans on extracting a majority of the
 * offsets as separate lists; as such, it holds onto the entire original list.
 *
 * @param list The list from which to select elements.
 * @param offset The index within each stride to extract. Must be less than
 * stride.
 * @param stride The distance between elements to extrat. Must be non-zero.
 * Need not divide evenly into the length of list.
 * @return A list containing every element at ix in list where
 * (offset == ix % stride).
 */
ava_list_value ava_list_proj_demux(ava_list_value list,
                                   size_t offset, size_t stride);

/**
 * Groups adjacent elements in the list into sub-lists.
 *
 * Every group of group_sz elements is placed into a sublist of length
 * group_sz, except for the final group, which may be smaller if group_sz does
 * not divide evenly into the length of the list.
 *
 * For example, group([a b c d e f g h], 3) yields [[a b c] [d e f] [g h]].
 *
 * @param list The list whose elements are to be grouped.
 * @param group_sz The size of each group.
 * @return A list containing the elements of list grouped into sub-lists.
 */
ava_value ava_list_proj_group(ava_value list, size_t group_sz);

/**
 * Flattens a list.
 *
 * Each element is interpreted as a list and is concatenated with all the
 * elements before it. For example, flatten([[a b c] [d e]]) produces the list
 * [a b c d e].
 *
 * This function is aware of values produced by ava_list_proj_group() and can
 * invert it efficiently. It is otherwise not an actual projection and requires
 * copying elements.
 *
 * @param list The list to flatten.
 * @return The flattened list.
 */
ava_value ava_list_proj_flatten(ava_value list);

#endif /* AVA_RUNTIME_LIST_PROJ_H_ */
