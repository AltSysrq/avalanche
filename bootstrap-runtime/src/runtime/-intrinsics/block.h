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
#ifndef AVA_RUNTIME__INTRINSICS_BLOCK_H_
#define AVA_RUNTIME__INTRINSICS_BLOCK_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The block-void, block-last, and block-only control macros.
 *
 * Syntax:
 *   "block-void" block
 *   "block-last" block
 *   "block-only" block
 *
 * block must be a Block.
 *
 * The macro expands to a sequence containing the contents of block. The return
 * policy is found by dereferencing the macro userdata.
 *
 * The main purpose of this macro family is for user macros which need to have
 * more than one statement. One might expect lone braces to be usable for this
 * purpose, eg with the syntax
 *
 * {
 *   ; do stuff
 * }
 *
 * However, this would fall apart should someone attempt to use such a macro in
 * a location where it could be interpreted as an expression --- the macro's
 * attempted multi-statement expansion would be wrapped in a lambda.
 *
 * The explicit head also allows specifying the return policy.
 *
 * Note that while block-last allows more than one statement, its contents are
 * never considered a void context since that would mean that control macros
 * behave inconsistently within.
 */
ava_macro_subst_result ava_intr_block_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

#endif /* AVA_RUNTIME__INTRINSICS_BLOCK_H_ */
