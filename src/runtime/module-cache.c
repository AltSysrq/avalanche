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

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/module-cache.h"
#include "bsd.h"

typedef struct ava_pcode_global_list_s ava_pcode_global_list;

typedef struct ava_mmcache_entry_s {
  ava_string name;
  const ava_pcode_global_list* object;

  RB_ENTRY(ava_mmcache_entry_s) map;
} ava_mmcache_entry;

static signed ava_compare_mmcache_entry(
  const ava_mmcache_entry* a, const ava_mmcache_entry* b);

RB_HEAD(ava_mmcache_map, ava_mmcache_entry_s);
RB_PROTOTYPE_STATIC(ava_mmcache_map, ava_mmcache_entry_s, map,
                    ava_compare_mmcache_entry);
RB_GENERATE_STATIC(ava_mmcache_map, ava_mmcache_entry_s, map,
                   ava_compare_mmcache_entry);

typedef struct {
  ava_module_cache header;

  struct ava_mmcache_map map;
} ava_mmcache;

static const ava_pcode_global_list*
ava_mmcache_get(const ava_mmcache* this, ava_string name, ava_string* error);
static void
ava_mmcache_put(ava_mmcache* this, ava_string name,
                const ava_pcode_global_list* pcode);

const ava_pcode_global_list*
ava_module_cache_get(const ava_module_cache_stack* cache,
                     ava_string name,
                     ava_string* error) {
  ava_module_cache* elt;
  const ava_pcode_global_list* found;

  for (elt = LIST_FIRST(cache); elt; elt = LIST_NEXT(elt, next)) {
    *error = AVA_ABSENT_STRING;
    if (elt->get)
      found = (*elt->get)(elt, name, error);

    if (ava_string_is_present(*error)) return NULL;

    if (found) {
      for (elt = LIST_PREV(elt, cache, ava_module_cache_s, next); elt;
           elt = LIST_PREV(elt, cache, ava_module_cache_s, next))
        if (elt->put)
          (*elt->put)(elt, name, found);

      return found;
    }
  }

  return NULL;
}

void ava_module_cache_put(ava_module_cache_stack* cache,
                          ava_string name,
                          const ava_pcode_global_list* pcode) {
  ava_module_cache* elt;

  LIST_FOREACH(elt, cache, next)
    if (elt->put)
      (*elt->put)(elt, name, pcode);
}

static signed ava_compare_mmcache_entry(
  const ava_mmcache_entry* a, const ava_mmcache_entry* b
) {
  return ava_strcmp(a->name, b->name);
}


ava_module_cache* ava_memory_module_cache_new(void) {
  ava_mmcache* this;

  this = AVA_NEW(ava_mmcache);
  this->header.get = (ava_module_cache_get_f)ava_mmcache_get;
  this->header.put = (ava_module_cache_put_f)ava_mmcache_put;
  RB_INIT(&this->map);

  return (ava_module_cache*)this;
}

static const ava_pcode_global_list*
ava_mmcache_get(const ava_mmcache* this, ava_string name,
                ava_string* error) {
  ava_mmcache_entry exemplar, * found;

  exemplar.name = name;
  found = RB_FIND(ava_mmcache_map, (struct ava_mmcache_map*)&this->map,
                  &exemplar);
  if (found)
    return found->object;
  else
    return NULL;
}

static void
ava_mmcache_put(ava_mmcache* this, ava_string name,
                const ava_pcode_global_list* pcode) {
  ava_mmcache_entry* entry;

  entry = AVA_NEW(ava_mmcache_entry);
  entry->name = name;
  entry->object = pcode;
  RB_INSERT(ava_mmcache_map, &this->map, entry);
}
