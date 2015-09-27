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
 * Sentinal value used for symbolic labels indicating that the symbolic label
 * is not defined in the current context.
 */
#define AVA_LABEL_NONE ((ava_uint)0xFFFFFFFF)
/**
 * Sentinal value used for symbolic labels indicating that the symbolic label,
 * while defined in the current context, may not currently be used.
 */
#define AVA_LABEL_SUPPRESS ((ava_uint)0xFFFFFFFE)

/**
 * State container for code generation within a single function.
 *
 * Each function gets its own code generation context. There is no context for
 * global code generation; instead, any global state that may need to be
 * tracked is shared between the context and its children.
 */
typedef struct ava_codegen_context_s ava_codegen_context;

/**
 * Function type for use with ava_codegen_push_jprot().
 *
 * Called for any code-path which would transfer control outside a
 * jump-protected section. Generates any code necessary for the jump
 * protection. Note that it may not effect a net change upon the register
 * stacks, and that it will necessarily be called more than once (on distinct
 * code-paths) when the contained code performs an early exit.
 *
 * This function may safely generate its own early exit without fear of
 * infinite recursion.
 *
 * @param context The calling codegen context.
 * @param location The location of the jump crossing this jump protector. NULL
 * if the jump protector is being run because it's region is being terminated
 * with ava_codegen_pop_jprot().
 * @param userdata The userdata passed into ava_codegen_push_jprot().
 */
typedef void (*ava_codegen_jprot_exit_f)(
  ava_codegen_context* context,
  const ava_compile_location* location,
  void* userdata);
/**
 * Context data used by ava_codegen_push_jprot().
 *
 * This is usually stack-allocated.
 */
typedef struct ava_codegen_jprot_s {
  /**
   * The label ordinal assigned to this jump protector. This is not an actual
   * label in P-Code, but rather displaces one.
   *
   * A label is within the jump protector if its key is greater than the jprot
   * ordinal and is encountered while the protector is still on the protector
   * stack.
   */
  ava_uint ordinal;

  /**
   * Function to call along any code-path exiting the jump protector.
   */
  ava_codegen_jprot_exit_f exit;
  /**
   * Userdata to pass to exit.
   */
  void* userdata;

  SLIST_ENTRY(ava_codegen_jprot_s) next;
} ava_codegen_jprot;

/**
 * Identifies a type of symlabel.
 *
 * Two symlabel names are equal only if they share the same memory address. The
 * embedded string is used for debugging.
 */
typedef struct {
  const char* name;
} ava_codegen_symlabel_name;

/**
 * Context data used by ava_codegen_push_symlabel().
 *
 * This is usually stack-allocated.
 */
typedef struct ava_codegen_symlabel_s {
  const ava_codegen_symlabel_name* name;
  ava_uint label;

  SLIST_ENTRY(ava_codegen_symlabel_s) next;
} ava_codegen_symlabel;

/**
 * Identifies a type of symreg.
 *
 * Two symreg names are equal only if the share the same memory address. The
 * embedded string is used for debugging.
 */
typedef struct {
  const char* name;
} ava_codegen_symreg_name;

/**
 * Context data used by ava_codegen_push_symreg().
 *
 * This is usually stack-allocated.
 */
typedef struct ava_codegen_symreg_s {
  const ava_codegen_symreg_name* name;
  ava_pcode_register reg;

  SLIST_ENTRY(ava_codegen_symreg_s) next;
} ava_codegen_symreg;

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
 * Pushes a jump-protector onto the jump-protection stack.
 *
 * A jump-protector is a code segment that must be generated along *any*
 * code-path which leaves the area within the protector, such as cleanup of
 * exception handlers.
 *
 * Specifically, and branch or ret instruction within the protector (identified
 * by the presence of the protector on the stack when the instruction is
 * generated) to a label outside the protector will provoke generation of the
 * protector's exit segment before the actual instruction.
 *
 * The algorithm for identifying branches leading out of a protected segment
 * strongly assumes a purely structural form. That is, for code within the
 * protected segment to know about a label outside of it, that label must have
 * come from some structure that wraps the protected segment itself, and which
 * generated the label name with ava_codegen_genlabel() before the protected
 * segment was pushed.
 *
 * This call must be balanced with a call to ava_codegen_pop_jprot().
 *
 * @param elt Stack element in which to store this part of the jump-protector
 * stack. Completely initialised by this call.
 * @param context The current codegen context.
 * @param exit The function to run to generate code needed to exit the
 * jump-protector segment.
 * @param userdata Arbitrary data to pass to the exit function.
 */
void ava_codegen_push_jprot(ava_codegen_jprot* elt,
                            ava_codegen_context* context,
                            ava_codegen_jprot_exit_f exit,
                            void* userdata);
/**
 * Pops a jump-protector from the stack.
 *
 * Balances a call to ava_codegen_push_jprot().
 *
 * The exit function is implicitly invoked immediately after the jump-protector
 * itself has been popped from the protection stack.
 */
void ava_codegen_pop_jprot(ava_codegen_context* context);

/**
 * Pushes a new symbolic label onto the symbolic label stack of the given
 * context, such that calls to ava_codegen_get_symlabel() with the given name
 * will return the current label.
 *
 * Symbolic labels are used for resolving references to fixed symbolic
 * locations defined by containing structures, such as where to jump upon a
 * "continue" statement.
 *
 * This call must be balanced with a call to ava_codegen_pop_symlabel().
 */
void ava_codegen_push_symlabel(ava_codegen_symlabel* elt,
                               ava_codegen_context* context,
                               const ava_codegen_symlabel_name* name,
                               ava_uint label);

/**
 * Pops a symbolic label off the stack.
 *
 * Balances a call to ava_codegen_push_symlabel().
 */
void ava_codegen_pop_symlabel(ava_codegen_context* context);

/**
 * Looks a symbolic label up by name.
 *
 * @param context The context to search.
 * @param name The name of the symbolic label to find.
 * @return The symbolic label, or AVA_LABEL_NONE if no symbolic label with that
 * name exists.
 */
ava_uint ava_codegen_get_symlabel(const ava_codegen_context* context,
                                  const ava_codegen_symlabel_name* name);

/**
 * Pushes a new symbolic register onto the symbolic register stack of the given
 * context, such that that register will be returned from calls to
 * ava_codegen_get_symreg() with that name.
 *
 * Symbolic registers are used for providing access to registers used by
 * enclosing contexts, such as the loop accumulator which can be set by the
 * "break" macro.
 *
 * This call must be balanced with a call to ava_codegen_pop_symreg().
 */
void ava_codegen_push_symreg(ava_codegen_symreg* elt,
                             ava_codegen_context* context,
                             const ava_codegen_symreg_name* name,
                             ava_pcode_register reg);

/**
 * Pops a symbolic label off the stack.
 *
 * Balances a call to ava_codegen_pop_symreg().
 */
void ava_codegen_pop_symreg(ava_codegen_context* context);

/**
 * Looks a symbolic register up by name.
 *
 * @param context The context to search.
 * @param name The name of the symbolic register to find.
 * @return A pointer to the matched register, or NULL if the given name is
 * unbound in the context. The pointer is only valid until the next call to an
 * ava_codegen_*_symreg() function.
 */
const ava_pcode_register* ava_codegen_get_symreg(
  const ava_codegen_context* context,
  const ava_codegen_symreg_name* name);

/**
 * Generates a label id unique to the function being generated by the given
 * context.
 *
 * This does not produce an actual ava_pcxt_label instruction.
 */
ava_uint ava_codegen_genlabel(ava_codegen_context* context);
/**
 * Emits an ava_pcxt_branch instruction to the current context.
 *
 * If this crosses any jump protectors, they are invoked in the branch-taken
 * path. This must therefore be used instead of the lower-level builder
 * function.
 *
 * All arguments beyond location as per ava_pcxb_branch().
 */
void ava_codegen_branch(ava_codegen_context* context,
                        const ava_compile_location* location,
                        ava_pcode_register key,
                        ava_integer value,
                        ava_bool invert,
                        ava_uint target);
/**
 * Emits an ava_pcxt_goto instruction to the current context.
 *
 * If this crosses any jump protectors, they are invoked before the actual jump
 * occurs. This must therefore be used instead of the lower-level builder
 * function.
 *
 * All arguments beyond location as per ava_pcxb_goto().
 */
void ava_codegen_goto(ava_codegen_context* context,
                      const ava_compile_location* location,
                      ava_uint target);
/**
 * Emits an ava_pcxt_ret instruction to the current context.
 *
 * If this crosses any jump protectors, they are invoked before the ret
 * instruction occurs. This must therefore be used instead of the lower-level
 * builder function.
 *
 * All arguments beyond location as per ava_pcxb_ret().
 */
void ava_codegen_ret(ava_codegen_context* context,
                     const ava_compile_location* location,
                     ava_pcode_register value);

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
