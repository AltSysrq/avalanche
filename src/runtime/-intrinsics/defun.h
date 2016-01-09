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
#ifndef AVA_RUNTIME__INTRINSICS_DEFUN_H_
#define AVA_RUNTIME__INTRINSICS_DEFUN_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The `fun`, `Fun`, and `FUN` control macros.
 *
 * Syntax:
 *   fun name {argument}+ {body}
 *   {argument} ::= "(" ")"
 *              |   pos-name
 *              |   -named-name
 *              |   \*pos-name
 *              |   "[" pos-name [default] "]"
 *              |   "[" -named-name [default] "]"
 *   {body} ::= "{" {statements...} "}"
 *          | "=" expression...
 *
 * name is a bareword which names the resulting function. pos-name is any
 * bareword not beginning with "-"; named-name is any bareword beginning with
 * "-". default is any single-unit constexpr, or nothing at all to stand for
 * the empty string.
 *
 * The symbol table gains a function `name`. The arguments are derived from the
 * given list of arguments as described below; the function may have implicit
 * arguments before the first listed one to account for captures if this
 * function is nested within another. When called, the function's body is
 * evaluated. The braced form of body is a void sequence, and the function
 * returns the empty string if control reaches the end of the sequence; the
 * equals-expression form evaluates the given expression and returns its
 * result.
 *
 * The () argument form indicates an anonymous positional argument whose value
 * MUST be the empty string upon invocation.
 *
 * The pos-name argument form indicates a mandatory positional argument. The
 * argument is accessible within the function body as a variable whose name is
 * `pos-name`.
 *
 * The -named-name argument form indicates a mandatory named argument. The name
 * of the argument is exactly `-named-name`. The value of the argument is
 * accessible as `-named-name` with the leading hyphen removed.
 *
 * The \*pos-name argument form indicates a varargs argument. The argument is
 * accessible within the function body as a variable whose name is `pos-name`.
 *
 * The [pos-name [default]] and [-named-name [default]] argument forms are
 * identical to the pos-name and -named-name argument forms, respectively,
 * except that the arguments are optional, and default to the given default
 * when not passed. If default is omitted, the empty string is used as the
 * default.
 *
 * The visibility of the resulting symbol is intrinsic to the macro, derived
 * from *(ava_visibility*)userdata on the self symbol.
 */
AVA_DEF_MACRO_SUBST(ava_intr_fun_subst);

/**
 * Implementation for lambda expression syntax units.
 *
 * Essentially equivalent to
 *   block-last {
 *     fun ?gensym 1 [2] [3] [4] = block-only <body>
 *     $?gensym
 *   }
 */
ava_ast_node* ava_intr_lambda_expr(
  ava_macsub_context* context,
  ava_parse_unit* unit);

#endif /* AVA_RUNTIME__INTRINSICS_DEFUN_H_ */
