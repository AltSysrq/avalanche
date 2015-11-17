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
#error "Don't include avalanche/interval.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_INTERVAL_H_
#define AVA_RUNTIME_INTERVAL_H_

#include "defs.h"
#include "value.h"
#include "integer.h"

/**
 * @file
 *
 * Defines the (integer) interval type and utilities for working with it.
 *
 * An interval describes either a single point on the integer line, or a range
 * between two points on the integer line. Its primary use is in indexing
 * array-like data structures.
 *
 * A singular interval is semantically exactly equivalent to an integer, and is
 * internally represented as an integer (with ava_integer_type). The default
 * value of a singular interval is "end". A positive singular interval refers
 * to an absolute index. A negative singular interval is an offset from the
 * length of the indexed structure; eg, -1 refers to the very last element, -2
 * to the second-to-last, and so forth. "end" is logically "negative zero" and
 * thus refers to one past the end of the structure. (The behaviour of "end" is
 * useful for appending items to a structure. This is why it is also the
 * default; a list can be appended with `foo[] = bar`.)
 *
 * A range interval is distinguished in string form by containing a '~'
 * character. The part of the value before the '~' is an integer, default 0,
 * indicating the start of the interval. That after the '~' is an integer,
 * default "end", indicating the end of the interval. Both integers are
 * converted to absolute indices in the indexed structure according to the
 * rules for singular intervals. Range intervals always describe half-open
 * ranges; 1~2 includes one index, 1. This is different from some other systems
 * which support negative-from-end indexing, where 3~-1 might describe all but
 * the first three indices; here, that means all but the first three and final
 * index, whereas 3~end includes everything but the first three.
 *
 * The use of '~' as the delimeter (rather than perhaps ':') is to permit it to
 * also be a binary operator with the same character.
 *
 * Internally, there are two representations for range intervals; see
 * ava_compact_interval_type and ava_wide_interval_type.
 *
 * Normal form for singular intervals is the normal form of the backing
 * integer.
 *
 * Normal form for range intervals is the normal form of the begin integer, the
 * character '~', and then normal form of the end integer, except if the latter
 * is equal to "end", the literal string "end" is used.
 */

/**
 * Value pointed by ptr field of value for values whose root attribute is
 * ava_wide_interval_type.
 *
 * This should not generally be used externally.
 */
typedef struct {
  ava_integer begin, end;
} ava_wide_interval;

#ifndef AVA__IN_INTERVAL_C
/**
 * Root attribute for values which represent normal range intervals in compact
 * form.
 *
 * This is used when both endpoints are between -0x7FFFFFFF and +0x7FFFFFFF
 * (inclusive) or equal to AVA_INTEGER_END.
 *
 * The begin integer is stored in the lower 32 bits of the value's ulong, and
 * the end integer in the upper 32 bits. Both values are signed.
 * AVA_INTEGER_END is represented as 0x80000000.
 */
extern const ava_attribute ava_compact_interval_type;
/**
 * Root attribute for values which represent normal range intervals in wide
 * form.
 *
 * This is used for range intervals which do not fit in compact format.
 *
 * The ptr of the value is a `const ava_wide_interval*`.
 */
extern const ava_attribute ava_wide_interval_type;
#else
extern const ava_value_trait ava_compact_interval_type;
extern const ava_value_trait ava_wide_interval_type;
#endif

/**
 * Format-safe type for values in normal interval format.
 *
 * The root attribute of an ava_interval_value MUST always be one of
 * &ava_integer_type, &ava_compact_interval_type, or &ava_wide_interval_type.
 *
 * @see ava_interval_value_of()
 */
typedef struct {
  ava_value v;
} ava_interval_value;

/**
 * Internally used by ava_interval_value_of().
 */
ava_interval_value ava_interval_value_of_other(ava_value val);

/**
 * Returns the normal interval equal to the given value.
 *
 * @param value The value to parse/normalise.
 * @return The interval equal to val.
 * @throws ava_format_exception if the value is not a valid interval.
 */
static inline ava_interval_value ava_interval_value_of(ava_value val) {
  const void* type = ava_value_attr(val);
  ava_interval_value ret;

  if (type == &ava_integer_type ||
      type == &ava_compact_interval_type ||
      type == &ava_wide_interval_type) {
    ret.v = val;
    return ret;
  } else {
    return ava_interval_value_of_other(val);
  }
}

/**
 * Returns an normal singular interval referencing the given index.
 *
 * @param ix The index the interval references.
 */
static inline ava_interval_value ava_interval_value_of_singular(
  ava_integer ix
) {
  ava_interval_value ret;

  ret.v = ava_value_with_slong(&ava_integer_type, ix);
  return ret;
}

/**
 * Allocates a new ava_wide_interval with the given begin and end peints.
 *
 * This is only useful for ava_interval_value_of_range().
 */
const ava_wide_interval* ava_wide_interval_new(
  ava_integer begin, ava_integer end) AVA_PURE AVA_MALLOC;

/**
 * Returns a normal range interval representing the chosen range.
 *
 * @param begin The begin index.
 * @param end The end index.
 * @return An interval describing the range from begin, inclusive, to end,
 * exclusive.
 */
static inline ava_interval_value ava_interval_value_of_range(
  ava_integer begin, ava_integer end
) {
  ava_interval_value ret;

  if (((begin >= -0x7FFFFFFFLL && begin <= +0x7FFFFFFFLL) ||
       AVA_INTEGER_END == begin) &&
      ((end >= -0x7FFFFFFFLL && end <= +0x7FFFFFFFLL) ||
       AVA_INTEGER_END == end)) {
    begin |= (begin & AVA_INTEGER_END) >> 32;
    end |= (end & AVA_INTEGER_END) >> 32;
    ret.v = ava_value_with_ulong(
      &ava_compact_interval_type,
      (begin & 0xFFFFFFFF) | (end << 32));
  } else {
    ret.v = ava_value_with_ptr(
      &ava_wide_interval_type,
      ava_wide_interval_new(begin, end));
  }

  return ret;
}

/**
 * Returns whether the given interval is singular.
 *
 * If false, the interval is a range.
 */
static inline ava_bool ava_interval_is_singular(ava_interval_value v) {
  return &ava_integer_type == ava_value_attr(v.v);
}

/**
 * Maps a relative index to absolute index using the singular interval rules.
 *
 * @param index The logical index to map.
 * @param length The length of the indexed structure.
 * @return The absolute index into the indexed structure described by index.
 * This may still be outside the [0,length-1] range.
 */
static inline ava_integer ava_interval_to_absolute(
  ava_integer index, ava_integer length
) {
  if (index >= 0)
    return index;
  else
    /* Casting index to an unsigned before negating it is necessary, since
     * otherwise the compiler is allowed to assume it doesn't overflow, and
     * thus that ANDing -index with 0x7FF... is a noop because negating a
     * negative value (since we already know index<0 here) always produces a
     * positive value if it doesn't overflow.
     */
    return length - (-(ava_ulong)index & ~AVA_INTEGER_END);
}

/**
 * Like ava_interval_to_absolute(), but operates on a 32-bit index.
 */
static inline ava_integer ava_interval_to_absolute_32(
  ava_sint index, ava_integer length
) {
  if (index >= 0)
    return index;
  else
    return length - (-(ava_uint)index & 0x7FFFFFFF);
}

/**
 * Returns the absolute index referenced by the given singular interval.
 *
 * The return value is undefined if v is not a singular interval.
 *
 * @param v The singular interval (as per ava_interval_is_singular()) to
 * absolutise.
 * @param length The length of the structure being indexed by the interval.
 * @return An absolute index into the indexed structure. This may be outside
 * the structure's bounds.
 */
static inline ava_integer ava_interval_get_singular(
  ava_interval_value v, ava_integer length
) {
  return ava_interval_to_absolute(ava_value_slong(v.v), length);
}

/**
 * Returns the absolute index of the inclusive lower-bound of the given range
 * interval.
 *
 * Behaviour is undefined if v is not a range interval.
 *
 * @param v The range interval (as per !ava_interval_is_singular()) whose lower
 * bound is to be extracted and absolutised.
 * @param length The length of the structure being indexed by the interval.
 * @return The absolute lower bound, inclusive, into the indexed structure.
 * This may be outside the structure's bounds.
 */
static inline ava_integer ava_interval_get_begin(
  ava_interval_value v, ava_integer length
) {
  if (&ava_compact_interval_type == ava_value_attr(v.v))
    return ava_interval_to_absolute_32(ava_value_slong(v.v), length);
  else
    return ava_interval_to_absolute(
      ((const ava_wide_interval*)ava_value_ptr(v.v))->begin, length);
}

/**
 * Returns the absolute index of the exclusive upper-bound of the given range
 * interval.
 *
 * Behaviour is undefined if v is not a range interval.
 *
 * @param v The range interval (as per !ava_interval_is_singular()) whose upper
 * bound is to be extracted and absolutised.
 * @param length The length of the structure being indexed by the interval.
 * @return The absolute upper bound, exclusive, into the indexed structure.
 * This may be outside the structure's bounds, and may be less than the value
 * returned by ava_interval_get_begin() on the same interval.
 */
static inline ava_integer ava_interval_get_end(
  ava_interval_value v, ava_integer length
) {
  if (&ava_compact_interval_type == ava_value_attr(v.v))
    return ava_interval_to_absolute_32(ava_value_slong(v.v) >> 32, length);
  else
    return ava_interval_to_absolute(
      ((const ava_wide_interval*)ava_value_ptr(v.v))->end, length);
}

#endif /* AVA_RUNTIME_INTERVAL_H_ */
