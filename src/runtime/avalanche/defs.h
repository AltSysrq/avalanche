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

#if defined(__GNUC__) || defined(__clang__)
#define AVA_MALLOC __attribute__((__malloc__))
#define AVA_PURE __attribute__((__pure__))
#define AVA_CONSTFUN __attribute__((__const__))
#define AVA_NORETURN __attribute__((__noreturn__))
#else
#define AVA_MALLOC
#define AVA_PURE
#define AVA_CONSTFUN
#define AVA_NORETURN
#endif

#ifdef __cplusplus
#define AVA_BEGIN_DECLS extern "C" {
#define AVA_END_DECLS }
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

#define AVA_GLUE(x,y) AVA__GLUE(x,y)
#define AVA__GLUE(x,y) x##y

typedef int ava_bool;
#define ava_true 1
#define ava_false 0

#endif /* AVA_RUNTIME_DEFS_H_ */
