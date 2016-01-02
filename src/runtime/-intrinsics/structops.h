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
#ifndef AVA_RUNTIME__INTRINSICS_STRUCTOPS_H_
#define AVA_RUNTIME__INTRINSICS_STRUCTOPS_H_

#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"

/**
 * @file
 *
 * Function-like macros for working with structs.
 *
 * All these macros are found within the intrinsics.esoterica.unsafe.strangelet
 * namespace, which is not otherwise noted in the documentation.
 *
 * In all cases, an "sxt" arg is a constant expression naming a struct symbol,
 * and "field" is a constant expression naming a field within that struct.
 * Arguments whose names begin with 'S' are strangelets pointing to an instance
 * of sxt.
 *
 * In general, the argument order is:
 *
 *   sxt strangelet-to-sxt fieldname options other-operands
 *
 * While this order is admittedly a bit awkward, it makes the macros more
 * consistent. Ensuring that sxt and the main operatee are adjacent opens up
 * some possibilities for macros.
 */

/**
 * The general memory allocation macro.
 *
 *   new sxt [-s] [-z] [-atomic] [-precise] [-t n] [-n n]
 *
 * Evaluates to a strangelet which references an instance of sxt, or an array
 * of sxt instances if -n was given.
 *
 * If -s is given, the memory is scoped to the current function call,
 * effectively allocated on the stack, and may be destroyed via the setsp
 * intrinsics.
 *
 * If -z is given, the memory is zero-initialised. Otherwise, its initial
 * contents are undefined.
 *
 * If -atomic is given, pointers within the allocated memory are not guaranteed
 * to retain the objects to which they point.
 *
 * If -precise is given, pointers to the allocated memory not equal to the
 * value produced by the macro are not guaranteed to retain the allocation.
 *
 * -t specifies the length of the tail field on sxt. If given, sxt must be a
 * structure with a tail field. The value may be any expression.
 *
 * -n specifies the length of an array of instances to allocate. It may not be
 * used on non-composable structures, and is mutually exclusive with -t. The
 * value may be any expression.
 *
 * This macro is pure.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_new_subst);

/**
 * Macro to copy one struct instance to another.
 *
 *   cpy sxt [-move] [-t n] Sdst Ssrc
 *
 * The contents of Ssrc are copied onto Sdst. Behaviour is undefined if Sdst
 * and Ssrc refer to the same memory.
 *
 * If -t is given, sxt must be a struct with a tail field, and this gives an
 * arbitrary expression indicating the length of the tail to copy. If not
 * given, the tail is not copied.
 *
 * If -move is given, Ssrc is destroyed, and further use of that strangelet
 * yields undefined behaviour.
 *
 * This macro produces no value.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_cpy_subst);

/**
 * Macro to copy an array of struct instances from one slice to another.
 *
 *   arraycpy sxt [-move] Sdst dstoff Ssrc srcoff count
 *
 * The conents of count instances of sxt are copied from Ssrc, starting at the
 * srcoffth instance, into Sdst, starting at the dstoffth instance.
 *
 * If -move is given, the copied instances in Ssrc are destroyed, and further
 * access to them results in undefined behaviour.
 *
 * This macro produces no value.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_arraycpy_subst);

/**
 * Macro to read a field in a struct.
 *
 *   get sxt Ssrc field [-order order] [-unatomic] [-volatile] [-int] [-ptr]
 *
 * The named field in Ssrc is read and produced as the value. For compose-like
 * fields, this results in a strangelet to the composed member.
 *
 * If -volatile is given, the compiler is not allowed to split, combine, or
 * eliminate this operation, nor reorder it with other volatile memory
 * accesses. This is only valid for field types where memory is actually
 * accessed.
 *
 * -order and -unatomic are valid only for atomic fields and are mutually
 * exclusive. -unatomic suppresses the atomicity of the field. -order
 * specifies a constexpr evaluating to the memory order to use (any value
 * accepted in P-Code, "undordered", "monotonic", "acquire", "release",
 * "acqrel", or "seqcst").
 *
 * -int and -ptr are only legal on hybrid fields, and exactly one must be given
 * for a hybrid field. It specifies whether the hybrid is to be interpreted as
 * an integer or a pointer. Behaviour is undefined if this type does not
 * correspond to the last type stored in the hybrid, or if it is uninitialised.
 *
 * This macro is pure.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_get_subst);

/**
 * Macro to write a field in a struct.
 *
 *   set sxt Sdst field [-order order] [-unatomic] [-volatile] [-int] [-ptr] value
 *
 * The named field in Sdst is set to value, which may be any expression. This
 * only makes sense for fields which hold values (e.g., compose fields cannot
 * be used).
 *
 * All options behave as described in ava_intr_S_get().
 *
 * For hybrid fields, behaviour is undefined if -int is used and value is an
 * even integer, or if -ptr is used and value is an unaligned strangelet.
 *
 * The macro produces value as its result but may be discraded.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_set_subst);

/**
 * Macro to determine the type stored in a hybrid field.
 *
 *   is-int sxt Ssrc field [-volatile]
 *
 * If the named hybrid field in Ssrc holds an integer, returns 1. Otherwise,
 * returns 0. Behaviour is undefined if the field is uninitialised.
 *
 * -volatile behaves as with ava_intr_S_get().
 *
 * This macro is pure.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_is_int_subst);

/**
 * Macro to perform atomic compare-and-swap of integers and pointers.
 *
 *   cas sxt Sdst field [-volatile] [-order order] [-forder order]
 *   \ [-weak] [-old lvalue] old-value new-value
 *
 * This is only valid for atomic fields.
 *
 * The given field in Sdst is atomically compared-and-swapped from old-value to
 * new-value. That is, the field may be set to new-value if it was equal to
 * old-value when this call was made. 1 is produced if and only if the field
 * was written, 0 otherwise.
 *
 * -volatile and -order behave as per ava_intr_S_get(). -forder specifies the
 * memory order for failed CaS operations. If omitted, it is the same as
 * -order; otherwise, -order is only effective for successful CaS operations.
 *
 * If -weak is given, the operation may fail spuriously. Otherwise, it is
 * guaranteed to succeed if old-value actually matched the content of the
 * field.
 *
 * If -old is given, it specifies an lvalue into which is written the actual
 * old value of the field when the operation took place, regardless of whether
 * it succeeded.
 *
 * This macro is impure.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_cas_subst);

/**
 * Macro to perform atomic read-modify-write operations on integers and
 * pointers.
 *
 *   rmw sxt Sdst field [-volatile] [-order order] operation value
 *
 * This is only valid for atomic fields.
 *
 * The given field in Sdst is read, combined with value according to operation,
 * and then written back to the field. The old value of the field is produced.
 *
 * -volatile and -order behave as per ava_intr_S_get().
 *
 * operation is a constexpr naming the operation to perform. For integers, this
 * may be any value accepted by P-Code (xchg, add, sub, and, nand, or, xor,
 * smax, smin, umax, umin). For pointers, it must be "xchg".
 *
 * This macro is impure.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_rmw_subst);

/**
 * Macro to index an array of structures.
 *
 *   ix sxt Sbase offset
 *
 * offset is an arbitrary expression. Produces a strangelet referencing the
 * offsetth instance of sxt in the array referenced by Sbase.
 *
 * sxt must be composable.
 *
 * This macro is pure.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_ix_subst);

/**
 * Macro to errect a hardware memory barrier.
 *
 *   membar order
 *
 * order is a P-Code memory ordering. This macro expands to exactly one P-Code
 * membar instruction with that order; the semantics are therefore defined
 * and documented with that instruction.
 *
 * This is not really a strangelet operation and is not itself unsafe, but is
 * grouped and namespaced with them since it is useless outside of the world of
 * strangelets.
 *
 * This macro produces no value.
 */
AVA_DEF_MACRO_SUBST(ava_intr_S_membar_subst);

#endif /* AVA_RUNTIME__INTRINSICS_STRUCTOPS_H_ */
