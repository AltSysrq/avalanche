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
#ifndef AVA_RUNTIME__INTRINSICS_ENUM_H_
#define AVA_RUNTIME__INTRINSICS_ENUM_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The `seqenum`, `Seqenum`, and `SEQENUM` control macros.
 *
 * Syntax:
 *   seqenum [ns-name] {
 *     {element}*
 *   }
 *   {element} ::= element-name ["=" constexpr]
 *
 * An expander macro for each element-name is defined to expand to a single
 * Verbatim containing a normalised integer. If the "=" syntax is used,
 * constexpr must be a constant expression which evaluates to an integer
 * (default 0). For elements where no explicit value is given, the element is
 * assigned a value one greater than the previous, or 0 if it is the very first
 * element in the enum.
 *
 * If given, ns-name is a bareword indicating the namespace of the enum; the
 * enum values will be defined as if the whole statement was wrapped in a
 * `namespace` with that ns-name. Otherwise, the enum values are not prefixed.
 *
 * Expander expansion is run on each statement within the body before it is
 * parsed and immediately after the previous element has been defined.
 *
 * The visibility of the defined symbols is found by following the pointer in
 * the macro userdata.
 */
AVA_DEF_MACRO_SUBST(ava_intr_seqenum_subst);

/**
 * The `bitenum`, `Bitenum`, and `BITENUM` control macros.
 *
 * This is exactly the same as `seqenum`, except that the default value for the
 * first element is 1 and subsequent values are derived by bit-shifting the
 * prior value left by 1.
 *
 * The C implementation does not do anything in particular if the value flows
 * over.
 */
AVA_DEF_MACRO_SUBST(ava_intr_bitenum_subst);

#endif /* AVA_RUNTIME__INTRINSICS_ENUM_H_ */
