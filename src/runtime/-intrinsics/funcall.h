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
#ifndef AVA_RUNTIME__INTRINSICS_FUNCALL_H_
#define AVA_RUNTIME__INTRINSICS_FUNCALL_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * @file
 *
 * Anonymous intrinsics for performing function calls.
 *
 * While this is, strictly speaking, part of the set of fundamentals, it is
 * split into a separate interface for the sake of manageability.
 */

/**
 * Constructs a function call node from the given statement, which must have at
 * least two units.
 *
 * @param context The macro substitution context.
 * @param statement The statement to interpret as a function call.
 * @return A node representing that function call.
 */
ava_ast_node* ava_intr_funcall_of(
  ava_macsub_context* context,
  const ava_parse_statement* statement);

/**
 * Constructs a function call node from the given AST nodes providing the
 * parameters. The zeroth parameter is the function itself.
 *
 * @param context The macro substitution context.
 * @param parms The parameters to the function, including the function itself
 * in position 0. This array is copied and need not outlive this function call.
 * @param num_parms The length of the parms array, including the function
 * itself. Must be at least 2.
 */
ava_ast_node* ava_intr_funcall_make(
  ava_macsub_context* context,
  ava_ast_node*const* parms,
  size_t num_parms);

#endif /* AVA_RUNTIME__INTRINSICS_FUNCALL_H_ */
