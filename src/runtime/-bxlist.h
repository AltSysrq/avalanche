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
#ifndef AVA_RUNTIME__BXLIST_H_
#define AVA_RUNTIME__BXLIST_H_

#include "avalanche/defs.h"

#define AVA_BXLIST_ORDER 64

/**
 * Internal type used in the B+ List.
 */
typedef struct ava_bxlist_node_s ava_bxlist_node;
/**
 * A timestamp on a B+ List. Used to track persistence in the presence of
 * in-place updates.
 */
typedef ava_uint ava_bxlist_timestamp;
/**
 * A function to determine the value weight of an arbitrary datum in a B+ List.
 */
typedef size_t (*ava_bxlist_weight_function)(const void*restrict);

/**
 * A B+ List is a persistent data structure which supports efficient
 * modification, usually with O(1) memory allocations per edit and reasonably
 * fast O(log(n)) access.
 *
 * B+ Lists are oblvivious to the nature of the values they contain; any data
 * type up to 255 bytes long may be stored within.
 *
 * Algorithmic complexities stated for mutating operations only apply to the
 * "latest version" of an instance of a structure. Mutations to other versions
 * generally have O(log(n)) overhead for both computation and allocations.
 * Mutations produce "latest versions" in either case, though.
 */
typedef struct {
  const ava_bxlist_node*restrict root;
  ava_bxlist_timestamp timestamp;
} ava_bxlist;

/**
 * Allocates a new, empty B+ List containing elements of the given size,
 * weighted by the given function.
 */
ava_bxlist ava_bxlist_new(size_t element_size,
                          ava_bxlist_weight_function weight_function);

/**
 * Appends the given set of data to the end of the B+ List.
 *
 * Complexity: Amortised O(m) allocation, O(m + log(n)) compute
 *
 * @param list The list to which to append.
 * @param data The data to append to the end of the list.
 * @param num_elements The number of elements in the data array.
 * @return A B+ List containing all the elements of list followed by all the
 * elements of data.
 */
ava_bxlist ava_bxlist_append(
  ava_bxlist list,
  const void*restrict data,
  size_t num_elements);
/**
 * Replaces a sequence of items within the B+ List.
 *
 * This is equivalent to
 *   ava_bxlist_insert(
 *     ava_bxlist_delete(list, index, num_elements),
 *     index, data, num_elements)
 * but is much efficient.
 *
 * Complexity: Amortised O(1) allocation, O(m + log(n)) compute
 *
 * @param list The list in which to replace elements.
 * @param index The index of the first element to replace.
 * @param data The replacement elements.
 * @param num_elemnts The number of elements to replace.
 * @return A B+ List containing the same elements as list, except with the
 * elements from index to index+num_elements replaced with the contents of
 * data.
 */
ava_bxlist ava_bxlist_replace(
  ava_bxlist list,
  size_t index,
  const void*restrict data,
  size_t num_elements);
/**
 * Inserts a sequence of items into a B+ List.
 *
 * Complexity: Amortised O(m) allocation, O(m + log(n)) compute
 *
 * @param list The list into which to insert elements.
 * @param index The index after which new elements are to be inserted.
 * @param data The elements to insert.
 * @param num_elements The number of elements to insert.
 * @return A B+ List containing the same elements as list, except with
 * the num_elements elements from data inserted before the element which was at
 * index in the original list.
 */
ava_bxlist ava_bxlist_insert(
  ava_bxlist list,
  size_t index,
  const void*restrict data,
  size_t num_elements);
/**
 * Deletes a sequence of items from a B+ List.
 *
 * Complexity: O(log(n)) allocation, O(log(n)) compute
 *
 * @param list The list from which to delete elements.
 * @param index The index of the first element to delete.
 * @param num_elements The number of elements to delete.
 * @return A B+ List containing the same elements as list, except without the
 * elements between index and index+num_elements.
 */
ava_bxlist ava_bxlist_delete(
  ava_bxlist list,
  size_t index,
  size_t num_elements);

/**
 * Concatenates two B+ Lists.
 *
 * It is assumed that both lists contain the same element type using the same
 * weight function. Behaviour is undefined if this is not the case.
 *
 * Complexity: O(log(n_l) + log(n_r)) runtime and allocation in most cases. If
 * both lists share the same ancestry, it may be O(min(n,m)) instead.
 *
 * @param left The first B+ List.
 */
ava_bxlist ava_bxlist_concat(ava_bxlist left, ava_bxlist right);

/**
 * Returns the value weight of the given B+ List.
 *
 * The weight is the sum of all values contained within the B+ List, including
 * those not reachable from this version but which are still held by reference.
 */
size_t ava_bxlist_weight(ava_bxlist list) AVA_PURE;
/**
 * Returns the number of elements in the given B+ List.
 */
size_t ava_bxlist_length(ava_bxlist list) AVA_PURE;

/**
 * Returns a pointer to one or more elements within a B+ List.
 *
 * @param list The list in which to access elements.
 * @param index The index of the first element to access. Must be less than the
 * length of the list.
 * @param available If non-NULL, the actual number of elements accessible (via
 * array access) from the return value is written back into the pointee. This
 * is guaranteed to always be at least 1, but will often be greater except in
 * highly patched lists.
 * @return A pointer to the element at the given index, and possibly more
 * beyond it.
 */
const void* ava_bxlist_access(ava_bxlist list,
                              size_t index,
                              size_t*restrict available) AVA_PURE;

#endif /* AVA_RUNTIME__BXLIST_H_ */
