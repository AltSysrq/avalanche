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
#ifndef AVA_RUNTIME__ATREE_H_
#define AVA_RUNTIME__ATREE_H_

/**
 * Internal type; in the header so it can be used in immediates.
 *
 * Used to track the t^R of an ava_atree_node reference.
 */
typedef ava_ushort ava_atree_timestamp;
/**
 * Opaque struct.
 *
 * An ava_atree reference is constructed from a pointer to this struct and an
 * ava_atree_attr.
 *
 * Within an ava_value, this is usually stored in r1.ptr.
 */
typedef struct ava_atree_node_s ava_atree_node;

/**
 * Auxilliary data required for an ava_atree reference.
 *
 * Within an ava_value, this is usually stored in r2.ulong.
 */
typedef union {
  /**
   * The attr as a 64-bit integer, for storage in an ava_value.
   */
  ava_ulong ulong;
  struct {
    /**
     * The t^R of this reference.
     */
    ava_ulong timestamp : sizeof(ava_atree_timestamp)*8;
    /**
     * The k^R of this reference.
     */
    ava_ulong length    : (sizeof(ava_ulong) - sizeof(ava_atree_timestamp))*8;
  } v;
} ava_atree_attr;

/**
 * An ATree is a persistent array-like data structure which provides efficient
 * implementation of appending, indexing, and in-place updates.
 *
 * Other operations require a full copy (as with normal arrays).
 */
typedef struct {
  /**
   * The reference to the actual tree data.
   */
  const ava_atree_node*restrict root;
  /**
   * Auxilliary data required to make the reference meaningful.
   */
  ava_atree_attr attr;
} ava_atree;

/**
 * Describes the elements stored by an ava_atree.
 */
typedef struct {
  /**
   * The size, in bytes, of each element.
   */
  size_t elt_size;
  /**
   * A function which "weighs" each element, similar to ava_value_weight().
   *
   * @param data The element(s) to weigh
   * @param num_elements The number of elements to weigh.
   * @return The total weight of the given elements.
   */
  size_t (*weight_function)(const void*restrict data, size_t num_elements);
} ava_atree_spec;

/**
 * Allocates a new, empty ava_atree.
 *
 * @param spec Information on the elements the tree will store. All invocations
 * of ava_atree functions on the return value which require a spec must be
 * given the same spec.
 */
ava_atree ava_atree_new(const ava_atree_spec*restrict spec);

/**
 * Returns a pointer to the ixth element of tree.
 *
 * @param tree The tree in which to access the element.
 * @param ix The index of the first element to retrieve. Behaviour is undefined
 * if ix >= ava_atree_length(tree).
 * @param spec The spec for the elements in the tree.
 * @param avail If non-NULL, *avail is set to the number of elements available
 * from the return value. This is always at least 1.
 * @return A pointer to the element at ix, and possibly more following it.
 */
const void* ava_atree_get(ava_atree tree, size_t ix,
                          const ava_atree_spec*restrict spec,
                          size_t*restrict avail);

/**
 * Appends elements to the given ava_atree.
 *
 * @param tree The tree to which to append elements.
 * @param data The elements to append.
 * @param num_elts The number of elements to append.
 * @param spec The element spec.
 * @return An ava_atree containing all the elements in tree followed by the
 * num_elts elements in data.
 */
ava_atree ava_atree_append(ava_atree tree,
                           const void*restrict data,
                           size_t num_elts,
                           const ava_atree_spec*restrict spec);

/**
 * Returns the number of elements in tree.
 */
static inline size_t ava_atree_length(ava_atree tree) {
  return tree.attr.v.length;
}

/**
 * Returns the cumulative "weight" of the given tree.
 */
size_t ava_atree_weight(ava_atree tree);

#endif /* AVA_RUNTIME__ATREE_H_ */
