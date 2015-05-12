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

/* The macro AVA_NOGC can be used to suppress all use of the Boehm GC, and
 * instead use malloc() for everything.
 *
 * This makes library leak memory like a sieve, but makes it possible to use
 * diagnostic tools like valgrind to check for other memory errors. It also
 * makes working with gdb a bit less irritating, since one need not constantly
 * tell it "handle SIGSEGV noprint nostop".
 */

#ifndef AVA_NOGC
#if defined(HAVE_GC_GC_H)
#include <gc/gc.h>
#elif defined(HAVE_GC_H)
#include <gc.h>
#else
#error "Neither <gc/gc.h> nor <gc.h> could be found."
#endif
#else /* AVA_NOGC */
#define GC_MALLOC(sz) (calloc(1, (sz)))
#define GC_MALLOC_ATOMIC(sz) (malloc(sz))
#define GC_MALLOC_UNCOLLECTABLE(sz) (calloc(1, (sz)))
#define GC_FREE(ptr) (free(ptr))
#endif /* !AVA_NOGC */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "bsd.h"

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/alloc.h"

static inline void* ava_oom_if_null(void* ptr) {
  if (!ptr)
    errx(EX_UNAVAILABLE, "out of memory");

  return ptr;
}

void ava_heap_init(void) {
#ifndef AVA_NOGC
  GC_INIT();
  GC_enable_incremental();
#endif
}

void* ava_alloc(size_t sz) {
  return ava_oom_if_null(GC_MALLOC(sz));
}

void* ava_alloc_atomic(size_t sz) {
  return ava_oom_if_null(GC_MALLOC_ATOMIC(sz));
}

void* ava_alloc_unmanaged(size_t sz) {
  return ava_oom_if_null(GC_MALLOC_UNCOLLECTABLE(sz));
}

void ava_free_unmanaged(void* ptr) {
  GC_FREE(ptr);
}

void* ava_clone(const void*restrict src, size_t sz) {
  void* dst = ava_alloc(sz);
  memcpy(dst, src, sz);
  return dst;
}

void* ava_clone_atomic(const void*restrict src, size_t sz) {
  void* dst = ava_alloc_atomic(sz);
  memcpy(dst, src, sz);
  return dst;
}
