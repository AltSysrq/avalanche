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

#include <assert.h>
#include <stdlib.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/symbol.h"
#include "avalanche/varscope.h"
#include "bsd.h"

typedef struct ava_varscope_varref_s {
  const ava_symbol* var;
  ava_uint offset;

  RB_ENTRY(ava_varscope_varref_s) map;
} ava_varscope_varref;

typedef struct ava_varscope_scoperef_s {
  ava_varscope* referrer;

  SLIST_ENTRY(ava_varscope_scoperef_s) next;
} ava_varscope_scoperef;

static signed ava_compare_varscope_varref(
  const ava_varscope_varref* a, const ava_varscope_varref* b
) {
  return (a->var > b->var) - (a->var < b->var);
}

SLIST_HEAD(ava_varscope_referrer_list, ava_varscope_scoperef_s);
RB_HEAD(ava_varscope_map, ava_varscope_varref_s);
RB_PROTOTYPE_STATIC(ava_varscope_map, ava_varscope_varref_s, map,
                    ava_compare_varscope_varref);
RB_GENERATE_STATIC(ava_varscope_map, ava_varscope_varref_s, map,
                   ava_compare_varscope_varref);

struct ava_varscope_s {
  struct ava_varscope_referrer_list referrers;

  size_t num_captures;
  struct ava_varscope_map captures;
  size_t num_locals;
  struct ava_varscope_map locals;
};

ava_varscope* ava_varscope_new(void) {
  ava_varscope* this = AVA_NEW(ava_varscope);

  SLIST_INIT(&this->referrers);
  this->num_captures = 0;
  RB_INIT(&this->captures);
  this->num_locals = 0;
  RB_INIT(&this->locals);
  return this;
}

void ava_varscope_put_local(
  ava_varscope* this, const ava_symbol* var
) {
  ava_varscope_varref* ref;

  ref = AVA_NEW(ava_varscope_varref);
  ref->var = var;
  ref->offset = this->num_locals;
  if (!RB_INSERT(ava_varscope_map, &this->locals, ref))
    ++this->num_locals;
  else
    /* Violated "must not already be in a varscope" */
    abort();
}

void ava_varscope_ref_var(
  ava_varscope* this, const ava_symbol* var
) {
  const ava_varscope_scoperef* referrer;
  ava_varscope_varref exemplar, * new;
  const ava_varscope_varref* existing;

  assert(ava_st_local_variable == var->type);

  exemplar.var = var;
  existing = RB_FIND(ava_varscope_map, &this->locals, &exemplar);
  if (existing) return;
  existing = RB_FIND(ava_varscope_map, &this->captures, &exemplar);
  if (existing) return;

  /* Not local or already captured, need to capture it */
  new = AVA_NEW(ava_varscope_varref);
  new->var = var;
  new->offset = this->num_captures++;
  RB_INSERT(ava_varscope_map, &this->captures, new);

  /* Referrers must also reference this var */
  SLIST_FOREACH(referrer, &this->referrers, next)
    ava_varscope_ref_var(referrer->referrer, var);
}

void ava_varscope_ref_scope(
  ava_varscope* referrer, ava_varscope* referrent
) {
  ava_varscope_scoperef* scoperef;
  ava_varscope_varref* varref;

  if (referrent == referrer) return;
  if (!referrent) return;

  /* Check whether this entry already exists */
  SLIST_FOREACH(scoperef, &referrent->referrers, next)
    if (referrer == scoperef->referrer)
      return;

  /* No existing entry, create */
  scoperef = AVA_NEW(ava_varscope_scoperef);
  scoperef->referrer = referrer;
  SLIST_INSERT_HEAD(&referrent->referrers, scoperef, next);

  /* Referrer needs to reference all of the referrent's captures */
  RB_FOREACH(varref, ava_varscope_map, &referrent->captures)
    ava_varscope_ref_var(referrer, varref->var);
}

ava_uint ava_varscope_get_index(
  const ava_varscope* scope, const ava_symbol* var
) {
  ava_varscope_varref exemplar;
  const ava_varscope_varref* existing;

  assert(ava_st_local_variable == var->type);

  exemplar.var = var;
  existing = RB_FIND(ava_varscope_map, (struct ava_varscope_map*)&scope->locals,
                     &exemplar);
  if (existing) return scope->num_captures + existing->offset;

  existing = RB_FIND(ava_varscope_map, (struct ava_varscope_map*)&scope->captures,
                     &exemplar);
  if (existing) return existing->offset;

  abort();
}

size_t ava_varscope_num_captures(const ava_varscope* scope) {
  return scope? scope->num_captures : 0;
}

size_t ava_varscope_num_vars(const ava_varscope* scope) {
  return scope? scope->num_captures + scope->num_locals : 0;
}

void ava_varscope_get_vars(
  const ava_symbol** dst, const ava_varscope* src, size_t count
) {
  ava_varscope_varref* ref;

  if (!src) {
    assert(0 == count);
    return;
  }

  assert(count <= src->num_locals + src->num_captures);

  RB_FOREACH(ref, ava_varscope_map, (struct ava_varscope_map*)&src->captures)
    if (ref->offset < count)
      dst[ref->offset] = ref->var;

  RB_FOREACH(ref, ava_varscope_map, (struct ava_varscope_map*)&src->locals)
    if (ref->offset + src->num_captures < count)
      dst[ref->offset + src->num_captures] = ref->var;
}
