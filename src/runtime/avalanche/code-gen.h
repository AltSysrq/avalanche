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
#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/code-gen.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_CODE_GEN_H_
#define AVA_RUNTIME_CODE_GEN_H_

#include "errors.h"
#include "parser.h"
#include "macsub.h"
#include "pcode.h"

/**
   @file
   @brief General definitions and utilities for P-Code generation.

   This file defines the common interfaces for P-Code generation, their
   contracts, and a partial front-end to the P-Code builders to make certain
   patterns easier.

   Code generation of a node is mostly directed by its immediate container.
   There are three modes of generation:

   - evaluate. The node is being evaluated as an expression and is to store the
     result in a V or D register of the caller's choosing.

   - discard. The node is being evaluated for its side-effects or definitions
     and is to discard any result it produces.

   - define. The node is to define any global definitions needed by nodes that
     may reference it. Nothing is to be evaluated at the current
     code-generation point. This method is only supported on nodes which place
     themselves within the symbol table.

   In either case, if the node requires any global support code, it is the
   node's responsibility to generate it exactly once. Any stack-like state a
   code generation method changes must be restored by the time it returns.
 */

struct ava_symbol_s;

/**
 * State container for code generation within a single function.
 *
 * Each function gets its own code generation context. There is no context for
 * global code generation; instead, any global state that may need to be
 * tracked is shared between the context and its children.
 */
typedef struct ava_codegen_context_s ava_codegen_context;

/**
 * Creates a new code generation context for the given function code builder.
 *
 * Global state is inherited from the given parent context.
 */
ava_codegen_context* ava_codegen_context_new(
  const ava_codegen_context* parent,
  ava_pcx_builder* builder);

/**
 * Adds an error to the given codegen context.
 */
void ava_codegen_error(ava_codegen_context* context,
                       const ava_ast_node* node,
                       ava_compile_error* error);

/**
 * Front-end to ava_pcxb_push() which tracks the stack height.
 *
 * This must be used instead of the lower-level builder function.
 *
 * @param register_type The register type to push.
 * @param count The number of registrs to push.
 * @return The index of the first pushed register.
 * @see ava_codegen_pop_reg()
 */
ava_pcode_register_index ava_codegen_push_reg(
  ava_codegen_context* context,
  ava_pcode_register_type register_type,
  ava_uint count);

/**
 * Front-end to ava_pcxb_pop() which tracks the stack height.
 *
 * This must be used instead of the lower-level builder function.
 *
 * @see ava_codegen_push_reg()
 */
void ava_codegen_pop_reg(ava_codegen_context* context,
                         ava_pcode_register_type register_type,
                         ava_uint count);

/**
 * Sets the current source location within the P-Code for the context's
 * function.
 *
 * Instructions are only emitted as necessary when locations actually change
 * relative to the last call.
 */
void ava_codegen_set_location(
  ava_codegen_context* context,
  const ava_compile_location* location);

/**
 * Like ava_codegen_set_location(), but on a global builder.
 *
 * This does not track any state, so every invocation adds statements to the
 * builder.
 */
void ava_codegen_set_global_location(
  ava_codegen_context* context,
  const ava_compile_location* location);

/**
 * Creates any global export statement necessary to export the given symbol
 * with its visibility and pcode_index.
 */
void ava_codegen_export(
  ava_codegen_context* context, const struct ava_symbol_s* symbol);

/**
 * Performs code-generation on the given AST node.
 *
 * It is up to the root node to know how to correctly handle being the root,
 * ie, the semantics around global scope and so forth.
 *
 * @param root The AST node at the root of the file.
 * @param errors A list to which any encountered errors are encountered.
 * @return The generated P-Code. The P-Code is not necessarily valid if errors
 * is non-empty.
 */
ava_pcode_global_list* ava_codegen_run(
  ava_ast_node* root,
  ava_compile_error_list* errors);

/**
 * Returns the pcode executable builder associated with the given code
 * generation context.
 */
ava_pcx_builder* ava_codegen_get_builder(
  const ava_codegen_context* context) AVA_PURE;

/**
 * Convenience for writing
 *   ava_pcxb_OP(ava_codegen_get_builder(context), ...)
 */
#define AVA_PCXB(op, ...)                                               \
  (ava_pcxb_##op(ava_codegen_get_builder(context), __VA_ARGS__))
/**
 * Convenience for writing
 *   ava_pcgb_OP(ava_pcx_builder_get_parent(
 *                 ava_codegen_get_builder(context)), ...)
 */
#define AVA_PCGB(op, ...)                                               \
  (ava_pcgb_##op(ava_pcx_builder_get_parent(                            \
                   ava_codegen_get_builder(context)), __VA_ARGS__))

#endif /* AVA_RUNTIME_CODE_GEN_H_ */
