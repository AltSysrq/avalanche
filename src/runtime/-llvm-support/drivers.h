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
#ifndef AVA_RUNTIME__LLVM_SUPPORT_DRIVERS_H_
#define AVA_RUNTIME__LLVM_SUPPORT_DRIVERS_H_

/**
 * @file
 *
 * Declares symbols defined by drivers and similar blobs compiled into the
 * runtime library. These blobs are specific to the target for which this
 * runtime library is built.
 */

#include <stdlib.h>

AVA_BEGIN_DECLS

/**
 * The number of bytes in ava_driver_isa_unchecked_data.
 */
extern const size_t ava_driver_isa_unchecked_size;
/**
 * ISA driver compiled without any runtime checks enabled. Anything stated to
 * have undefined behaviour really has undefined behaviour.
 */
extern const char*const ava_driver_isa_unchecked_data;

/**
 * The number of bytes in ava_driver_main_data.
 */
extern const size_t ava_driver_main_size;
/**
 * Driver providing the main() function for compiled programs.
 */
extern const char*const ava_driver_main_data;

AVA_END_DECLS

#endif /* AVA_RUNTIME__LLVM_SUPPORT_DRIVERS_H_ */
