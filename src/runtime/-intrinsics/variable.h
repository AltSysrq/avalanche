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
#ifndef AVA_RUNTIME__INTRINSICS_VARIABLE_H_
#define AVA_RUNTIME__INTRINSICS_VARIABLE_H_

#include "../avalanche/string.h"
#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The intrinsic #var# control macro.
 *
 * Syntax:
 *   #var# name
 *
 * name is a bareword identifying the variable to read.
 */
ava_macro_subst_result ava_intr_var_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * The intrinsic #set# control macro.
 *
 * Syntax:
 *   #set# target expression
 *
 * target and expression are indivually macro-substituted in isolation. target
 * is then converted to an lvalue wrapping expression, with the lvalue reader
 * discarded.
 */
ava_macro_subst_result ava_intr_set_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * Returns a variable lvalue.
 *
 * This is basically the lvalue form of a bareword.
 *
 * @param context The macro substitution context.
 * @param name The name of the variable.
 * @param location The location where the name occurs.
 * @param producer The AST node which produces a value to assign to the
 * variable.
 * @param reader Outvar for an AST node that will read the old value of the
 * variable.
 * @return An AST node representing the variable as an lvalue.
 */
ava_ast_node* ava_intr_variable_lvalue(
  ava_macsub_context* context,
  ava_string name,
  const ava_compile_location* location,
  ava_ast_node* producer,
  ava_ast_node** reader);

#endif /* AVA_RUNTIME__INTRINSICS_VARIABLE_H_ */
