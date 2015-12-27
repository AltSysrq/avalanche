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

/**
 * Defines the symbols associated with the given struct.
 *
 * logical_name gives the actual name to use for symbols, which is not
 * necessarily the same as the linkage name of the struct. The current prefix
 * of the macro substitution context is applied to the logical name to use as a
 * basis of all symbols defined. This combined prefix is notated NAME below.
 * All the S.-prefixed macros operate on strangelets and are unsafe.
 *
 * NAME - A struct symbol referencing the given struct.
 *
 * NAME.S.new.stack [-zero] tail - Pure function-like macro which produces a
 * strangelet referencing a stack-allocated instance of the struct. If -zero is
 * specified, the memory is zero-initialised; otherwise, its contents are
 * undefined. If the struct has a tail field, the tail parameter is interpreted
 * as an integer indicating the length of the tail. Otherwise, the tail field
 * must be a constexpr evaluating to the empty string.
 *
 * NAME.S.new.stack-array [-zero] length - Defined for structs with no tail
 * field. Pure function-like macro which produces a strangelet referencing a
 * stack-allocated array of struct instances. It takes one argument indicating
 * the length of the array.
 *
 * NAME.S.new.heap [-zero] [-atomic] [-precise] tail - As with
 * NAME.S.new.stack, but allocates on the heap. If -zero is specified, the
 * memory is zero-initialised, otherwise its contents are undefined. If -atomic
 * is specified, the memory will not retain pointers. If -precise is specified,
 * only a strangelet referencing the base of the allocation will retain the
 * allocation.
 *
 * NAME.S.new.heap-array [-zero] [-atomic] [-precise] length - As with
 * NAME.S.new.stack-array, but allocates on the heap.
 *
 * NAME.S.cpy [-move] dst src - Non-value-producing function-like macro which
 * copies the contents of src to dst. If -move is specified, src is destroyed,
 * and further access to it results in undefined behaviour. If the struct has a
 * tail field, an additional argument follows indicating the length of the tail
 * to copy.
 *
 * NAME.S.cpy-array [-move] dst dst-off src src-off count - Similar to
 * NAME.S.cpy, but copies count instances of the struct from src+src-off to
 * dst+dst-off. Only defined for structures without a tail field.
 *
 * NAME.S..FIELD [-order order] [-nonatomic] [-volatile] src - Pure
 * function-like macro which evaluates to the value of the given field within
 * src. Defined for all fields but hybrids. For compose, array, and tail, this
 * is a strangelet referencing the base address of the member. The -order and
 * -nonatomic arguments are only available for atomic fields. By default,
 * atomic fields do an atomic, unordered read. -nonatomic suppresses the
 * atomicity of the read. -order changes the ordering used for an atomic read,
 * and is one of the values accepted by P-Code ("unordered", "monotonic",
 * "acquire", "release", "acqrel", or "seqcst"). -volatile may be given for
 * anything but compose/array/tail fields, and makes the memory access
 * volatile.
 *
 * NAME.S..FIELD.set [-order order] [-nonatomic] [-volatile] dst src - Impure
 * function-like macro which sets the given field in dst to the value in src.
 * Defined for all fields other than hybrid, compose, array, and tail. Optional
 * arguments behave as per NAME.S..FIELD. The result is src.
 *
 * NAME.S..FIELD.is-int [-volatile] src - Pure function-like macro defined for
 * hybrid fields. Evaluates to 1 if the field contains an integer, 0 if it
 * contains a pointer. -volatile is as per NAME.S..FIELD. All notes for the
 * S-hy-intp instruction apply here.
 *
 * NAME.S..FIELD.int [-volatile] src - Pure function-like macro defined for
 * hybrid fields. Evaluates to the integer within the field. All notes for the
 * S-hi-ld instruction apply here.
 *
 * NAME.S..FIELD.int.set [-volatile] dst src - Function-like macro defined for
 * hybrid fields. src is interpreted as an integer and stored in the given
 * hybrid field in dst. Evaluates to src. All notes for the S-hi-st instruction
 * apply here.
 *
 * NAME.S..FIELD.ptr [-volatile] src - Pure function-like macro defined for
 * hybrid fields. Evaluates to the pointer (as a strangelet) within the field.
 * All notes for the S-p-ld instruction applied to hybrids apply here.
 *
 * NAME.S..FIELD.ptr.set [-volatile] dst src - Function-like macro defined for
 * hybrid fields. src is a strangelet and stored in the given hybrid field in
 * dst. Evaluates to src. All notes for the S-p-st instruction applied to
 * hybrid fields apply here.
 *
 * NAME.S..FIELD.cas [-volatile] [-weak] [-old lvalue] [-order order] -in dst
 * -from from -to to - Function-like macro defined for atomic integers and
 * pointers. Performs an atomic compare-and-swap from $from to $to on the
 * chosen field within dst. from and to are both interpreted as values of the
 * appropriate type. -volatile and -order are as per NAME.S..FIELD. If -weak is
 * given, the operation is permitted to fail spuriously. If -old is given, the
 * value of the field before the operation is written back into the given
 * lvalue. Evaluates to 1 if the operation succeds or 0 if it fails.
 *
 * NAME.S..FIELD.xchg [-volatile] [-order order] dst src - Function-like macro
 * defined for atomic integers and pointers. Atomically sets the chosen field
 * in dst to src, evaluating to the prior value in that field. -volatile and
 * -order as per NAME.S..FIELD.
 *
 * NAME.S..FIELD.add  [-volatile] [-order order] dst src
 * NAME.S..FIELD.sub  [-volatile] [-order order] dst src
 * NAME.S..FIELD.and  [-volatile] [-order order] dst src
 * NAME.S..FIELD.nand [-volatile] [-order order] dst src
 * NAME.S..FIELD.or   [-volatile] [-order order] dst src
 * NAME.S..FIELD.xor  [-volatile] [-order order] dst src
 * NAME.S..FIELD.smax [-volatile] [-order order] dst src
 * NAME.S..FIELD.smin [-volatile] [-order order] dst src
 * NAME.S..FIELD.umax [-volatile] [-order order] dst src
 * NAME.S..FIELD.umin [-volatile] [-order order] dst src
 *   Function-like macros defined for atomic integers. Atomically performs the
 * given read-modify-write operation on the chosen field in dst. Evaluates to
 * the old value of the field.
 *
 * NAME.S.plus src offset - Pure function-like macro defined for tailless
 * structs. src is assumed to reference an array of struct instances; offset is
 * interpreted as an integer. Evaluates to a reference to the offsetth instance
 * within the array.
 *
 * @param context The current macro substitution context.
 * @param sxt The struct to define.
 * @param logical_name The name to use for symbols (rather than the linkage
 * name of the struct).
 * @param define Whether the emitted decl-sxt instruction is a define or a
 * declaration.
 * @return An AST node represting the structure itself. The caller may safely
 * ignore this.
 */
ava_ast_node* ava_intr_struct_define_symbols(
  ava_macsub_context* context, const struct ava_struct_s* sxt,
  ava_string logical_name, ava_bool define);

#endif /* AVA_RUNTIME__INTRINSICS_STRUCTDEF_H_ */
