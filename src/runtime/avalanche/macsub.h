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
#error "Don't include avalanche/macsub.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_MACSUB_H_
#define AVA_RUNTIME_MACSUB_H_

#include "defs.h"
#include "string.h"
#include "symbol-table.h"
#include "function.h"
#include "parser.h"
#include "name-mangle.h"

/**
 * @file
 *
 * Interface for the macro processor (ie, the Syntax III handler).
 */

/**
 * The maximum precedence, inclusive, of an operator macro.
 */
#define AVA_MAX_OPERATOR_MACRO_PRECEDENCE 40

/**
 * The type of a symbol in a symbol table.
 */
typedef enum {
  /**
   * A symbol which  references a variable or constant (but  not a function) at
   * global scope.
   */
  ava_st_global_variable,
  /**
   * A symbol which references a function at global scope.
   *
   * All  properties  valid for  a  global  variable  are  valid for  a  global
   * function.
   */
  ava_st_global_function,
  /**
   * A symbol which references a variable or constant (but not a function) in a
   * non-global scope.
   */
  ava_st_local_variable,
  /**
   * A symbol which references a function in a non-global scope.
   *
   * All properties valid for a local variable are valid for a local function.
   */
  ava_st_local_function,
  /**
   * A symbol which is a control macro.
   *
   * A control macro is invoked when it is named as the first bareword in a
   * statement with at lest one token. Control macros are expanded before all
   * other macros in the same statement.
   */
  ava_st_control_macro,
  /**
   * A symbol which is an operator macro.
   *
   * Operator macros are expanded anywhere they  are named by a bareword. Order
   * of expansion is controlled by precedence as described in SPEC.md.
   */
  ava_st_operator_macro,
  /**
   * A symbol which is a function macro.
   *
   * A function macro  is invoked when it  is named as the first  bareword in a
   * statement with more than one token. Function macros are expanded after all
   * other macros in the same statement.
   */
  ava_st_function_macro
} ava_symbol_type;

/**
 * Defines the visiblity of a symbol.
 */
typedef enum {
  /**
   * Indicates that the symbol is visible everywhere.
   */
  ava_v_public,
  /**
   * Indicates that the symbol is only  visible within the package that defines
   * it.
   */
  ava_v_internal,
  /**
   * Indicates that the symbol is only visible in the module that defines it.
   */
  ava_v_private
} ava_visibility;

/**
 * A symbol in a symbol table.
 */
typedef struct ava_symbol_s ava_symbol;
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
    ava_parse_statement statement;
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
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

struct ava_symbol_s {
  /**
   * The type of this symbol.
   */
  ava_symbol_type type;

  /**
   * The function nesting level of this symbol.
   *
   * Level 0  refers to global scope.  Level 1 is  the inner scope of  a global
   * function; level 2 is the inner scope of a level 1 function, and so on.
   */
  ava_uint level;
  /**
   * The visibility of this symbol. Always ava_v_private if level > 0.
   */
  ava_visibility visibility;

  union {
    /**
     * Information about local and global variable and function symbols.
     */
    struct {
      /**
       * Whether the variable is mutable.
       *
       * If false, variable substitutions on it cannot be used as lvalues.
       */
      ava_bool is_mutable;
      /**
       * The  original  name  of  the symbol,  including  any  needed  mangling
       * information.
       */
      ava_demangled_name name;
      /**
       * For  functions,  a  partial  function  sufficient  to  perform  static
       * binding. The function is not invokable; it may not have an initialised
       * FFI field and probably does not point to an actual native function.
       */
      ava_function fun;
    } var;

    /**
     * Information about macro symbols of all types.
     */
    struct {
      /**
       * If an operator macro, its precedence, between 0 and
       * AVA_MAX_OPERATOR_MACRO_PRECEDENCE, both inclusive.
       *
       * Always 0 for control and function macros.
       */
      unsigned precedence;
      /**
       * The function to invoke to substitute this macro.
       */
      ava_macro_subst_f macro_subst;
      /**
       * Arbitrary userdata for use by macro_subst.
       */
      const void* userdata;
    } macro;
  } v;
};

/**
 * Defines the high-level operations AST nodes must support.
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
};

struct ava_ast_node_vtable_s {
  /**
   * Converts the AST node to a string for diagnostic purposes.
   *
   * This is not reversible; the result need  not be (and usually is not) valid
   * Avalanche syntax.
   */
  ava_string (*to_string)(const ava_ast_node*);
  /**
   * Converts this AST node into an equivalent lvalue AST node.
   *
   * If this node  cannot be used as  an lvalue, the node must  record an error
   * and return a valid lvalue AST node.
   */
  ava_ast_node* (*to_lvalue)(const ava_ast_node*);
  /**
   * Performs post-processing on the given node.
   *
   * This  is  primarily used  for  second-pass  name resolution  and  function
   * binding.
   *
   * Nodes which have sub-nodes must delegate to their children.
   *
   * Any context needed by the node must have been saved at construction.
   *
   * Calling this function more than once has no observable effect.
   */
  void (*postprocess)(ava_ast_node*);
};

/**
 * Struct for use with ava_macsub_save_symbol_table() and
 * ava_macsub_get_saved_symbol_table().
 *
 * This is essentially a pair of an ava_symbol_table and ava_import_list, but
 * has additional bookkeeping so that fewer instances need be created.
 */
typedef struct ava_macsub_saved_symbol_table_s ava_macsub_saved_symbol_table;

/**
 * Describes how a sequence of statements determines what value to return as a
 * result of evaluation.
 */
typedef enum {
  /**
   * Indicates that the sequence always returns the empty string.
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
 * @param errors List into which errors will be acumulated.
 * @param symbol_prefix The implicit prefix for all names defined within the
 * context.
 * @return The new context.
 */
ava_macsub_context* ava_macsub_context_new(
  ava_symbol_table* root_symbol_table,
  ava_compile_error_list* errors,
  ava_string symbol_prefix);

/**
 * Returns the current, mutable symbol table of the given context.
 */
ava_symbol_table* ava_macsub_get_current_symbol_table(
  const ava_macsub_context* context);
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
 * Creates a new context representing a major scope nested within the given
 * parent.
 *
 * Major scopes are essentially function boundaries; names defined within are
 * not visible outside the major scope. The level of a major scope is one
 * greater than its parent.
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
 * and any names defined within are also visible in the parent.
 *
 * @param parent The parent context.
 * @param interfix New string to append to the implicit prefix of the resulting
 * context.
 * @return The child context.
 */
ava_macsub_context* ava_macsub_context_push_minor(
  const ava_macsub_context* parent, ava_string interfix);

/**
 * Captures the current symbol table and import list so that it can be
 * reapplied in the postprocessing pass, so as to be able to see names defined
 * later in the input.
 *
 * @param context The context whose symbol table / import list is to be saved.
 * @param location A source location where the symbol table may later be
 * recalled. Used for diagnostic messages resulting from rebuilding the table.
 * @return A value suitable to be passed to
 * ava_macsub_get_saved_symbol_table(). Instances are reused as much as
 * possible.
 */
ava_macsub_saved_symbol_table* ava_macsub_save_symbol_table(
  ava_macsub_context* context,
  const ava_compile_location* location);
/**
 * Rebuilds a symbol table from a saved symbol table and import list.
 *
 * Calling this more than once on the same object yields the same value. It is
 * only meaningful to call this postprocessing; behaviour otherwise will be
 * unpredictable.
 *
 * If any errors occur, they will be added to the error list, but a valid
 * symbol table will be returned nonetheless.
 *
 * @param saved Value returned from ava_macsub_save_symbol_table() whose state
 * is to be reconstructed.
 * @return The new symbol table.
 */
const ava_symbol_table* ava_macsub_get_saved_symbol_table(
  ava_macsub_saved_symbol_table* saved);

/**
 * Runs full macro substitution on the given list of statements, producing a
 * single root AST node.
 *
 * Whether processing was successful can be determined by testing whether any
 * errors were added to the context.
 *
 * @param context The context in which macro substitution is to run.
 * @param start The location where this statement sequence starts.
 * @param statements The list of statements to process.
 * @param return_policy The return policy for this sequence of statements.
 * @return A valid AST node representing the result of processing.
 */
ava_ast_node* ava_macsub_run(ava_macsub_context* context,
                             const ava_compile_location* start,
                             const ava_parse_statement_list* statements,
                             ava_intr_seq_return_policy return_policy);

/**
 * Equivalent to calling ava_macsub_run() with a statement list containing the
 * given statement and all statements that follow it.
 *
 * @param context The context in which macro substitution is to run.
 * @param start The location where this statement sequence starts.
 * @param statement The first of the list of statements to process.
 * @param return_policy The return policy for this sequence of statements.
 * @return A valid AST node representing the result of processing.
 */
ava_ast_node* ava_macsub_run_from(ava_macsub_context* context,
                                  const ava_compile_location* start,
                                  const ava_parse_statement* statement,
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
 * @return A valid AST node representing the result of processing.
 */
ava_ast_node* ava_macsub_run_single(ava_macsub_context* context,
                                    const ava_compile_location* start,
                                    const ava_parse_statement* statement);

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
 * Records an error with the given message and location, providing an AST node
 * as a placeholder for the error.
 *
 * @param context The current macro substitution context.
 * @param message The error message (ava_compile_error.message)
 * @param location The error location (ava_compile_error.location)
 * @return An AST node that stands in as a placeholder for the node that failed
 * substitution. The node pretends to implement everything that can be called
 * in the macro substitution stage; for example, it can be used as an lvalue.
 */
ava_ast_node* ava_macsub_error(ava_macsub_context* context,
                               ava_string message,
                               const ava_compile_location* location);

/**
 * Convenience for calling ava_macsub_error() and wrapping it in an
 * ava_macro_subst_result.
 */
ava_macro_subst_result ava_macsub_error_result(
  ava_macsub_context* context, ava_string message,
  const ava_compile_location* location);

#endif /* AVA_RUNTIME_MACSUB_H_ */
