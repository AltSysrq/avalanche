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
#ifndef AVA_RUNTIME__INTRINSICS_ESOTERICA_H_
#define AVA_RUNTIME__INTRINSICS_ESOTERICA_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * @file
 *
 * Function-like macros under the "esoterica" namespace which don't really fit
 * in anywhere else.
 *
 * The "S." prefix is shorthand here for "esoterica.unsafe.strangelet".
 */

/**
 * Macro to get the current stack-pointer.
 *
 *   S.get-sp ()
 *
 * Produces a strangelet referencing the current stack pointer, so that it can
 * later be restored with S.set-sp.
 *
 * This is a direct front-end to the S-get-sp P-Code instruction; documentation
 * for exact behaviour can be found there.
 *
 * While not inherently unsafe, this macro is in the S namespace since it is
 * useless without S.set-sp, which is extremely unsafe.
 *
 * This macro is pure.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_get_sp_subst);

/**
 * Macro to reset the stack-pointer.
 *
 *   S.set-sp sp
 *
 * sp is interpreted a strangelet that was produced by S.get-sp in the current
 * function invocation, and the stack pointer is restored to that location,
 * freeing any stack allocations that occurred between the two points.
 *
 * This is a direct front-end to the S-set-sp P-Code instruction; documentation
 * for exact behaviour can be found there.
 *
 * This macro produces no value.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_set_sp_subst);

/**
 * Macro to hint to the hardware that it is within a spinloop.
 *
 *   S.cpu-pause ()
 *
 * While not inherently unsafe, it is located under S because (a) it does not
 * generally have any useful effect outside of very low-level code using other
 * unsafe intrinsics, and (b) to reduce the liklihood of it being mistaken for
 * a primitive which yields the CPU to another thread, which this does not do.
 *
 * This is a direct front-end to the cpu-pause P-Code instruction;
 * documentation for the exact behaviour can be found there.
 *
 * This macro produces no value.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_cpu_pause_subst);

#endif /* AVA_RUNTIME__INTRINSICS_ESOTERICA_H_ */
