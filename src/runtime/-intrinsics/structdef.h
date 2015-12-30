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
#ifndef AVA_RUNTIME__INTRINSICS_STRUCTDEF_H_
#define AVA_RUNTIME__INTRINSICS_STRUCTDEF_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

struct ava_struct_s;

/**
 * The "struct" and "union" control macros.
 *
 * The userdata is a pointer to the visibility of the defined symbols. Whether
 * the macro defines a struct or a macro is dependent on the substitution
 * function used.
 *
 * Syntax:
 *   "struct" name ["extends" parent] body
 *   "union" name body
 *
 * name is a bareword indicating the name of the struct. If parent is present,
 * it is a bareword which must resolve to a struct symbol indicating the parent
 * struct.
 *
 * body is a block. Each statement within the block defines one field in the
 * struct.
 *
 *   {field} ::= {field-spec} field-name
 *   {field-spec} ::= {int-field} | {atomic-int-field} | {real-field} |
 *                    {value-field} | {ptr-field} | {atomic-ptr-field} |
 *                    {hybrid-field} | {compose-field} | {array-field} |
 *                    {tail-field}
 *   {int-field} ::= {int-size} {int-adj}*
 *   {int-size} ::= "integer" | "byte" | "short" | "int" | "long"|
 *                  "c-short" | "c-int" | "c-long" | "c-llong" |
 *                  "c-size" | "c-intptr" | "word"
 *   {int-adj} ::= {signedness} | {alignment} | {byte-order}
 *   {signedness} ::= "signed" | "unsigned"
 *   {alignment} ::= "align" alignment
 *   {byte-order} ::= "preferred" | "native" | "big" | "little"
 *   {atomic-int-field} ::= "atomic" {signedness}?
 *   {real-field} ::= {real-size} {real-adj}*
 *   {real-size} ::= "real" | "single" | "double" | "extended"
 *   {real-adj} ::= {alignment} | {byte-order}
 *   {value-field} ::= "value"
 *   {ptr-field} ::= prototype
 *   {atomic-ptr-field} ::= "atomic" prototype
 *   {hybrid-field} ::= "hybrid" prototype
 *   {compose-field} ::= "struct" member
 *   {array-field} ::= "struct" member "[" length "]"
 *   {tail-field} ::= "struct" member "[" "]"
 *
 * alignment is a bareword holding an integer which is a power of two less than
 * 2**14, or the keyword "native" or "natural". prototype is a bareword ending
 * in "*" or "&". The tag of the prototype must resolve to a struct symbol.
 * member is a bareword which must resolve to a struct symbol which must be
 * composable.
 *
 * Semantics: The given fields are assembled into a struct. Symbols are defined
 * as per ava_intr_struct_define_symbols().
 */
ava_macro_subst_result ava_intr_struct_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);
/**
 * @see ava_intr_struct_subst()
 */
ava_macro_subst_result ava_intr_union_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements);

#endif /* AVA_RUNTIME__INTRINSICS_STRUCTDEF_H_ */
