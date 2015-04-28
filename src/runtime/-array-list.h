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
 * The threshold above which btree lists should be preferred over array lists.
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
