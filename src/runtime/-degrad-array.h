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
#ifndef AVA_RUNTIME__DEGRAD_ARRAY_H_
#define AVA_RUNTIME__DEGRAD_ARRAY_H_

#include "avalanche/value.h"

#define AVA_DARRAY_ELTS_PER_ZONE 64
#define AVA_DARRAY_ZONES_PER_PAGE 64
#define AVA_DARRAY_ELTS_PER_PAGE \
  (AVA_DARRAY_ELTS_PER_ZONE*AVA_DARRAY_ZONES_PER_PAGE)

/**
 * A pointer to the degradation array proper.
 */
typedef struct ava_darray_handle_s ava_darray_handle;
/**
 * A timestamp for version tracking within a degradation array.
 *
 * Client code cannot do anything meaningful with values of this type but
 * preserve them.
 */
typedef ava_uint ava_darray_timestamp;
/**
 * A partial length for version tracking within a degradation array.
 *
 * Client code cannot do anything meaningful with values of this type but
 * preserve them.
 */
typedef ava_uint ava_darray_length_offset;

/**
 * A degradation array provides very efficient implementations of the most
 * common list operations; in most cases, it behaves like a dynamic array.
 *
 * This struct is only exposed in the header so that it can be used as a value
 * type. Use ava_darray_into_value() and ava_darray_from_value() to
 * store/retrieve its representation as an ava_value.
 */
typedef struct {
  const ava_darray_handle*restrict handle;
  ava_darray_timestamp toff;
  ava_darray_length_offset loff;
} ava_darray;

/**
 * Describes the data within a degradation array.
 */
typedef struct {
  /**
   * The size of each element in the degradation array.
   *
   * Elements are guaranteed to always be stored contiguously and to be at
   * least pointer-aligned. Client data is only ever stored in memory allocated
   * with (*allocator)().
   *
   * This MUST be a multiple of sizeof(ava_uint).
   */
  size_t elt_size;
  /**
   * Allocator for data regions used to contain client data.
   *
   * This is virtually always ava_alloc() or ava_alloc_atomic().
   *
   * While the usage of the returned memory is unspecified, it is guaranteed
   * that no pointers will be added by the degradation array itself. (Pointers
   * in client data notwithstanding.)
   *
   * @param sz The amount of memory to allocate. This is not guaranteed to be a
   * multiple of the element size.
   * @return Newly-allocated memory in which to store client data.
   */
  void* (*allocator)(size_t sz);
  /**
   * Returns the "weight" of the given data.
   *
   * @param data A pointer to an arbitrary amount of client data. This is
   * guaranteed to be element-aligned.
   * @param sz The number of bytes of data to weigh. This is guaranteed to be a
   * multiple of the element size.
   * @return The weight of the data.
   */
  size_t (*weight_function)(const void*restrict data, size_t sz);
} ava_darray_spec;

/**
 * Stores the ava_darray fields within the given value in a way that can be
 * retrieved from ava_darray_from_value().
 *
 * ava_darray does not provide its own list value type; it is up to the client
 * to provide the type field of the value.
 *
 * This completely uses both r1 and r2 of the value.
 */
static inline void ava_darray_into_value(ava_value*restrict value,
                                         ava_darray array) {
  value->r1.ptr = array.handle;
  value->r2.uints[0] = array.toff;
  value->r2.uints[1] = array.loff;
}

/**
 * Extracts the ava_darray stored in the given value by
 * ava_darray_into_value().
 */
static inline ava_darray ava_darray_from_value(ava_value value) {
  return (ava_darray) {
    .handle = value.r1.ptr,
    .toff = value.r2.uints[0],
    .loff = value.r2.uints[1]
  };
}

/**
 * Creates a new, empty degradation array.
 *
 * @param spec Information about the type of element the degradation array will
 * contain.
 * @param initial_capacity A hint about the initial capacity to allocate. The
 * actual initial capacity may be greater than this value.
 * @return The new degradation array.
 */
ava_darray ava_darray_new(const ava_darray_spec* spec,
                          size_t initial_capacity);

/**
 * Accesses elements within a degradation array.
 *
 * Complexity: Amortised O(1). Non-amortised, usually O(1), O(log(n))
 * worst-case.
 *
 * @param array The array in which to access elements.
 * @param index The index of the first element to access.
 * @param available If non-NULL, will be set to the number of elements directly
 * accessible following index. Always at least 1.
 * @return A pointer to at least one element at index, and possibly subsequent
 * elements that happen to be contiguous.
 */
const void* ava_darray_access(ava_darray array,
                              size_t index,
                              size_t*restrict available);

/**
 * Returns the number of elements a degradation array.
 *
 * Complexity: O(1)
 *
 * @param array The array whose length to query
 * @return The number of elements in array
 */
size_t ava_darray_length(ava_darray array) AVA_PURE;

/**
 * Appends elements to a degradation array.
 *
 * Complexity: Amortised O(count); O(n+count) worst-case.
 *
 * @param The array to which to append elements.
 * @param data The data to append to the array.
 * @param count The number of elements to append.
 * @return An array consisting of all the elements in array followed by count
 * elements from data.
 */
ava_darray ava_darray_append(ava_darray array,
                             const void*restrict data,
                             size_t count);

/**
 * Overwrites elements in a degradation array.
 *
 * Complexity: Amortised O(count); O(count + count*log(n)) worst-case.
 *
 * @param array The array in which to overwrite elements.
 * @param index The index of the first element to overwrite.
 * @param data The elements to use as a replacement.
 * @param count The number of elements to replace.
 * @return An array containing the same elements as array, except with elements
 * from index to index+count replaced with those in data.
 */
ava_darray ava_darray_overwrite(ava_darray array,
                                size_t index,
                                const void*restrict data,
                                size_t count);

#endif /* AVA_RUNTIME__DEGRAD_ARRAY_H_ */
