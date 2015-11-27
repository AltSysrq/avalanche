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

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
#include "../avalanche/alloc.h"
#include "../avalanche/string.h"
#include "../avalanche/value.h"
#include "../avalanche/macsub.h"
#include "../avalanche/macro-arg.h"
#include "../avalanche/symbol.h"
#include "../avalanche/symtab.h"
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/errors.h"
#include "pasta.h"

/**
 * Type for label symbols.
 *
 * The userdata on the symbols in NULL except within the codegen context of the
 * pasta block, where it is set to a const ava_uint* pointing to the label.
 */
static const ava_symbol_other_type ava_intr_pasta_label_type = {
  .name = "pasta label"
};

typedef struct {
  /* The label, if any */
  ava_symbol* symbol;
  ava_ast_node* body;
} ava_intr_pasta_clause;

typedef struct {
  ava_ast_node header;

  size_t num_clauses;
  ava_intr_pasta_clause clauses[];
} ava_intr_pasta;

static ava_string ava_intr_pasta_to_string(const ava_intr_pasta* node);
static void ava_intr_pasta_postprocess(ava_intr_pasta* node);
static void ava_intr_pasta_cg_discard(
  ava_intr_pasta* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_pasta_vtable = {
  .name = "pasta",
  .to_string = (ava_ast_node_to_string_f)ava_intr_pasta_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_pasta_postprocess,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_pasta_cg_discard,
};

ava_macro_subst_result ava_intr_pasta_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  ava_intr_pasta* this;
  const ava_parse_unit* unit, * label_unit, * body_unit;
  ava_symbol* symbol;
  ava_string label_name;
  size_t num_args, num_clauses, clause;
  ava_bool first_clause_unlabelled;

  num_args = 0;
  for (unit = TAILQ_NEXT(provoker, next); unit; unit = TAILQ_NEXT(unit, next))
    ++num_args;

  num_clauses = (num_args + 1) / 2;
  first_clause_unlabelled = num_args & 1;
  this = ava_alloc(sizeof(ava_intr_pasta) +
                   sizeof(ava_intr_pasta_clause) * num_clauses);
  this->header.v = &ava_intr_pasta_vtable;
  this->header.location = provoker->location;
  this->header.context = context;
  this->num_clauses = num_clauses;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      if (first_clause_unlabelled) {
        AVA_MACRO_ARG_BLOCK(unit, "body");
        this->clauses[0].symbol = NULL;
        this->clauses[0].body = ava_macsub_run_contents(context, unit);
      }

      for (clause = first_clause_unlabelled;
           clause < num_clauses;
           ++clause) {
        AVA_MACRO_ARG_CURRENT_UNIT(label_unit, "label");
        AVA_MACRO_ARG_BAREWORD(label_name, "label");
        AVA_MACRO_ARG_BLOCK(body_unit, "body");
        symbol = this->clauses[clause].symbol = AVA_NEW(ava_symbol);
        symbol->type = ava_st_other;
        symbol->full_name = ava_macsub_apply_prefix(context, label_name);
        symbol->level = ava_macsub_get_level(context);
        symbol->visibility = ava_v_private;
        symbol->v.other.type = &ava_intr_pasta_label_type;
        ava_macsub_put_symbol(context, symbol, &label_unit->location);
        this->clauses[clause].body =
          ava_macsub_run_contents(context, body_unit);
      }
    }
  }

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static ava_string ava_intr_pasta_to_string(const ava_intr_pasta* this) {
  ava_string accum;
  size_t i;

  accum = AVA_ASCII9_STRING("pasta {");
  for (i = 0; i < this->num_clauses; ++i) {
    if (this->clauses[i].symbol) {
      accum = ava_strcat(accum, this->clauses[i].symbol->full_name);
      accum = ava_strcat(accum, AVA_ASCII9_STRING(": "));
    }
    accum = ava_strcat(
      accum, ava_ast_node_to_string(this->clauses[i].body));
    accum = ava_strcat(accum, AVA_ASCII9_STRING("; "));
  }
  accum = ava_strcat(accum, AVA_ASCII9_STRING(" }"));

  return accum;
}

static void ava_intr_pasta_postprocess(ava_intr_pasta* this) {
  size_t i;

  for (i = 0; i < this->num_clauses; ++i)
    ava_ast_node_postprocess(this->clauses[i].body);
}

static void ava_intr_pasta_cg_discard(
  ava_intr_pasta* this, ava_codegen_context* context
) {
  ava_uint labels[this->num_clauses];
  size_t i;

  for (i = 0; i < this->num_clauses; ++i) {
    if (this->clauses[i].symbol) {
      labels[i] = ava_codegen_genlabel(context);
      this->clauses[i].symbol->v.other.userdata = labels + i;
    }
  }

  for (i = 0; i < this->num_clauses; ++i) {
    if (this->clauses[i].symbol)
      AVA_PCXB(label, labels[i]);

    ava_ast_node_cg_discard(this->clauses[i].body, context);
  }

  for (i = 0; i < this->num_clauses; ++i)
    if (this->clauses[i].symbol)
      this->clauses[i].symbol->v.other.userdata = NULL;
}

typedef struct {
  ava_ast_node header;

  ava_string target_name;
  const ava_symbol* target;
  ava_compile_location target_location;
} ava_intr_goto;

static ava_string ava_intr_goto_to_string(const ava_intr_goto* node);
static void ava_intr_goto_postprocess(ava_intr_goto* node);
static void ava_intr_goto_cg_discard(
  ava_intr_goto* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_goto_vtable = {
  .name = "goto",
  .to_string = (ava_ast_node_to_string_f)ava_intr_goto_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_goto_postprocess,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_goto_cg_discard,
};

ava_macro_subst_result ava_intr_goto_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  ava_intr_goto* this;
  const ava_parse_unit* target_unit = NULL;
  ava_string target_name;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_CURRENT_UNIT(target_unit, "target");
      AVA_MACRO_ARG_BAREWORD(target_name, "target");
    }
  }

  this = AVA_NEW(ava_intr_goto);
  this->header.v = &ava_intr_goto_vtable;
  this->header.location = provoker->location;
  this->header.context = context;
  this->target_name = target_name;
  this->target_location = target_unit->location;

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };

}

static void ava_intr_goto_postprocess(ava_intr_goto* this) {
  ava_macsub_context* context = this->header.context;
  const ava_symbol** symbols;
  size_t num_symbols;

  if (this->target) return;

  num_symbols = ava_symtab_get(&symbols, ava_macsub_get_symtab(context),
                               this->target_name);
  if (0 == num_symbols) {
    ava_macsub_record_error(
      context, ava_error_no_such_label(&this->target_location,
                                       this->target_name));
    return;
  }

  if (num_symbols > 1) {
    ava_macsub_record_error(
      context, ava_error_ambiguous_label(&this->target_location,
                                         this->target_name));
    return;
  }

  switch (symbols[0]->type) {
  case ava_st_global_variable:
  case ava_st_local_variable:
    ava_macsub_record_error(
      context, ava_error_use_of_other_as_label(
        &this->target_location, symbols[0]->full_name,
        AVA_ASCII9_STRING("variable")));
    return;

  case ava_st_global_function:
  case ava_st_local_function:
    ava_macsub_record_error(
      context, ava_error_use_of_other_as_label(
        &this->target_location, symbols[0]->full_name,
        AVA_ASCII9_STRING("function")));
    return;

  case ava_st_control_macro:
  case ava_st_operator_macro:
  case ava_st_function_macro:
    ava_macsub_record_error(
      context, ava_error_use_of_other_as_label(
        &this->target_location, symbols[0]->full_name,
        AVA_ASCII9_STRING("macro")));
    return;

  case ava_st_other:
    if (&ava_intr_pasta_label_type == symbols[0]->v.other.type)
      break;

    ava_macsub_record_error(
      context, ava_error_use_of_other_as_label(
        &this->target_location, symbols[0]->full_name,
        ava_string_of_cstring(symbols[0]->v.other.type->name)));
    return;
  }

  if (ava_macsub_get_level(context) != symbols[0]->level) {
    ava_macsub_record_error(
      context, ava_error_use_of_label_in_enclosing_scope(
        &this->target_location));
    return;
  }

  this->target = symbols[0];
}

static ava_string ava_intr_goto_to_string(const ava_intr_goto* this) {
  return ava_strcat(AVA_ASCII9_STRING("goto "),
                           this->target? this->target->full_name :
                           this->target_name);
}

static void ava_intr_goto_cg_discard(
  ava_intr_goto* this, ava_codegen_context* context
) {
  if (!this->target->v.other.userdata) {
    ava_codegen_error(
      context, (ava_ast_node*)this,
      ava_error_use_of_inaccessible_label(&this->target_location));
  } else {
    ava_codegen_goto(context, &this->header.location,
                     *(const ava_uint*)this->target->v.other.userdata);
  }
}
