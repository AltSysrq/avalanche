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
#error "Don't include avalanche/real.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_REAL_H_
#define AVA_RUNTIME_REAL_H_

#include "defs.h"
#include "value.h"
#include "integer.h"

#ifndef AVA__IN_REAL_C
/**
 * The basic floating-point type.
 *
 * Floating-point values can generally be expected to have this attribute at
 * the head of their attribute chain. As normal, the absence of this attribute
 * does not imply that the value is not a real.
 *
 * All strings legal for integers are also legal for floating-point values.
 * Additionally, the full syntax of ISO C99 strtod(3) is accepted for
 * floating-point values, except that it is not locale-dependent. Period and
 * comma are treated equivalently as the decimal point.
 *
 * Note that integer syntax does not in general extend to the floating-point
 * syntax; for example, the characters "x1" followed by 32 zeroes, while
 * exactly representable as a floating-point value, is rejected since it is not
 * a legal integer, and not part of the ISO C99 strtod(3) syntax.
 *
 * As with integers, surrounding whitespace is ignored, but other trailing
 * garbage raises an error.
 *
 * Normal form is defined simply to be the form that results from converting a
 * value to a real and back.
 */
extern const ava_attribute ava_real_type;
#else
extern const ava_value_trait ava_real_type;
#endif

/**
 * Returns the real parsable from the given value.
 *
 * @param value The value to parse.
 * @param dfault The value to return if value is a string containing no
 * non-whitespace characters.
 * @throws ava_format_exception if the value is not a valid real.
 * @return The resulting real, either parsed from value or dfault.
 */
static inline ava_real ava_real_of_value(ava_value value, ava_real dfault);
/**
 * Internal function.
 */
ava_real ava_real_of_nonnumeric_value(ava_value value, ava_real dfault);

static inline ava_real ava_real_of_value(
  ava_value value, ava_real dfault
) {
  /* Constant propagation */
  if (&ava_real_type == ava_value_attr(value))
    return ava_value_real(value);
  else if (&ava_integer_type == ava_value_attr(value))
    return ava_value_slong(value);
  else
    return ava_real_of_nonnumeric_value(value, dfault);
}

/**
 * Returns a value containing the given real.
 */
static inline ava_value ava_value_of_real(ava_real r) AVA_CONSTFUN;
static inline ava_value ava_value_of_real(ava_real r) {
  return ava_value_with_real(&ava_real_type, r);
}

#endif /* AVA_RUNTIME_REAL_H_ */
