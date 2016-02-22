/*-
 * Copyright (c) 2016, Jason Lingle
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
#ifndef AVA_PLATFORM_NATIVE_DEFS_H_
#define AVA_PLATFORM_NATIVE_DEFS_H_

#include "ava-config.h"

#ifndef __cplusplus
#define AVA_BEGIN_DECLS
#define AVA_END_DECLS
#else
#define AVA_BEGIN_DECLS extern "C" {
#define AVA_END_DECLS }
#endif

/* Eventually for the dllimport/dllexport cruft. This is a GCC style attribute
 * rather than something for the position MSVC expects so that other things can
 * be added naturally later. (We require LLVM to do anything useful with the
 * native platform, so requiring Clang as a compiler on Windows isn't really a
 * large leap.)
 */
#define AVA_RTAPI

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

#endif /* AVA_PLATFORM_NATIVE_DEFS_H_ */
