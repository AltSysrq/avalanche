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
#ifndef AVA_PLATFORM_NATIVE_MEMORY_H_
#define AVA_PLATFORM_NATIVE_MEMORY_H_

#include "abi.h"

/**
 * Throw an exception indicating that the system has run out of memory or some
 * other memory-related resource limit has been reached.
 *
 * This is normally only called by client code when using unmanaged
 * allocations.
 */
void ava_mem_oom(AVA_DEF_ARGS0()) AVA_NORETURN AVA_RTAPI;

/**
 * Allocates a block of memory suitable for storing a standard object or opaque
 * data.
 *
 * A memory region with aligned to AVA_ALIGNMENT of at least size bytes is
 * allocated and a pointer to the head of the block returned. It is not
 * initialised. If the memory cannot be allocated, an exception is thrown as
 * with ava_mem_oom().
 */
void* ava_mem_alloc_obj(AVA_DEF_ARGS1(size_t size)) AVA_MALLOC AVA_RTAPI;

#endif /* AVA_PLATFORM_NATIVE_MEMORY_H_ */
