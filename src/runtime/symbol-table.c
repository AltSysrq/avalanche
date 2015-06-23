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
#include <assert.h>

#define AVA__INTERNAL_INCLUDE 1
#include "../bsd.h"
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/symbol-table.h"

typedef struct ava_import_list_entry_s {
  ava_string old_prefix, new_prefix;
  ava_bool is_strong, is_auto;

  TAILQ_ENTRY(ava_import_list_entry_s) next;
} ava_import_list_entry;

TAILQ_HEAD(ava_import_list_s,ava_symbol_table_import_entry_s);

typedef struct ava_symbol_table_entry_s {
  /**
   * The name of this entry.
   */
  ava_string name;
  /**
   * Whether this binding is strong.
   */
  ava_bool strong;
  /**
   * Whether this binding was a direct result of ava_symbol_table_put. All
   * original bindings should be strong.
   */
  ava_bool original;
  /**
   * Whether this binding was created by an in-progress import.
   *
   * This is used to prevent a one-time import from applying more than once to
   * the same entry.
   *
   * When an import completes, it clears this field on all entries.
   */
  ava_bool fresh_import;
  /**
   * The symbol bound to this name, or NULL if ambiguous.
   */
  const void* symbol;

  RB_ENTRY(ava_symbol_table_entry_s) map;
} ava_symbol_table_entry;

static signed ava_compare_symbol_table_entry(
  const ava_symbol_table_entry* a,
  const ava_symbol_table_entry* b);

RB_HEAD(ava_symbol_table_tree, ava_symbol_table_entry_s);
RB_PROTOTYPE_STATIC(ava_symbol_table_tree, ava_symbol_table_entry_s,
                    map, ava_compare_symbol_table_entry);
RB_GENERATE_STATIC(ava_symbol_table_tree, ava_symbol_table_entry_s,
                   map, ava_compare_symbol_table_entry);

struct ava_symbol_table_s {
  ava_import_list imports;
  struct ava_symbol_table_tree names;

  ava_symbol_table* parent;
  ava_bool transparent_parent;
};

static ava_symbol_table_entry* ava_symbol_table_entry_new(
  ava_string name, const void* symbol,
  ava_bool strong, ava_bool orig);
static ava_symbol_table_put_status ava_symbol_table_put_local(
  ava_symbol_table* this, ava_symbol_table_entry* entry,
  ava_bool mark_fresh);
static ava_bool ava_symbol_table_insert(ava_symbol_table* this,
                                        ava_symbol_table_entry* entry);
static ava_symbol_table_import_status ava_symbol_table_apply_import_to_entry(
  ava_symbol_table* this, const ava_import_list_entry* import,
  const ava_symbol_table_entry* entry,
  ava_bool mark_fresh);
/**
 * Changes the prefix of name from old_prefix to new_prefix.
 *
 * If name does not start with old_prefix, returns the absent string.
 */
static ava_string ava_symbol_table_reprefix(
  ava_string name, ava_string old_prefix, ava_string new_prefix);

static signed ava_compare_symbol_table_entry(
  const ava_symbol_table_entry* a,
  const ava_symbol_table_entry* b
) {
  return ava_strcmp(a->name, b->name);
}

ava_symbol_table* ava_symbol_table_new(const ava_symbol_table* parent,
                                       ava_bool transparent_parent) {
  ava_symbol_table* this;

  assert(parent || !transparent_parent);

  this = AVA_NEW(ava_symbol_table);
  this->parent = (ava_symbol_table*)parent;
  this->transparent_parent = transparent_parent;
  TAILQ_INIT(&this->imports);
  RB_INIT(&this->names);

  return this;
}

ava_symbol_table_put_status ava_symbol_table_put(
  ava_symbol_table* this, ava_string name, const void* symbol
) {
  ava_symbol_table_entry* entry;
  ava_symbol_table_put_status status;

  do {
    entry = ava_symbol_table_entry_new(name, symbol, ava_true, ava_true);
    status = ava_symbol_table_put_local(this, entry, ava_false);

    if (ava_stps_ok != status)
      return status;
  } while (this->transparent_parent && (this = this->parent));

  return ava_stps_ok;
}

static ava_symbol_table_put_status ava_symbol_table_put_local(
  ava_symbol_table* this, ava_symbol_table_entry* entry,
  ava_bool mark_fresh
) {
  const ava_import_list_entry* import;

  if (!ava_symbol_table_insert(this, entry))
    return ava_stps_redefined_strong_local;

  TAILQ_FOREACH(import, &this->imports, next) {
    if (import->is_auto) {
      switch (ava_symbol_table_apply_import_to_entry(this, import, entry,
                                                     mark_fresh)) {
      case ava_stis_ok:
      case ava_stis_no_symbols_imported:
        break;

      case ava_stis_redefined_strong_local:
        return ava_stps_redefined_strong_local_by_auto_import;
      }
    }
  }

  return ava_stps_ok;
}

static ava_symbol_table_entry* ava_symbol_table_entry_new(
  ava_string name, const void* symbol,
  ava_bool strong, ava_bool orig
) {
  ava_symbol_table_entry* entry;

  entry = AVA_NEW(ava_symbol_table_entry);
  entry->name = name;
  entry->symbol = symbol;
  entry->strong = strong;
  entry->original = orig;

  return entry;
}

static ava_bool ava_symbol_table_insert(
  ava_symbol_table* this, ava_symbol_table_entry* entry
) {
  ava_symbol_table_entry* existing;

  existing = RB_FIND(ava_symbol_table_tree, &this->names, entry);

  if (existing) {
    if (existing->strong && entry->strong) {
      /* Conflict, insert fails unless both actually refer to the same thing. */
      return existing->symbol == entry->symbol;
    } else if (existing->strong) {
      /* Existing strong entry shadows new weak entry.
       * Insert is successful.
       */
      return ava_true;
    } else if (entry->strong) {
      /* New strong entry shadows old weak entry. */
      RB_REMOVE(ava_symbol_table_tree, &this->names, existing);
      RB_INSERT(ava_symbol_table_tree, &this->names, entry);
      return ava_true;
    } else {
      /* Both are weak, the name is now ambiguous unless they happen to refer
       * to the same thing (which can happen via imports, for example).
       */
      if (existing->symbol != entry->symbol)
        existing->symbol = NULL;
      return ava_true;
    }
  }

  RB_INSERT(ava_symbol_table_tree, &this->names, entry);
  return ava_true;
}

static ava_symbol_table_import_status ava_symbol_table_apply_import_to_entry(
  ava_symbol_table* this, const ava_import_list_entry* import,
  const ava_symbol_table_entry* entry,
  ava_bool mark_fresh
) {
  ava_string new_name;
  ava_symbol_table_entry* new;

  new_name = ava_symbol_table_reprefix(
    entry->name, import->old_prefix, import->new_prefix);
  if (ava_string_is_present(new_name)) {
    new = ava_symbol_table_entry_new(
      new_name, entry->symbol, import->is_strong, ava_false);
    new->fresh_import = mark_fresh;

    switch (ava_symbol_table_put_local(this, new, mark_fresh)) {
    case ava_stps_ok:
      return ava_stis_ok;

    case ava_stps_redefined_strong_local:
    case ava_stps_redefined_strong_local_by_auto_import:
      return ava_stis_redefined_strong_local;
    }

    /* Unreachable */
    abort();
  } else {
    return ava_stis_no_symbols_imported;
  }
}

static ava_string ava_symbol_table_reprefix(
  ava_string name, ava_string old_prefix, ava_string new_prefix
) {
  size_t name_len, old_prefix_len;

  name_len = ava_string_length(name);
  old_prefix_len = ava_string_length(old_prefix);

  if (name_len < old_prefix_len)
    return AVA_ABSENT_STRING;

  if (ava_strcmp(ava_string_slice(name, 0, old_prefix_len), old_prefix))
    return AVA_ABSENT_STRING;

  return ava_string_concat(
    new_prefix, ava_string_slice(name, old_prefix_len, name_len));
}

ava_symbol_table_get_result ava_symbol_table_get(
  const ava_symbol_table* this,
  ava_string name
) {
  ava_symbol_table_entry exemplar = { .name = name };
  const ava_symbol_table_entry* existing;

  do {
    existing = RB_FIND(ava_symbol_table_tree,
                       (struct ava_symbol_table_tree*)&this->names, &exemplar);
    if (existing) {
      if (existing->symbol) {
        return (ava_symbol_table_get_result) {
          .status = ava_stgs_ok,
          .symbol = existing->symbol
        };
      } else {
        return (ava_symbol_table_get_result) {
          .status = ava_stgs_ambiguous_weak
        };
      }
    }
  } while ((this = this->parent));

  return (ava_symbol_table_get_result) {
    .status = ava_stgs_not_found
  };
}

ava_symbol_table_import_status ava_symbol_table_import(
  ava_symbol_table* table, ava_string old_prefix, ava_string new_prefix,
  ava_bool is_strong, ava_bool is_auto
) {
  ava_import_list_entry* import;
  const ava_symbol_table* source;
  ava_symbol_table_entry* entry;
  ava_symbol_table_entry exemplar = { .name = old_prefix };
  ava_symbol_table_import_status ret = ava_stis_no_symbols_imported;

  import = AVA_NEW(ava_import_list_entry);
  import->old_prefix = old_prefix;
  import->new_prefix = new_prefix;
  import->is_strong = is_strong;
  import->is_auto = is_auto;
  TAILQ_INSERT_TAIL(&table->imports, import, next);

  for (source = table; source; source = source->parent) {
    for (entry = RB_NFIND(ava_symbol_table_tree,
                          (struct ava_symbol_table_tree*)&source->names,
                          &exemplar);
         entry;
         entry = RB_NEXT(ava_symbol_table_tree, &source->names, entry)) {
      /* When iterating over table, we may encounter symbols inserted as a
       * result of this import; skip them.
       */
      if (entry->fresh_import) continue;

      switch (ava_symbol_table_apply_import_to_entry(
                table, import, entry, ava_true)) {
      case ava_stis_ok:
        if (ava_stis_no_symbols_imported == ret)
          ret = ava_stis_ok;
        break;

      case ava_stis_no_symbols_imported:
        /* We've left the range of names beginning with new_prefix */
        goto next_source;

      case ava_stis_redefined_strong_local:
        ret = ava_stis_redefined_strong_local;
        goto done;
      }
    }

    next_source:;
  }

  done:
  /* Clear fresh-import marks */
  RB_FOREACH(entry, ava_symbol_table_tree, &table->names)
    entry->fresh_import = ava_false;

  return ret;
}

const ava_import_list* ava_symbol_table_get_imports(
  const ava_symbol_table* this
) {
  ava_import_list* imports;
  ava_import_list_entry* import;
  const ava_import_list_entry* src_import;

  imports = AVA_NEW(ava_import_list);
  TAILQ_INIT(imports);

  TAILQ_FOREACH(src_import, &this->imports, next) {
    import = ava_clone(src_import, sizeof(*import));
    TAILQ_INSERT_TAIL(imports, import, next);
  }

  return imports;
}

ava_symbol_table_import_status ava_symbol_table_apply_imports(
  ava_symbol_table** dst,
  const ava_symbol_table* input, const ava_import_list* imports
) {
  ava_symbol_table* this;
  ava_symbol_table_entry* src_entry;
  ava_symbol_table_entry* entry;
  ava_import_list_entry* import;
  ava_symbol_table_import_status ret = ava_stis_ok;

  this = ava_symbol_table_new(input->parent, input->transparent_parent);
  *dst = this;

  RB_FOREACH(src_entry, ava_symbol_table_tree,
             (struct ava_symbol_table_tree*)&input->names) {
    if (src_entry->original) {
      entry = ava_clone(src_entry, sizeof(*entry));
      ava_symbol_table_insert(this, entry);
    }
  }

  TAILQ_FOREACH(import, imports, next) {
    switch (ava_symbol_table_import(
              this, import->old_prefix, import->new_prefix,
              import->is_strong, import->is_auto)) {
    case ava_stis_ok:
    case ava_stis_no_symbols_imported:
      break;

    case ava_stis_redefined_strong_local:
      ret = ava_stis_redefined_strong_local;
      break;
    }
  }

  return ret;
}
