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
#error "Don't include avalanche/symbol-table.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_SYMBOL_TABLE_H_
#define AVA_RUNTIME_SYMBOL_TABLE_H_

#include "defs.h"
#include "string.h"

/**
 * Implements Avalanche's name-resolution rules.
 *
 * A symbol table is essentially a map from names to symbols, but also tracks
 * whether those bindings are weak/ambiguous. Furthermore, each symbol table
 * may have a parent; any name not resolvable in the child will be searched in
 * the parent, recursively.
 *
 * Nested symbol tables may be opaque or transparent. With a transparent child,
 * any new symbols introduced by ava_symbol_table_put() also propagate to the
 * parent.
 */
typedef struct ava_symbol_table_s ava_symbol_table;

/**
 * A list of imports which have been applied to a symbol table.
 *
 * This is used to remember the imports in effect at a particular location,
 * while delaying resolution until symbols defined in later locations have
 * already been added to the symbol table.
 *
 * @see ava_symbol_table_get_imports()
 * @see ava_symbol_table_apply_imports()
 */
typedef struct ava_import_list_s ava_import_list;

/**
 * Status returned by a call to ava_symbol_table_put().
 */
typedef enum {
  /**
   * The call succeeded.
   */
  ava_stps_ok = 0,
  /**
   * The call failed because the given name is already a strong name in the
   * table or one of its transparent ancestors.
   *
   * Whether the symbol has been inserted/rebound in the table or its
   * transparent ancestors is unspecified.
   */
  ava_stps_redefined_strong_local,
  /**
   * Like ava_stps_redefined_strong_local, but indicates that an auto-import in
   * effect resulted in the conflict.
   */
  ava_stps_redefined_strong_local_by_auto_import
} ava_symbol_table_put_status;

/**
 * Status of a call to ava_symbol_table_get().
 */
typedef enum {
  /**
   * The name was resolved to a symbol successfully.
   */
  ava_stgs_ok = 0,
  /**
   * No symbol could be found because the name has multiple weak bindings.
   */
  ava_stgs_ambiguous_weak,
  /**
   * No symbol is bound to the given name.
   */
  ava_stgs_not_found
} ava_symbol_table_get_status;

/**
 * Return type from the ava_symbol_table_import*() functions.
 */
typedef enum {
  /**
   * Import succeeded.
   */
  ava_stis_ok = 0,
  /**
   * No names starting with old_prefix were found, so the symbol table is
   * effectively unchanged.
   *
   * This is nominally a success as far as the symbol table itself is
   * concerned.
   */
  ava_stis_no_symbols_imported,
  /**
   * A strong import resulted in the attempted redefinition of a strong name
   * local to this symbol table.
   *
   * Whether this symbol was redefined or whether the import was actually
   * otherwise completed is unspecified.
   */
  ava_stis_redefined_strong_local
} ava_symbol_table_import_status;

/**
 * Return value from ava_symbol_table_get_result.
 */
typedef struct {
  /**
   * The status of the result.
   */
  ava_symbol_table_get_status status;
  /**
   * If status == ava_stgs_ok, the symbol found by the lookup.
   */
  const void* symbol;
} ava_symbol_table_get_result;

/**
 * Allocates a new, empty symbol table.
 *
 * @param parent The parent symbol table of the new symbol table, or NULL if it
 * is to have no parent.
 * @param transparent_parent Whether the parent, if any, is transparent. If
 * there is no parent, must be ava_false.
 * @return The new symbol table.
 */
ava_symbol_table* ava_symbol_table_new(const ava_symbol_table* parent,
                                       ava_bool transparent_parent);

/**
 * Defines a new name/symbol pair in the given symbol table.
 *
 * If the table has a transparent parent, the put is applied to the parent as
 * well.
 *
 * Any auto-imports in effect are implicitly applied to the result.
 *
 * @param table The table to modify.
 * @param name The name to bind to the symbol.
 * @param symbol The symbol to bind to the name.
 * @return The status of the binding.
 */
ava_symbol_table_put_status ava_symbol_table_put(
  ava_symbol_table* table, ava_string name, const void* symbol);
/**
 * Looks the given name up within the symbol table.
 *
 * @param table The initial table to search. If the name is not local to this
 * table, the parent is recursively searched.
 * @param name The name to look up.
 * @return The result of the lookup.
 */
ava_symbol_table_get_result ava_symbol_table_get(
  const ava_symbol_table* table, ava_string name);

/**
 * Performs a one-time import within the given table.
 *
 * All names visible to the table (including names in ancestors) which begin
 * with old_prefix are reinserted with old_prefix substituted with new_prefix.
 *
 * This import is not applied to later symbols added with
 * ava_symbol_table_put() unless is_auto is true.
 *
 * @param table The table to mutate.
 * @param old_prefix The prefix to match against and strip from visible names.
 * @param new_prefix The new prefix to prepend to reinserted names.
 * @param is_strong Whether the new name bindings are strong.
 * @param is_auto Whether this import lingers and is applied to new symbols
 * created directly by ava_symbol_table_put() against this table or one of its
 * transparent children.
 * @return The status of the import. If the import is unsuccessful, the import
 * may have been partially applied.
 */
ava_symbol_table_import_status ava_symbol_table_import(
  ava_symbol_table* table, ava_string old_prefix, ava_string new_prefix,
  ava_bool is_strong, ava_bool is_auto);

/**
 * Returns a frozen list of imports that have been applied to the given table
 * (both one-time and automatic imports).
 *
 * @see ava_symbol_table_apply_imports()
 */
const ava_import_list* ava_symbol_table_get_imports(
  const ava_symbol_table* table);
/**
 * Builds a new symbol table using the original names from one and a list of
 * imports from ava_symbol_table_get_imports().
 *
 * A new symbol table with the same parent (and transparency) as input is
 * created, and all bindings created directly via ava_symbol_table_put() are
 * copied. Then, all imports in the import list are applied in sequence.
 *
 * @param dst Out-pointer for the new symbol table. This is always populated
 * with a valid symbol table, even on failure.
 * @param input The symbol table to strip of existing imports and reapply
 * imports to.
 * @param imports The frozen import list to apply to the stripped table.
 * @return The overall import status. If any import fails, the failure status
 * of that import is returned; otherwise, ava_stis_ok is returned.
 */
ava_symbol_table_import_status ava_symbol_table_apply_imports(
  ava_symbol_table** dst,
  const ava_symbol_table* input, const ava_import_list* imports);

#endif /* AVA_RUNTIME_SYMBOL_TABLE_H_ */
