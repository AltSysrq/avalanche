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
#error "Don't include avalanche/defs.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_DEFS_H_
#define AVA_RUNTIME_DEFS_H_

#include "ava-config.h"

#include <stdlib.h>
#include <setjmp.h>
#ifdef AVA_HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef AVA_HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#if defined(AVA_HAVE_SYS_QUEUE_H)
#include <sys/queue.h>
#elif defined(AVA_HAVE_BSD_SYS_QUEUE_H)
#include <bsd/sys/queue.h>
#else
#error "No BSD sys/queue.h found. You may need to install libbsd-dev."
#endif

/* As of 2015-09-08, even libbsd in debian sid doesn't have TAILQ_SWAP. Provide
 * it if the host does not.
 */
#ifndef TAILQ_SWAP
#define TAILQ_SWAP(head1, head2, type, field) do {			\
	struct type *swap_first = (head1)->tqh_first;			\
	struct type **swap_last = (head1)->tqh_last;			\
	(head1)->tqh_first = (head2)->tqh_first;			\
	(head1)->tqh_last = (head2)->tqh_last;				\
	(head2)->tqh_first = swap_first;				\
	(head2)->tqh_last = swap_last;					\
	if ((swap_first = (head1)->tqh_first) != NULL)			\
		swap_first->field.tqe_prev = &(head1)->tqh_first;	\
	else								\
		(head1)->tqh_last = &(head1)->tqh_first;		\
	if ((swap_first = (head2)->tqh_first) != NULL)			\
		swap_first->field.tqe_prev = &(head2)->tqh_first;	\
	else								\
		(head2)->tqh_last = &(head2)->tqh_first;		\
} while (0)
#endif

/* Same for TAILQ_FOREACH_SAFE */
#ifndef TAILQ_FOREACH_SAFE
#define	TAILQ_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = TAILQ_FIRST((head));				\
	    (var) && ((tvar) = TAILQ_NEXT((var), field), 1);		\
	    (var) = (tvar))
#endif

#if defined(__GNUC__) || defined(__clang__)
#define AVA_MALLOC __attribute__((__malloc__))
#define AVA_PURE __attribute__((__pure__))
#define AVA_CONSTFUN __attribute__((__const__))
#define AVA_NORETURN __attribute__((__noreturn__))
#define AVA_UNUSED __attribute__((__unused__))
#define AVA_LIKELY(x) __builtin_expect(!!(x), 1)
#define AVA_UNLIKELY(x) __builtin_expect((x), 0)
#define AVA_ALIGN(n) __attribute__((__aligned__(n)))
#else
#define AVA_MALLOC
#define AVA_PURE
#define AVA_CONSTFUN
#define AVA_NORETURN
#define AVA_UNUSED
#define AVA_LIKELY(x) (x)
#define AVA_UNLIKELY(x) (x)
#define AVA_ALIGN(n)
#endif

#ifdef __cplusplus
#define AVA_BEGIN_DECLS extern "C" {
#define AVA_END_DECLS }
/* So indentation works as desired */
#define AVA_BEGIN_FILE_PRIVATE namespace {
#define AVA_END_FILE_PRIVATE }
#else
#define AVA_BEGIN_DECLS
#define AVA_END_DECLS
#endif

/* Assign names to the exactly-sized integer types, since I feel it's more
 * readable, and because autoconf-provided definitions are ava_ prefixed, while
 * those from stdint are not.
 */

#ifdef ava_int8_t
typedef ava_int8_t    ava_sbyte;
#else
typedef int8_t        ava_sbyte;
#endif
#ifdef ava_uint8_t
typedef ava_uint8_t   ava_ubyte;
#else
typedef uint8_t       ava_ubyte;
#endif
#ifdef ava_int16_t
typedef ava_int16_t   ava_sshort;
#else
typedef int16_t       ava_sshort;
#endif
#ifdef ava_uint16_t
typedef ava_uint16_t  ava_ushort;
#else
typedef uint16_t      ava_ushort;
#endif
#ifdef ava_int32_t
typedef ava_int32_t   ava_sint;
#else
typedef int32_t       ava_sint;
#endif
#ifdef ava_uint32_t
typedef ava_uint32_t  ava_uint;
#else
typedef uint32_t      ava_uint;
#endif
#ifdef ava_int64_t
typedef ava_int64_t   ava_slong;
#else
typedef int64_t       ava_slong;
#endif
#ifdef ava_uint64_t
typedef ava_uint64_t  ava_ulong;
#else
typedef uint64_t      ava_ulong;
#endif
#ifdef ava_intptr_t
typedef ava_intptr_t  ava_intptr;
#else
typedef intptr_t      ava_intptr;
#endif

#define AVA_GLUE(x,y) AVA__GLUE(x,y)
#define AVA__GLUE(x,y) x##y
#define AVA_STRINGIFY(x) AVA__STRINGIFY(x)
#define AVA__STRINGIFY(x) #x

typedef int ava_bool;
#define ava_true 1
#define ava_false 0

/**
 * Floating-point type used in real arithmetic calculations.
 */
typedef double ava_real;

/**
 * The root of the package provided by the runtime and the standard library.
 */
#define AVA_AVAST_PACKAGE "org.ava-lang.avast"
/**
 * The namespace used by intrinsics provided by the runtime.
 */
#define AVA_AVAST_INTRINSIC AVA_AVAST_PACKAGE ":intrinsic"

#endif /* AVA_RUNTIME_DEFS_H_ */
