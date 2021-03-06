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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* This file needs to be an C++ since it is used as a native driver and calls
 * functions which may throw. (Clang incorrectly tags everything with nounwind,
 * including calls themselves, when compiling C.)
 */

#include <stdlib.h>
#include <math.h>
#include <limits.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
AVA_BEGIN_DECLS
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/list-proj.h"
#include "avalanche/map.h"
#include "avalanche/integer.h"
#include "avalanche/real.h"
#include "avalanche/pointer.h"
#include "avalanche/interval.h"
#include "avalanche/map.h"
#include "avalanche/exception.h"
#include "avalanche/errors.h"

/*
  This file contains the C portion of the org.ava-lang.avast package.

  This is both compiled into the runtime (so that interpreted code can access
  it) and added as a driver to native builds (so trivial things like integer
  adds can be inlined and optimised).

  There is no documentation to be had here; that is / will be in the Avalanche
  code which declares these functions.

  AVAST_CHECK_LEVEL can be defined to three meaningful values:
  0: Undefined behaviour is really undefined.
  1: Checks for integer overflow for cheap operations removed, making them
     truly undefined. Other checks, including bounds checks, remain.
  2: All checks in place; any undefined behaviour should result in an
     exception.
 */

#ifndef AVAST_CHECK_LEVEL
#define AVAST_CHECK_LEVEL 2
#endif

#define fun(name) a$org__ava_lang__avast___##name
#define defun(name)                             \
  ava_value fun(name)

/**
 * In checked builds, obfuscates the result of a comparison result (ie, results
 * where anything less than 0 indicates less-than, greater than 0 indicates
 * greater-than, and 0 indicates equality) to catch usages which incorrectly
 * test for equality with -1 or 1.
 */
static inline ava_integer obfuscate_comparison(ava_integer raw) {
#if AVAST_CHECK_LEVEL >= 1
  /* Use the stack pointer to produce an arbitrary value. */
  volatile unsigned dummy;
  ava_integer obfus = 1 | ((((ava_intptr)&dummy) >> 5) & 0xFF);

  if (raw < 0) return -obfus;
  if (raw > 0) return +obfus;
  return 0;
#else
  return raw;
#endif
}

void fun(throw__singular_out_of_bounds)
(ava_integer index, ava_integer max) AVA_NORETURN;
void fun(throw__range_out_of_bounds)
(ava_integer range_min, ava_integer range_max, ava_integer max) AVA_NORETURN;
void fun(throw__inverted_range)
(ava_integer range_min, ava_integer range_max) AVA_NORETURN;

#ifndef COMPILING_DRIVER
void fun(throw__singular_out_of_bounds)(ava_integer ix, ava_integer max) {
  AVA_STATIC_STRING(out_of_bounds, "out-of-bounds");
  ava_throw_uex(&ava_error_exception, out_of_bounds,
                ava_error_singular_index_out_of_bounds(ix, max));
}

void fun(throw__range_out_of_bounds)(ava_integer range_min,
                                     ava_integer range_max,
                                     ava_integer max) {
  AVA_STATIC_STRING(out_of_bounds, "out-of-bounds");
  ava_throw_uex(&ava_error_exception, out_of_bounds,
                ava_error_range_index_out_of_bounds(
                  range_min, range_max, max));
}

void fun(throw__inverted_range)(ava_integer range_min,
                                ava_integer range_max) {
  AVA_STATIC_STRING(illegal_range, "illegal-range");
  ava_throw_uex(&ava_error_exception, illegal_range,
                ava_error_range_inverted(range_min, range_max));
}
#endif

static inline void strict_index_check(ava_integer ix, ava_integer max) {
  if (ix < 0 || ix >= max)
    fun(throw__singular_out_of_bounds)(ix, max);
}

static inline void strict_range_check(ava_integer range_min,
                                      ava_integer range_max,
                                      ava_integer max) {
  if (range_max < range_min)
    fun(throw__inverted_range)(range_min, range_max);

  if (range_min < 0 || range_min > max ||
      range_max < 0 || range_max > max)
    fun(throw__range_out_of_bounds)(range_min, range_max, max);
}

static inline ava_bool lenient_index_check(ava_integer ix, ava_integer max) {
  return ix >= 0 && ix < max;
}

static inline void lenient_range_check(ava_integer* range_min,
                                       ava_integer* range_max,
                                       ava_integer max) {
  if (*range_min < 0) *range_min = 0;
  else if (*range_min > max) *range_min = max;
  if (*range_max > max) *range_max = max;

  if (*range_max < *range_min)
    *range_max = *range_min;
}

/******************** STRING OPERATIONS ********************/

defun(byte_string__concat)(ava_value a, ava_value b) {
  return ava_value_of_string(
    ava_strcat(
      ava_to_string(a), ava_to_string(b)));
}

defun(byte_string__length)(ava_value a) {
  return ava_value_of_integer(
    ava_strlen(ava_to_string(a)));
}

defun(byte_string__equ)(ava_value a, ava_value b) {
  return ava_value_of_integer(ava_value_equal(a, b));
}

defun(byte_string__neq)(ava_value a, ava_value b) {
  return ava_value_of_integer(!ava_value_equal(a, b));
}

defun(byte_string__compare)(ava_value a, ava_value b) {
  return ava_value_of_integer(obfuscate_comparison(ava_value_strcmp(a, b)));
}

#define COMPARATOR(name, ck)                                    \
  defun(byte_string__##name)(ava_value a, ava_value b) {        \
    return ava_value_of_integer(ava_value_strcmp(a, b) ck);     \
  }
COMPARATOR(slt, < 0)
COMPARATOR(leq, <= 0)
COMPARATOR(sgt, > 0)
COMPARATOR(geq, >= 0)
#undef COMPARATOR

defun(byte_string__index)(ava_value str, ava_value index) {
  ava_string s, ret;
  ava_integer max, begin, end;
  ava_interval_value ival;

  s = ava_to_string(str);
  max = ava_strlen(s);
  ival = ava_interval_value_of(index);
  if (ava_interval_is_singular(ival)) {
    begin = ava_interval_get_singular(ival, max);
    strict_index_check(begin, max);
    ret = ava_string_slice(s, begin, begin+1);
  } else {
    begin = ava_interval_get_begin(ival, max);
    end = ava_interval_get_end(ival, max);
    strict_range_check(begin, end, max);
    ret = ava_string_slice(s, begin, end);
  }

  return ava_value_of_string(ret);
}

defun(byte_string__set)(ava_value str, ava_value index, ava_value neu) {
  ava_string s, prefix, suffix;
  ava_integer max, begin, end;
  ava_interval_value ival;

  s = ava_to_string(str);
  max = ava_strlen(s);
  ival = ava_interval_value_of(index);
  if (ava_interval_is_singular(ival)) {
    begin = ava_interval_get_singular(ival, max);
    strict_index_check(begin, max + 1);
    end = begin + 1;
  } else {
    begin = ava_interval_get_begin(ival, max);
    end = ava_interval_get_end(ival, max);
    strict_range_check(begin, end, max);
  }

  prefix = ava_string_trunc(s, begin);
  suffix = end < max? ava_string_behead(s, end) : AVA_EMPTY_STRING;
  return ava_value_of_string(
    ava_strcat(ava_strcat(prefix, ava_to_string(neu)), suffix));
}

defun(byte_string__index_lenient)(ava_value str, ava_value index) {
  ava_string s, ret;
  ava_integer max, begin, end;
  ava_interval_value ival;

  s = ava_to_string(str);
  max = ava_strlen(s);
  ival = ava_interval_value_of(index);
  if (ava_interval_is_singular(ival)) {
    begin = ava_interval_get_singular(ival, max);
    if (lenient_index_check(begin, max))
      ret = ava_string_slice(s, begin, begin+1);
    else
      ret = AVA_EMPTY_STRING;
  } else {
    begin = ava_interval_get_begin(ival, max);
    end = ava_interval_get_end(ival, max);
    lenient_range_check(&begin, &end, max);
    ret = ava_string_slice(s, begin, end);
  }

  return ava_value_of_string(ret);
}

/******************** INTEGER OPERATIONS ********************/

defun(integer__add)(ava_value a, ava_value b) {
  signed long long ai, bi, res;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0);
#if AVAST_CHECK_LEVEL >= 2
  if (__builtin_saddll_overflow(ai, bi, &res))
    ava_throw_str(&ava_undefined_behaviour_exception,
                  ava_error_undef_integer_overflow(
                    ai, AVA_ASCII9_STRING("+"), bi));
#else
  res = ai + bi;
#endif

  return ava_value_of_integer(res);
}

defun(integer__sub)(ava_value a, ava_value b) {
  signed long long ai, bi, res;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0);
#if AVAST_CHECK_LEVEL >= 2
  if (__builtin_ssubll_overflow(ai, bi, &res))
    ava_throw_str(&ava_undefined_behaviour_exception,
                  ava_error_undef_integer_overflow(
                    ai, AVA_ASCII9_STRING("-"), bi));
#else
  res = ai - bi;
#endif

  return ava_value_of_integer(res);
}

defun(integer__mul)(ava_value a, ava_value b) {
  signed long long ai, bi, res;

  ai = ava_integer_of_value(a, 1);
  bi = ava_integer_of_value(b, 1);
#if AVAST_CHECK_LEVEL >= 2
  if (__builtin_smulll_overflow(ai, bi, &res))
    ava_throw_str(&ava_undefined_behaviour_exception,
                  ava_error_undef_integer_overflow(
                    ai, AVA_ASCII9_STRING("*"), bi));
#else
  res = ai * bi;
#endif

  return ava_value_of_integer(res);
}

defun(integer__div)(ava_value a, ava_value b) {
  ava_integer ai, bi, res;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 1);
#if AVAST_CHECK_LEVEL >= 1
  if (0 == bi)
    ava_throw_str(&ava_undefined_behaviour_exception,
                  ava_error_undef_int_div_by_zero(
                    ai, AVA_ASCII9_STRING("/"), bi));
  if (-1LL == bi && (ava_integer)0x8000000000000000LL == bi)
    ava_throw_str(&ava_undefined_behaviour_exception,
                  ava_error_undef_integer_overflow(
                    ai, AVA_ASCII9_STRING("/"), bi));
#endif
  res = ai / bi;
  return ava_value_of_integer(res);
}

defun(integer__mod)(ava_value a, ava_value b) {
  ava_integer ai, bi, res;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0x8000000000000000LL);
#if AVAST_CHECK_LEVEL >= 1
  if (0 == bi)
    ava_throw_str(&ava_undefined_behaviour_exception,
                  ava_error_undef_int_div_by_zero(
                    ai, AVA_ASCII9_STRING("/"), bi));
#endif
  res = ai % bi;

#if (-1 >> 1) != -1
#error ">> operator is not arithmetic shift on signed integers"
#endif
  res += (res >> (sizeof(ava_integer) * 8 - 1)) & (bi >= 0? bi : -bi);

  return ava_value_of_integer(res);
}

defun(integer__rem)(ava_value a, ava_value b) {
  ava_integer ai, bi, res;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0x8000000000000000LL);
  if (0 == bi)
    res = ai;
  else if (-1 == bi)
    /* Traps on -0x8000000000000000LL % -1 since it may be implemented with the
     * same machine instruction as division.
     */
    res = 0;
  else
    res = ai % bi;

  return ava_value_of_integer(res);
}

defun(integer__and)(ava_value a, ava_value b) {
  ava_integer ai, bi, res;

  ai = ava_integer_of_value(a, ~(ava_integer)0);
  bi = ava_integer_of_value(b, ~(ava_integer)0);
  res = ai & bi;

  return ava_value_of_integer(res);
}

defun(integer__or)(ava_value a, ava_value b) {
  ava_integer ai, bi, res;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0);
  res = ai | bi;

  return ava_value_of_integer(res);
}

defun(integer__xor)(ava_value a, ava_value b) {
  ava_integer ai, bi, res;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0);
  res = ai ^ bi;

  return ava_value_of_integer(res);
}

defun(integer__lsh)(ava_value a, ava_value b) {
  ava_integer ai, res;
  unsigned long long bi;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0);
  res = (ai << bi) &~- (bi >= sizeof(ava_integer) * 8);

  return ava_value_of_integer(res);
}

defun(integer__rsh)(ava_value a, ava_value b) {
  unsigned long long ai, bi, res;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0);
  res = (ai >> bi) &~- (bi >= sizeof(ava_integer) * 8);

  return ava_value_of_integer(res);
}

defun(integer__ash)(ava_value a, ava_value b) {
  ava_integer ai, res;
  unsigned long long bi;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0);
  if (bi < sizeof(ava_integer) * 8)
    res = ai >> bi;
  else
    res = ai >> (sizeof(ava_integer) * 8 - 1);

  return ava_value_of_integer(res);
}

defun(integer__not)(ava_value a) {
  return ava_value_of_integer(
    ~ava_integer_of_value(a, 0));
}

#define COMPARATOR(name,operator,left_default,right_default)    \
  defun(integer__##name)(ava_value a, ava_value b) {            \
    ava_integer ai, bi, res;                                    \
    ai = ava_integer_of_value(a, left_default);                 \
    bi = ava_integer_of_value(b, right_default);                \
    res = ai operator bi;                                       \
    return ava_value_of_integer(res);                           \
  }

COMPARATOR(equ, ==,     0,                      0)
COMPARATOR(neq, !=,     0,                      0)
COMPARATOR(slt, < ,     0x7FFFFFFFFFFFFFFFLL,   -0x8000000000000000LL)
COMPARATOR(leq, <=,     0x7FFFFFFFFFFFFFFFLL,   -0x8000000000000000LL)
COMPARATOR(sgt, > ,     -0x8000000000000000LL,  0x7FFFFFFFFFFFFFFFLL)
COMPARATOR(geq, >=,     -0x8000000000000000LL,  0x7FFFFFFFFFFFFFFFLL)
#undef COMPARATOR

defun(integer__lnot)(ava_value a) {
  return ava_value_of_integer(!ava_integer_of_value(a, 0));
}

/******************** UNSIGNED OPERATIONS ********************/

defun(unsigned__add)(ava_value a, ava_value b) {
  ava_ulong ai, bi;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0);
  return ava_value_of_integer(ai + bi);
}

defun(unsigned__sub)(ava_value a, ava_value b) {
  ava_ulong ai, bi;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0);
  return ava_value_of_integer(ai - bi);
}

defun(unsigned__mul)(ava_value a, ava_value b) {
  ava_ulong ai, bi;

  ai = ava_integer_of_value(a, 1);
  bi = ava_integer_of_value(b, 1);
  return ava_value_of_integer(ai * bi);
}

defun(unsigned__div)(ava_value a, ava_value b) {
  ava_ulong ai, bi;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 1);

#if AVAST_CHECK_LEVEL >= 1
  if (!bi)
    ava_throw_str(&ava_undefined_behaviour_exception,
                  ava_error_undef_int_div_by_zero(
                    ai, AVA_ASCII9_STRING("u/"), bi));
#endif

  return ava_value_of_integer(ai / bi);
}

defun(unsigned__mod)(ava_value a, ava_value b) {
  ava_ulong ai, bi;

  ai = ava_integer_of_value(a, 0);
  bi = ava_integer_of_value(b, 0);

  if (!bi)
    return ava_value_of_integer(ai);
  else
    return ava_value_of_integer(ai % bi);
}

#define COMPARATOR(name, op, left_default, right_default)       \
  defun(unsigned__##name)(ava_value a, ava_value b) {           \
    ava_ulong ai, bi;                                           \
    ai = ava_integer_of_value(a, left_default);                 \
    bi = ava_integer_of_value(b, right_default);                \
    return ava_value_of_integer(ai op bi);                      \
  }

COMPARATOR(slt,  <,     -1LL,   0LL)
COMPARATOR(leq, <=,     -1LL,   0LL)
COMPARATOR(sgt,  >,     0LL,    -1LL)
COMPARATOR(geq, >=,     0LL,    -1LL)
#undef COMPARATOR

/******************** REAL OPERATIONS ********************/

#ifndef COMPILING_DRIVER
defun(real__fpclassify)(ava_value a) {
  AVA_STATIC_STRING(error_base, "Unexpected fpclassify() result: ");
  ava_real r;
  int klasse;
  ava_string res;

  r = ava_real_of_value(a, NAN);
  switch ((klasse = fpclassify(r))) {
  case FP_INFINITE: res = AVA_ASCII9_STRING("infinite"); break;
  case FP_NAN:      res = AVA_ASCII9_STRING("nan"); break;
  case FP_NORMAL:   res = AVA_ASCII9_STRING("normal"); break;
  case FP_SUBNORMAL:res = AVA_ASCII9_STRING("subnormal"); break;
  case FP_ZERO:     res = AVA_ASCII9_STRING("zero"); break;
  default:
    ava_throw_str(&ava_internal_exception,
                  ava_strcat(
                    error_base,
                    ava_to_string(ava_value_of_integer(klasse))));
  }

  return ava_value_of_string(res);
}
#endif

/* The below can't be included in the driver since GNU libc uses inline
 * assembly, which apparently LLVMs JIT can't handle.
 */
#ifndef COMPILING_DRIVER
defun(real__is_finite)(ava_value a) {
  return ava_value_of_integer(!!isfinite(ava_real_of_value(a, NAN)));
}

defun(real__is_infinite)(ava_value a) {
  return ava_value_of_integer(!!isinf(ava_real_of_value(a, 0.0)));
}

defun(real__is_nan)(ava_value a) {
  return ava_value_of_integer(!!isnan(ava_real_of_value(a, 0.0)));
}

defun(real__is_normal)(ava_value a) {
  return ava_value_of_integer(!!isnormal(ava_real_of_value(a, NAN)));
}

defun(real__signbit_is_negative)(ava_value a) {
  return ava_value_of_integer(!!signbit(ava_real_of_value(a, +0.0)));
}

defun(real__mantissa)(ava_value a) {
  int dummy;

  return ava_value_of_real(frexp(ava_real_of_value(a, 0.0), &dummy));
}

defun(real__exponent)(ava_value a) {
  int ret;

  (void)frexp(ava_real_of_value(a, 0.0), &ret);
  return ava_value_of_integer(ret);
}

defun(real__fractional)(ava_value a) {
  double dummy;

  return ava_value_of_real(modf(ava_real_of_value(a, 0.0), &dummy));
}

defun(real__integral)(ava_value a) {
  double ret;

  (void)modf(ava_real_of_value(a, 0.0), &ret);
  return ava_value_of_real(ret);
}
#endif /* COMPILING_DRIVER */

defun(real__min)(ava_value a, ava_value b) {
  return ava_value_of_real(
    fmin(ava_real_of_value(a, INFINITY),
         ava_real_of_value(b, INFINITY)));
}

defun(real__max)(ava_value a, ava_value b) {
  return ava_value_of_real(
    fmax(ava_real_of_value(a, -INFINITY),
         ava_real_of_value(b, -INFINITY)));
}

defun(real__add)(ava_value a, ava_value b) {
  return ava_value_of_real(
    ava_real_of_value(a, 0.0) + ava_real_of_value(b, 0.0));
}

defun(real__sub)(ava_value a, ava_value b) {
  return ava_value_of_real(
    ava_real_of_value(a, 0.0) - ava_real_of_value(b, 0.0));
}

defun(real__mul)(ava_value a, ava_value b) {
  return ava_value_of_real(
    ava_real_of_value(a, 1.0) * ava_real_of_value(b, 1.0));
}

defun(real__div)(ava_value a, ava_value b) {
  return ava_value_of_real(
    ava_real_of_value(a, 0.0) / ava_real_of_value(b, 1.0));
}

defun(real__rem)(ava_value a, ava_value b) {
  return ava_value_of_real(
    fmod(ava_real_of_value(a, 0.0),
         ava_real_of_value(b, INFINITY)));
}

defun(real__mod)(ava_value a, ava_value b) {
  ava_real ar, br, res;

  ar = ava_real_of_value(a, 0.0);
  br = ava_real_of_value(b, INFINITY);

  if (isinf(br)) {
    if (ar < 0)
      res = NAN;
    else
      res = ar;
  } else {
    res = ar - fabs(br) * floor(ar / fabs(br));
  }

  return ava_value_of_real(res);
}

defun(real__pow)(ava_value a, ava_value b) {
  return ava_value_of_real(
    pow(ava_real_of_value(a, 1.0),
        ava_real_of_value(b, 1.0)));
}

#define COMPARATOR(name, op)                            \
  defun(real__##name)(ava_value a, ava_value b) {       \
    return ava_value_of_integer(                        \
      ava_real_of_value(a, NAN) op                      \
      ava_real_of_value(b, NAN));                       \
  }
COMPARATOR(equ, ==)
COMPARATOR(neq, !=)
COMPARATOR(slt, < )
COMPARATOR(leq, <=)
COMPARATOR(sgt, > )
COMPARATOR(geq, >=)
#undef COMPARATOR

defun(real__of)(ava_value a, ava_value b) {
  return ava_value_of_real(
    ava_real_of_value(
      a, ava_real_of_value(b, NAN)));
}

/******************** MAP OPERATIONS ********************/

/*
  It might initially seem like the cursor API could be presented directly to
  Avalanche, and this other stuff implemented in Avalanche itself.

  However, cursors are sensitive to the underlying representation, which would
  violate the semantics of Avalanche values. Additionally, there is no way to
  validate an arbitrary cursor value.
 */

defun(map__npairs)(ava_value m) {
  return ava_value_of_integer(ava_map_npairs_v(m));
}

defun(map__get_last_impl)
(ava_map_value map, ava_value key);

defun(map__get_last)(ava_value map, ava_value key) {
  return fun(map__get_last_impl)(ava_map_value_of(map), key);
}

#ifndef COMPILING_DRIVER
AVA_STATIC_STRING(no_such_key_type, "no-such-key");

defun(map__get_last_impl)(ava_map_value map, ava_value key) {
  ava_map_cursor cursor, next_cursor;

  cursor = ava_map_find_f(map, key);
  if (AVA_MAP_CURSOR_NONE == cursor)
    ava_throw_uex(&ava_error_exception, no_such_key_type,
                  ava_error_map_no_such_key(key));

  while (AVA_MAP_CURSOR_NONE !=
         (next_cursor = ava_map_next_f(map, cursor)))
    cursor = next_cursor;

  return ava_map_get_f(map, cursor);
}
#endif

defun(map__get_last_or_empty_impl)
(ava_map_value map, ava_value key);

defun(map__get_last_or_empty)(ava_value map, ava_value key) {
  return fun(map__get_last_or_empty_impl)(ava_map_value_of(map), key);
}

#ifndef COMPILING_DRIVER
defun(map__get_last_or_empty_impl)(ava_map_value map, ava_value key) {
  ava_map_cursor cursor, next_cursor;

  cursor = ava_map_find_f(map, key);
  if (AVA_MAP_CURSOR_NONE == cursor)
    return ava_value_of_string(AVA_EMPTY_STRING);

  while (AVA_MAP_CURSOR_NONE !=
         (next_cursor = ava_map_next_f(map, cursor)))
    cursor = next_cursor;

  return ava_map_get_f(map, cursor);
}
#endif

defun(map__get_all_impl)
(ava_map_value map, ava_value key);

defun(map__get_all)(ava_value map, ava_value key) {
  return fun(map__get_all_impl)(ava_map_value_of(map), key);
}

#ifndef COMPILING_DRIVER
defun(map__get_all_impl)(ava_map_value map, ava_value key) {
  ava_list_value ret;
  ava_map_cursor cursor;

  ret = ava_empty_list();
  for (cursor = ava_map_find_f(map, key);
       AVA_MAP_CURSOR_NONE != cursor;
       cursor = ava_map_next_f(map, cursor))
    ret = ava_list_append_f(ret, ava_map_get_f(map, cursor));

  return ret.v;
}
#endif

defun(map__add)(ava_value map, ava_value key, ava_value value) {
  return ava_map_add_v(map, key, value);
}

defun(map__remap_one_impl)
(ava_map_value map, ava_value key, ava_value value);

defun(map__remap_one)(ava_value map, ava_value key, ava_value value) {
  return fun(map__remap_one_impl)(ava_map_value_of(map), key, value);
}


#ifndef COMPILING_DRIVER
defun(map__remap_one_impl)(ava_map_value map, ava_value key, ava_value value) {
  ava_map_cursor cursor;

  cursor = ava_map_find_f(map, key);
  if (AVA_MAP_CURSOR_NONE == cursor) {
    map = ava_map_add_f(map, key, value);
  } else {
    while (AVA_MAP_CURSOR_NONE != ava_map_next_f(map, cursor)) {
      map = ava_map_remove_f(map, cursor);
      cursor = ava_map_find_f(map, key);
    }

    map = ava_map_set_f(map, cursor, value);
  }

  return map.v;
}
#endif

defun(map__remap_all_impl)
(ava_map_value map, ava_value key, ava_list_value values);

defun(map__remap_all)(ava_value map, ava_value key, ava_value values) {
  return fun(map__remap_all_impl)(
    ava_map_value_of(map), key, ava_list_value_of(values));
}

#ifndef COMPILING_DRIVER
defun(map__remap_all_impl)(
  ava_map_value map, ava_value key, ava_list_value values
) {
  size_t ix, in_list, in_map;
  ava_map_cursor cursor;

  ix = 0;
  in_list = ava_list_length_f(values);
  in_map = 0;
  for (cursor = ava_map_find_f(map, key);
       AVA_MAP_CURSOR_NONE != cursor;
       cursor = ava_map_next_f(map, cursor))
    ++in_map;

  while (in_map > in_list) {
    map = ava_map_remove_f(map, ava_map_find_f(map, key));
    --in_map;
  }

  for (cursor = ava_map_find_f(map, key);
       AVA_MAP_CURSOR_NONE != cursor;
       cursor = ava_map_next_f(map, cursor))
    map = ava_map_set_f(map, cursor, ava_list_index_f(values, ix++));

  while (ix < in_list)
    map = ava_map_add_f(map, key, ava_list_index_f(values, ix++));

  return map.v;
}
#endif

size_t fun(map__count_impl)
(ava_map_value map, ava_value key);

defun(map__count)(ava_value map, ava_value key) {
  return ava_value_of_integer(
    fun(map__count_impl)(ava_map_value_of(map), key));
}

#ifndef COMPILING_DRIVER
size_t fun(map__count_impl)(ava_map_value map, ava_value key) {
  size_t count;
  ava_map_cursor cursor;

  for (count = 0, cursor = ava_map_find_f(map, key);
       AVA_MAP_CURSOR_NONE != cursor;
       cursor = ava_map_next_f(map, cursor))
    ++count;

  return count;
}
#endif

defun(interval__of)(ava_value begin, ava_value end) {
  return ava_interval_value_of_range(
    ava_integer_of_value(begin, 0),
    ava_integer_of_value(end, AVA_INTEGER_END)).v;
}

/******************** LIST OPERATIONS ********************/

defun(list__length)(ava_value list) {
  return ava_value_of_integer(ava_list_length_v(list));
}

defun(list__index)(ava_value raw_list, ava_value index) {
  ava_list_value list;
  ava_integer max, begin, end;
  ava_interval_value ival;

  list = ava_list_value_of(raw_list);
  max = ava_list_length_f(list);

  ival = ava_interval_value_of(index);
  if (ava_interval_is_singular(ival)) {
    begin = ava_interval_get_singular(ival, max);
    strict_index_check(begin, max);
    return ava_list_index_f(list, begin);
  } else {
    begin = ava_interval_get_begin(ival, max);
    end = ava_interval_get_end(ival, max);
    strict_range_check(begin, end, max);
    return ava_list_slice_f(list, begin, end).v;
  }
}

defun(list__index_lenient)(ava_value raw_list, ava_value index) {
  ava_list_value list;
  ava_integer max, begin, end;
  ava_interval_value ival;

  list = ava_list_value_of(raw_list);
  max = ava_list_length_f(list);

  ival = ava_interval_value_of(index);
  if (ava_interval_is_singular(ival)) {
    begin = ava_interval_get_singular(ival, max);
    if (!lenient_index_check(begin, max))
      return ava_value_of_string(AVA_EMPTY_STRING);
    return ava_list_index_f(list, begin);
  } else {
    begin = ava_interval_get_begin(ival, max);
    end = ava_interval_get_end(ival, max);
    lenient_range_check(&begin, &end, max);
    return ava_list_slice_f(list, begin, end).v;
  }
}

ava_value fun(list__set_range)
(ava_list_value list, size_t begin, size_t end, size_t listlen,
 ava_list_value replacement);

defun(list__set)(ava_value raw_list, ava_value index, ava_value val) {
  ava_list_value list;
  ava_integer max, begin, end;
  ava_interval_value ival;

  list = ava_list_value_of(raw_list);
  max = ava_list_length_f(list);

  ival = ava_interval_value_of(index);
  if (ava_interval_is_singular(ival)) {
    begin = ava_interval_get_singular(ival, max);
    strict_index_check(begin, max + 1);

    if (begin == max)
      return ava_list_append_f(list, val).v;
    else
      return ava_list_set_f(list, begin, val).v;
  } else {
    begin = ava_interval_get_begin(ival, max);
    end = ava_interval_get_end(ival, max);
    strict_range_check(begin, end, max);

    return fun(list__set_range)(list, begin, end, max, ava_list_value_of(val));
  }
}

#ifndef COMPILING_DRIVER
ava_value fun(list__set_range)
(ava_list_value list, size_t begin, size_t end, size_t listlen,
 ava_list_value repl)
{
  size_t repl_len, i;

  if (0 == begin && end == listlen) return repl.v;
  if (begin == listlen) return ava_list_concat_f(list, repl).v;
  if (end == 0) return ava_list_concat_f(repl, list).v;

  repl_len = ava_list_length_f(repl);

  /* If we have to do an insertion, just rebuild the whole list */
  if (repl_len > end - begin)
    return ava_list_concat_f(
      ava_list_concat_f(
        ava_list_slice_f(list, 0, begin), repl),
      ava_list_slice_f(list, end, listlen)).v;

  /* Delete elements as necessary */
  if (repl_len < end - begin)
    list = ava_list_remove_f(list, begin + repl_len, end);

  /* Replace what remain */
  for (i = 0; i < repl_len; ++i)
    list = ava_list_set_f(list, begin + i, ava_list_index_f(repl, i));

  return list.v;
}
#endif

defun(list__concat)(ava_value a, ava_value b) {
  return ava_list_concat_v(a, b);
}

defun(list__interleave)
(ava_value lists);
defun(list__demux)
(ava_value list, ava_value offset, ava_value stride);
defun(list__group)
(ava_value list, ava_value group_size);

#ifndef COMPILING_DRIVER
defun(list__interleave)(ava_value raw_lists) {
  AVA_STATIC_STRING(illegal_argument, "illegal-argument");

  ava_list_value* array, on_stack[16];
  ava_list_value lists = ava_list_value_of(raw_lists), l;
  size_t num_lists = ava_list_length_f(lists), i, list_length;

  array = num_lists < 16? on_stack :
    (ava_list_value*)ava_alloc(sizeof(ava_list_value) * num_lists);

  for (i = 0; i < num_lists; ++i) {
    l = ava_list_value_of(ava_list_index_f(lists, i));
    if (0 == i) {
      list_length = ava_list_length_f(l);
    } else {
      if (list_length != ava_list_length_f(l))
        ava_throw_uex(&ava_error_exception, illegal_argument,
                      ava_error_interleaved_lists_not_of_same_length(
                        i, list_length, ava_list_length_f(l)));
    }
    array[i] = l;
  }

  return ava_list_proj_interleave(array, num_lists).v;
}

defun(list__demux)(ava_value list, ava_value raw_offset, ava_value raw_stride) {
  AVA_STATIC_STRING(illegal_argument, "illegal-argument");
  ava_integer offset, stride;

  offset = ava_integer_of_value(raw_offset, 0);
  stride = ava_integer_of_value(raw_stride, 1);

  if (offset < 0)
    ava_throw_uex(&ava_error_exception, illegal_argument,
                  ava_error_illegal_argument(
                    AVA_ASCII9_STRING("offset"), raw_offset));
  if (stride <= offset)
    ava_throw_uex(&ava_error_exception, illegal_argument,
                  ava_error_illegal_argument(
                    AVA_ASCII9_STRING("stride"), raw_stride));

  return ava_list_proj_demux(ava_list_value_of(list), offset, stride).v;
}

defun(list__group)(ava_value list, ava_value raw_group_size) {
  AVA_STATIC_STRING(illegal_argument, "illegal-argument");
  AVA_STATIC_STRING(group_size_name, "group-size");
  ava_integer group_size;

  group_size = ava_integer_of_value(raw_group_size, 1);
  if (group_size <= 0)
    ava_throw_uex(&ava_error_exception, illegal_argument,
                  ava_error_illegal_argument(
                    group_size_name, raw_group_size));

  return ava_list_proj_group(ava_list_value_of(list), group_size).v;
}
#endif

defun(list__flatten)(ava_value list) {
  return ava_list_proj_flatten(ava_list_value_of(list)).v;
}

/******************** POINTER OPERATIONS ********************/

/* All pointer operations are safe; there is deliberately no exposure of ways
 * to actually dereference them. Ultimately, these functions provide no
 * functionality that the application couldn't achieve via string manipulation.
 */

defun(pointer__is_null)(ava_value pointer) {
  return ava_value_of_integer(
    !ava_pointer_get_const_v(pointer, AVA_EMPTY_STRING));
}

defun(pointer__address)(ava_value pointer) {
  return ava_value_of_integer(
    (ava_intptr)ava_pointer_get_const_v(pointer, AVA_EMPTY_STRING));
}

defun(pointer__is_const)(ava_value pointer) {
  return ava_value_of_integer(ava_pointer_is_const_v(pointer));
}

defun(pointer__tag)(ava_value pointer) {
  return ava_value_of_string(ava_pointer_get_tag_v(pointer));
}

defun(pointer__const_cast)(ava_value pointer, ava_value is_const) {
  return ava_pointer_const_cast_to_v(
    pointer, !!ava_integer_of_value(is_const, 1));
}

defun(pointer__reinterpret_cast)(ava_value pointer, ava_value tag) {
  return ava_pointer_reinterpret_cast_to_v(
    pointer, ava_to_string(tag));
}

defun(pointer__add)(ava_value pointer, ava_value offset) {
  return ava_pointer_adjust_v(pointer, ava_integer_of_value(offset, 0));
}

defun(pointer__sub)(ava_value pointer, ava_value offset) {
  return ava_pointer_adjust_v(pointer, -ava_integer_of_value(offset, 0));
}

AVA_END_DECLS
