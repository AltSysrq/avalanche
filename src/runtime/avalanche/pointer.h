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
#error "Don't include avalanche/pointer.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_POINTER_H_
#define AVA_RUNTIME_POINTER_H_

#include "defs.h"
#include "string.h"
#include "value.h"
#include "integer.h"

#include "pointer-trait.h"

/**
 * Attribute tag used with ava_pointer_prototype.
 */
extern const ava_attribute_tag ava_pointer_prototype_tag;

/**
 * Stores the tag and constness of a pointer in the attribute chain.
 *
 * When possible, these objects should be declared statically (see
 * AVA_INIT_POINTER_PROTOTYPE) to avoid unnecessary heap allocations.
 */
typedef struct {
  ava_attribute header;
  /**
   * The tag, or empty string if no tag.
   */
  ava_string tag;
  /**
   * Whether the pointer is const.
   */
  ava_bool is_const;
} ava_pointer_prototype;

#ifndef AVA_IN_POINTER_C
/**
 * The "next" attribute after an ava_pointer_prototype.
 */
extern const ava_attribute ava_pointer_pointer_impl;
#endif

/**
 * Expands to a static initialiser for an ava_pointer_prototype.
 *
 * @param tag_ A constant expression evaluating to an ava_string indicating the
 * tag for this prototype.
 * @param is_const_ A constant expression evaluating to an ava_bool indicating
 * whether the prototype represents a const pointer.
 */
#define AVA_INIT_POINTER_PROTOTYPE(tag_, is_const_) {                   \
    .header = {                                                         \
      .tag = &ava_pointer_prototype_tag,                                \
      .next = (const ava_attribute*)&ava_pointer_pointer_impl           \
    },                                                                  \
    .tag = tag_,                                                        \
    .is_const = is_const_                                               \
  }

/**
 * Standard prototype for untagged void* pointers.
 */
extern const ava_pointer_prototype ava_pointer_proto_mut_void;
/**
 * Standard prototype for untagged const void* pointers.
 */
extern const ava_pointer_prototype ava_pointer_proto_const_void;

/**
 * Constructs a pointer with the given prototype and native pointer.
 */
ava_pointer_value ava_pointer_of_proto(
  const ava_pointer_prototype*restrict prototype,
  const void*restrict ptr);

/**
 * Parses the given string into a pointer prototype.
 *
 * @param protostr The string to parse.
 * @return A prototype parsed from the string, or NULL if protostr is not a
 * valid prototype string.
 */
const ava_pointer_prototype* ava_pointer_prototype_parse(ava_string protostr);

/**
 * Converts the given pointer prototype to a string.
 */
ava_string ava_pointer_prototype_to_string(
  const ava_pointer_prototype* prototype);

#endif /* AVA_RUNTIME_POINTER_H_ */
