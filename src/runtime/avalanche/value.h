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
#error "Don't include avalanche/value.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_VALUE_H_
#define AVA_RUNTIME_VALUE_H_

#include "defs.h"
#include "string.h"

/******************** VALUE AND TYPE SYSTEM ********************/

/**
 * Avalanche's "everything is a string" type system, as in later Tcl versions,
 * is merely an illusion. In reality, higher-level representations of values
 * are maintained for the sake of performance.
 *
 * In Tcl, this was accomplished by heap-allocating values, each of which has
 * either or both of a string representation and a higher-level representation.
 * In Avalanche, values are actual immutable value types. They may have one or
 * two representations at any given time, but what those representations are is
 * determined by the dynamic type of the value.
 *
 * Core concepts:
 *
 * - Value. A value is an instance of ava_value. Values are almost always
 *   passed by value; this allows the C compiler to perform a large number of
 *   optimisations, in particular constant propagation of the dynamic type and
 *   dead store elimination. A value is associated with one or two
 *   representations and a type.
 *
 * - Type. A type defines (a) a set of permissible string values which is a
 *   subset the set of all strings; (b) a higher-level internal representation
 *   that may be more performant than raw strings; (c) a set of method
 *   implementations. The finite set of types in the process is determined at
 *   compile time; types can only be introduced by C code. Note tha types are
 *   mostly conceptual, rather than a physical part of the API. For the most
 *   part, a type can essentially be considered as the set of trait
 *   implementations on a value.
 *
 * - Representation. The physical way a value is stored. Without knowledge of
 *   the particular type, the representation of a value is totally opaque.
 *   Certain types, such as ava_string_type, MAY make their representation
 *   public.
 *
 * - Trait. A trait is a set of operations that are *guaranteed* to be
 *   supported on a value, generally in a manner more efficient than direct
 *   string manuplation. Traits are usually (but not necessarily) intimately
 *   connected to the type of a value. The lack of a trait on a value does
 *   *not* imply the operation cannot be performed on that value; rather, the
 *   value must first be reparsed as a type that does. There is no simple
 *   ava_trait structure; the generic API works with attributes directly, and
 *   every trait is a different extension of the attribute structure.
 *
 * - Attribute. A structure attached to a value. Every trait is an attribute,
 *   but not all attributes are traits. Certain types may store actual data in
 *   an attribute, which is then typically the first attribute in the chain.
 *
 * An important thing to note is that values *always* preserve their native
 * string representation. If a string "0x01" is interpreted as integer 1, the
 * value remains the string "0x01". Functions which return *new* values may
 * (and usually do) define themselves to produce _normalised_ values, in which
 * case this does not apply. (Eg, "0x0a"+"0" produces "10", not "0x0a".)
 */
typedef struct ava_value_s ava_value;

/**
 * A single representation of a value.
 *
 * The usage of this union is entirely up to the type on the value.
 */
typedef union {
  ava_ulong             ulong;
  ava_slong             slong;
  const void*restrict   ptr;
  /* There is no field
   *   ava_string       str;
   * because, on the System V AMD64 ABI, this (nesting unions) prevents the
   * value from being passed/returned in registers during function calls.
   *
   * Use ava_string_to_datum() and ava_string_of_datum() to store strings
   * within an ava_datumj
   */
} ava_datum;

/**
 * A tag identifying the type of an attribute on a value.
 *
 * Generally, code does nothing with this structure except compare pointers to
 * it.
 */
typedef struct {
  /**
   * A human-readable name for this attribute type.
   */
  const char* name;
} ava_attribute_tag;

/**
 * The basic structure shared by all attributes.
 *
 * This structure is not in and of itself useful, except to locate a desired
 * attribute; clients must cast it to the structure they actually want.
 */
typedef struct ava_attribute_s ava_attribute;

struct ava_attribute_s {
  /**
   * The tag identifying the meaning and usage of this attribute.
   */
  const ava_attribute_tag*restrict tag;
  /**
   * The next attribute in the list, or NULL if there are no further
   * attributes.
   */
  const ava_attribute*restrict next;
};

struct ava_value_s {
  /**
   * The dynamic type and attributes of this value.
   */
  const ava_attribute*restrict attr;
  /**
   * The representation of this value, as controlled by the type. Some of the
   * representation may be stored in attributes.
   */
  ava_datum r1;
};

/**
 * Converts an ava_string to an ava_datum.
 *
 * This can be reversed by ava_string_of_datum().
 */
static inline ava_datum ava_string_to_datum(ava_string str) {
  return (ava_datum) { .ulong = str.ascii9 };
}

/**
 * Converts an ava_datum to an ava_string.
 *
 * (This does not mean stringification; it merely inverts
 * ava_string_to_datum().)
 */
static inline ava_string ava_string_of_datum(ava_datum datum) {
  return (ava_string) { .ascii9 = datum.ulong };
}

/**
 * Constructs a value using the given string as its datum.
 */
static inline ava_value ava_value_with_str(
  const void* attr, ava_string str
) {
  return (ava_value) { .attr = attr,
                       .r1 = ava_string_to_datum(str) };
}

/**
 * Constructs a value using the given pointer as its datum.
 */
static inline ava_value ava_value_with_ptr(
  const void* attr, const void* ptr
) {
  return (ava_value) { .attr = attr, .r1 = { .ptr = ptr } };
}

/**
 * Constructs a value using the given signed long as its value.
 */
static inline ava_value ava_value_with_slong(
  const void* attr, ava_slong sl
) {
  return (ava_value) { .attr = attr, .r1 = { .slong = sl } };
}

/**
 * Constructs a value using the given unsigned long as its value.
 *
 * The ulong representation is suitable for opaquely moving ava_value contents
 * around separately.
 */
static inline ava_value ava_value_with_ulong(
  const void* attr, ava_ulong ul
) {
  return (ava_value) { .attr = attr, .r1 = { .ulong = ul } };
}

/**
 * Returns the root attribute on the given ava_value.
 */
static inline const void* ava_value_attr(ava_value v) {
  return v.attr;
}

/**
 * Returns the datum of the given ava_value interpreted as a string.
 */
static inline ava_string ava_value_str(ava_value v) {
  return ava_string_of_datum(v.r1);
}

/**
 * Returns the datum of the given ava_value interpreted as a pointer.
 */
static inline const void* ava_value_ptr(ava_value v) {
  return v.r1.ptr;
}

/**
 * Returns the datum of the given ava_value interpreted as a signed long.
 */
static inline ava_slong ava_value_slong(ava_value v) {
  return v.r1.slong;
}

/**
 * Returns the datum of the given ava_value interpreted as an unsigned long.
 *
 * The ulong representation is suitable for opaquely moving ava_value contents
 * around separately.
 */
static inline ava_ulong ava_value_ulong(ava_value v) {
  return v.r1.ulong;
}

/**
 * The tag for ava_value_trait.
 */
extern const ava_attribute_tag ava_value_trait_tag;

/**
 * The generic trait, which is present on all values.
 */
typedef struct {
  ava_attribute header;

  /**
   * A human-readable name of the type of the value, for diagnostic and
   * debugging purposes.
   */
  const char* name;

  /**
   * Defines how to reproduce the string representation of a value of this
   * type.
   *
   * Do not call this directly; use ava_to_string() instead.
   *
   * If the underlying type can produce string fragments in chunks more
   * efficiently, it should implement string_chunk_iterator() and
   * iterate_string_chunk() instead and use ava_string_of_chunk_iterator
   * instead.
   *
   * This function is must be pure.
   *
   * @param value The value which is to be converted to a string; guaranteed to
   * have a type equal to this type.
   * @return The string representation of the value.
   */
  ava_string (*to_string)(ava_value value);
  /**
   * Certain types, such as lists, can efficiently produce strings in chunks;
   * similarly, some APIs can perform equally well or better with a sequence of
   * smaller string chunks as with a monolithic string.
   *
   * string_chunk_iterator() begins iterating the string chunks in a value. It
   * returns an arbitrary representation object with which it can track its
   * state.
   *
   * iterate_string_chunk() can be called successively with a pointer to its
   * state to obtain each successive chunk in the value. The end of the
   * sequence is indicated by returning an absent string.
   *
   * If the underlying type more naturally produces a monolithic string,
   * implement to_string() instead, and use
   * ava_singleton_string_chunk_iterator() and
   * ava_iterate_singleton_string_chunk() for these fields instead.
   *
   * Do not call this function directly; use ava_string_chunk_iterator()
   * instead.
   *
   * @param value The value which is to be converted to a string in chunks.
   * @return The initial state of the iterator.
   */
  ava_datum (*string_chunk_iterator)(ava_value value);
  /**
   * Continues iteration over the chunks of a value.
   *
   * Do not call this function directly; use ava_iterate_string_chunk()
   * instead.
   *
   * @see string_chunk_iterator().
   * @param it A pointer to the current iterator state.
   * @param value The value being converted to a string in chunks.
   * @return The next chunk in the string, or an absent string to indicate that
   * iteration has completed.
   */
  ava_string (*iterate_string_chunk)(ava_datum*restrict it,
                                     ava_value value);

  /**
   * Queries the "cost" of maintaining a reference to the given value.
   *
   * Do not call this function directly; use ava_value_weight() instead.
   *
   * The weight of a value approximates the cost of incorrectly maintaining a
   * reference to the value; this is considered to be in "bytes" for the
   * purposes of callibration.
   *
   * Value weights are generally used to evaluate between the options of
   * copying a larger data structure (so that any logically dead references can
   * be garbage collected) or marking a value as deleted without removing it
   * (which could cause a logically unreachable reference to remain).
   *
   * Weights are not necessarily constant for a value; internal operations may
   * make the value become lighter or heavier dynamically.
   */
  size_t (*value_weight)(ava_value);
} ava_value_trait;

/**
 * Returns the first attribute on value matching the given tag, or NULL if none
 * can be found with that identity.
 *
 * @see AVA_GET_ATTRIBUTE
 */
const void* ava_get_attribute(
  ava_value value, const ava_attribute_tag*restrict tag) AVA_PURE;

/**
 * Returns a pointer to the first attribute tagged with type_tag, cast to const
 * type*restrict.
 */
#define AVA_GET_ATTRIBUTE(value, type) \
  ((const type*restrict)ava_get_attribute((value), &type##_tag))

/**
 * Converts the given value into a monolithic string.
 *
 * @see ava_value_trait.to_string()
 */
static inline ava_string ava_to_string(ava_value value) AVA_PURE;
static inline ava_string ava_to_string(ava_value value) {
  return AVA_GET_ATTRIBUTE(value, ava_value_trait)->to_string(value);
}

/**
 * Begins iterating string chunks in the given value.
 *
 * @see ava_value_trait.string_chunk_iterator()
 */
static inline ava_datum ava_string_chunk_iterator(ava_value value) {
  return AVA_GET_ATTRIBUTE(value, ava_value_trait)
    ->string_chunk_iterator(value);
}

/**
 * Continues iterating string chunks in the given value.
 *
 * @see ava_value_trait.iterate_string_chunk()
 */
static inline ava_string ava_iterate_string_chunk(
  ava_datum*restrict it, ava_value value
) {
  return AVA_GET_ATTRIBUTE(value, ava_value_trait)
    ->iterate_string_chunk(it, value);
}

/**
 * Returns the approximate "weight" of the given value.
 *
 * @see ava_value_trait.value_weight()
 */
static inline size_t ava_value_weight(ava_value value) {
  return AVA_GET_ATTRIBUTE(value, ava_value_trait)
    ->value_weight(value);
}

/**
 * Constructs a string by using the chunk-iterator API on the given value.
 *
 * This is a good implementation of ava_value_trait.to_string() for types that
 * implement string_chunk_iterator() and iterate_string_chunk() naturally.
 */
ava_string ava_string_of_chunk_iterator(ava_value value) AVA_PURE;

/**
 * Prepares an iterator for use with ava_iterate_singleton_string_chunk().
 *
 * This is a reasonable implementation of
 * ava_value_trait.string_chunk_iterator() for types that only implement
 * to_string() naturally.
 */
ava_datum ava_singleton_string_chunk_iterator(ava_value value);

/**
 * Iterates a value as a single string chunk.
 *
 * This is a reasonable implementation of ava_value_trait.iterate_string_chunk()
 * for types that only implement to_string() naturally. It must be used with
 * ava_singleton_string_chunk_iterator().
 */
ava_string ava_iterate_singleton_string_chunk(ava_datum* rep,
                                              ava_value value);

/**
 * Returns a value which contains the given string, with a string type.
 */
ava_value ava_value_of_string(ava_string str) AVA_PURE;

/**
 * Convenience for ava_value_of_string(ava_value_of_cstring(str)).
 */
ava_value ava_value_of_cstring(const char*) AVA_PURE;

/**
 * Compares the string representations of two ava_values.
 *
 * This is used to implement strict equality as defined by Avalanche; two
 * values are strictly equal if any only if this function returns zero. This is
 * distinct from logical equality; for example, the values "42" and "0x2A"
 * represent the same integer, but are distinct strings, and therefore are not
 * strictly equal.
 *
 * This function operates on unsigned bytes in the string regardless of the
 * signedness of the platform's char type, such that 'a\x80' is always ordered
 * after 'ab'.
 *
 * If one value is a prefix of the other, the shorter one is considered
 * lexicographically first.
 *
 * @return Zero if the two values are strictly equal; a negative integer if a
 * is lexicographically before b; a positive integer if a is lexicographically
 * before a.
 * @see ava_value_equal()
 */
signed ava_value_strcmp(ava_value a, ava_value b) AVA_PURE;
/**
 * Returns whether the two values are strictly equal, as defined by
 * ava_value_strcmp().
 *
 * This is essentially a convenience for (0==ava_value_strcmp(a,b)).
 *
 * @see ava_value_strcmp()
 */
ava_bool ava_value_equal(ava_value a, ava_value b) AVA_PURE;

/**
 * Returns the hash of the string value of the given value.
 *
 * Any two values which are strictly equal (as per ava_value_strcmp()) produce
 * the same hash code *within the same process*. Values not strictly equal
 * produce different hash codes with extremely high probability and effectively
 * randomly distributed through the 64-bit integer space.
 */
ava_ulong ava_value_hash(ava_value value) AVA_PURE;

/**
 * Initialises the process-wide hashing key.
 *
 * There is generally no reason to call this directly; call ava_init() instead.
 *
 * This should be called exactly once at process start-up.
 */
void ava_value_hash_init(void);

#endif /* AVA_RUNTIME_VALUE_H_ */
