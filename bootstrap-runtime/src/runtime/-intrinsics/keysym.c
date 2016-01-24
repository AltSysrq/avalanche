/*-
 * Copyright (c) 2016, Jason Lingle
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
#include "../avalanche/defs.h"
#include "../avalanche/alloc.h"
#include "../avalanche/string.h"
#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"
#include "../avalanche/symbol.h"
#include "../avalanche/symtab.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/macro-arg.h"
#include "../avalanche/errors.h"
#include "fundamental.h"
#include "keysym.h"

AVA_DEF_MACRO_SUBST(ava_intr_keysym_subst) {
  const ava_parse_unit* keysym_unit = NULL;
  ava_string keysym = AVA_ABSENT_STRING;
  const ava_symtab* symtab;
  const ava_symbol** results;
  size_t num_results;
  ava_parse_unit* result_string;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_CURRENT_UNIT(keysym_unit, "keysym");
      AVA_MACRO_ARG_BAREWORD(keysym, "keysym");
    }
  }

  symtab = ava_macsub_get_symtab(context);
  num_results = ava_symtab_get(&results, symtab, keysym);

  if (0 == num_results) {
    return ava_macsub_error_result(
      context, ava_error_no_such_keysym(
        &keysym_unit->location, keysym));
  } else if (num_results > 1) {
    return ava_macsub_error_result(
      context, ava_error_ambiguous_keysym(
        &keysym_unit->location, keysym, num_results,
        results[0]->full_name, results[1]->full_name));
  } else if (ava_st_keysym != results[0]->type) {
    return ava_macsub_error_result(
      context, ava_error_not_a_keysym(
        &keysym_unit->location, results[0]->full_name,
        ava_symbol_type_name(results[0])));
  }

  result_string = AVA_NEW(ava_parse_unit);
  result_string->type = ava_put_astring;
  result_string->location = provoker->location;
  result_string->v.string = results[0]->v.keysym;

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v.node = ava_intr_unit(context, result_string),
  };
}

typedef struct {
  ava_ast_node header;

  ava_symbol* sym;
} ava_intr_defkeysym;

static ava_string ava_intr_defkeysym_to_string(const ava_intr_defkeysym* node);
static void ava_intr_defkeysym_cg_discard(
  ava_intr_defkeysym* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_defkeysym_vtable = {
  .name = "keysym definition",
  .to_string = (ava_ast_node_to_string_f)ava_intr_defkeysym_to_string,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_defkeysym_cg_discard,
};

AVA_DEF_MACRO_SUBST(ava_intr_defkeysym_subst) {
  const ava_parse_unit* top_subst, * keysym_kw, * keysym_name_unit;
  ava_string keysym_name;
  ava_intr_defkeysym* node;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_UNIT(top_subst, "keysym");
    }
  }

  if (ava_put_substitution != top_subst->type)
    goto invalid_syntax;
  if (!TAILQ_FIRST(&top_subst->v.statements))
    goto invalid_syntax;
  if (TAILQ_NEXT(TAILQ_FIRST(&top_subst->v.statements), next))
    goto invalid_syntax;

  keysym_kw = TAILQ_FIRST(&TAILQ_FIRST(&top_subst->v.statements)->units);
  if (!keysym_kw)
    goto invalid_syntax;
  if (ava_put_bareword != keysym_kw->type)
    goto invalid_syntax;
  if (!ava_string_equal(AVA_ASCII9_STRING("#keysym#"), keysym_kw->v.string))
    goto invalid_syntax;

  keysym_name_unit = TAILQ_NEXT(keysym_kw, next);
  if (!keysym_name_unit)
    goto invalid_syntax;
  if (ava_put_bareword != keysym_name_unit->type)
    goto invalid_syntax;
  if (TAILQ_NEXT(keysym_name_unit, next))
    goto invalid_syntax;

  keysym_name = keysym_name_unit->v.string;
  node = AVA_NEW(ava_intr_defkeysym);
  node->header.v = &ava_intr_defkeysym_vtable;
  node->header.location = provoker->location;
  node->header.context = context;
  node->sym = AVA_NEW(ava_symbol);
  node->sym->type = ava_st_keysym;
  node->sym->level = ava_macsub_get_level(context);
  node->sym->visibility = *(const ava_visibility*)self->v.macro.userdata;
  node->sym->full_name = ava_macsub_apply_prefix(context, keysym_name);
  node->sym->v.keysym = node->sym->full_name;
  ava_macsub_put_symbol(context, node->sym, &keysym_name_unit->location);

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v.node = (ava_ast_node*)node,
  };

  invalid_syntax:
  return ava_macsub_error_result(
    context, ava_error_defkeysym_invalid_syntax(
      &top_subst->location));
}

static ava_string ava_intr_defkeysym_to_string(const ava_intr_defkeysym* node) {
  ava_string accum;

  switch (node->sym->visibility) {
  case ava_v_private:  accum = AVA_ASCII9_STRING("keysym "); break;
  case ava_v_internal: accum = AVA_ASCII9_STRING("Keysym "); break;
  case ava_v_public:   accum = AVA_ASCII9_STRING("KEYSYM "); break;
  }

  accum = ava_strcat(accum, node->sym->v.keysym);
  return accum;
}

static void ava_intr_defkeysym_cg_discard(
  ava_intr_defkeysym* node, ava_codegen_context* context
) {
  if (node->sym->visibility > ava_v_private)
    AVA_PCGB(keysym, node->sym->full_name, node->sym->v.keysym,
             node->sym->visibility > ava_v_internal);
}
