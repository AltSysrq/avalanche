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
#ifndef AVA_RUNTIME__INTRINSICS_EH_H_
#define AVA_RUNTIME__INTRINSICS_EH_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The "try" control macro.
 *
 * Syntax:
 *   "try" {body} {catch-clause}* [{finally-clause}]
 *   {catch-clause} ::= {catch-spec} {body}
 *   {catch-spec} ::= "catch" lvalue
 *                |   "workaround" lvalue
 *                |   "on-any-bad-format" lvalue
 *                |   "workaround-undefined" lvalue
 *   {body} ::= block | substitution
 *   {finally-clause} ::= "finally" block
 *
 * All {body}s must be of the same type. At least one catch-clause or the
 * finally clause must be present.
 *
 * Semantics:
 *
 * The main body is executed. If it runs to completion, control continues
 * naturally. If body is a substitution, the try as a whole produces the value
 * of that body. If the main body throws, each catch clause is tested in
 * sequence. If one matches, its body is executed (producing the overall result
 * for the try if a substitution) and the exception is dropped. Otherwise, the
 * exception propagets out of the try.
 *
 * Each clause type matches as follows:
 *
 * - "catch": ava_user_exception; lvalue is set to the exception value.
 *
 * - "workaround": ava_error_exception; lvalue is set to the exception value.
 *
 * - "on-bad-any-format": ava_format_exception; lvalue is set to the exception
 *   message.
 *
 * - "workaround-undefined": ava_undefined_behaviour_exception; lvalue is set
 *   to the exception message.
 *
 * On any condition in which control leaves the try, including naturally, via
 * an exception, or a direct transfer of flow (eg, `ret`), the finally block is
 * executed, if any. The finally block may not explictly transfer control out
 * of itself. If the finally block throws, the new exception replaces the old
 * one.
 *
 * TODO: Once destructuring assignment is a thing, it should be used for
 * matching exception clauses instead.
 *
 * Rationales:
 *
 * The "finally" clause does not permit transferring control out of it because
 * the results are incredibly confusing and almost never correct. For example,
 * in Java it is possible for a finally block to veto a return
 *
 *   for (;;) try { return; } finally { continue; }
 *
 * or to cause an exception to be silently dropped
 *
 *   try { throw new Exception(); } finally { return; }
 *
 * The behaviour of an exception being thrown out of finally is unfortunate,
 * but is one of three equally bad options, the others being to drop the
 * finally's exception and rethrow the original, and to abort the program in
 * this situation. Propagating the exception out of the finally was chosen on
 * the grounds of being the most common in existing languages and the easiest
 * to implement correctly.
 *
 * "catch" can only catch normal user exceptions because other exception types
 * should not be caught in this manner ordinarily.
 *
 * "workaround" and "workaround-undefined" are provided to enable last-resort
 * workarounds of problems in third-party libraries, akin to catching
 * NullPointerException in Java. Their names reflect this intent.
 *
 * "on-any-bad-format" is named such to emphasise how it is impossible to
 * distinguish the source of the exception programatically. It is provided
 * mostly for the same reasons as the workaround clauses above, but also has
 * some use in small-scale expressions.
 */
ava_macro_subst_result ava_intr_try_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * The #throw# intrinsic macro.
 *
 * Syntax:
 *   "#throw#" type value
 *
 * type is a bareword integer which is a throwable value of
 * ava_pcode_exception_type. value is evaluated as an expression.
 *
 * Semantics: An exception of the given type and value is thrown.
 *
 * This is a very low-level macro. The core library provides a friendlier
 * front-end to throwing exceptions.
 */
ava_macro_subst_result ava_intr_throw_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * The defer intrinsic macro.
 *
 * Syntax:
 *   "defer" {statement}
 *   body...
 *
 *   {statement} ::= block
 *               | syntax-unit+
 *
 * If statement is a single block, the contents of the block are evaluated as
 * described below. Otherwise, the statement resulting from the remaning syntax
 * units in the defer statement are used. All statements following the defer
 * are used as its body.
 *
 * Semantics: The defer macro has exactly the same semantics as
 *
 *   try { body } finally { statement }
 *
 * Rationale: This defer construct is intended to serve much the same purpose
 * as the construct of the same name in Go, providing a convenient way to
 * guarantee destruction of a resource which is located in source immediately
 * after its acquisition and which doesn't require adding another level of
 * explicit nesting / indentation.
 *
 * Note that it has very different semantics than the defer statement in Go. Go
 * requires the statement to be a function call and evaluates its arguments
 * when the defer statement is encountered; Avalanche allows any statement (or
 * even group of statements), but evaluates nothing until the defer statement
 * is actually run. Furthermore, Go defer statements add elements to a stack of
 * pending actions which occur when the function exits, while Avalanche's defer
 * takes effect when the lexical scope is exited.
 *
 * In the majority of cases, the two end up being mostly equivalent. When they
 * do differ, lexical scoping is felt to usually be more useful. The use of
 * lexical scoping also greatly diminishes the need to evaluate parameters
 * early; furthermore, evaluating nothing until the deferred statement is
 * executed permits inspecting the results of the body.
 *
 * It is possible to construct a Go-like defer using Avalanche defer, but the
 * reverse is not possible.
 */
ava_macro_subst_result ava_intr_defer_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

#endif /* AVA_RUNTIME__INTRINSICS_EH_H_ */
