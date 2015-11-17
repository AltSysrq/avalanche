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
#ifndef AVA_RUNTIME__INTRINSICS_SUBSCRIPT_H_
#define AVA_RUNTIME__INTRINSICS_SUBSCRIPT_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The #name-subscript#, #numeric-subscript#, and #string-subscript# macros.
 *
 * Syntax:
 *   #subscript# type composite key
 *
 * When used as an rvalue, roughly equivalent to writing
 *   `#subscript#gettype (composite) (key)`
 * (eg, "$foo[42]?" -> "#subscript#get#?# $foo 42"). In this case, an error
 * occurs if composite is a bareword, since such usage is doomed to fail in
 * almost all cases and is almost certainly intended to be a variable read.
 *
 * When used as an lvalue, the read form expands as above (keeping in mind any
 * implicit read semantics applied to the composite lvalue), and the write form
 * expands to
 *   `#subscript#withtype (composite) (key) (produced)`
 * (where `produced` is the result from the lvalue producer).
 *
 * Note that the above "expansions" are not processed for macro expansion; that
 * is, `#subscript#gettype` must resolve to a function name.
 *
 * Evaluation order for lvalues is as follows:
 * - composite is evaluated
 * - key is evaluated
 * - the producer is invoked, possibly invoking the getter
 * - the wither is invoked
 * - the result of the wither is returned
 *
 * The macro userdata is a C string indicating the value of `#subscript#`.
 */
ava_macro_subst_result ava_intr_subscript_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

#endif /* AVA_RUNTIME__INTRINSICS_SUBSCRIPT_H_ */
