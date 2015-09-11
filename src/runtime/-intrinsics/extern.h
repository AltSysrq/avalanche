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
#ifndef AVA_RUNTIME__INTRINSICS_EXTERN_H_
#define AVA_RUNTIME__INTRINSICS_EXTERN_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The `extern`, `Extern`, and `EXTERN` control macros.
 *
 * Syntax:
 *   extern ava-name native-name prototype...
 *
 * TODO: Make the syntax more like whatever `fun` ends up using.
 *
 * The symbol table gains a function `ava-name` which references the C function
 * with the given `native-name` (which must already be mangled, if applicable)
 * and prototype.
 *
 * The elements of prototype are aggregated into a list, with a `1` as an
 * implicit zeroth element, and the result converted to an ava_function.
 *
 * ava-name must be a function name. native-name and all elements of prototype
 * may be arbitrary constant expressions.
 *
 * The visibility of the resulting symbol is intrinsic to the macro, derived
 * from *(ava_visibility*)userdata on the self symbol.
 */
ava_macro_subst_result ava_intr_extern_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

#endif /* AVA_RUNTIME__INTRINSICS_EXTERN_H_ */
