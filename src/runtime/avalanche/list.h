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
 * A normal value of list format.
 *
 * A list is a string acceptable by the standard lexer. Only simple tokens (ie,
 * barewords, a-strings, and verbatims) and newline tokens are permitted. The
 * list is said to contain one element for each simple token it contains;
 * newline tokens are discarded.
 *
 * Normal form for a list is comprised of each element string escaped with
 * ava_list_escape(), separated by exactly one space character.
 */
typedef struct { ava_value v; } ava_list_value;

/**
 * Trait tag for list manipulation.
 *
 * Implementing this trait implies that (a) every value of the backing type is
 * guaranteed to conform to the list format; (b) the underlying type can
 * provide efficient amortised access to the elements of the list as defined by
 * that format.
 */
extern const ava_attribute_tag ava_list_trait_tag;

typedef struct ava_list_trait_s ava_list_trait;

/**
 * An ava_value with a pre-extracted ava_list_trait.
 */
typedef struct {
  /**
   * The implementation of the list primitives for this list.
   */
  const ava_list_trait*restrict v;
  /**
   * The actual value.
   */
  ava_list_value c;
} ava_fat_list_value;

/**
 * Defines the operations that can be performed on a list structure.
 *
 * Supporting this trait implies that the value can be used directly as an
 * ava_list_value; ie, that it is a normal value of list format.
 *
 * Documnented complexities of methods are the worst acceptable complexities
 * for implementations; implementations are often better in practise.
 *
 * All "mutating" methods can be implemented with the ava_list_copy_*()
 * functions, if the underlying implementation has no better way to implement
 * them.
 */
struct ava_list_trait_s {
  ava_attribute header;

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
   * Complexity: Amortised O(1)
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
   * Complexity: Amortised O(1)
   */
  ava_list_value (*append)(ava_list_value list, ava_value element);
  /**
   * Returns a new list which contains all the elements of left followed by all
   * the elements of right.
   *
   * Complexity: Amortised O(length)
   */
  ava_list_value (*concat)(ava_list_value left, ava_list_value right);
  /**
   * Returns a new list which contains the elements in list from 0 to begin
   * exclusive, and from end inclusive to the end of list.
   *
   * Effect is undefined if begin > end or end >= length().
   *
   * Complexity: Amortised O(length)
   */
  ava_list_value (*delete)(ava_list_value list, size_t begin, size_t end);

  /**
   * Returns a new list which contains the elements in list, except with the
   * element at the selected index replaced with the given value.
   *
   * Behaviour is undefined if index > length().
   *
   * Complexity: Amortised O(1)
   */
  ava_list_value (*set)(ava_list_value list, size_t index, ava_value element);
};

/**
 * Converts the given value into a normal value of list format.
 *
 * @throws ava_format_exception if value is not conform to list format.
 */
ava_list_value ava_list_value_of(ava_value value) AVA_PURE;

/**
 * Produces a *normalised* ava_fat_list_value from the given ava_value.
 *
 * If the value does not currently have a type that permits efficient list
 * access, it will be converted to one that does. In such a case, the string
 * value of the new list will be the same as the string value of value. (I.e.,
 * no normalisation occurs).
 *
 * Note that ava_to_string(ava_fat_list_value_of(x)) is not necessarily equivalent
 * to ava_to_string(x); the returned value is conceptually different.
 *
 * @throws ava_format_exception if value is not conform to list format.
 */
ava_fat_list_value ava_fat_list_value_of(ava_value value) AVA_PURE;

/**
 * Copies the given list into a new list using a reasonable type for the
 * contents. The result will be a normalised list.
 *
 * The only real use for this is for pseudo-list implementations that cannot
 * actually implement list mutation operations themselves.
 */
ava_fat_list_value ava_list_copy_of(ava_fat_list_value list, size_t begin, size_t end);

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
 * Implementation of ava_list_trait.delete which copies the list into a new
 * list of unspecified type.
 */
ava_list_value ava_list_copy_delete(ava_list_value list,
                                    size_t begin, size_t end);
/**
 * Implementation of ava_list_trait.copy which copies the list into a new list
 * of unspecified type.
 */
ava_list_value ava_list_copy_set(ava_list_value list, size_t ix, ava_value val);

#define ava_list_length(list)                                   \
  _Generic((list),                                              \
           ava_value:           ava_list_length_v,              \
           ava_list_value:      ava_list_length_lv)(list)

static inline size_t ava_list_length_v(ava_value list_val) {
  ava_fat_list_value list = ava_fat_list_value_of(list_val);
  return list.v->length(list.c);
}

static inline size_t ava_list_length_lv(ava_list_value list_val) {
  return ava_list_length_v(list_val.v);
}

#define ava_list_index(list, ix)                        \
  _Generic((list),                                      \
           ava_value:           ava_list_index_v,       \
           ava_list_value:      ava_list_index_lv)      \
  ((list), (ix))

static inline ava_value ava_list_index_v(ava_value list_val, size_t ix) {
  ava_fat_list_value list = ava_fat_list_value_of(list_val);
  return list.v->index(list.c, ix);
}

static inline ava_value ava_list_index_lv(ava_list_value list_val, size_t ix) {
  return ava_list_index_v(list_val.v, ix);
}

#define ava_list_slice(list, begin, end)                \
  _Generic((list),                                      \
           ava_value:           ava_list_slice_v,       \
           ava_list_value:      ava_list_slice_lv)      \
  ((list), (begin), (end))

static inline ava_list_value ava_list_slice_lv(ava_list_value list_val,
                                               size_t begin, size_t end) {
  ava_fat_list_value list = ava_fat_list_value_of(list_val.v);
  return list.v->slice(list.c, begin, end);
}

static inline ava_value ava_list_slice_v(ava_value list_val,
                                         size_t begin, size_t end) {
  return ava_list_slice_lv(ava_list_value_of(list_val), begin, end).v;
}

#define ava_list_append(list, elt)                      \
  _Generic((list),                                      \
           ava_value:           ava_list_append_v,      \
           ava_list_value:      ava_list_append_lv)     \
  ((list), (elt))

static inline ava_list_value ava_list_append_lv(ava_list_value list_val,
                                                ava_value elt) {
  ava_fat_list_value list = ava_fat_list_value_of(list_val.v);
  return list.v->append(list.c, elt);
}

static inline ava_value ava_list_append_v(ava_value list_val, ava_value elt) {
  return ava_list_append_lv(ava_list_value_of(list_val), elt).v;
}

#define ava_list_concat(left, right)                    \
  _Generic((left),                                      \
           ava_value:           ava_list_concat_v,      \
           ava_list_value:      ava_list_concat_lv)     \
  ((left), (right))

static inline ava_list_value ava_list_concat_lv(ava_list_value left,
                                                ava_list_value right) {
  ava_fat_list_value list = ava_fat_list_value_of(left.v);
  return list.v->concat(list.c, right);
}

static inline ava_value ava_list_concat_v(ava_value left, ava_value right) {
  return ava_list_concat_lv(
    ava_list_value_of(left), ava_list_value_of(right)).v;
}

#define ava_list_delete(list, begin, end)               \
  _Generic((list),                                      \
           ava_value:           ava_list_delete_v,      \
           ava_list_value:      ava_list_delete_lv)     \
  ((list), (begin), (end))

static inline ava_list_value ava_list_delete_lv(
  ava_list_value list, size_t begin, size_t end
) {
  ava_fat_list_value l = ava_fat_list_value_of(list.v);
  return l.v->delete(l.c, begin, end);
}

static inline ava_value ava_list_delete_v(
  ava_value list, size_t begin, size_t end
) {
  return ava_list_delete_lv(ava_list_value_of(list),
                            begin, end).v;
}

#define ava_list_set(list, index, element)              \
  _Generic((list),                                      \
           ava_value:           ava_list_set_v,         \
           ava_list_value:      ava_list_set_lv)        \
  ((list), (index), (element))

static inline ava_list_value ava_list_set_lv(
  ava_list_value list, size_t index, ava_value element
) {
  ava_fat_list_value l = ava_fat_list_value_of(list.v);
  return l.v->set(l.c, index, element);
}

static inline ava_value ava_list_set_v(
  ava_value list, size_t index, ava_value element
) {
  return ava_list_set_lv(ava_list_value_of(list), index, element).v;
}

/**
 * The empty list.
 */
ava_list_value ava_empty_list(void);

#endif /* AVA_RUNTIME_LIST_H_ */
