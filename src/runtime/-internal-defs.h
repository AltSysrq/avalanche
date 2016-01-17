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
#ifndef AVA_RUNTIME__INTERNAL_DEFS_H_
#define AVA_RUNTIME__INTERNAL_DEFS_H_

#if defined(__GNUC__) && defined(__SSE2__) && !defined(__clang__)
#define AVA_SPINLOOP __builtin_ia32_pause()
#elif defined(__SSE2__) && defined(__clang__)
/* Strangely, clang lacks the above builtin */
#define AVA_SPINLOOP asm volatile ("pause" : )
#else
#define AVA_SPINLOOP
#endif

/**
 * Expands to an integer indicating the ABI alignment of the given type.
 */
#define ava_alignof(type)                                       \
  offsetof(struct { char padding; type target; }, target)

/**
 * Expands to the length of the given array.
 */
#define ava_lenof(array)                        \
  (sizeof(array) / sizeof((array)[0]))

#endif /* AVA_RUNTIME__INTERNAL_DEFS_H_ */
