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
#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/list.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_LIST_H_
#define AVA_RUNTIME_LIST_H_

#include "defs.h"
#include "value.h"

#include "list-trait.h"

/**
 * @file
 *
 * Defines common semantics for lists.
 *
 * A list's elements are roughly the tokens produced by tokenising the string;
 * see ava_list_value for more details. Elements of a list are indexed from 0.
 *
 * There is no single "list" type; the list trait is used to access efficient
 * list manuplation functionality independent of the type.
 */

/**
 * Copies the given list into a new list using a reasonable type for the
 * contents. The result will be a normalised list.
 *
 * The only real use for this is for pseudo-list implementations that cannot
 * actually implement list mutation operations themselves.
 */
ava_fat_list_value ava_list_copy_of(ava_fat_list_value list,
                                    size_t begin, size_t end);

/**
 * Returns a list containing the given sequence of values.
 *
 * @param values The values that comprise the list. This array is copied, and
 * need not be prerved after this function returns.
 * @param count The number of values to copy.
 */
ava_list_value ava_list_of_values(const ava_value*restrict values,
                                  size_t count);

/**
 * Escapes the given string so that it can be used in a string representation
 * of a list.
 */
ava_string ava_list_escape(ava_string);

/**
 * The standard implementation of ava_value.string_chunk_iterator() for
 * normalised lists.
 */
ava_datum ava_list_string_chunk_iterator(ava_value);

/**
 * The standard implementation of ava_value.iterate_string_chunk() for
 * normalised lists.
 */
ava_string ava_list_iterate_string_chunk(ava_datum*restrict i,
                                         ava_value val);

/**
 * Implementation of ava_list_trait.slice which copies the input list into a
 * new list of an unspecified type.
 */
ava_list_value ava_list_copy_slice(ava_list_value list, size_t begin, size_t end);
/**
 * Implementation of ava_list_trait.append which copies the input list and new
 * element into a new list of an unspecified type.
 */
ava_list_value ava_list_copy_append(ava_list_value list, ava_value elt);
/**
 * Implementation of ava_list_trait.concat which copies the lists into a new
 * list of an unspecified type.
 */
ava_list_value ava_list_copy_concat(ava_list_value left, ava_list_value right);
/**
 * Implementation of ava_list_trait.remove which copies the list into a new
 * list of unspecified type.
 */
ava_list_value ava_list_copy_remove(ava_list_value list,
                                    size_t begin, size_t end);
/**
 * Implementation of ava_list_trait.copy which copies the list into a new list
 * of unspecified type.
 */
ava_list_value ava_list_copy_set(ava_list_value list, size_t ix, ava_value val);

/**
 * The empty list.
 */
ava_list_value ava_empty_list(void) AVA_CONSTFUN;

#endif /* AVA_RUNTIME_LIST_H_ */
