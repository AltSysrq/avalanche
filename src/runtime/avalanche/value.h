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
 *   compile time; types can only be introduced by C code.
 *
 * - Representation. The physical way a value is stored. Without knowledge of
 *   the particular type, the representation of a value is totally opaque.
 *   Certain types, such as ava_string_type, MAY make their representation
 *   public.
 *
 * An important thing to note is that values *always* preserve their native
 * string representation. If a string "0x01" is converted to integer 1, it must
 * still appear to be the string "0x01". Functions which return *new* values
 * may define themselves to produce _normalised_ values, in which case this
 * does not apply. (Eg, "0x0a"+"0" produces "10", not "0x0a".)
 */
typedef struct ava_value_s ava_value;

/**
 * A single representation of a value.
 *
 * The usage of this union is entirely up to the type on the value.
 */
typedef union {
  ava_ubyte             ubytes[8];
  ava_sbyte             sbytes[8];
  char                  chars[8];
  ava_ushort            ushorts[4];
  ava_sshort            sshorts[4];
  ava_uint              uint[2];
  ava_sint              sint[2];
  ava_ulong             ulong;
  ava_slong             slong;
  const void*restrict   ptr;
  ava_string            str;
} ava_datum;

/**
 * Defines the type of a value and how its representation is interpreted.
 *
 * As previously described, types provide *performance*, not functionality.
 * This is formalised by several rules:
 *
 * - Every value of every type has a string representation which perfectly
 *   represents the value. to_type(to_string(value_of_type)) == value_of_type
 *
 * - Every value of every type provides sufficient information to reproduce the
 *   original string. This means that types supporting denormalised string
 *   representations must typically preserve the original string until passed
 *   through some explicitly normalising operation.
 *   to_string(to_type(string)) == string
 *
 * - The effect of performing any operation on a value of a type is exactly
 *   equivalent to having performed the same operation on the string
 *   representation. f(value_of_type) == f(to_string(value_of_type))
 */
typedef struct ava_value_type_s ava_value_type;

struct ava_value_s {
  /**
   * The one or two representations of this value, as controlled by the type.
   */
  ava_datum r1, r2;
  /**
   * The dynamic type of this value.
   */
  const ava_value_type*restrict type;
};

/**
 * An accelerator identifies a set of operations that can be performed on any
 * value, but which may be implemented more efficiently on particular types.
 *
 * An ava_accelerator itself contains no information; it is meerly a pointer
 * used to guarantee uniqueness.
 *
 * Use the AVA_DECLARE_ACCELERATOR and AVA_DEFINE_ACCELERATOR to define values
 * of this type.
 */
typedef struct {
  /**
   * sizeof(ava_accelerator)
   */
  size_t size;
} ava_accelerator;

/**
 * Declares the existence of a particular accelerator, appropriate for use in a
 * header file.
 */
#define AVA_DECLARE_ACCELERATOR(name) extern const ava_accelerator name
/**
 * Defines an accelerator; must be located in a source file.
 */
#define AVA_DEFINE_ACCELERATOR(name) \
  const ava_accelerator name = { .size = sizeof(ava_accelerator) }

struct ava_value_type_s {
  /**
   * sizeof(ava_value_type)
   */
  size_t size;

  /**
   * A human-readable name of this type, for diagnostic and debugging purposes.
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
   * This function is assumed to be pure.
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
   * Queries whether the type supports the given accelerator.
   *
   * Do not call this function directly; use ava_query_accelerator() instead.
   *
   * If the type recognises the given accelerator, it MAY return a value
   * particular to that type; otherwise, it MUST return dfault.
   *
   * The exact semantics of dfault and the return value are entirely dependent
   * on the accelerator; typically, it will be a struct containing function
   * pointers and possibly some properties global to the type.
   *
   * Generally, dfault should provide an implementation of the operations that
   * operates on string values.
   *
   * The implementation may not inspect the contents of *accel, and may not do
   * anything whatsoever with dfault except return it.
   *
   * @see ava_noop_query_accelerator()
   * @param accel The accelerator being queried.
   * @param dfault The value to return if the type does not know about accel.
   * @return An accelerator-dependent value.
   */
  const void* (*query_accelerator)(const ava_accelerator* accel,
                                   const void* dfault);

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
};

/**
 * Converts the given value into a monolithic string.
 *
 * @see ava_value_type.to_string()
 */
static inline ava_string ava_to_string(ava_value value) AVA_PURE;
static inline ava_string ava_to_string(ava_value value) {
  return (*value.type->to_string)(value);
}

/**
 * Begins iterating string chunks in the given value.
 *
 * @see ava_value_type.string_chunk_iterator()
 */
static inline ava_datum ava_string_chunk_iterator(ava_value value) {
  return (*value.type->string_chunk_iterator)(value);
}

/**
 * Continues iterating string chunks in the given value.
 *
 * @see ava_value_type.iterate_string_chunk()
 */
static inline ava_string ava_iterate_string_chunk(
  ava_datum*restrict it, ava_value value
) {
  return (*value.type->iterate_string_chunk)(it, value);
}

/**
 * This is a workaround to hint to GCC that (*type->query_accelerator) is
 * const, since there doesn't seem to be any way to attach attributes to
 * function pointers.
 *
 * This is not considered part of the public API; do not call it yourself.
 */
static inline const void* ava___invoke_query_accelerator_const(
  const void* (*f)(const ava_accelerator*, const void*),
  const ava_accelerator* accel,
  const void* dfault
) AVA_CONSTFUN;
/**
 * Returns the implementation corresponding to the given accelerator of the
 * type associated with the given value.
 *
 * @see ava_value_type.query_accelerator()
 */
static inline const void* ava_query_accelerator(
  ava_value value,
  const ava_accelerator* accel,
  const void* dfault
) AVA_PURE;

static inline const void* ava___invoke_query_accelerator_const(
  const void* (*f)(const ava_accelerator*, const void*),
  const ava_accelerator* accel,
  const void* dfault
) {
  return (*f)(accel, dfault);
}

static inline const void* ava_query_accelerator(
  ava_value value,
  const ava_accelerator* accel,
  const void* dfault
) {
  return ava___invoke_query_accelerator_const(
    value.type->query_accelerator, accel, dfault);
}

/**
 * Returns the approximate "weight" of the given value.
 *
 * @see ava_value_type.value_weight()
 */
static inline size_t ava_value_weight(ava_value value) {
  return (*value.type->value_weight)(value);
}

/**
 * Constructs a string by using the chunk-iterator API on the given value.
 *
 * This is a good implementation of ava_value_type.to_string() for types that
 * implement string_chunk_iterator() and iterate_string_chunk() naturally.
 */
ava_string ava_string_of_chunk_iterator(ava_value value) AVA_PURE;

/**
 * Prepares an iterator for use with ava_iterate_singleton_string_chunk().
 *
 * This is a reasonable implementation of
 * ava_value_type.string_chunk_iterator() for types that only implement
 * to_string() naturally.
 */
ava_datum ava_singleton_string_chunk_iterator(ava_value value);

/**
 * Iterates a value as a single string chunk.
 *
 * This is a reasonable implementation of ava_value_type.iterate_string_chunk()
 * for types that only implement to_string() naturally. It must be used with
 * ava_singleton_string_chunk_iterator().
 */
ava_string ava_iterate_singleton_string_chunk(ava_datum* rep,
                                              ava_value value);

/**
 * Implementation for ava_value_type.query_accelerator() that always returns
 * the default.
 */
const void* ava_noop_query_accelerator(const ava_accelerator* accel,
                                       const void* dfault) AVA_CONSTFUN;

/**
 * The type for string values, the universal type.
 *
 * Representational semantics:
 *
 * - r1 has the str field set to the string value.
 */
extern const ava_value_type ava_string_type;
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
