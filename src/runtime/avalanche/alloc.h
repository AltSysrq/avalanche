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
#error "Don't include avalanche/alloc.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_ALLOC_H_
#define AVA_RUNTIME_ALLOC_H_

#include "defs.h"

/******************** MEMORY MANAGEMENT ********************/
/**
 * Initialises the Avalanche heap. This should be called once, at the start of
 * the process.
 *
 * There is generally no reason to call this function directly; use ava_init()
 * instead.
 */
void ava_heap_init(void);
/**
 * Allocates and returns a block of memory of at least the given size. The
 * memory is initialised to zeroes.
 *
 * There is no way to explicitly free this memory; it will be released
 * automatically when no GC-visible pointers remain.
 *
 * If memory allocation fails, the process is aborted.
 */
void* ava_alloc(size_t) AVA_MALLOC;
/**
 * Allocates and returns a block of memory of at least the given size. The
 * memory is *not* initialised to zeroes; its contents are undefined.
 *
 * The caller may not store any pointers in this memory.
 *
 * There is no way to explicitly free this memory; it will be released
 * automatically when no GC-visible pointers remain.
 *
 * If memory allocation fails, the process is aborted.
 */
void* ava_alloc_atomic(size_t) AVA_MALLOC;
/**
 * Allocates and returns a block of memory of at least the given size.
 *
 * This memory must be explicitly freed with ava_free_unmanaged(). Its chief
 * difference from a plain malloc() is that the GC is aware of this memory, so
 * it may contain pointers to managed memory.
 *
 * If memory allocation fails, the process is aborted.
 */
void* ava_alloc_unmanaged(size_t) AVA_MALLOC;
/**
 * Frees the given memory allocated from ava_alloc_unmanaged().
 */
void ava_free_unmanaged(void*);
/**
 * Performs an ava_alloc() with the given size, then copies that many bytes
 * from the given source pointer into the new memory before returning it.
 *
 * If memory allocation fails, the process is aborted.
 */
void* ava_clone(const void*restrict, size_t) AVA_MALLOC;
/**
 * Performs an ava_alloc_atomic() with the given size, then copies that many
 * bytes from the given source pointer into the new memory before returning it.
 *
 * If memory allocation fails, the process is aborted.
 */
void* ava_clone_atomic(const void*restrict, size_t) AVA_MALLOC;
/**
 * Syntax sugar for calling ava_alloc() with the size of the selected type and
 * casting it to a pointer to that type.
 */
#define AVA_NEW(type) ((type*)ava_alloc(sizeof(type)))

#endif /* AVA_RUNTIME_ALLOC_H_ */

