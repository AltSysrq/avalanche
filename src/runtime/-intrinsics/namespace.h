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
#ifndef AVA_RUNTIME__INTRINSICS_NAMESPACE_H_
#define AVA_RUNTIME__INTRINSICS_NAMESPACE_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * The `namespace` control macro.
 *
 * Syntax:
 *   namespace name [body]
 *
 * name is a bareword specifying the sub-namespace.
 *
 * body is evaluated in a minor scope whose namespace prefix is that of the
 * containing scope followed by name. A "." is implicitly appended to name.
 *
 * If body is omitted, all statements following the macro are used as the body
 * instead. Otherwise, body must be a block, which will be evaluated as a void
 * sequence.
 *
 * Within body, a strong automatic import from the new namespace prefix to the
 * empty string is in effect.
 */
ava_macro_subst_result ava_intr_namespace_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * The `import` control macro.
 *
 * Syntax:
 *   import source [dest]
 *
 * source and dest are both barewords specifying namespace prefixes.
 *
 * A weak, non-automatic import is effected on the symbol table of the current
 * context, reprefixing source to dest. If dest is the bareword "*", it is
 * treated as if it were an empty A-String literal. If source or dest is a
 * bareword which does not end in a '.' or ':', a '.' is implicitly appended.
 * If dest is omitted, it defaults to the characters in source immediately
 * after the pennultimate character from the set {'.',':'} in source. If source
 * fewer than two such characters, dest is mandatory.
 */
ava_macro_subst_result ava_intr_import_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

/**
 * The `alias`, `Alias`, and `ALIAS` control macros.
 *
 * Syntax:
 *   alias dest = source
 *
 * dest and source are both barewords. "=" is a bareword with exactly that content.
 *
 * The symbol identified by source is looked up, and is cloned outright to a
 * new symbol whose name is given by dest (subject to namespace prefixing). The
 * visibility of the alias must not be greater than the visibility of the
 * original symbol.
 *
 * The visibility of the resulting symbol is intrinsic to the macro, derived
 * from *(ava_visibility*)userdata on the self symbol.
 */
ava_macro_subst_result ava_intr_alias_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

#endif /* AVA_RUNTIME__INTRINSICS_NAMESPACE_H_ */
