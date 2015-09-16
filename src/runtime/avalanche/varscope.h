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

#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/varscope.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_VARSCOPE_H_
#define AVA_RUNTIME_VARSCOPE_H_

struct ava_symbol_s;

/**
 * Manages local variable scopes.
 *
 * A varscope tracks the following data:
 * - A list of variable symbols it owns
 * - A list of variable symbols it captures from parent scope(s)
 * - A list of scopes that reference it
 *
 * It can be used to determine:
 * - The P-Code V-register index of each variable in a function
 * - The number of V-registers needed by a function
 * - The implicit arguments needed to capture closed-over variables, both for
 *   the closure itself and any references to the closure
 *
 * V-register indices are assigned as follows. All captured variables from
 * other scopes come first, in encounter order. This necessarily means that
 * they comprise the first n arguments to the function owning the scope. Then
 * come the variables local to the function, in encounter order. The varscope
 * does not distinguish between arguments and non-arguments; callers must
 * simply ensure their arguments are added before any non-arguments and in the
 * correct order.
 *
 * Note that any action that adds new captures will necessarily shunt the
 * V-register index of all owned variables by 1.
 *
 * A scope captures a variable if
 * - It does not own that variable, and
 * - Either
 *   - It directly references the variable, or
 *   - It references another scope which captures the variable
 */
typedef struct ava_varscope_s ava_varscope;

/**
 * Allocates a new varscope with no owned variables, no captures, and no
 * referrers.
 */
ava_varscope* ava_varscope_new(void);
/**
 * Adds the given symbol as a local variable (possibly a parameter) in the
 * given scope.
 *
 * @param scope The varscope to mutate.
 * @param var A symbol of type ava_st_local_variable to be added to the given
 * scope. The symbol must not yet be in any scope.
 */
void ava_varscope_put_local(
  ava_varscope* scope, const struct ava_symbol_s* var);
/**
 * Notes that the given varscope references the given symbol.
 *
 * @param from The varscope to record the reference as necessary.
 * @param referrent The symbol being referenced. Must be an
 * ava_st_local_variable.
 */
void ava_varscope_ref_var(
  ava_varscope* from, const struct ava_symbol_s* referrent);
/**
 * Notes that the given scope references the given other scope.
 *
 * @param from The varscope referencing another.
 * @param referrent The varscope being referenced. May be NULL, which indicates
 * an external scope with no visible properties.
 */
void ava_varscope_ref_scope(
  ava_varscope* from, ava_varscope* referrent);

/**
 * Returns the P-Code V-register index of the given variable symbol within the
 * given scope.
 *
 * The var must have been previously referenced with ava_varscope_ref_var() or
 * transitively referenced via ava_varscope_ref_scope().
 */
ava_uint ava_varscope_get_index(
  const ava_varscope* scope, const struct ava_symbol_s* var);
/**
 * Returns the number of captured variables in the given scope.
 */
size_t ava_varscope_num_captures(const ava_varscope*);
/**
 * Returns the total number of variables (captured and local) in the given
 * scope.
 */
size_t ava_varscope_num_vars(const ava_varscope*);
/**
 * Copies variable references into the given array.
 *
 * @param dst The array of symbol pointers to populate. Exactly count entries
 * will be written. The array is indexed by P-Code V-register index.
 * @param src The scope from which to obtain symbols.
 * @param count The number of symbols to copy. Must be less than or equal to
 * ava_varscope_num_vars().
 */
void ava_varscope_get_vars(
  const struct ava_symbol_s** dst, const ava_varscope* src, size_t count);

#endif /* AVA_RUNTIME_VARSCOPE_H_ */
