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
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * Creates an AST node which directly reads the given symbol.
 *
 * @param context The macro substitution context in which this occurs.
 * @param symbol The symbol of a variable or function to read.
 * @param location The location to report if anything goes wrong.
 */
ava_ast_node* ava_intr_var_read_new(
  ava_macsub_context* context, const ava_symbol* symbol,
  const ava_compile_location* location);

/**
 * The intrinsic #set# and #update# control macros.
 *
 * Syntax:
 *   #set# target expression
 *   #update# target expression
 *
 * target and expression are indivually macro-substituted in isolation. target
 * is then converted to an lvalue wrapping expression. In the #set# case, the
 * lvalue reader is discarded. In the #update# case, the lvalue reader is read
 * and the value stored in a gensymmed variable, which is set as the context
 * variable within expression.
 *
 * The macro userdata differentiates between the two cases. #set# has NULL
 * userdata, whereas #update# has any non-NULL userdata.
 */
ava_macro_subst_result ava_intr_set_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * Generates a local/global variable symbol with a unique name (using tag
 * somewhere within the name), adds it to the given macro susbstitution
 * context, and returns it.
 *
 * The resulting variable is always private and mutable. It will be a global
 * variable if found at global scope.
 *
 * Note that it is the caller's responsibility to call ava_macsub_gensym_seed()
 * as necessary to ensure that the names are actually unique.
 */
struct ava_symbol_s* ava_var_gen(
  ava_macsub_context* context,
  ava_string tag, const ava_compile_location* location);

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
