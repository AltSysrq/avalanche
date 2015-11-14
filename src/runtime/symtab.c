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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/symbol.h"
#include "avalanche/symtab.h"
#include "bsd.h"

/**
 * An entry in a symtab's name map. The proper key is the full_name of the
 * symbol.
 */
typedef struct ava_symtab_entry_s {
  /**
   * The symbol held by this entry. The full_name is this entry's key.
   */
  const ava_symbol* symbol;
  /**
   * Control for the entry comparator.
   *
   * If true, this entry is considered equal to any other entry where this
   * entry's key is a prefix of the other's. This is never true for entries
   * actually in the map; it is only used in the exemplar while searching.
   *
   * This admittedly is somewhat hackish, but it's still sound since it is
   * sufficiently consistent with the normal string comparator. For any case
   * the full string comparator returns equality, so does this comparison; for
   * any inequality, it either returns the same inequality, or returns
   * equality. In the former case, the tree search continues normally, since
   * the behaviour is the same. In the latter case, it causes the tree search
   * to terminate immediately and return whatever result was found, which is
   * sufficient for the one use case (import absolutisation).
   *
   * This scheme would likely result in issues if used in a SPLAY instead of RB
   * tree.
   */
  ava_bool prefix_only;
  RB_ENTRY(ava_symtab_entry_s) map;
} ava_symtab_entry;

/**
 * Comparator for use with ava_symtab_map.
 */
static signed ava_compare_symtab_entry(
  const ava_symtab_entry* a, const ava_symtab_entry* b);

RB_HEAD(ava_symtab_map, ava_symtab_entry_s);
RB_PROTOTYPE_STATIC(ava_symtab_map, ava_symtab_entry_s, map,
                    ava_compare_symtab_entry);
RB_GENERATE_STATIC(ava_symtab_map, ava_symtab_entry_s, map,
                   ava_compare_symtab_entry);

/**
 * A single import entry added by a call to ava_entry_import().
 */
typedef struct ava_symtab_import_entry_s {
  /**
   * Prefix which matches, and is stripped from, names being resolved.
   */
  ava_string new_prefix;
  /**
   * Prefix which is prepended to matched names being resolved.
   */
  ava_string old_prefix;
  /**
   * Whether this import is "strong".
   */
  ava_bool is_strong;
  /**
   * The next import entry belonging to the same symtab.
   *
   * This is not an SLIST_ENTRY because there is no single SLIST_HEAD
   * corresponding to it. The imports in fact form an inverted tree, where each
   * symtab references one of the leaves, which can be followed to the eventual
   * NULL root.
   */
  const struct ava_symtab_import_entry_s* next;
} ava_symtab_import_entry;

struct ava_symtab_s {
  /**
   * The parent of this symtab, if it has one.
   */
  const ava_symtab* parent;
  /**
   * The name map, possibly shared with other symtabs with the same immediate
   * parent.
   */
  struct ava_symtab_map* names;
  /**
   * The imports of this symtab. These do not link to the imports of the
   * parent; the two are separate lists.
   */
  const ava_symtab_import_entry* imports;
};

/**
 * Function passed to ava_symtab_search() whenever a matching result is found.
 *
 * @param effective_name The name that was used to locate the matching symbol.
 * For an exact search, this exactly matches the symbol's full_name. For prefix
 * search, this is the prefix used to match the symbol.
 * @param symbol The symbol that was found in this location.
 * @param context Arbitrary data passed in by the ava_symtab_search() caller.
 */
typedef void (*ava_symtab_search_acceptor_f)(
  ava_string effective_name, const ava_symbol* symbol,
  void* context);

/**
 * Searches the given symbol table for the given target name.
 *
 * @param symtab The base symbol table to search.
 * @param target The string for which to search.
 * @param prefix_only If true, symbols will match if target is a prefix of
 * their full_name.
 * @param acceptor Called at least once for each matching result within the
 * first stage that has matching results. The search terminates after the first
 * such stage.
 */
static void ava_symtab_search(
  const ava_symtab* symtab, ava_string target, ava_bool prefix_only,
  ava_symtab_search_acceptor_f acceptor, void* context);

/**
 * Like ava_symtab_search(), but only looks in this symbol table's name map,
 * and does not do import matching.
 *
 * @return Whether anything was found.
 */
static ava_bool ava_symtab_find(
  const ava_symtab* symtab, ava_string effective_name, ava_bool prefix_only,
  ava_symtab_search_acceptor_f acceptor, void* context);

/**
 * Attempts to match the given simple name to an import.
 *
 * @param effective_name If the import matches, set to the fully-qualified name
 * resulting from applying the import.
 * @param import The import to try to apply to the given name.
 * @param name The simple name to which to apply the import.
 * @return Whether the import matched.
 */
static ava_bool ava_symtab_import_match(
  ava_string* effective_name, const ava_symtab_import_entry* import,
  ava_string name);

#define AVA_SYMTAB_GET_ACCUM_STATIC_ENTRIES 16

/**
 * Data used for accumulating the results of ava_symtab_get().
 */
typedef struct {
  /**
   * RB tree of matching entries.
   */
  struct ava_symtab_map found;
  /**
   * The number of distinct entries found.
   */
  size_t num_found;
  /**
   * Memory to use for the first AVA_SYMTAB_GET_ACCUM_STATIC_ENTRIES distinct
   * results. When this is exhausted, the heap is used instead.
   */
  ava_symtab_entry static_entries[AVA_SYMTAB_GET_ACCUM_STATIC_ENTRIES];
} ava_symtab_get_accum;

static void ava_symtab_get_accept(ava_string effective, const ava_symbol* found,
                                  void* context);

/**
 * Data used for accumulating the results of absolutisation in
 * ava_symtab_import().
 */
typedef struct {
  /**
   * The string in which to store the primary result. Initially absent (absent
   * string, not a NULL pointer).
   */
  ava_string* primary;
  /**
   * The string in which to store any arbitrary result not equal to *primary.
   * Initially absent.
   */
  ava_string* ambiguous;
} ava_symtab_a12n_accum;

static void ava_symtab_a12n_accept(ava_string effective, const ava_symbol* found,
                                   void* context);

static signed ava_compare_symtab_entry(const ava_symtab_entry* a,
                                       const ava_symtab_entry* b) {
  size_t alen, blen;
  ava_string as = a->symbol->full_name, bs = b->symbol->full_name;

  if (a->prefix_only || b->prefix_only) {
    alen = ava_string_length(as);
    blen = ava_string_length(bs);
    if (a->prefix_only && alen < blen) {
      bs = ava_string_slice(bs, 0, alen);
      blen = alen;
    }
    if (b->prefix_only && blen < alen) {
      as = ava_string_slice(as, 0, blen);
      alen = blen;
    }
  }

  return ava_strcmp(as, bs);
}

ava_symtab* ava_symtab_new(const ava_symtab* parent) {
  ava_symtab* this = AVA_NEW(ava_symtab);

  this->parent = parent;
  this->names = AVA_NEW(struct ava_symtab_map);
  RB_INIT(this->names);
  this->imports = NULL;

  return this;
}

const ava_symbol* ava_symtab_put(ava_symtab* symtab, const ava_symbol* symbol) {
  ava_symtab_entry* new_entry, * old_entry;

  new_entry = AVA_NEW(ava_symtab_entry);
  new_entry->prefix_only = ava_false;
  new_entry->symbol = symbol;
  old_entry = RB_INSERT(ava_symtab_map, symtab->names, new_entry);

  return old_entry && old_entry->symbol != symbol?
    old_entry->symbol : NULL;
}

size_t ava_symtab_get(const ava_symbol*** result, const ava_symtab* symtab,
                      ava_string key) {
  ava_symtab_get_accum accum;
  ava_symtab_entry* entry;
  size_t i;

  RB_INIT(&accum.found);
  accum.num_found = 0;

  ava_symtab_search(symtab, key, ava_false,
                    ava_symtab_get_accept, &accum);

  if (0 == accum.num_found) {
    *result = NULL;
  } else {
    *result = ava_alloc(sizeof(ava_symbol*) * accum.num_found);
    i = 0;
    RB_FOREACH(entry, ava_symtab_map, &accum.found)
      (*result)[i++] = entry->symbol;
  }

  return accum.num_found;
}

static void ava_symtab_get_accept(ava_string effective, const ava_symbol* found,
                                  void* vaccum) {
  ava_symtab_get_accum* accum = vaccum;
  ava_symtab_entry* entry;

  if (accum->num_found < AVA_SYMTAB_GET_ACCUM_STATIC_ENTRIES)
    entry = accum->static_entries + accum->num_found;
  else
    entry = AVA_NEW(ava_symtab_entry);

  entry->prefix_only = ava_false;
  entry->symbol = found;
  if (!RB_INSERT(ava_symtab_map, &accum->found, entry))
    ++accum->num_found;
}

ava_symtab* ava_symtab_import(ava_string* absolutised, ava_string* ambiguous,
                              ava_symtab* symtab,
                              ava_string old_prefix, ava_string new_prefix,
                              ava_bool absolute, ava_bool is_strong) {
  ava_symtab_a12n_accum accum = { .primary = absolutised,
                                  .ambiguous = ambiguous };
  const ava_symtab_import_entry* old_import;
  ava_symtab_import_entry new_import;

  *absolutised = AVA_ABSENT_STRING;
  *ambiguous = AVA_ABSENT_STRING;

  if (!absolute) {
    ava_symtab_search(symtab, old_prefix, ava_true,
                      ava_symtab_a12n_accept, &accum);
    if (ava_string_is_present(*absolutised))
      old_prefix = *absolutised;
  }

  /* Check for existing old import */
  for (old_import = symtab->imports; old_import; old_import = old_import->next)
    if (is_strong == old_import->is_strong &&
        !ava_strcmp(old_prefix, old_import->old_prefix) &&
        !ava_strcmp(new_prefix, old_import->new_prefix))
      /* Already present in this symtab */
      return symtab;

  /* No existing import, create a new one */
  new_import.is_strong = is_strong;
  new_import.old_prefix = old_prefix;
  new_import.new_prefix = new_prefix;
  new_import.next = symtab->imports;

  symtab = AVA_CLONE(*symtab);
  symtab->imports = AVA_CLONE(new_import);
  return symtab;
}

static void ava_symtab_a12n_accept(ava_string effective, const ava_symbol* found,
                                   void* vaccum) {
  const ava_symtab_a12n_accum* accum = vaccum;

  if (!ava_string_is_present(*accum->primary) ||
      !ava_strcmp(*accum->primary, effective))
    *accum->primary = effective;
  else
    *accum->ambiguous = effective;
}

static void ava_symtab_search(
  const ava_symtab* symtab, ava_string target, ava_bool prefix_only,
  ava_symtab_search_acceptor_f acceptor, void* context
) {
  const ava_symtab* import_source, * name_source;
  const ava_symtab_import_entry* import;
  ava_string effective_name;
  ava_bool is_strong, any_found;

  for (name_source = symtab; name_source; name_source = name_source->parent) {
    if (ava_symtab_find(name_source, target, prefix_only, acceptor, context))
      return;

    for (import_source = symtab; import_source;
         import_source = import_source->parent) {
      for (is_strong = ava_true; is_strong >= ava_false; --is_strong) {
        any_found = ava_false;
        for (import = import_source->imports; import; import = import->next) {
          if (is_strong == import->is_strong &&
              ava_symtab_import_match(&effective_name, import, target) &&
              ava_symtab_find(name_source, effective_name, prefix_only,
                              acceptor, context)) {
            any_found = ava_true;
          }
        }
        if (any_found) return;
      }
    }
  }
}

static ava_bool ava_symtab_find(
  const ava_symtab* symtab, ava_string effective_name, ava_bool prefix_only,
  ava_symtab_search_acceptor_f acceptor, void* context
) {
  ava_symbol sym = { .full_name = effective_name };
  ava_symtab_entry exemplar = { .prefix_only = prefix_only, .symbol = &sym };
  const ava_symtab_entry* found;

  found = RB_FIND(ava_symtab_map, symtab->names, &exemplar);
  if (found)
    (*acceptor)(effective_name, found->symbol, context);

  return !!found;
}

static ava_bool ava_symtab_import_match(
  ava_string* effective_name, const ava_symtab_import_entry* import,
  ava_string name
) {
  if (ava_string_starts_with(name, import->new_prefix)) {
    /* Match */
    *effective_name = ava_strcat(
      import->old_prefix, ava_string_slice(
        name, ava_string_length(import->new_prefix),
        ava_string_length(name)));
    return ava_true;
  } else {
    return ava_false;
  }
}
