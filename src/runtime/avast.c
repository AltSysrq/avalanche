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

#include <stdlib.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/map.h"
#include "avalanche/integer.h"
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

#define defun(name)                             \
  ava_value a$org__ava_lang__avast___##name

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

/******************** STRING OPERATIONS ********************/

defun(byte_string__concat)(ava_value a, ava_value b) {
  return ava_value_of_string(
    ava_string_concat(
      ava_to_string(a), ava_to_string(b)));
}

defun(byte_string__length)(ava_value a) {
  return ava_value_of_integer(
    ava_string_length(ava_to_string(a)));
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
