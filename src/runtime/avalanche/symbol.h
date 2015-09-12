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
#error "Don't include avalanche/symbol.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_SYMBOL_H_
#define AVA_RUNTIME_SYMBOL_H_

#include "string.h"
#include "macsub.h"

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
 *
 * These are sorted by increasing visibility.
 */
typedef enum {
  /**
   * Indicates that the symbol is only visible in the module that defines it.
   */
  ava_v_private,
  /**
   * Indicates that the symbol is only  visible within the package that defines
   * it.
   */
  ava_v_internal,
  /**
   * Indicates that the symbol is visible everywhere.
   */
  ava_v_public
} ava_visibility;

/**
 * A symbol in a symbol table.
 */
typedef struct ava_symbol_s ava_symbol;

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

  /**
   * If this symbol defines a global element in P-Code, the index of its
   * element.
   *
   * Initialised by invoking cg_define() on the associated AST node.
   */
  ava_uint pcode_index;

  /**
   * If this symbol defines a global element in P-Code, the AST node which is
   * responsible for it.
   */
  ava_ast_node* definer;

  /**
   * The original fully-qualified name of this symbol, used in diagnostics.
   */
  ava_string full_name;

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

#endif /* AVA_RUNTIME_SYMBOL_H_ */