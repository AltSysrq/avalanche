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
#ifndef AVA_RUNTIME__INTRINSICS_LOOP_H_
#define AVA_RUNTIME__INTRINSICS_LOOP_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The loop control macros. The keyword for the first clause is implicit in the
 * identity of the macro.
 *
 * Syntax:
 *   {clause}+ [{else-clause}]
 *   {clause} ::= "each" lvalue+ "in" rvalue
 *            |   "for" block rvalue block
 *            |   "while" rvalue
 *            |   "until" rvalue
 *            |   ( "do" substitution | ["do"] block )
 *            |   "collect" rvalue
 *            |   "collecting"
 *   {else-clause} ::= "else" ( block | substitution )
 *
 * Semantics:
 *
 * The loop executes is four phases: initialisation, iteration, update, and
 * completion. The loop begins with initialisation.
 *
 * Initialisation:
 *
 * 1. A loop accumulator is allocated and initialised to the empty string.
 *
 * 2. For each clause,
 *    - The rvalue of an each clause is evaluated and converted to a list and
 *      stored in a dedicated location.
 *    - The first block of a for clause is executed.
 *
 * 3. The loop switches to the iteration stage.
 *
 * Iteration:
 *
 * 1. A loop iteration value is initialised to the empty string.
 *
 * 2. For each clause:
 *
 *   - For an each clause with _n_ lvalues, the first _n_ elements are removed
 *     from the head of the list and assigned to each lvalue in order. If no
 *     elements at all were available, the loop switches to the completion
 *     stage. Otherwise, if any lvalue was not assigned anything, an
 *     error_exception of type bad-list-multiplicity it thrown.
 *
 *   - For a for clause, the middle rvalue is evaluated and interpreted as an
 *     integer, default 0. If it is zero, the loop switches to the completion
 *     stage.
 *
 *   - For a while or until clause, the rvalue is evaluated and interpreted as
 *     an integer, default 0. If it is zero or non-zero for a while or until
 *     clause, respectively, the loop switches to the completion stage.
 *
 *   - For a do clause, the block or substitution is evaluated. In the case of
 *     a substitution, the loop iteration value is set to the result of the
 *     evaluation.
 *
 *   - For a collect clause, the rvalue is evaluated. The loop accumulator is
 *     interpreted as a list, and set to that list with the rvalue appended.
 *
 *   - For a collecting clause, the loop accumulator is interpreted as a list,
 *     and set to that list with the loop iteration value appended.
 *
 * 3. The loop proceeds to the update stage.
 *
 * Update:
 *
 * 1. Starting from the last clause which was reached (began execution) in the
 *    last operation of the iteration stage and moving backwards to the first
 *    clause, for each clause:
 *
 *     - The rightmost block of a for clause is evaluated.
 *
 * 2. The loop returns to the iteration stage.
 *
 * Completion:
 *
 * 1. If there is an else clause, its block or substitution is evaluated. If it
 *    is a substitution, the result of evaluation replaces the loop
 *    accumulator.
 *
 * 2. The loop terminates.
 *
 * The evaluation result of the loop is the value of the loop accumulator upon
 * termination of the loop.
 *
 * The macro userdata for this function is a C string indicating the implicit
 * first token.
 */
ava_macro_subst_result ava_intr_loop_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

#endif /* AVA_RUNTIME__INTRINSICS_LOOP_H_ */
