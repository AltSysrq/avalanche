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
#ifndef AVA_RUNTIME__INTRINSICS_REQUIRE_H_
#define AVA_RUNTIME__INTRINSICS_REQUIRE_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The `reqmod` control macro.
 *
 * Syntax:
 *   reqmod module+
 *
 * Each module is a bareword whose text matches the grammar:
 *   {module} :: {word} ("/" {word})*
 *   {word} ::= [a-zA-Z0-9_]+
 *   {delim} ::= [-./]
 *
 * The exported symbols from each named module are added to the current symbol
 * table. An error occurs if a module cannot be loaded or if the module has
 * already been loaded into a visible symbol table.
 *
 * If a module cannot be found, an attempt is made to compile it from source.
 * The filename is derived by appending ".ava" onto the module name. Any errors
 * that occur from this attempt are added to the context's error list.
 *
 * Errors in this macro set the panic state on the macro substitution context.
 */
ava_macro_subst_result ava_intr_reqmod_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * The `reqpkg` control macro.
 *
 * Syntax:
 *   reqpkg package+
 *
 * Each package is a bareword whose text matches the grammar:
 *   {package} ::= {word} ({delim} {word})*
 *   {word} ::= [a-zA-Z0-9_]+
 *   {delim} ::= [-.]
 *
 * The exported symbols from each named package are added to the current symbol
 * table. An error occurs if a package cannot be found or if the package has
 * already been loaded into a visible symbol table.
 *
 * Errors in this macro set the panic state on the macro substitution context.
 */
ava_macro_subst_result ava_intr_reqpkg_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

#endif /* AVA_RUNTIME__INTRINSICS_REQUIRE_H_ */
