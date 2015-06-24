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

#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/symbol-table.h"

defsuite(symbol_table);

/* Used to produce pointers for the sake of pointers */
static char symbol[16];

#define S(s) ava_string_of_cstring(#s)
#define DECL ava_symbol_table_get_result res AVA_UNUSED; \
  ava_symbol_table* table = NULL
#define NEW(transparent) (table = ava_symbol_table_new(table, transparent))
#define PUT(key,sym)                                                    \
  ck_assert_int_eq(ava_stps_ok, ava_symbol_table_put(                   \
                     table, ava_string_of_cstring(#key), symbol + sym))
#define GET(key)                                                        \
  (res = ava_symbol_table_get(table, ava_string_of_cstring(#key)))
#define IMPORT(expected,from,to,strong,auto)                    \
  ck_assert_int_eq(expected, ava_symbol_table_import(           \
                     table, ava_string_of_cstring(#from),       \
                     ava_string_of_cstring(#to),                \
                     strong, auto))

deftest(negative_lookup_in_empty) {
  DECL;
  NEW(0);
  GET(foo);
  ck_assert_int_eq(ava_stgs_not_found, res.status);
}

deftest(positive_lookup_in_singleton) {
  DECL;
  NEW(0);
  PUT(foo, 0);
  GET(foo);

  ck_assert_int_eq(ava_stgs_ok, res.status);
  ck_assert_ptr_eq(symbol + 0, res.symbol);
}

deftest(negative_lookup_in_singleton) {
  DECL;
  NEW(0);
  PUT(foo, 0);
  GET(bar);

  ck_assert_int_eq(ava_stgs_not_found, res.status);
}

deftest(positive_lookup_from_parent) {
  DECL;
  NEW(0);
  PUT(foo, 0);
  NEW(0);
  PUT(bar, 1);
  GET(foo);

  ck_assert_int_eq(ava_stgs_ok, res.status);
  ck_assert_ptr_eq(symbol + 0, res.symbol);
}

deftest(positive_lookup_from_child_shadowing_parent) {
  DECL;
  NEW(0);
  PUT(foo, 0);
  NEW(0);
  PUT(foo, 1);
  GET(foo);

  ck_assert_int_eq(ava_stgs_ok, res.status);
  ck_assert_ptr_eq(symbol + 1, res.symbol);
}

deftest(error_from_name_redifiniton) {
  DECL;
  NEW(0);
  PUT(foo, 0);

  ck_assert_int_eq(ava_stps_redefined_strong_local,
                   ava_symbol_table_put(table, S(foo), symbol + 1));
}

deftest(no_error_from_redefining_name_to_same_thing) {
  DECL;
  NEW(0);
  PUT(foo, 0);
  PUT(foo, 0);
}

deftest(simple_import) {
  DECL;
  NEW(0);
  PUT(foo.bar, 0);
  IMPORT(ava_stis_ok, foo.,, 0, 0);
  GET(bar);

  ck_assert_int_eq(ava_stgs_ok, res.status);
  ck_assert_ptr_eq(symbol + 0, res.symbol);
}

deftest(simple_import_with_new_prefix) {
  DECL;
  NEW(0);
  PUT(foo.bar, 0);
  IMPORT(ava_stis_ok, foo., xyzzy., 0, 0);
  GET(xyzzy.bar);

  ck_assert_int_eq(ava_stgs_ok, res.status);
  ck_assert_ptr_eq(symbol + 0, res.symbol);
}

deftest(import_from_parent) {
  DECL;
  NEW(0);
  PUT(foo.bar, 0);
  NEW(0);
  IMPORT(ava_stis_ok, foo., , 0, 0);
  GET(bar);

  ck_assert_int_eq(ava_stgs_ok, res.status);
  ck_assert_ptr_eq(symbol + 0, res.symbol);
}

deftest(weak_import_doesnt_overwrite_existing_strong) {
  DECL;
  NEW(0);
  PUT(foo.bar, 0);
  PUT(bar, 1);
  IMPORT(ava_stis_ok, foo., , 0, 0);
  GET(bar);

  ck_assert_int_eq(ava_stgs_ok, res.status);
  ck_assert_ptr_eq(symbol + 1, res.symbol);
}

deftest(new_strong_overwrites_weak_imported) {
  DECL;
  NEW(0);
  PUT(foo.bar, 0);
  IMPORT(ava_stis_ok, foo., , 0, 0);
  PUT(bar, 1);
  GET(bar);

  ck_assert_int_eq(ava_stgs_ok, res.status);
  ck_assert_ptr_eq(symbol + 1, res.symbol);
}

deftest(conflicting_weak_imports_produce_ambiguous_symbol) {
  DECL;
  NEW(0);
  PUT(foo.plugh, 0);
  PUT(bar.plugh, 1);
  IMPORT(ava_stis_ok, foo., , 0, 0);
  IMPORT(ava_stis_ok, bar., , 0, 0);
  GET(plugh);

  ck_assert_int_eq(ava_stgs_ambiguous_weak, res.status);
}

deftest(weak_imports_not_ambiguous_if_same_symbol) {
  DECL;
  NEW(0);
  PUT(foo.plugh, 0);
  PUT(bar.plugh, 0);
  IMPORT(ava_stis_ok, foo., , 0, 0);
  IMPORT(ava_stis_ok, bar., , 0, 0);
  GET(plugh);

  ck_assert_int_eq(ava_stgs_ok, res.status);
  ck_assert_ptr_eq(symbol + 0, res.symbol);
}

deftest(strong_import_conflict_is_error) {
  DECL;
  NEW(0);
  PUT(foo.plugh, 0);
  PUT(bar.plugh, 1);
  IMPORT(ava_stis_ok, foo., , 1, 0);
  IMPORT(ava_stis_redefined_strong_local, bar., , 1, 0);
}

deftest(import_does_not_affect_parent) {
  ava_symbol_table* parent;
  DECL;
  NEW(0);
  PUT(foo.plugh, 0);
  parent = table;
  /* Transparent so that any put-related bugs are revealed */
  NEW(1);
  IMPORT(ava_stis_ok, foo., , 0, 0);

  GET(plugh);
  ck_assert_int_eq(ava_stgs_ok, res.status);
  ck_assert_int_eq(symbol + 0, res.symbol);

  table = parent;
  GET(plugh);
  ck_assert_int_eq(ava_stgs_not_found, res.status);
}

deftest(import_does_not_reapply_to_own_output) {
  DECL;
  NEW(0);
  /* Import "a" -> "" results in "ab", which is inserted after "aab" and does
   * start with the same prefix.
   */
  PUT(aab, 0);
  IMPORT(ava_stis_ok, a, , 0, 0);

  GET(ab);
  ck_assert_int_eq(ava_stgs_ok, res.status);

  GET(b);
  ck_assert_int_eq(ava_stgs_not_found, res.status);
}

deftest(non_automatic_imports_not_applied_retroactively) {
  DECL;
  NEW(0);
  IMPORT(ava_stis_no_symbols_imported, foo., bar., 0, 0);
  PUT(foo.quux, 0);
  GET(bar.quux);

  ck_assert_int_eq(ava_stgs_not_found, res.status);
}

deftest(automaic_imports_applied_retroactively) {
  DECL;
  NEW(0);
  IMPORT(ava_stis_no_symbols_imported, foo., bar., 0, 1);
  PUT(foo.quux, 0);
  GET(bar.quux);

  ck_assert_int_eq(ava_stgs_ok, res.status);
}

deftest(automatic_imports_stack) {
  DECL;
  NEW(0);
  IMPORT(ava_stis_no_symbols_imported, x, , 0, 1);
  IMPORT(ava_stis_no_symbols_imported, y, , 0, 1);
  PUT(xyxyyxxxfoo, 0);
  GET(foo);

  ck_assert_int_eq(ava_stgs_ok, res.status);
}

deftest(automatic_imports_applied_to_imported_symbols) {
  DECL;
  NEW(0);
  IMPORT(ava_stis_no_symbols_imported, x, , 0, 1);
  IMPORT(ava_stis_no_symbols_imported, y, , 0, 1);
  PUT(quux.xyxyyxxxfoo, 0);
  IMPORT(ava_stis_ok, quux., , 0, 0);
  GET(foo);

  ck_assert_int_eq(ava_stgs_ok, res.status);
}

deftest(put_to_opaque_child_doesnt_propagate_to_parent) {
  ava_symbol_table* parent;
  DECL;
  NEW(0);
  parent = table;
  NEW(0);
  PUT(foo, 0);
  table = parent;
  GET(foo);

  ck_assert_int_eq(ava_stgs_not_found, res.status);
}

deftest(put_to_transparent_child_propagates_to_parent) {
  ava_symbol_table* parent;
  DECL;
  NEW(0);
  parent = table;
  NEW(1);
  PUT(foo, 0);
  table = parent;
  GET(foo);

  ck_assert_int_eq(ava_stgs_ok, res.status);
}

deftest(save_and_apply_imports) {
  const ava_import_list* imports;
  DECL;
  NEW(0);
  IMPORT(ava_stis_no_symbols_imported, foo., bar., 0, 0);
  PUT(foo.quux, 0);
  imports = ava_symbol_table_get_imports(table);
  IMPORT(ava_stis_ok, foo., xyzzy., 0, 0);
  ck_assert_int_eq(
    ava_stis_ok,
    ava_symbol_table_apply_imports(&table, table, imports));

  GET(xyzzy.quux);
  ck_assert_int_eq(ava_stgs_not_found, res.status);

  GET(bar.quux);
  ck_assert_int_eq(ava_stgs_ok, res.status);
}

deftest(apply_imports_returns_failure_on_strong_conflict) {
  const ava_import_list* imports;
  DECL;
  NEW(0);
  IMPORT(ava_stis_no_symbols_imported, foo., , 1, 0);
  IMPORT(ava_stis_no_symbols_imported, bar., , 1, 0);
  imports = ava_symbol_table_get_imports(table);
  PUT(foo.x, 0);
  PUT(bar.x, 1);
  ck_assert_int_eq(
    ava_stis_redefined_strong_local,
    ava_symbol_table_apply_imports(&table, table, imports));
}

deftest(apply_imports_doesnt_return_empty_import) {
  const ava_import_list* imports;
  DECL;
  NEW(0);
  IMPORT(ava_stis_no_symbols_imported, foo., , 0, 0);
  IMPORT(ava_stis_no_symbols_imported, bar., , 0, 0);
  imports = ava_symbol_table_get_imports(table);
  ck_assert_int_eq(
    ava_stis_ok,
    ava_symbol_table_apply_imports(&table, table, imports));
}

deftest(apply_imports_preserves_parent_relationship) {
  ava_symbol_table* parent;
  const ava_import_list* imports;
  DECL;
  NEW(0);
  PUT(foo, 0);
  parent = table;
  NEW(1);
  imports = ava_symbol_table_get_imports(table);
  ck_assert_int_eq(
    ava_stis_ok,
    ava_symbol_table_apply_imports(&table, table, imports));

  GET(foo);
  ck_assert_int_eq(ava_stgs_ok, res.status);

  PUT(bar, 1);
  table = parent;
  GET(bar);
  ck_assert_int_eq(ava_stgs_ok, res.status);
  ck_assert_ptr_eq(symbol + 1, res.symbol);
}
