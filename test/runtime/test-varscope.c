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
#include "test.c"

#include "runtime/avalanche/alloc.h"
#include "runtime/avalanche/symbol.h"
#include "runtime/avalanche/varscope.h"

defsuite(varscope);

static ava_symbol* symbol(void) {
  /* varscope only cares about identity, plus asserts on the type */
  ava_symbol* sym = AVA_NEW(ava_symbol);
  sym->type = ava_st_local_variable;
  return sym;
}

static ava_symbol** mksymbols(size_t count) {
  ava_symbol** s;
  size_t i;

  s = ava_alloc(sizeof(ava_symbol*) * count);
  for (i = 0; i < count; ++i)
    s[i] = symbol();

  return s;
}

static ava_varscope** mkscopes(size_t count) {
  ava_varscope** s;
  size_t i;

  s = ava_alloc(sizeof(ava_symbol*) * count);
  for (i = 0; i < count; ++i)
    s[i] = ava_varscope_new();

  return s;
}

#define SCOPES_SYMS(nscope,nsym)                \
  ava_symbol** symbols = mksymbols(nsym);       \
  ava_varscope** scopes = mkscopes(nscope)

#define PUT(scope,sym) ava_varscope_put_local(scopes[scope], symbols[sym])

#define VREF(from,to) ava_varscope_ref_var(scopes[from], symbols[to])
#define SREF(from,to) ava_varscope_ref_scope(scopes[from], scopes[to])

#define INDEX(expected, scope, sym)             \
  ck_assert_int_eq((expected),                  \
                   ava_varscope_get_index(      \
                     scopes[scope],             \
                     symbols[sym]))

deftest(simple_indexing) {
  SCOPES_SYMS(1, 2);
  PUT(0, 0);
  PUT(0, 1);
  INDEX(0, 0, 0);
  INDEX(1, 0, 1);

  ck_assert_int_eq(0, ava_varscope_num_captures(scopes[0]));
  ck_assert_int_eq(2, ava_varscope_num_vars(scopes[0]));
}

deftest(self_reference) {
  SCOPES_SYMS(1, 1);
  PUT(0, 0);
  SREF(0, 0);
  INDEX(0, 0, 0);

  ck_assert_int_eq(0, ava_varscope_num_captures(scopes[0]));
}

deftest(local_reference) {
  SCOPES_SYMS(1, 1);
  PUT(0, 0);
  VREF(0, 0);
  INDEX(0, 0, 0);

  ck_assert_int_eq(0, ava_varscope_num_captures(scopes[0]));
}

deftest(simple_capture) {
  SCOPES_SYMS(2, 1);
  PUT(0, 0);
  VREF(1, 0);
  INDEX(0, 0, 0);
  INDEX(0, 1, 0);

  ck_assert_int_eq(1, ava_varscope_num_captures(scopes[1]));
  ck_assert_int_eq(1, ava_varscope_num_vars(scopes[1]));
}

deftest(transitive_prefacto_capture) {
  SCOPES_SYMS(3, 1);
  PUT(0, 0);
  VREF(1, 0);
  SREF(2, 1);
  INDEX(0, 0, 0);
  INDEX(0, 1, 0);
  INDEX(0, 2, 0);

  ck_assert_int_eq(1, ava_varscope_num_captures(scopes[2]));
  ck_assert_int_eq(1, ava_varscope_num_vars(scopes[2]));
}

deftest(transitive_postfacto_capture) {
  SCOPES_SYMS(3, 1);
  PUT(0, 0);
  SREF(2, 1);
  VREF(1, 0);
  INDEX(0, 0, 0);
  INDEX(0, 1, 0);
  INDEX(0, 2, 0);

  ck_assert_int_eq(1, ava_varscope_num_captures(scopes[2]));
  ck_assert_int_eq(1, ava_varscope_num_vars(scopes[2]));
}

deftest(circular_reference) {
  SCOPES_SYMS(3, 1);
  PUT(0, 0);
  SREF(2, 1);
  SREF(1, 2);
  VREF(1, 0);
  INDEX(0, 0, 0);
  INDEX(0, 1, 0);
  INDEX(0, 2, 0);

  ck_assert_int_eq(1, ava_varscope_num_captures(scopes[1]));
  ck_assert_int_eq(1, ava_varscope_num_vars(scopes[1]));
  ck_assert_int_eq(1, ava_varscope_num_captures(scopes[2]));
  ck_assert_int_eq(1, ava_varscope_num_vars(scopes[2]));
}

deftest(repeated_reference) {
  SCOPES_SYMS(3, 1);
  PUT(0, 0);
  VREF(1, 0);
  SREF(2, 1);
  SREF(2, 1);
  INDEX(0, 0, 0);
  INDEX(0, 1, 0);
  INDEX(0, 2, 0);

  ck_assert_int_eq(1, ava_varscope_num_captures(scopes[2]));
  ck_assert_int_eq(1, ava_varscope_num_vars(scopes[2]));
}

deftest(null_scope_reference) {
  SCOPES_SYMS(1, 1);
  PUT(0, 0);
  ava_varscope_ref_scope(scopes[0], NULL);
}

deftest(indexing_with_captures) {
  SCOPES_SYMS(2, 4);
  PUT(0, 0);
  PUT(0, 1);
  PUT(1, 2);
  PUT(1, 3);
  VREF(1, 1);
  VREF(1, 0);
  INDEX(0, 0, 0);
  INDEX(1, 0, 1);
  INDEX(1, 1, 0);
  INDEX(0, 1, 1);
  INDEX(2, 1, 2);
  INDEX(3, 1, 3);

  ck_assert_int_eq(0, ava_varscope_num_captures(scopes[0]));
  ck_assert_int_eq(2, ava_varscope_num_vars(scopes[0]));
  ck_assert_int_eq(2, ava_varscope_num_captures(scopes[1]));
  ck_assert_int_eq(4, ava_varscope_num_vars(scopes[1]));
}

deftest(partial_get_vars) {
  const ava_symbol* read[2];
  SCOPES_SYMS(2, 2);
  PUT(0, 0);
  PUT(1, 1);
  VREF(1, 0);

  ck_assert_int_eq(2, ava_varscope_num_vars(scopes[1]));
  ck_assert_int_eq(1, ava_varscope_num_captures(scopes[1]));
  read[1] = NULL;
  ava_varscope_get_vars(read, scopes[1], 1);
  ck_assert_ptr_eq(symbols[0], read[0]);
  ck_assert_ptr_eq(NULL, read[1]);
}

deftest(full_get_vars) {
  const ava_symbol* read[2];
  SCOPES_SYMS(2, 2);
  PUT(0, 0);
  PUT(1, 1);
  VREF(1, 0);

  ck_assert_int_eq(2, ava_varscope_num_vars(scopes[1]));
  ck_assert_int_eq(1, ava_varscope_num_captures(scopes[1]));
  ava_varscope_get_vars(read, scopes[1], 2);
  ck_assert_ptr_eq(symbols[0], read[0]);
  ck_assert_ptr_eq(symbols[1], read[1]);
}
