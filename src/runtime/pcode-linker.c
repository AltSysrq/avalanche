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
#include <assert.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/map.h"
#include "avalanche/integer.h"
#include "avalanche/pcode.h"
#include "avalanche/errors.h"
#include "avalanche/name-mangle.h"
#include "avalanche/pcode-linker.h"
#include "../bsd.h"

typedef struct ava_pcode_linker_entry_s {
  ava_string name;
  const ava_pcode_global_list* pcode;
  ava_bool consumed;

  RB_ENTRY(ava_pcode_linker_entry_s) map;
} ava_pcode_linker_entry;

static signed ava_compare_pcode_linker_entry(
  const ava_pcode_linker_entry* a, const ava_pcode_linker_entry* b);

RB_HEAD(ava_pcode_linker_map, ava_pcode_linker_entry_s);
RB_PROTOTYPE_STATIC(ava_pcode_linker_map, ava_pcode_linker_entry_s, map,
                    ava_compare_pcode_linker_entry);
RB_GENERATE_STATIC(ava_pcode_linker_map, ava_pcode_linker_entry_s, map,
                   ava_compare_pcode_linker_entry);

struct ava_pcode_linker_s {
  struct ava_pcode_linker_map packages, modules;

  /* If a module or package is added more than once, its name is recorded here
   * so that ava_pcode_linker_link() can return an error.
   */
  ava_string duplicate_name;
};

static size_t ava_pcode_global_length(const ava_pcode_global_list* pcode);
static ava_bool ava_pcode_keep_in_interface(
  const ava_pcode_global* elt, size_t ix,
  const unsigned char*restrict exported);

static size_t ava_pcode_linker_concat_size(const ava_pcode_linker* this);
static ava_pcode_global** ava_pcode_linker_concat_all(
  size_t* length, ava_pcode_linker* this, ava_compile_error_list* errors);
static ava_bool ava_pcode_linker_concat_package(
  ava_pcode_global** dst,
  ava_pcode_linker* this, ava_string package_name,
  size_t* offset, ava_compile_error_list* errors);
static ava_bool ava_pcode_linker_concat_module(
  ava_pcode_global** dst,
  ava_pcode_linker* this, ava_string module_name,
  size_t* offset, ava_compile_error_list* errors);
static void ava_pcode_linker_concat_object(
  ava_pcode_global** dst,
  ava_pcode_linker* this, const ava_pcode_global_list* src,
  size_t* offset, ava_compile_error_list* errors);
static void ava_pcode_linker_clone_fun_body(
  ava_pcg_fun* fun, size_t object_base);
static ava_map_value ava_pcode_linker_select_canonical(
  ava_pcode_global*const* pcode, size_t length,
  ava_compile_error_list* errors);
static void ava_pcode_linker_relink_canonical(
  ava_pcode_global*const* pcode, size_t length,
  ava_map_value canonical_indices);
static void ava_pcode_linker_delete_noncanonical(
  ava_pcode_global** pcode, size_t length,
  ava_map_value canonical_indices);
static ava_pcode_global_list* ava_pcode_linker_compact(
  ava_pcode_global*const* pcode, size_t length);

static ava_bool ava_pcode_linker_participates_in_linkage(
  const ava_pcode_global* elt);
static void ava_pcode_linker_unknown_location(
  ava_compile_location* location);
static void ava_pcode_linker_set_location(
  ava_compile_location* location, const ava_pcode_global* elt);
static ava_integer ava_pcode_linker_get_canonical(
  ava_pcode_global*const* pcode, ava_integer ref,
  ava_map_value canonical_indices);

ava_pcode_global_list* ava_pcode_to_interface(
  const ava_pcode_global_list* src
) {
  size_t src_length = ava_pcode_global_length(src);
  size_t* index_map, src_ix, dst_ix, i;
  ava_integer global_ref;
  unsigned char* exported;
  ava_pcode_global_list* dst;
  const ava_pcode_global* src_elt;
  ava_pcode_global* dst_elt;

  index_map = ava_alloc_atomic(sizeof(size_t) * src_length);
  exported = ava_alloc_atomic(src_length);

  /* Find out what elements have been exported */
  TAILQ_FOREACH(src_elt, src, next) {
    if (ava_pcgt_export == src_elt->type) {
      const ava_pcg_export* export = (const ava_pcg_export*)src_elt;
      assert(export->global >= 0 && (size_t)export->global < src_length);
      exported[export->global] = 1;
    }
  }

  dst = AVA_NEW(ava_pcode_global_list);
  TAILQ_INIT(dst);

  /* Copy all the elements we want to keep, changing fun and var into ext-fun
   * and ext-var appropriately.
   */
  src_ix = dst_ix = 0;
  TAILQ_FOREACH(src_elt, src, next) {
    index_map[dst_ix++] = src_ix;
    if (ava_pcode_keep_in_interface(src_elt, src_ix, exported)) {
      switch (src_elt->type) {
      case ava_pcgt_fun: {
        const ava_pcg_fun* sfun = (const ava_pcg_fun*)src_elt;
        ava_pcg_ext_fun* dfun = AVA_NEW(ava_pcg_ext_fun);

        dfun->name = sfun->name;
        dfun->prototype = sfun->prototype;
        dst_elt = (ava_pcode_global*)dfun;
      } break;

      case ava_pcgt_var: {
        const ava_pcg_var* svar = (const ava_pcg_var*)src_elt;
        ava_pcg_ext_var* dvar = AVA_NEW(ava_pcg_ext_var);

        dvar->name = svar->name;
        dst_elt = (ava_pcode_global*)dvar;
      } break;

      default:
        dst_elt = ava_pcode_global_clone(src_elt);
        break;
      }

      TAILQ_INSERT_TAIL(dst, dst_elt, next);
      ++dst_ix;
    }
  }

  /* Fix global references */
  TAILQ_FOREACH(dst_elt, dst, next) {
    for (i = 0;
         ava_pcode_global_get_global_entity_ref(&global_ref, dst_elt, i);
         ++i) {
      ava_pcode_global_set_global_entity_ref(
        dst_elt, i, index_map[global_ref]);
    }
  }

  return dst;
}

static size_t ava_pcode_global_length(const ava_pcode_global_list* pcode) {
  size_t len;
  const ava_pcode_global* elt;

  len = 0;
  TAILQ_FOREACH(elt, pcode, next) ++len;
  return len;
}

static ava_bool ava_pcode_keep_in_interface(
  const ava_pcode_global* elt, size_t ix,
  const unsigned char*restrict exported
) {
  const ava_pcode_global* other;

  switch (elt->type) {
  case ava_pcgt_src_pos:
    /* Only keep if there's something else we'll be keeping following it that
     * isn't another src-pos.
     */
    for (other = TAILQ_NEXT(elt, next); other; other = TAILQ_NEXT(other, next)) {
      ++ix;
      if (ava_pcgt_src_pos == other->type)
        return ava_false;
      else if (ava_pcode_keep_in_interface(other, ix, exported))
        return ava_true;
    }

    return ava_false;

  case ava_pcgt_ext_var:
  case ava_pcgt_ext_fun:
  case ava_pcgt_var:
  case ava_pcgt_fun:
    return exported[ix];

  case ava_pcgt_export:
  case ava_pcgt_macro:
    return ava_true;

  case ava_pcgt_load_pkg:
  case ava_pcgt_load_mod:
  case ava_pcgt_init:
    return ava_false;
  }

  /* unreachable */
  abort();
}

static signed ava_compare_pcode_linker_entry(
  const ava_pcode_linker_entry* a, const ava_pcode_linker_entry* b
) {
  return ava_strcmp(a->name, b->name);
}

ava_pcode_linker* ava_pcode_linker_new(void) {
  ava_pcode_linker* this = AVA_NEW(ava_pcode_linker);

  RB_INIT(&this->modules);
  RB_INIT(&this->packages);
  this->duplicate_name = AVA_ABSENT_STRING;

  return this;
}

void ava_pcode_linker_add_module(
  ava_pcode_linker* this,
  ava_string module_name,
  const ava_pcode_global_list* module
) {
  ava_pcode_linker_entry* entry = AVA_NEW(ava_pcode_linker_entry);

  entry->name = module_name;
  entry->pcode = module;
  entry->consumed = ava_false;

  if (RB_INSERT(ava_pcode_linker_map, &this->modules, entry))
    this->duplicate_name = module_name;
}

void ava_pcode_linker_add_package(
  ava_pcode_linker* this,
  ava_string package_name,
  const ava_pcode_global_list* package
) {
  ava_pcode_linker_entry* entry = AVA_NEW(ava_pcode_linker_entry);

  entry->name = package_name;
  entry->pcode = package;
  entry->consumed = ava_false;

  if (RB_INSERT(ava_pcode_linker_map, &this->packages, entry))
    this->duplicate_name = package_name;
}

ava_pcode_global_list* ava_pcode_linker_link(
  ava_pcode_linker* this,
  ava_compile_error_list* errors
) {
  ava_pcode_global** concatted;
  size_t concat_size;
  ava_map_value canonical_indices;

  /* Dump everything into one bucket in dependency order */
  concatted = ava_pcode_linker_concat_all(&concat_size, this, errors);
  /* For elements participating in linkage, select one element per name to
   * represent everything in that name, update all references to point to it,
   * and then delete all the duplicates.
   */
  canonical_indices = ava_pcode_linker_select_canonical(
    concatted, concat_size, errors);
  ava_pcode_linker_relink_canonical(concatted, concat_size, canonical_indices);
  ava_pcode_linker_delete_noncanonical(
    concatted, concat_size, canonical_indices);

  if (!TAILQ_EMPTY(errors))
    return NULL;

  /* Compact the array (removing NULLs), relinking as necessary, and link
   * everything together into a list.
   */
  return ava_pcode_linker_compact(concatted, concat_size);
}

static size_t ava_pcode_linker_concat_size(const ava_pcode_linker* this) {
  size_t sum = 0;
  ava_pcode_linker_entry* entry;

  RB_FOREACH(entry, ava_pcode_linker_map,
             (struct ava_pcode_linker_map*)&this->packages)
    sum += ava_pcode_global_length(entry->pcode);
  RB_FOREACH(entry, ava_pcode_linker_map,
             (struct ava_pcode_linker_map*)&this->modules)
    sum += ava_pcode_global_length(entry->pcode);

  return sum;
}

static ava_pcode_global** ava_pcode_linker_concat_all(
  size_t* out_length,
  ava_pcode_linker* this, ava_compile_error_list* errors
) {
  ava_pcode_global** dst;
  size_t offset, length;
  ava_pcode_linker_entry* entry;

  offset = 0;
  *out_length = length = ava_pcode_linker_concat_size(this);
  dst = ava_alloc(sizeof(ava_pcode_global*) * length);

  RB_FOREACH(entry, ava_pcode_linker_map, &this->packages)
    ava_pcode_linker_concat_package(
      dst, this, entry->name, &offset, errors);

  RB_FOREACH(entry, ava_pcode_linker_map, &this->modules)
    ava_pcode_linker_concat_module(
      dst, this, entry->name, &offset, errors);

  assert(offset == length);
  return dst;
}

static ava_bool ava_pcode_linker_concat_package(
  ava_pcode_global** dst,
  ava_pcode_linker* this, ava_string package_name,
  size_t* offset, ava_compile_error_list* errors
) {
  ava_pcode_linker_entry exemplar, * found;

  exemplar.name = package_name;
  found = RB_FIND(ava_pcode_linker_map, &this->packages, &exemplar);
  if (found && !found->consumed) {
    found->consumed = ava_true;
    ava_pcode_linker_concat_object(dst, this, found->pcode, offset, errors);
  }

  return !!found;
}

static ava_bool ava_pcode_linker_concat_module(
  ava_pcode_global** dst,
  ava_pcode_linker* this, ava_string module_name,
  size_t* offset, ava_compile_error_list* errors
) {
  ava_pcode_linker_entry exemplar, * found;

  exemplar.name = module_name;
  found = RB_FIND(ava_pcode_linker_map, &this->modules, &exemplar);
  if (found && !found->consumed) {
    found->consumed = ava_true;
    ava_pcode_linker_concat_object(dst, this, found->pcode, offset, errors);
  }

  return !!found;
}

static void ava_pcode_linker_concat_object(
  ava_pcode_global** dst,
  ava_pcode_linker* this, const ava_pcode_global_list* pcode,
  size_t* offset, ava_compile_error_list* errors
) {
  size_t object_base, i, ix, srclen;
  unsigned char* discard;
  ava_integer global_ref;
  ava_bool reexport;
  const ava_pcode_global* src_elt;
  ava_pcode_global* dst_elt;

  srclen = ava_pcode_global_length(pcode);
  discard = ava_alloc_atomic(srclen);

  /* Check for load-pkg and load-mod globals that resolve, and mark them as
   * discarded.
   */
  ix = 0;
  TAILQ_FOREACH(src_elt, pcode, next) {
    if (ava_pcgt_load_pkg == src_elt->type) {
      const ava_pcg_load_pkg* lp = (const ava_pcg_load_pkg*)src_elt;
      if (ava_pcode_linker_concat_package(dst, this, lp->name, offset, errors))
        discard[ix] = 1;
    }
    ++ix;
  }
  ix = 0;
  TAILQ_FOREACH(src_elt, pcode, next) {
    if (ava_pcgt_load_mod == src_elt->type) {
      const ava_pcg_load_mod* lm = (const ava_pcg_load_mod*)src_elt;
      if (ava_pcode_linker_concat_module(dst, this, lm->name, offset, errors))
        discard[ix] = 1;
    }
    ++ix;
  }

  /* Copy pcode into the dst array. Discarded elements are inserted as NULL so
   * that the more complicated compaction step can happen later, all at once.
   */
  object_base = *offset;
  ix = 0;
  TAILQ_FOREACH(src_elt, pcode, next) {
    /* Discard non-reexported exports and elements already marked above */
    if (discard[ix] ||
        (ava_pcode_global_get_reexport(&reexport, src_elt, 0) &&
         !reexport)) {
      dst_elt = NULL;
    } else {
      dst_elt = ava_pcode_global_clone(src_elt);
      /* Relink global refs */
      for (i = 0;
           ava_pcode_global_get_global_entity_ref(&global_ref, dst_elt, i);
           ++i) {
        ava_pcode_global_set_global_entity_ref(
          dst_elt, i, global_ref + object_base);
      }

      /* If this is a function, its body must be cloned and global references
       * adjusted.
       */
      if (ava_pcgt_fun == dst_elt->type)
        ava_pcode_linker_clone_fun_body((ava_pcg_fun*)dst_elt, object_base);
    }

    dst[*offset++] = dst_elt;
    ++ix;
  }
}

static void ava_pcode_linker_clone_fun_body(
  ava_pcg_fun* fun, size_t object_base
) {
  const ava_pcode_exe* src_elt;
  ava_pcode_exe* dst_elt;
  ava_pcode_exe_list* dst;
  unsigned i;
  ava_integer global_ref;

  dst = AVA_NEW(ava_pcode_exe_list);
  TAILQ_INIT(dst);

  TAILQ_FOREACH(src_elt, fun->body, next) {
    dst_elt = ava_pcode_exe_clone(dst_elt);
    for (i = 0; ava_pcode_exe_get_global_ref(&global_ref, dst_elt, i); ++i)
      ava_pcode_exe_set_global_ref(dst_elt, i, global_ref + object_base);

    TAILQ_INSERT_TAIL(dst, dst_elt, next);
  }

  fun->body = dst;
}

static ava_map_value ava_pcode_linker_select_canonical(
  ava_pcode_global*const* pcode, size_t length,
  ava_compile_error_list* errors
) {
  ava_compile_location location;
  ava_map_value canonical_indices = ava_empty_map();
  ava_map_cursor cursor;
  ava_demangled_name demangled_name;
  ava_string mangled_name;
  size_t i;

  /* Look for definitions */
  ava_pcode_linker_unknown_location(&location);
  for (i = 0; i < length; ++i) {
    if (pcode[i]) {
      ava_pcode_linker_set_location(&location, pcode[i]);
      if (ava_pcode_global_is_linkage_definition(pcode[i]) &&
          ava_pcode_linker_participates_in_linkage(pcode[i])) {
        if (!ava_pcode_global_get_linkage_name(&demangled_name, pcode[i], 0))
          abort();

        mangled_name = ava_name_mangle(demangled_name);
        cursor = ava_map_find(
          canonical_indices, ava_value_of_string(mangled_name));
        if (AVA_MAP_CURSOR_NONE != cursor) {
          TAILQ_INSERT_TAIL(
            errors, ava_error_linker_symbol_redefined(
              &location, demangled_name.name),
            next);
        } else {
          canonical_indices = ava_map_add(
            canonical_indices,
            ava_value_of_string(mangled_name),
            ava_value_of_integer(i));
        }
      }
    }
  }

  /* For everything else, the first occurrance wins, unless there's already a
   * definition found above.
   */
  ava_pcode_linker_unknown_location(&location);
  for (i = 0; i < length; ++i) {
    if (pcode[i]) {
      ava_pcode_linker_set_location(&location, pcode[i]);
      if (ava_pcode_linker_participates_in_linkage(pcode[i])) {
        if (!ava_pcode_global_get_linkage_name(&demangled_name, pcode[i], 0))
          abort();

        mangled_name = ava_name_mangle(demangled_name);
        cursor = ava_map_find(
          canonical_indices, ava_value_of_string(mangled_name));
        if (AVA_MAP_CURSOR_NONE == cursor)
          canonical_indices = ava_map_add(
            canonical_indices,
            ava_value_of_string(mangled_name),
            ava_value_of_integer(i));
      }
    }
  }

  return canonical_indices;
}

static ava_bool ava_pcode_linker_participates_in_linkage(
  const ava_pcode_global* elt
) {
  ava_bool published;

  if (ava_pcode_global_is_effectively_published(elt))
    return ava_true;

  if (ava_pcode_global_get_publish(&published, elt, 0))
    return published;

  return ava_false;
}

static void ava_pcode_linker_relink_canonical(
  ava_pcode_global*const* pcode, size_t length,
  ava_map_value canonical_indices
) {
  size_t i, j;
  ava_integer ref;
  ava_pcode_global* src_glob;
  ava_pcode_exe* src_exe;

  for (i = 0; i < length; ++i) {
    src_glob = pcode[i];
    if (!src_glob) continue;

    for (j = 0;
         ava_pcode_global_get_global_entity_ref(&ref, src_glob, j);
         ++j) {
      assert(ref >= 0 && (size_t)ref < length);
      ava_pcode_global_set_global_entity_ref(
        src_glob, j, ava_pcode_linker_get_canonical(
          pcode, ref, canonical_indices));
    }

    /* For functions, must also relink all the global refs within */
    if (ava_pcgt_fun == src_glob->type) {
      ava_pcg_fun* fun = (ava_pcg_fun*)src_glob;
      TAILQ_FOREACH(src_exe, fun->body, next) {
        for (j = 0; ava_pcode_exe_get_global_ref(&ref, src_exe, j); ++j) {
          assert(ref >= 0 && (size_t)ref < length);
          ava_pcode_exe_set_global_ref(
            src_exe, j, ava_pcode_linker_get_canonical(
              pcode, ref, canonical_indices));
        }
      }
    }
  }
}

static ava_integer ava_pcode_linker_get_canonical(
  ava_pcode_global*const* pcode, ava_integer ref,
  ava_map_value canonical_indices
) {
  const ava_pcode_global* target_glob;
  ava_demangled_name demangled_name;
  ava_string mangled_name;
  ava_map_cursor cursor;
  ava_integer canonical;

  target_glob = pcode[ref];

  if (!ava_pcode_linker_participates_in_linkage(target_glob))
    return ref;

  if (!ava_pcode_global_get_linkage_name(
        &demangled_name, target_glob, 0))
    abort();
  mangled_name = ava_name_mangle(demangled_name);
  cursor = ava_map_find(canonical_indices, ava_value_of_string(mangled_name));
  assert(AVA_MAP_CURSOR_NONE != cursor);
  canonical = ava_integer_of_value(
    ava_map_get(canonical_indices, cursor), -1);
  assert(canonical >= 0);
  return canonical;
}

static void ava_pcode_linker_delete_noncanonical(
  ava_pcode_global** pcode, size_t length,
  ava_map_value canonical_indices
) {
  size_t i;

  for (i = 0; i < length; ++i) {
    if (!pcode[i]) continue;

    if (ava_pcode_linker_participates_in_linkage(pcode[i]) &&
        i != (size_t)ava_pcode_linker_get_canonical(
          pcode, i, canonical_indices))
      pcode[i] = NULL;
  }
}

static ava_pcode_global_list* ava_pcode_linker_compact(
  ava_pcode_global*const* pcode, size_t length
) {
  size_t* index_map, src_ix, dst_ix, dst_length, i;
  ava_pcode_global_list* dst;
  ava_integer ref;

  index_map = ava_alloc_atomic(sizeof(size_t) * length);
  dst = AVA_NEW(ava_pcode_global_list);
  TAILQ_INIT(dst);

  for (src_ix = dst_ix = 0; src_ix < length; ++src_ix) {
    if (pcode[src_ix]) {
      index_map[src_ix] = dst_ix++;
    } else {
      index_map[src_ix] = ~(size_t)0u;
    }
  }
  dst_length = dst_ix;

  for (src_ix = 0; src_ix < length; ++src_ix) {
    if (!pcode[src_ix]) continue;

    TAILQ_INSERT_TAIL(dst, pcode[src_ix], next);
    /* Relink global refs */
    for (i = 0;
         ava_pcode_global_get_global_entity_ref(&ref, pcode[src_ix], i);
         ++i) {
      assert(ref >= 0 && (size_t)ref < length);
      assert(index_map[ref] < dst_length);
      ava_pcode_global_set_global_entity_ref(
        pcode[src_ix], i, index_map[ref]);
    }

    if (ava_pcgt_fun == pcode[src_ix]->type) {
      ava_pcg_fun* fun = (ava_pcg_fun*)pcode[src_ix];
      ava_pcode_exe* exe;

      TAILQ_FOREACH(exe, fun->body, next) {
        for (i = 0; ava_pcode_exe_get_global_ref(&ref, exe, i); ++i) {
          assert(ref >= 0 && (size_t)ref < length);
          assert(index_map[ref] < dst_length);
          ava_pcode_exe_set_global_ref(exe, i, index_map[ref]);
        }
      }
    }
  }

  return dst;
}

static void ava_pcode_linker_unknown_location(
  ava_compile_location* location
) {
  AVA_STATIC_STRING(init_filename, "<linker-input>");

  location->filename = init_filename;
  location->source = AVA_ABSENT_STRING;
  location->line_offset = 0;
  location->start_line = 0;
  location->end_line = 0;
  location->start_column = 0;
  location->end_column = 0;
}

static void ava_pcode_linker_set_location(
  ava_compile_location* location,
  const ava_pcode_global* global
) {
  const ava_pcg_src_pos* pos;

  if (ava_pcgt_src_pos != global->type) return;

  pos = (const ava_pcg_src_pos*)global;
  location->filename = pos->filename;
  location->line_offset = pos->line_offset;
  location->start_line = pos->start_line;
  location->end_line = pos->end_line;
  location->start_column = pos->start_column;
  location->end_column = pos->end_column;
}
