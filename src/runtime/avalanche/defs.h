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

#endif /* AVA_RUNTIME_DEFS_H_ */
