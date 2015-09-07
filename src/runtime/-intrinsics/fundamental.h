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
#ifndef AVA_RUNTIME__INTRINSICS_FUNDAMENTAL_H_
#define AVA_RUNTIME__INTRINSICS_FUNDAMENTAL_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * @file
 *
 * Anonymous intrinsics fundamental to the macro processing stage, such as
 * statement nodes and the concatenative string pseudo-macros.
 */

/**
 * An AST node which contains some number of nodes corresponding to statements.
 */
typedef struct ava_intr_seq_s ava_intr_seq;

/**
 * Creates a new, empty seq with the given return policy.
 */
ava_intr_seq* ava_intr_seq_new(
  ava_macsub_context* context,
  const ava_compile_location* start_location,
  ava_intr_seq_return_policy return_policy);

/**
 * Appends the given node as a new statement to the end of seq.
 *
 * The node need not be a node produced by ava_intr_statement().
 */
void ava_intr_seq_add(ava_intr_seq* seq, ava_ast_node* node);

/**
 * Converts an ava_intr_seq to a mundane ava_ast_node.
 *
 * The result and the input reference the same memory.
 */
ava_ast_node* ava_intr_seq_to_node(ava_intr_seq* seq);

/**
 * Creates an AST node holding the given statement which has no remaining macro
 * substitutions.
 */
ava_ast_node* ava_intr_statement(ava_macsub_context* context,
                                 const ava_parse_statement* statement,
                                 const ava_compile_location* location);

/**
 * Creates an AST node holding the given parse unit.
 *
 * Statements within the unit may be modified in-place by macro substitution.
 */
ava_ast_node* ava_intr_unit(ava_macsub_context* context,
                            ava_parse_unit* unit);

/**
 * Implements the pseudo-macro used to process L-Strings, R-Strings, and
 * LR-Strings.
 *
 * Parameters as per ava_macro_subst_f, except for the symbol, which has no
 * effect since it is not meaningful for Strings.
 */
ava_macro_subst_result ava_intr_string_pseudomacro(
  const ava_symbol* ignored,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

#endif /* AVA_RUNTIME__INTRINSICS_FUNDAMENTAL_H_ */
