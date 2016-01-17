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
#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/macsub.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_MACSUB_H_
#define AVA_RUNTIME_MACSUB_H_

#include "defs.h"
#include "string.h"
#include "value.h"
#include "list.h"
#include "parser.h"
#include "errors.h"
#include "name-mangle.h"

/**
 * @file
 *
 * Interface for the macro processor (ie, the Syntax III handler).
 */

struct ava_codegen_context_s;
struct ava_pcode_register_s;
struct ava_symbol_s;
struct ava_symtab_s;
struct ava_varscope_s;
struct ava_compenv_s;
struct ava_pcode_global_list_s;

/**
 * An AST node after macro processing.
 *
 * Unlike most  conventional AST models, the  set of node types  in Avalanche's
 * AST is not  fixed; for example, most intrinsic macros  define their own node
 * type. Because of this, directly walking the AST is not generally viable.
 */
typedef struct ava_ast_node_s ava_ast_node;
/**
 * Context in which macro substitution occurs.
 */
typedef struct ava_macsub_context_s ava_macsub_context;

/**
 * Represents possible stati of macro substitution.
 */
typedef enum {
  /**
   * Indicates that macro substitution has completed and produced an AST node.
   */
  ava_mss_done,
  /**
   * Indicates that macro substitution has resulted in a new statement that
   * must be reexamined.
   *
   * Macros that return this status must not consume statements following the
   * input statement.
   */
  ava_mss_again
} ava_macro_subst_status;

/**
 * Return type from ava_macro_subst_f.
 */
typedef struct {
  /**
   * The status of the substitution.
   */
  ava_macro_subst_status status;
  union {
    /**
     * If ava_mss_done == status, the valid, non-NULL node produced.
     */
    ava_ast_node* node;
    /**
     * If ava_mss_again == status, the nonempty statement to reexamine.
     */
    ava_parse_statement* statement;
  } v;
} ava_macro_subst_result;

/**
 * A function used to process substitution for a single macro.
 *
 * @param self The symbol which pointed the macro processor to this function.
 * @param context The current macro substitution context.
 * @param statement The statement in which the macro was found. The macro may
 * examine the statements following this statement if it sets
 * *consumed_other_statements to ava_true. This pointer is not guaranteed to
 * remain valid after the function returns.
 * @param invoker The parse unit (always a bareword except for the internal
 * lstring, rstring, and lrstring intrinsics) which provoked this macro
 * substitution. The macro should use this to determine what its "left" and
 * "right" arguments are. This pointer is not guaranteed to remain valid after
 * the function returns.
 * @param consumed_other_statements If the function  sets this to ava_true, the
 * macro processor assumes that the macro has consumed the statements following
 * statement itself, and will terminate processing of the statement list.
 * @return The result of substitution.
 */
typedef ava_macro_subst_result (*ava_macro_subst_f)(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * Defines the high-level operations AST nodes must support.
 *
 * Only the name field and the to_string method must be set; defaults are used
 * for other fields.
 */
typedef struct ava_ast_node_vtable_s ava_ast_node_vtable;

struct ava_ast_node_s {
  /**
   * Table of operations against this node.
   */
  const ava_ast_node_vtable* v;

  /**
   * The "representative" location of this node.
   */
  ava_compile_location location;

  /**
   * The context which owns this node.
   */
  ava_macsub_context* context;

  /**
   * Whether this AST node is currently between cg_set_up() and cg_tear_down()
   * calls.
   *
   * This is maintained by the ava_ast_node_cg_*() functions and should not be
   * accessed externally.
   */
  unsigned setup_count;
};

/**
 * Converts the AST node to a string for diagnostic purposes.
 *
 * This is not reversible; the result need not be (and usually is not) valid
 * Avalanche syntax.
 */
typedef ava_string (*ava_ast_node_to_string_f)(const ava_ast_node*);
/**
 * Converts this AST node into an equivalent lvalue AST node.
 *
 * An lvalue AST node *wraps* the node that produces the new value to store
 * within the lvalue. While perhaps unexpected, this provides a natural and
 * flexible way for lvalues to be stacked. The producer can access the old
 * value of the lvalue by generating code for the reader. This implies that the
 * lvalue must be otherwise initialised before execution, but is optional, and
 * the producer may simply discard the reader if it does not intend to read the
 * prior value.
 *
 * If this node cannot be used as an lvalue, the node must record an error and
 * return a valid lvalue AST node.
 *
 * If the value of the producer may be evaluated outside of the L-Value's
 * control, the actual controller should surround evaluation of the actual
 * producer and the L-Value itself with calls to ava_ast_node_cg_set_up() and
 * ava_ast_node_cg_tear_down() on the lvalue.
 *
 * Note that, somewhat unusually, the result of assigning a value to an lvalue
 * is the final value of the *innermost* value. For example, the code
 * ```
 *   foo = [0 1 2]
 *   bar = ($foo[1] = 42)
 *   cstdio.puts $bar
 * ```
 * prints "0 42 2". This is primarily to support the assign-barrier operator in
 * a consistent way.
 *
 * @param lvalue The node to be converted to an lvalue.
 * @param producer The AST node which will determine the value to write back
 * into the lvalue.
 * @param reader An outvar for an AST node which can be used to read the
 * pre-assignment value of the lvalue. The reader is only guaranteed to be
 * meaningful between setup and teardown of the returned node.
 */
typedef ava_ast_node* (*ava_ast_node_to_lvalue_f)(
  const ava_ast_node* lvalue, ava_ast_node* producer,
  ava_ast_node** reader);
/**
 * Performs post-processing on the given node.
 *
 * This is primarily used for second-pass name resolution and function binding.
 *
 * Nodes which have sub-nodes must delegate to their children.
 *
 * Any context needed by the node must have been saved at construction.
 *
 * Calling this function more than once has no observable effect.
 */
typedef void (*ava_ast_node_postprocess_f)(ava_ast_node*);
/**
 * Extracts the compile-time constant value of this AST node, if there is one.
 *
 * @param dst If this AST node has a compile-time constant value, set to that
 * value.
 * @return Whether this AST node has a compile-time constant value.
 */
typedef ava_bool (*ava_ast_node_get_constexpr_f)(
  const ava_ast_node*, ava_value* dst);
/**
 * Extracts the compile-time constant value of this AST node, if there is one
 * and this is a spread node.
 *
 * @param dst If this AST node has a compile-time constant value which is a
 * list and this node is a spread, set to that value.
 * @return Whether this AST node has a compile-time constant value which is a
 * spread list.
 */
typedef ava_bool (*ava_ast_node_get_constexpr_spread_f)(
  const ava_ast_node*, ava_list_value* dst);
/**
 * If this AST node may act like a function name, extracts its text.
 *
 * @return The function name represented by this AST node, or the absent string
 * if it does not represent a function name.
 */
typedef ava_string (*ava_ast_node_get_funname_f)(const ava_ast_node*);
/**
 * If this AST node can be evaluated to produce a value, generates the
 * necessary code to save the value in the given D- or V-register.
 */
typedef void (*ava_ast_node_cg_evaluate_f)(
  ava_ast_node* node, const struct ava_pcode_register_s* dst,
  struct ava_codegen_context_s* context);
/**
 * If this AST node can be evaluated as a spread, generates the necessary code
 * to save the value in the given L-register.
 *
 * The presence of this method on a node indicates the node *is* a spread. It
 * must never be called for nodes that do not define it.
 */
typedef void (*ava_ast_node_cg_spread_f)(
  ava_ast_node* node, const struct ava_pcode_register_s* dst,
  struct ava_codegen_context_s* context);
/**
 * If this AST node has side-effects or produces definitions, generates the
 * necessary code to produce those and discard any results.
 *
 * Nodes which do not implement this are assumed pure; ie, it is an error to
 * discard the result.
 */
typedef void (*ava_ast_node_cg_discard_f)(
  ava_ast_node* node, struct ava_codegen_context_s* context);
/**
 * If this AST node can produce a value, has the same effect as cg_evaluate().
 * Otherwise, equivalent to cg_discard(), except that it must load the empty
 * string into the destination register.
 */
typedef void (*ava_ast_node_cg_force_f)(
  ava_ast_node* node, const struct ava_pcode_register_s* dst,
  struct ava_codegen_context_s* context);
/**
 * If this AST node may insert itself into the symbol table, creates any
 * definitions required for references to the node to be generated.
 */
typedef void (*ava_ast_node_cg_define_f)(
  ava_ast_node* node, struct ava_codegen_context_s* context);

/**
 * The code-generation set-up function for an AST node.
 *
 * All calls to the cg_* methods which generate executable code occur between
 * single calls to cg_set_up() and cg_tear_down(). Nodes do not have to be
 * prepared for generating code without cg_set_up() having been called, or for
 * cg_set_up() to be called more than once before cg_tear_down(), etc.
 *
 * Setup of an AST node may have side-effects (for example, subscripting needs
 * to evaluate its composite and the subscript proper) and may affect the
 * register stacks, but may not transfer flow by direct control instructions.
 *
 * All L-Values should forward this call to their producer with
 * ava_ast_node_cg_set_up() after they have finished setting up (even if they
 * have no setup to do).
 */
typedef void (*ava_ast_node_cg_set_up_f)(
  ava_ast_node* node, struct ava_codegen_context_s* context);
/**
 * The code-generation tear-down function for an AST node.
 *
 * This must restore the register stacks, etc, to what they were before the
 * corresponding call to cg_set_up().
 *
 * @see ava_ast_node_cg_tear_down_f()
 */
typedef void (*ava_ast_node_cg_tear_down_f)(
  ava_ast_node* node, struct ava_codegen_context_s* context);

struct ava_ast_node_vtable_s {
  /**
   * A human-readable name for this AST node, used in diagnostics.
   *
   * This field is mandatory.
   */
  const char* name;

  /**
   * The to_string method. This is mandatory.
   */
  ava_ast_node_to_string_f to_string;
  ava_ast_node_to_lvalue_f to_lvalue;
  ava_ast_node_postprocess_f postprocess;
  ava_ast_node_get_constexpr_f get_constexpr;
  ava_ast_node_get_constexpr_spread_f get_constexpr_spread;
  ava_ast_node_get_funname_f get_funname;
  ava_ast_node_cg_evaluate_f cg_evaluate;
  ava_ast_node_cg_spread_f cg_spread;
  ava_ast_node_cg_discard_f cg_discard;
  ava_ast_node_cg_force_f cg_force;
  ava_ast_node_cg_define_f cg_define;
  ava_ast_node_cg_set_up_f cg_set_up;
  ava_ast_node_cg_tear_down_f cg_tear_down;
};

/**
 * Convenience for (*node->v->to_string)(node).
 */
ava_string ava_ast_node_to_string(const ava_ast_node* node);
/**
 * Calls the given node's to_lvalue method if there is one; otherwise, executes
 * a default implementation.
 */
ava_ast_node* ava_ast_node_to_lvalue(
  const ava_ast_node* node, ava_ast_node* producer,
  ava_ast_node** reader);
/**
 * Calls the given node's postprocess method if there is one.
 */
void ava_ast_node_postprocess(ava_ast_node* node);
/**
 * Calls the given node's get_constexpr method if there is one; otherwise,
 * executes a default implementation.
 */
ava_bool ava_ast_node_get_constexpr(const ava_ast_node* node,
                                    ava_value* dst);
/**
 * Calls the given node's get_constexpr_spread method if there is one;
 * otherwise, executes a default implementation.
 */
ava_bool ava_ast_node_get_constexpr_spread(
  const ava_ast_node* node, ava_list_value* dst);
/**
 * Calls the given node's get_funname method if there is one; otherwise,
 * executes a default implementation.
 */
ava_string ava_ast_node_get_funname(const ava_ast_node* node);
/**
 * Calls the given node's cg_evaluate method if there is one; otherwise,
 * records an error that the node does not produce a value.
 *
 * This call implies calls to ava_ast_node_cg_set_up() and
 * ava_ast_node_cg_tear_down().
 */
void ava_ast_node_cg_evaluate(ava_ast_node* node,
                              const struct ava_pcode_register_s* dst,
                              struct ava_codegen_context_s* context);
/**
 * Calls the given node's cg_spread method.
 *
 * The process aborts if the method is not defined on the node.
 *
 * This call implies calls to ava_ast_node_cg_set_up() and
 * ava_ast_node_cg_tear_down().
 */
void ava_ast_node_cg_spread(ava_ast_node* node,
                            const struct ava_pcode_register_s* dst,
                            struct ava_codegen_context_s* context);
/**
 * Calls the given node's cg_discard method if there is one; otherwise, records
 * an error that the node is pure.
 *
 * This call implies calls to ava_ast_node_cg_set_up() and
 * ava_ast_node_cg_tear_down().
 */
void ava_ast_node_cg_discard(ava_ast_node* node,
                             struct ava_codegen_context_s* context);
/**
 * Calls the given node's cg_force method if there is one. Otherwise, chooses a
 * default implementation based on whether the node defines cg_evaluate().
 *
 * This call implies calls to ava_ast_node_cg_set_up() and
 * ava_ast_node_cg_tear_down().
 */
void ava_ast_node_cg_force(ava_ast_node* node,
                           const struct ava_pcode_register_s* dst,
                           struct ava_codegen_context_s* context);
/**
 * Calls the given node's cg_define method.
 *
 * The process aborts if this method is not defined on the node.
 *
 * This call does nothing if node is NULL.
 */
void ava_ast_node_cg_define(ava_ast_node* node,
                            struct ava_codegen_context_s* context);

/**
 * Ensures the cg_set_up() method has been called on the given node, if it has
 * one.
 *
 * This call is balanced by calls to ava_ast_node_cg_tear_down(); if n calls
 * are made to this function, the cg_tear_down() method is invoked only after n
 * calls have been made to it.
 */
void ava_ast_node_cg_set_up(ava_ast_node* node,
                            struct ava_codegen_context_s* context);

/**
 * Balance for ava_ast_node_cg_tear_down().
 */
void ava_ast_node_cg_tear_down(ava_ast_node* node,
                               struct ava_codegen_context_s* context);

/**
 * Describes how a sequence of statements determines what value to return as a
 * result of evaluation.
 */
typedef enum {
  /**
   * Indicates that the sequence does not return a value.
   */
  ava_isrp_void,
  /**
   * Indicates that the sequence returns the result of its final statement, if
   * it has one, or the empty string otherwise.
   */
  ava_isrp_last,
  /**
   * Indicates that the sequence returns the result of its only statement if it
   * contains exactly one statement, or the empty string otherwise.
   */
  ava_isrp_only
} ava_intr_seq_return_policy;

/**
 * Creates a new, global-level macro substitution context.
 *
 * @param root_symbol_table The symbol table representing global scope.
 * @param compenv The compilation environment associated with the context.
 * @param errors List into which errors will be acumulated.
 * @param symbol_prefix The implicit prefix for all names defined within the
 * context.
 * @return The new context.
 */
ava_macsub_context* ava_macsub_context_new(
  struct ava_symtab_s* root_symbol_table,
  struct ava_compenv_s* compenv,
  ava_compile_error_list* errors,
  ava_string symbol_prefix);

/**
 * Returns the current, mutable symbol table of the given context.
 */
struct ava_symtab_s* ava_macsub_get_symtab(
  const ava_macsub_context* context);

/**
 * Returns the compilation environment controlling this macro substitution
 * context.
 */
struct ava_compenv_s* ava_macsub_get_compenv(
  const ava_macsub_context* context);

/**
 * Returns the varscope governing the scope of the given context.
 */
struct ava_varscope_s* ava_macsub_get_varscope(
  const ava_macsub_context* context);

/**
 * Performs an import on the given context's symtab, replacing the symtab with
 * the new one produced.
 *
 * All arguments are as per ava_symtab_import().
 */
void ava_macsub_import(
  ava_string* absolutised, ava_string* ambiguous,
  ava_macsub_context* context,
  ava_string old_prefix, ava_string new_prefix,
  ava_bool absolute, ava_bool is_strong);

/**
 * Returns the error accumulation for the given context.
 */
ava_compile_error_list* ava_macsub_get_errors(
  const ava_macsub_context* context);
/**
 * Applies the implicit name prefix of the given context to the given name.
 *
 * @param context The context whose prefix is to be applied.
 * @param simple_name The basic name to be prefixed.
 * @return simple_name with the prefix prepended.
 */
ava_string ava_macsub_apply_prefix(
  const ava_macsub_context* context, ava_string simple_name);
/**
 * Returns the current major scope nesting level of the context.
 */
unsigned ava_macsub_get_level(
  const ava_macsub_context* context);

/**
 * Generates a new gensym prefix for the given location.
 *
 * After this call, any string previously returned by ava_macsub_gensym() in
 * this context will never be returned again from the same context, even if the
 * same location is used as a seed.
 */
void ava_macsub_gensym_seed(
  ava_macsub_context* context, const ava_compile_location* location);

/**
 * Returns a unique symbol with the given key.
 *
 * Between calls of ava_macsub_gensym_seed(), this function will return the
 * same symbol when called with the same key.
 *
 * If key is all-lower-case, so is the result.
 */
ava_string ava_macsub_gensym(const ava_macsub_context* context,
                             ava_string key);

/**
 * Creates a new context representing a major scope nested within the given
 * parent.
 *
 * Major scopes are essentially function boundaries; names defined within are
 * not visible outside the major scope. The level of a major scope is one
 * greater than its parent, and the inner scope has a fresh varscope from that
 * of the parent.
 *
 * @param parent The parent context.
 * @param interfix New string to append to the implicit prefix of the resulting
 * context.
 * @return The child context.
 */
ava_macsub_context* ava_macsub_context_push_major(
  const ava_macsub_context* parent, ava_string interfix);
/**
 * Creates a new context representing a minor scope nested within the given
 * parent.
 *
 * Minor scopes are used for namespace sections and such, which may use a
 * different prefix and have different imports, but still define symbols at the
 * same level. The level of a minor scope is the same as that of its parent,
 * and any names defined within are also visible in the parent. The minor
 * scope's varscope is the same as the parent's.
 *
 * @param parent The parent context.
 * @param interfix New string to append to the implicit prefix of the resulting
 * context.
 * @return The child context.
 */
ava_macsub_context* ava_macsub_context_push_minor(
  const ava_macsub_context* parent, ava_string interfix);

/**
 * Returns the symbol (always type ava_st_local_variable or
 * ava_st_global_variable) representing the context variable "$" in the current
 * context, or NULL if there is no current context variable.
 */
struct ava_symbol_s* ava_macsub_get_context_var(
  const ava_macsub_context* context);

/**
 * Returns a child macro substitution context identical to the parent except
 * that it has the given symbol as the context variable.
 */
ava_macsub_context* ava_macsub_context_with_context_var(
  const ava_macsub_context* parent, struct ava_symbol_s* symbol);

/**
 * Adds the given symbol to the context's symbol table.
 *
 * If an error occurs, either due to an issue with the symbol table or an
 * illegal visibility given the context, it is added to the error list.
 *
 * @param contet The macro substitution context.
 * @param symbol The symbol to add. It is added using its full_name.
 * @param location If an error occurs, the location at which to report the
 * error.
 * @return Whether the symbol was actually added. A false value indicates that
 * an error occurred.
 */
ava_bool ava_macsub_put_symbol(
  ava_macsub_context* context, struct ava_symbol_s* symbol,
  const ava_compile_location* location);

/**
 * Runs full macro substitution on the given list of statements, producing a
 * single root AST node.
 *
 * Whether processing was successful can be determined by testing whether any
 * errors were added to the context.
 *
 * @param context The context in which macro substitution is to run.
 * @param start The location where this statement sequence starts.
 * @param statements The list of statements to process. The statements may be
 * subject to in-place modification by macros.
 * @param return_policy The return policy for this sequence of statements.
 * @return A valid AST node representing the result of processing.
 */
ava_ast_node* ava_macsub_run(ava_macsub_context* context,
                             const ava_compile_location* start,
                             ava_parse_statement_list* statements,
                             ava_intr_seq_return_policy return_policy);

/**
 * Convenience for calling ava_macsub_run() on a block's statements or
 * ava_macsub_run_units() on a single substitution.
 *
 * This is used for the common convention of structures accepting either blocks
 * as void-multistatement structures or expressions as last-monostatement
 * structures.
 *
 * @param context The context in which macro substitution is to run.
 * @param container The unit containing the sequence of statements for which to
 * run macro substitution.
 * @return A valid AST node representing the result of processing.
 */
ava_ast_node* ava_macsub_run_contents(ava_macsub_context* context,
                                      const ava_parse_unit* container);

/**
 * Equivalent to calling ava_macsub_run() with a statement list containing the
 * given statement and all statements that follow it.
 *
 * @param context The context in which macro substitution is to run.
 * @param start The location where this statement sequence starts.
 * @param statement The first of the list of statements to process. The
 * statements may be subject to in-place modification by macros. A NULL
 * statement corresponds to a sequence of zero statements.
 * @param return_policy The return policy for this sequence of statements.
 * @return A valid AST node representing the result of processing.
 */
ava_ast_node* ava_macsub_run_from(ava_macsub_context* context,
                                  const ava_compile_location* start,
                                  ava_parse_statement* statement,
                                  ava_intr_seq_return_policy return_policy);

/**
 * Equivalent to calling ava_macsub_run() with a statement list containing only
 * the given statement.
 *
 * The return policy is implicitly ava_isrp_only.
 *
 * @param context The context in which macro substitution is to run.
 * @param start The location where this statement sequence starts.
 * @param statement The single statement to which to apply macro substitution.
 * The statement may be subject to in-place modification by macros.
 * @return A valid AST node representing the result of processing.
 */
ava_ast_node* ava_macsub_run_single(ava_macsub_context* context,
                                    const ava_compile_location* start,
                                    ava_parse_statement* statement);

/**
 * Equivalent to calling ava_macsub_run() with a statement list containing a
 * single statement containing the units between first and last, inclusive.
 *
 * The return policy is implicitly ava_isrp_last.
 *
 * @param context The context in which macro substitution is to run.
 * @param first The first parse unit, inclusive, to which to apply macro
 * substitution. This is assumed to be the start of a statement.
 * @param last The final parse unit, inclusive, to which to apply macro
 * substitution.
 * @return A valid AST node representing the result of processing.
 */
ava_ast_node* ava_macsub_run_units(ava_macsub_context* context,
                                   const ava_parse_unit* first,
                                   const ava_parse_unit* last);

/**
 * Records an error with the given message and location.
 *
 * @prama context The current macro substitution context.
 * @param error The error to record.
 */
void ava_macsub_record_error(ava_macsub_context* context,
                             ava_compile_error* error);

/**
 * Records an error with the given message and location, providing an AST node
 * as a placeholder for the error.
 *
 * @param context The current macro substitution context.
 * @param error The error to log.
 * @return An AST node that stands in as a placeholder for the node that failed
 * substitution. The node pretends to implement everything that can be called
 * in the macro substitution stage; for example, it can be used as an lvalue.
 */
ava_ast_node* ava_macsub_error(ava_macsub_context* context,
                               ava_compile_error* error);

/**
 * Convenience for calling ava_macsub_error() and wrapping it in an
 * ava_macro_subst_result.
 */
ava_macro_subst_result ava_macsub_error_result(
  ava_macsub_context* context, ava_compile_error* error);

/**
 * Sets the panic flag on the given macro substitution context.
 *
 * When the panic flag is set, no further macro substitution occurs, any any
 * attempts to evaluate input immediately return a silent error.
 *
 * The panic flag is shared between all contexts created from the same parent.
 */
void ava_macsub_panic(ava_macsub_context* context);

/**
 * Inserts into the given context's symbol table symbols exported from the
 * given P-Code module, as if the module were loaded with reqmod or reqpkg.
 *
 * @param context The context to modify.
 * @param module The P-Code to insert.
 * @param name The name of the package or module, as would be passed to reqmod
 * or reqpkg.
 * @param location The location to report for errors due to this insertion.
 * @param is_package True if this is a package, false if a module.
 */
/* Implemented in -intrinsics/require.c */
void ava_macsub_insert_module(
  ava_macsub_context* context, const struct ava_pcode_global_list_s* module,
  ava_string name,
  const ava_compile_location* location, ava_bool is_package);

/**
 * Returns an error AST node without emitting any errors.
 */
ava_ast_node* ava_macsub_silent_error(const ava_compile_location* location);

/**
 * Returns a macro substitution result wrapping a silent error as produced by
 * ava_macsub_silent_error().
 */
ava_macro_subst_result ava_macsub_silent_error_result(
  const ava_compile_location* location);

/**
 * Convenience function for declaring/defining standard macro substitution
 * functions.
 *
 * Qualifiers may be placed before the macro, and it may be followed by a body
 * or a semicolon.
 */
#define AVA_DEF_MACRO_SUBST(name)               \
  ava_macro_subst_result name(                  \
    const struct ava_symbol_s* self,            \
    ava_macsub_context* context,                \
    const ava_parse_statement* statement,       \
    const ava_parse_unit* provoker,             \
    ava_bool* consumed_other_statements)

#endif /* AVA_RUNTIME_MACSUB_H_ */
