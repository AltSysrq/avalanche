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

#include "../../../../common/bsd-defs.h"

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

AVA_BEGIN_DECLS
/**
 * Initialises the avalanche runtime.
 *
 * This should be called exactly once at process startup.
 *
 * @see ava_heap_init()
 * @see ava_value_hash_init()
 * @see ava_exception_init()
 */
void ava_init(void);
AVA_END_DECLS

#endif /* AVA_RUNTIME_DEFS_H_ */
