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
 * first be subjected to normalisation. For this reason, it is impossible to
 * simply imbue a value to an integer; the caller must extract the integer and
 * create a new value from it.
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
    .r2 = { .ptr = NULL },
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
int ava_string_is_integer(ava_string str);

#endif /* AVA_RUNTIME_INTEGER_H_ */
