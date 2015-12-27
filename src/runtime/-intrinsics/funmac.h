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
#ifndef AVA_RUNTIME__INTRINSICS_FUNMAC_H_
#define AVA_RUNTIME__INTRINSICS_FUNMAC_H_

#include "../avalanche/string.h"

struct ava_symbol_s;
struct ava_function_s;
struct ava_macsub_context_s;
struct ava_codegen_context_s;
struct ava_ast_node_s;
struct ava_compile_location_s;

/**
 * Sentinal non-AST-node used to indicate a function-like macro's argument
 * being bound to the constant "true".
 */
#define AVA_FUNMAC_TRUE ((ava_ast_node*)1)

/**
 * Called by a simplified function-like macro immediately after arguments have
 * been bound.
 *
 * This callback may modify the argument array however it chooses, and this
 * will be reflected in the array passed into the corresponding cg_evaluate
 * function.
 *
 * @param userdata The userdata passed into ava_funmac_of().
 * @param local_userdata Userdata particular to this usage of the macro.
 * Initially NULL, but changes made by this call will be retained.
 * @param context The current macro substitution context.
 * @param location The location of the AST node.
 * @param args The array of arguments that was bound. The array will contain
 * NULL entries for unbound optional or implicit arguments.
 */
typedef void (*ava_funmac_accept_f)(
  void* userdata,
  void** local_userdata,
  const struct ava_compile_location_s* location,
  struct ava_macsub_context_s* context,
  struct ava_ast_node_s** args);

/**
 * Generates the code for a function-like macro.
 *
 * @param userdata The userdata passed into ava_funmac_of().
 * @param local_userdata Userdata particular to this usage of the macro.
 * Passed as whatever value accept() set it to, or NULL if it was not set.
 * @param dst The P-Code D- or V-register into which to write the final result.
 * In the case of cg_discard, this is NULL.
 * @param context The current codegen context.
 * @param location The location of the macro's AST node.
 * @param args The arguments bound to the function-like macro. If the macro
 * defined an accept function, this array reflects any modifications made by
 * that callback.
 */
typedef void (*ava_funmac_cg_evaluate_f)(
  void* userdata,
  void* local_userdata,
  const struct ava_pcode_register_s* dst,
  struct ava_codegen_context_s* context,
  const struct ava_compile_location_s* location,
  struct ava_ast_node_s*const* args);

/**
 * Creates a symbol containing a simplified function-like macro.
 *
 * Simplified function-like macros are always nominally public and global (as
 * with all intrinsic macros) and of type ava_st_function_macro. On
 * substitution, each syntax unit to the right of the macro is run through
 * macro substitution individually. The resulting array of AST nodes is then
 * fed through the standard function parameter-argument binding mechanism
 * (according to the prototype argument) to permute the arguments into the
 * desired order.
 *
 * Omitted optional arguments (including implicits, which are considered always
 * omitted) are represented as NULL AST nodes in the final argument array.
 *
 * All argument binding forms other than varargs are supported. Macro
 * substitution fails if binding fails, since performing dynamic binding would
 * not make sense.
 *
 * If an argument binds to the constant value "true" (as with the "bool"
 * argument type; note that this cannot result from the constant expression
 * "true" being passed from user code), it is represented by the sentinal value
 * AVA_FUNMAC_TRUE rather than a real AST node.
 *
 * @param name The fully-qualified name of the macro.
 * @param prototype Function prototype which describes how to bind parameters
 * to arguments. This also dictates the size of the argument arrays passed to
 * accept and cg_evaluate.
 * @param accept Function to call immediately after argument binding, in case
 * the macro wishes to inspect or modify the arguments during macro
 * substitution. This may be NULL, indicating to do nothing.
 * @param cg_evaluate Function to call to generate the actual code for the
 * macro and produce a value. If NULL, the macro is considered to produce no
 * value.
 * @param cg_discard Function to call to generate the actual code for the
 * macro, producing no value. If NULL, the macro is considered pure and may not
 * be discarded. This is of the same type as cg_evaluate to simplify the common
 * case of impure expression macros.
 * @param userdata Userdata to pass to the callbacks.
 * @return A symbol containing the described function-like macro.
 */
const struct ava_symbol_s* ava_funmac_of(
  ava_string name,
  const struct ava_function_s* prototype,
  ava_funmac_accept_f accept,
  ava_funmac_cg_evaluate_f cg_evaluate,
  ava_funmac_cg_evaluate_f cg_discard,
  void* userdata);

#endif /* AVA_RUNTIME__INTRINSICS_FUNMAC_H_ */
