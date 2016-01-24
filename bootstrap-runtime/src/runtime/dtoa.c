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

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "-internal-defs.h"

/* dtoa configuration */

/* Byte order. Assuming that the integer byte order matches the float byte
 * order.
 */
#ifdef WORDS_BIGENDIAN
#define IEEE_MC68k
#else
#define IEEE_8087
#endif

/* "#define Long int on machines with 32-bit ints and 64-bit longs". This seems
 * a rather obtuse approach compared to "provide int32_t and int64_t", but oh
 * well.
 */
#if 4 == SIZEOF_INT && 8 == SIZEOF_LONG
#define Long int
#endif

/* Support multi-threaded environments */
#define MULTIPLE_THREADS
/* The locks only contain trivial operations, so use spinlocking. */
static AO_t dtoa_locks[2];
static inline void ACQUIRE_DTOA_LOCK(unsigned ix) {
  while (AVA_UNLIKELY(
           !AO_compare_and_swap_full(dtoa_locks + ix, 0, 1)))
    AVA_SPINLOOP;
}

static inline void FREE_DTOA_LOCK(unsigned ix) {
  AO_store_full(dtoa_locks + ix, 0);
}

/* errno is not our property */
#define NO_ERRNO

/* Larger private memory to reduce contention */
#define PRIVATE_MEM 32768

/* Move non-static symbols to our namespace */
#define gethex ava_dtoa_gethex
#define strtod ava_strtod
#define freedtoa ava_dtoa_free
#define dtoa ava_dtoa
#define g_fmt ava_dtoa_fmt

#include "../contrib/dtoa.c"
#include "../contrib/g_fmt.c"
