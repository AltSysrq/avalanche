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
#error "Don't include avalanche/alloc.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_ALLOC_H_
#define AVA_RUNTIME_ALLOC_H_

#include "defs.h"

/******************** MEMORY MANAGEMENT ********************/
/**
 * Initialises the Avalanche heap. This should be called once, at the start of
 * the process.
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
 * memory is initialised to zeroes.
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

