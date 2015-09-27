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
#ifndef AVA_RUNTIME__INTRINSICS_PASTA_H_
#define AVA_RUNTIME__INTRINSICS_PASTA_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The `pasta` control macro.
 *
 * Syntax:
 *   pasta [block] {clause}*
 *   clause ::= label block
 *
 * label is a bareword.
 *
 * Semantics:
 *
 * Each label is assigned to a private "other" symbol within the current scope.
 * In the absence of any other control effects, each block is executed in
 * sequence. The pasta structure does not produce a value.
 *
 * The pasta structure is intended for directly expressing state machines and
 * similar, though it can also be used to produce spaghetti. It is up to the
 * programmer to produce a responsible form of pasta.
 */
ava_macro_subst_result ava_intr_pasta_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * The `goto` control macro.
 *
 * Syntax:
 *   goto label
 *
 * label is a bareword.
 *
 * Semantics:
 *
 * Control immediately transfers into the block immediately following the
 * matching label, which must be a symbol defined by a pasta structure which
 * encloses the goto and is in the same function.
 */
ava_macro_subst_result ava_intr_goto_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

#endif /* AVA_RUNTIME__INTRINSICS_PASTA_H_ */
