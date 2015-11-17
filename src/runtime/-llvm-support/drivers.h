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

#define DEFDRIVER(name)                                 \
  extern const char*const ava_driver_##name##_data;     \
  extern const size_t ava_driver_##name##_size

/**
 * ISA driver compiled without any runtime checks enabled. Anything stated to
 * have undefined behaviour really has undefined behaviour.
 */
DEFDRIVER(isa_unchecked);

/**
 * Driver providing the main() function for compiled programs.
 */
DEFDRIVER(main);

/**
 * The low-level component of the org.ava-lang.avast package compiled in
 * unchecked mode.
 */
DEFDRIVER(avast_unchecked);

/**
 * The low-level component of the org.ava-lang.avast package compiled at check
 * level 1 (overflow and such unchecked; more common errors still checked).
 */
DEFDRIVER(avast_checked_1);

/**
 * The low-level component of the org.ava-lang.avast package compiled at check
 * level 2 (all undefined behaviour checked).
 */
DEFDRIVER(avast_checked_2);

#undef DEFDRIVER

AVA_END_DECLS

#endif /* AVA_RUNTIME__LLVM_SUPPORT_DRIVERS_H_ */
