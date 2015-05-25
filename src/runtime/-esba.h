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
#ifndef AVA_RUNTIME__ESBA_H_
#define AVA_RUNTIME__ESBA_H_

#include "avalanche/defs.h"
#include "avalanche/value.h"

/**
 * @file
 *
 * Provides facilities for creating and modifying Eager Shallow-Binding Arrays,
 * or ESBAs.
 *
 * What exactly an ESBA is is described in more detail in esba.c. From a client
 * perspective, an ESBA is a fully persistent data structure which in the most
 * common use cases has performance similar to a non-persistent dynamic array.
 *
 * The ESBA implementation does not itself interface with list values, but is
 * used as the basis for larger lists.
 */

/**
 * Opaque structure which contains esba data.
 *
 * An ava_esba_handle is an ava_attribute tagged with ava_esba_handle_tag.
 */
typedef struct ava_esba_handle_s ava_esba_handle;

/**
 * Tag attached to ava_esba_handles.
 */
extern const ava_attribute_tag ava_esba_handle_tag;

/**
 * A "fat pointer" to an ESBA.
 */
typedef struct {
  /**
   * The handle on the ESBA.
   *
   * In an ava_value, used as an attribute.
   */
  ava_esba_handle* handle;
  /**
   * The length of the ESBA.
   *
   * In an ava_value, conventionally in the value ulong.
   *
   * Don't access this directly; use ava_esba_length() instead.
   */
  size_t length;
} ava_esba;

/**c
 * An opaque value used for tracking ASBA transactions.
 *
 * No special action is required to destroy a transaction.
 */
typedef struct {
  const void*restrict ptr;
} ava_esba_tx;

/**
 * Function to "weigh" elements in an ESBA.
 *
 * @param next_attr The value of "next_attr" passed into ava_esba_new().
 * @param elements The elements to weigh.
 * @param num_elements The number of elements pointed to by elements.
 * @return The total weight of the elements.
 */
typedef size_t (*ava_esba_weight_function)(
  const void*restrict next_attr,
  const void*restrict elements,
  size_t num_elements);

/**
 * Allocates a new, empty ESBA.
 *
 * @param element_size The size of elements, in bytes, that will be stored in
 * the ESBA. Must be a multiple of sizeof(void*).
 * @param initial_capacity The minimum number of elements the ESBA is
 * guaranteed to be able to hold. Actual capacity may be greater.
 * @param weight_function A function to "weigh" elements added to the array.
 * @param allocator Function compatible with ava_alloc_atomic() to use to
 * allocate the array and any arrays derived from it. Callers must provide
 * ava_alloc() if their elements may have pointers within.
 * @param next_attr The value of the "next" field on the attribute field of the
 * handle. It need not be an actual ava_attribute unless the handle is used in
 * a context that would require it to be.
 * @return The new, empty array.
 */
ava_esba ava_esba_new(size_t element_size,
                      size_t initial_capacity,
                      ava_esba_weight_function weight_function,
                      void* (*allocator)(size_t),
                      const void*restrict next_attr);

/**
 * Starts a read transaction against the given ESBA.
 *
 * Transactions are essentially free: they only consume stack space in the
 * caller; there is no other overhead associated with keeping one open. No
 * action is required to destroy them.
 *
 * The operations the caller can do with the returned data are extremely
 * limited; the data within is not guaranteed to have valid data, or even to be
 * constant. This rules out, for example, dereferencing any pointers found
 * within. Generally, the only useful operation is to copy a subset of the data
 * to a stable location.
 *
 * ava_esba_check_access() can be used to test whether any concurrent
 * modifications occurred. If the check succeeds, the caller then knows that
 * whatever data it *already* obtained is in fact valid and consistent.
 *
 * Example usage:
 *
 *   mystruct copy;
 *   const mystruct* access;
 *   ava_esba_tx tx;
 *
 *   do {
 *     (* Get a pointer to the current version of the array *)
 *     access = ava_esba_access(esba, &tx);
 *     (* Copy a select element to stable (private) storage *)
 *     copy = access[42];
 *     (* Ensure that the read was valid. If check_access()
 *      * returns false, the data we read may have been modified
 *      * in-flight, and may contain garbage. In this case, simply
 *      * try again with another loop iteration.
 *      *)
 *   } while (!ava_esba_check_access(esba, access, tx));
 *   (* The access test succeeded, copy contains valid data.
 *    *
 *    * Note that *nothing* can be said about the contents of
 *    * *access unless another check_access() call is made
 *    * thereafter.
 *    *)
 *   do_something(copy);
 *
 * @param esba The array to access.
 * @param tx Set to the context data of the transaction, so it can be passed
 * into ava_esba_check_access().
 * @return A pointer to the volatile array data.
 * @see ava_esba_check_access()
 */
const void* ava_esba_access(ava_esba esba, ava_esba_tx* tx);
/**
 * Checks whether the data read in a transaction started by ava_esba_access()
 * can be considered valid.
 *
 * This does not "destroy" the transaction; it is sensible to call this
 * function multiple times with the same transaction context.
 *
 * @param esba The ESBA that was accessed.
 * @param accessed The array returned from the corresponding call to
 * ava_esba_access().
 * @param tx The tx initialised by the corresponding call to ava_esba_access().
 * @return Whether the transaction was successful; if false, the data array
 * returned by ava_esba_access() may have been modified between that call and
 * the call to ava_esba_check_access().
 * @see ava_esba_access()
 */
ava_bool ava_esba_check_access(ava_esba esba, const void* accessed, ava_esba_tx tx);

/**
 * Appends elements to the end of the array.
 *
 * @param esba The array to which to append elements.
 * @param data An array of elements to append, of length at least num_elements.
 * @param num_elements The number of elements to append.
 * @return An ESBA containing all the elements of esba followed by num_elements
 * elements from data.
 */
ava_esba ava_esba_append(ava_esba esba, const void*restrict data,
                         size_t num_elements);

/**
 * Begins an externally-controlled append operation.
 *
 * The given ESBA will be modified as necessary to permit the insertion of
 * exactly num_elements to the end of the array, and a pointer to that memory
 * is returned.
 *
 * The caller MUST either call ava_esba_finish_append() with the resulting ESBA
 * and the same number of elements, or discard the resulting reference.
 *
 * The effect of passing the new ESBA to any function other than
 * ava_esba_finish_append() is undefined.
 *
 * @param esba The ESBA to use for appending. Modified as necessary so that the
 * append operation will be possible.
 * @param num_elements The number of elements the caller plans to append.
 * @return A pointer to the location where the first element may be appended.
 * The caller may copy arbitrary data to this location, but the amount of data
 * copied must be exactly num_elements*element_size. This memory is stable
 * until ava_esba_finish_append() is called.
 */
void* ava_esba_start_append(ava_esba* esba, size_t num_elements);
/**
 * Finalises an externally-controlled append to an ESBA, making it safe for
 * reading or passing to any other function.
 *
 * After this is called, the pointer that had been returned from
 * ava_esba_start_append() is no longer safe; its contents may change at any
 * time.
 *
 * The effect of passing an ESBA not freshly produced by
 * ava_esba_start_append() into this function is undefined.
 *
 * @param esba The ESBA produced by a call to ava_esba_start_append().
 * @param num_elements The value of num_elements passed into
 * ava_esba_start_append().
 */
void ava_esba_finish_append(ava_esba esba, size_t num_elements);

/**
 * Changes the value of an element within an ESBA.
 *
 * @param esba The array in which to change an element.
 * @param index The index of the element to set. Behaviour is undefined if
 * greater than ava_esba_length().
 * @param data The new value of the element.
 * @return An ESBA containing all the elements of esba, except with the element
 * at the chosen index replaced by data.
 */
ava_esba ava_esba_set(ava_esba esba, size_t index, const void*restrict data);

/**
 * Returns the number of elements in the given ESBA.
 */
static inline size_t ava_esba_length(ava_esba esba) {
  return esba.length;
}

/**
 * Returns the cumulative weight of the ESBA.
 *
 * This includes elements not visible through this reference, and may change
 * over time.
 */
size_t ava_esba_weight(ava_esba esba);

/**
 * Returns the next_attr that was associated with the given esba when it was
 * created.
 */
static inline const void* ava_esba_next_attr(ava_esba esba) {
  return ((const ava_attribute*restrict)esba.handle)->next;
}

#endif /* AVA_RUNTIME__ESBA_H_ */
