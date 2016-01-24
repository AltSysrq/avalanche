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
#ifndef AVA_RUNTIME__INTRINSICS_IF_H_
#define AVA_RUNTIME__INTRINSICS_IF_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The `if` control macro.
 *
 * Syntax:
 *   if (condition result)+ ["else" result]
 *
 * Each condition is a substitution. Each result is either a substitution or a
 * block; all results in the same if statement must be of the same type. The
 * "else" is mandatory.
 *
 * Each condition is evaluated in sequence. Upon the first that produces a true
 * value, the corresponding result is evaluated and execution of the if
 * statement terminates. If no condition evaluates to true, the else result is
 * evaluated, if any.
 *
 * If the results are blocks, the statement does not produce a value. If the
 * results are substitutions, the if expression evaluates to the value of the
 * result that was evaluated, or the empty string if no condition matched and
 * there was no else-result.
 *
 * Rationales:
 *
 * - There is no "else if"/"elsif"/"elif" to deleniate the secondary
 *   conditions, because the conditions can actually line up without them. For
 *   example,
 *     if (foo) {
 *       do-something ()
 *     }  (bar) {
 *       do-something-else ()
 *     }
 *   And regardless of alignment, the else-if syntax would be nothing but
 *   syntax salt.
 *
 * - The substitution and block forms are used as per Avalanche's general
 *   approach to being both statement- and expression-oriented without
 *   conflating the two.
 *
 * - Parentheses are required around conditions so that no sentinal is needed
 *   to find the end of the condition, which would impose awkward constraints
 *   on the syntax and generally make it harder to use overall.
 */
ava_macro_subst_result ava_intr_if_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

#endif /* AVA_RUNTIME__INTRINSICS_IF_H_ */
