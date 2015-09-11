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
#error "Don't include avalanche/symtab.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_SYMTAB_H_
#define AVA_RUNTIME_SYMTAB_H_

#include "string.h"
#include "symbol.h"

/**
 * The symtab (symbol table) is responsible for mapping strings to symbols,
 * including the rules for imports and nested scopes.
 *
 * A symtab is composed of three properties:
 * - An optional parent symtab
 * - A mutable string->symbol map shared with other symtabs at the same level
 * - An immutable import list
 *
 * An import is a singular prefix rewriting term that transforms a simple name
 * into a fully-qualified name; imports are not (re)applied to the output of
 * this transform so as to avoid an exponential growth in CPU time and/or
 * memory.
 *
 * Additionally, imports can be either "strong" or "weak". The only difference
 * is that strong imports are tried before weak imports, and do not result in
 * ambiguity if a weak import would also lead to the symbol.
 *
 * Name resolution is performed as follows:
 * - The symtab attempts to find the name verbatim in its own name map.
 * - The symtab uses its own strong imports to try to find the transformed name
 *   in its own name map.
 * - The symtab uses its own weak imports to try to find the transformed name
 *   in its own name map.
 * - The above two steps are repeated for the symtab's parent, then its
 *   grandparent, and so forth.
 * - The symtab recurses to its parent and starts over.
 *
 * The process stops after the first step which finds at least one symbol. An
 * ambiguity results if the same step found more than one distinct symbol.
 */
typedef struct ava_symtab_s ava_symtab;

/**
 * Creates a new symtab with an empty name map and import list, and the given
 * parent. The name map is independent of that of the parent.
 *
 * @param parent The parent of the new symtab, or NULL if none.
 */
ava_symtab* ava_symtab_new(const ava_symtab* parent);
/**
 * Adds a symbol to the given symtab's name map.
 *
 * @param symtab The symtab to modify.
 * @param symbol The new symbol to add.
 * @return NULL on success. If a symbol with that name already existed within
 * this symtab's name map, it is returned and the symbol table is not modified.
 */
const ava_symbol* ava_symtab_put(ava_symtab* symtab, const ava_symbol* symbol);
/**
 * Searches the given symtab for the given key.
 *
 * @param result Initialised by the call to point to an array of symbols which
 * were found by the call. The length of the array is indicated by the return
 * value. The pointer value will be NULL if no results were found.
 * @param symtab The symtab to searh.
 * @param key The simple or fully-qualified name for which to search.
 * @return The number of distinct symbols found.
 */
size_t ava_symtab_get(const ava_symbol*** result, const ava_symtab* symtab,
                      ava_string key);
/**
 * Creates a new symbol table with the given added import.
 *
 * Unless absolute is true, the symtab will attempt to absolutise the
 * old_prefix import. This is performed like a normal name lookup, except that
 * any symbol whose name is simply prefixed by the key or transformed key is
 * accepted. For absolutisation to succeed, exactly one distinct key or
 * transformed key (given imports before this call) must succeed. If
 * absolutisation is ambiguous, one option is chose arbitrarily, though the
 * calling code should handle this as an error.
 *
 * If absolutisation fails or is not performed, the old_prefix is used as-is.
 * Whether this is an error is context-dependent.
 *
 * The added import has the effect that any name of the form
 *   $new_prefix$$name
 * can be resolved to a symbol whose full name is of the form
 *   $old_prefix$$name
 * (after old_prefix has been absolutised, if applicable).
 *
 * The new symtab shares the same name map as the old one, but has the new
 * import appended to its import list.
 *
 * @param If absolutisation was performed and found at least one result, set to
 * the absolutised old_prefix. Otherwise, set to the absent string.
 * @param If absolutisation was performed and found more than one result, set
 * to an arbitrary example candidate that is not *absolutised. Otherwise, set
 * to the absent string. The caller should check for this condition if absolute
 * is not true.
 * @param symtab The symtab to use as the basis of this import.
 * @param old_prefix The prefix found on symbol names. May need to be subjected
 * to absolutisation.
 * @param new_prefix The prefix that will be stripped from names to which this
 * import could be applied before adding the absolutised old_prefix.
 * @param absolute Whether this import is known to be absolute. If true,
 * absolutisation is skipped and old_prefix is used as-is.
 * @param is_strong Whether this import is marked strong.
 * @return A new symtab with the same parent and name map as the old one, but
 * the new import appended to the import list. Modifications to the name map of
 * either symtab will be reflected in the other. If the new import was exactly
 * equivalent to one already in the symtab, the original is returned.
 */
ava_symtab* ava_symtab_import(ava_string* absolutised, ava_string* ambiguous,
                              ava_symtab* symtab,
                              ava_string old_prefix, ava_string new_prefix,
                              ava_bool absolute, ava_bool is_strong);

#endif /* AVA_RUNTIME_SYMTAB_H_ */
