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
#error "Don't include avalanche/integer.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_INTEGER_H_
#define AVA_RUNTIME_INTEGER_H_

#include "defs.h"
#include "value.h"

/**
 * The maximum length, in characters, of an integer value.
 */
#define MAX_INTEGER_LENGTH 65

/**
 * Integer type used in user integer arithmetic calculations.
 */
typedef ava_slong ava_integer;
/**
 * The basic integer type.
 *
 * The string representation of an integer matches the following regular
 * expression. Surrounding whitespace is implicitly discarded. It is not
 * case-sensitive.
 *
 * true|false|on|off|yes|no|null|[+-]?([0-9]+|0?b[01]+|0?o[0-7]+|0?x[0-9a-f]+)
 *
 * The strings "true", "on", and "yes" are parsed as 1. The strings "false",
 * "off", "no", and "null" are parsed as 0.
 *
 * Other than the above special cases, an integer literal is comprised of an
 * optional sign, an optional base indicator, and one or more digits in that
 * base. `b` indicates base-2, `o` indicates base-8, and `x` indicates base-16;
 * integers with no base indicator are base-10. For compatibility with existing
 * text formats, a `0` is permitted to prefix the base indicator. It has no
 * effect.
 *
 * It is considered a parse error if an integer value overflows an *unsigned*
 * ava_ulong; overflows into the opposite sign are permitted to ease working
 * with 64-bit unsigned integers.
 *
 * Whitespace in the above comprises space, table, line feed, and carraige
 * return.
 *
 * Including surrounding whitespace, an integer may not be more than 65 bytes
 * long.
 *
 * Normal form of an integer is its value in base-10, preceded with a negative
 * sign if negative.
 *
 * In all contexts, a string that is empty except for possible whitespace is
 * also considered a valid integer for parsing purposes. Its value depends on
 * the context; it is not directly representable as an ava_value, but must
 * first be subjected to normalisation.
 *
 * A value with integer type stores its integer value in r1.slong. r2 is unused
 * and contains NULL.
 */
extern const ava_value_type ava_integer_type;

/**
 * Returns the integer parsable from the given value.
 *
 * @param value The value to parse.
 * @param dfault The value to return if value is a string containing no
 * non-whitespace characters.
 * @throws ava_format_exception if the value is not a valid integer
 * @return The resulting integer, either parsed from value or dfault.
 */
static inline ava_integer ava_integer_of_value(
  ava_value value, ava_integer dfault);
/**
 * Internal function.
 */
ava_integer ava_integer_of_noninteger_value(
  ava_value value, ava_integer dfault);

static inline ava_integer ava_integer_of_value(
  ava_value value, ava_integer dfault
) {
  /* Optimise for constant propagation */
  if (&ava_integer_type == value.type)
    return value.r1.slong;

  return ava_integer_of_noninteger_value(value, dfault);
}

/**
 * Returns a value containing the given integer.
 */
static inline ava_value ava_value_of_integer(ava_integer i) AVA_CONSTFUN;
static inline ava_value ava_value_of_integer(ava_integer i) {
  ava_value ret = {
    .r1 = { .slong = i },
    .r2 = { .ulong = 0 },
    .type = &ava_integer_type
  };
  return ret;
}

/**
 * Returns whether the given string is syntactically interpretable as an
 * integer.
 *
 * This is substantially faster than attempting to parse the string as an
 * integer.
 *
 * Note that, even if this returns true, ava_integer_of_value() could still
 * throw if there is numeric overflow in the input.
 */
ava_bool ava_string_is_integer(ava_string str);

#endif /* AVA_RUNTIME_INTEGER_H_ */
