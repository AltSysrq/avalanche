/*-
 * Copyright (c) 2015, 2016, Jason Lingle
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
#ifndef AVA_RUNTIME__INTRINSICS_USER_MACRO_H_
#define AVA_RUNTIME__INTRINSICS_USER_MACRO_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"
#include "../avalanche/symbol.h"
#include "../avalanche/pcode.h"

/**
 * The `macro`, `Macro`, and `MACRO` control macros.
 *
 * Syntax:
 *   macro name type [precedence] ...
 *
 * The symbol table gains a macro named `name`. `type` is a bareword, one of
 * "expand", "control", "op", or "fun". If and only if the type is "op", the
 * `precedence` argument is consumed, and must be a bareword parseable as an
 * integer.
 *
 * The rest of the arguments form the macro definition, which behave as
 * documented in the Syntax III section of the spec.
 *
 * The visibility of the resulting symbol is intrinsic to the macro, derived
 * from *(ava_visibility*)userdata on the self symbol.
 */
ava_macro_subst_result ava_intr_user_macro_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * Substitution functions for user macros.
 *
 * The macro userdata is the ava_pcode_macro_list* which defines the macro.
 */
ava_macro_subst_result ava_intr_user_macro_eval(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * Defines a user macro in the given context.
 *
 * @param context The macro substitution context in which the macro will be
 * defined.
 * @param type The symbol type to create.
 * @param visibility The visibility of the symbol to create.
 * @param name The base name of the symbol to define.
 * @param precedence The macro precedence.
 * @param body The body of the macro.
 * @param main_location The location to report for the resulting AST node.
 * @param name_location The location to report if an issue with the name
 * arises.
 * @return An AST node which defines the macro.
 */
ava_ast_node* ava_intr_user_macro_put(
  ava_macsub_context* context, ava_symbol_type type,
  ava_visibility visibility, ava_string name,
  int precedence, ava_pcode_macro_list* body,
  const ava_compile_location* main_location,
  const ava_compile_location* name_location);

#endif /* AVA_RUNTIME__INTRINSICS_USER_MACRO_H_ */
