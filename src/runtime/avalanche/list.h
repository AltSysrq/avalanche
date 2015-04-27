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
#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/list.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_LIST_H_
#define AVA_RUNTIME_LIST_H_

#include "defs.h"
#include "value.h"

/**
 * @file
 *
 * Defines common semantics for lists.
 *
 * A list is a string acceptable by the standard lexer. Only simple tokens (ie,
 * barewords, a-strings, and verbatims) and newline tokens are permitted. The
 * list is said to contain one element for each simple token it contains;
 * newline tokens are discarded.
 *
 * Elements of a list are indexed from 0.
 *
 * Normal form for a list is comprised of each element string escaped with
 * ava_list_escape(), separated by exactly one space character.
 *
 * There is no single "list" type; the list accelerator is used to access
 * efficient list manuplation functionality independent of the type.
 */

/**
 * Accelerator for list manipulation.
 *
 * The return value for query_accelerator() is a const ava_list_iface*.
 * Implementing this accelerator implies that (a) every value of the backing
 * type is guaranteed to conform to the list format; (b) the underlying type
 * can provide efficient amortised access to the elements of the list as
 * defined by that format.
 *
 * Clients should generally use ava_list_value_of() rather than directly
 * interacting with the accelerator.
 */
AVA_DECLARE_ACCELERATOR(ava_list_accelerator);

typedef struct ava_list_iface_s ava_list_iface;

/**
 * Like an ava_value, but with a list vtable instead of a type.
 */
typedef struct {
  /**
   * r1 and r2 from ava_value.
   */
  ava_datum r1, r2;
  /**
   * The implementation of the list primitives for this list.
   */
  const ava_list_iface*restrict v;
} ava_list_value;

/**
 * Defines the operations that can be performed on a list structure.
 *
 * Documnented complexities of methods are the worst acceptable complexities
 * for implementations; implementations are often better in practise.
 */
struct ava_list_iface_s {
  /**
   * Converts the list back into a normal value.
   *
   * Requirement:
   *   to_value(ava_list_value_of(value)) == value
   *   Ie, the value produced must be exatcly equal to the value used to obtain
   *   the iface, if there was one. Typically this function just copies r1 and
   *   r2 into an ava_value and sets the type.
   */
  ava_value (*to_value)(ava_list_value list);

  /**
   * Returns the number of elements in the list.
   *
   * Complexity: O(1)
   */
  size_t (*length)(ava_list_value list);
  /**
   * Returns the element in the list at the given index.
   *
   * Effect is undefined if index >= length().
   *
   * Complexity: Amortised O(log(length))
   */
  ava_value (*index)(ava_list_value list, size_t index);
  /**
   * Returns a new list containing the elements of list between begin,
   * inclusive, and end, exclusive.
   *
   * Effect is undefined if begin > end or end > length().
   *
   * Complexity: Amortised O(log(length) + (end - begin))
   */
  ava_list_value (*slice)(ava_list_value list, size_t begin, size_t end);

  /**
   * Returns a new list which contains all the elements of list followed by the
   * given element.
   *
   * Complexity: Amortised O(log(length))
   */
  ava_list_value (*append)(ava_list_value list, ava_value element);
  /**
   * Returns a new list which contains all the elements of left followed by all
   * the elements of right.
   *
   * Complexity: Amortised O(log(length))
   */
  ava_list_value (*concat)(ava_list_value left, ava_list_value right);
  /**
   * Returns a new list which contains the elements in list from 0 to begin
   * exclusive, and from end inclusive to the end of list.
   *
   * Effect is undefined if begin > end or end >= length().
   *
   * Complexity: Amortised O(log(length))
   */
  ava_list_value (*delete)(ava_list_value list, size_t begin, size_t end);

  /**
   * The size of an iterator used for this list type.
   *
   * For all iterator_*() methods, the caller will supply a pointer-aligned
   * region of memory whose size is at least this value.
   *
   * Iterators are expected to be pure values; eg, if an iterator value is
   * copied, the copy and the original operate independently.
   *
   * Clients typically allocate iterators on the stack, so this should be
   * reasonably small.
   */
  size_t (*iterator_size)(ava_list_value list);
  /**
   * Positions the given iterator at the given index within the given list.
   *
   * ix may be beyond the end of the list. The resulting iterator is considered
   * invalid, but it must be possible to use iterator_move() to move the
   * iterator back into a valid range.
   *
   * Complexity: Amortised O(log(length))
   */
  void (*iterator_place)(ava_list_value list, void*restrict iterator, size_t ix);
  /**
   * Returns the value within the given list currently addressed by the given
   * iterator.
   *
   * The list must be the same list used with iterator_place().
   *
   * Effect is undefined if the iterator is outside the boundary of the list.
   *
   * Complexity: O(1)
   */
  ava_value (*iterator_get)(ava_list_value list, const void*restrict iterator);
  /**
   * Moves the given iterator within the given list by the given offset.
   *
   * The list must be the same list used with iterator_place().
   *
   * It is legal to move the iterator outside the string boundaries. Such an
   * iterator is considered invalid, but later moves may make it valid if they
   * move it back into the range of the list.
   *
   * Complexity: Amortised O(1) for small off
   */
  void (*iterator_move)(ava_list_value list, void*restrict iterator, ssize_t off);
  /**
   * Returns the current index of the given iterator, even if it is beyond the
   * end of the list.
   *
   * The list must be the same list used with iterator_place().
   *
   * Complexity: O(1)
   */
  size_t (*iterator_index)(ava_list_value list, const void*restrict iterator);
};

/**
 * Produces a *possibly normalised* ava_list_value from the given ava_value.
 *
 * If the value does not currently have a type that permits efficient list
 * access, it will be converted to one that does. In such a case, the string
 * value of the new list will be the same as the string value of value. (I.e.,
 * no normalisation occurs).
 *
 * Note that ava_to_string(ava_list_value_of(x)) is not necessarily equivalent
 * to ava_to_string(x); the returned value is conceptually different.
 *
 * @throws ava_format_exception if value is not conform to list format.
 */
ava_list_value ava_list_value_of(ava_value value);

/**
 * Copies the given list into a new list using a reasonable type for the
 * contents. The result will be a normalised list.
 *
 * The only real use for this is for pseudo-list implementations that cannot
 * actually implement list mutation operations themselves.
 */
ava_list_value ava_list_copy_of(ava_list_value list, size_t begin, size_t end);

/**
 * Declares a local variable named name which is usable as an iterator for the
 * given list.
 *
 * Note that the variable is an array, so it is passed to the iterator methods
 * without an explicit reference.
 */
#define AVA_LIST_ITERATOR(list, name)                                   \
  void* name[((list).v->iterator_size(list) + sizeof(void*) - 1)/sizeof(void*)]

/**
 * Escapes the given string so that it can be used in a string representation
 * of a list.
 */
ava_string ava_list_escape(ava_string);

/**
 * The empty list.
 */
extern const ava_list_value ava_empty_list;

#endif /* AVA_RUNTIME_LIST_H_ */
