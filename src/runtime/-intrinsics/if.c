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
#include "../avalanche/symbol.h"
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/errors.h"
#include "if.h"

typedef struct {
  /* NULL for the else clause */
  ava_ast_node* condition;
  ava_ast_node* result;
} ava_intr_if_clause;

typedef struct {
  ava_ast_node header;

  ava_bool expression_form;
  size_t num_clauses;
  ava_intr_if_clause clauses[];
} ava_intr_if;

static ava_string ava_intr_if_to_string(const ava_intr_if* node);
static void ava_intr_if_postprocess(ava_intr_if* node);
static void ava_intr_if_cg_common(
  ava_intr_if* node, const ava_pcode_register* dst,
  ava_codegen_context* context);
static void ava_intr_if_cg_evaluate(
  ava_intr_if* node, const ava_pcode_register* dst,
  ava_codegen_context* context);
static void ava_intr_if_cg_discard(
  ava_intr_if* node, ava_codegen_context* context);
static void ava_intr_if_cg_force(
  ava_intr_if* node, const ava_pcode_register* dst,
  ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_if_vtable = {
  .name = "if",
  .to_string = (ava_ast_node_to_string_f)ava_intr_if_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_if_postprocess,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_if_cg_evaluate,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_if_cg_discard,
  .cg_force = (ava_ast_node_cg_force_f)ava_intr_if_cg_force,
};

ava_macro_subst_result ava_intr_if_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  size_t num_args, num_clauses, ix;
  const ava_parse_unit* unit;
  ava_parse_unit* cond, * res;
  ava_intr_if* this;

  num_args = 0;
  for (unit = TAILQ_NEXT(provoker, next); unit; unit = TAILQ_NEXT(unit, next))
    ++num_args;

  if (num_args < 2)
    return ava_macsub_error_result(
      context, ava_error_macro_arg_missing(
        &TAILQ_LAST(&statement->units, ava_parse_unit_list_s)->location,
        self->full_name,
        AVA_ASCII9_STRING("result")));

  num_clauses = (1 + num_args) / 2;
  this = ava_alloc(sizeof(ava_intr_if) +
                   sizeof(ava_intr_if_clause) * num_clauses);
  this->header.v = &ava_intr_if_vtable;
  this->header.location = provoker->location;
  this->header.context = context;
  this->num_clauses = num_clauses;

  cond = TAILQ_NEXT(provoker, next);
  res = TAILQ_NEXT(cond, next);
  for (ix = 0; ix < num_clauses; ++ix) {
    if (cond) {
      if (ava_put_bareword == cond->type && ix > 0 && ix == num_clauses - 1) {
        if (ava_strcmp(AVA_ASCII9_STRING("else"), cond->v.string))
          return ava_macsub_error_result(
            context, ava_error_bad_macro_keyword(
              &cond->location,
              self->full_name, cond->v.string, AVA_ASCII9_STRING("else")));
        /* OK, explicit "else" condition" */
        this->clauses[ix].condition = NULL;
      } else if (ava_put_substitution != cond->type) {
        return ava_macsub_error_result(
          context, ava_error_macro_arg_must_be_substitution(
            &cond->location, AVA_ASCII9_STRING("condition")));
      } else {
        this->clauses[ix].condition = ava_macsub_run_units(context, cond, cond);
      }
    } else {
      /* Implicit "else" condition */
      if (num_clauses > 2 || !this->expression_form)
        return ava_macsub_error_result(
          context, ava_error_if_required_else_omitted(
            &res->location));

      this->clauses[ix].condition = NULL;
    }

    if (0 == ix) {
      this->expression_form = (ava_put_substitution == res->type);
    }

    if (ava_put_substitution == res->type) {
      if (!this->expression_form)
        return ava_macsub_error_result(
          context, ava_error_if_inconsistent_result_form(
            &res->location));

      this->clauses[ix].result = ava_macsub_run(
        context, &res->location, &res->v.statements, ava_isrp_last);
    } else if (ava_put_block == res->type) {
      if (this->expression_form)
        return ava_macsub_error_result(
          context, ava_error_if_inconsistent_result_form(
            &res->location));

      this->clauses[ix].result = ava_macsub_run(
        context, &res->location, &res->v.statements, ava_isrp_void);
    } else {
      return ava_macsub_error_result(
        context, ava_error_macro_arg_must_be_substitution_or_block(
          &res->location, AVA_ASCII9_STRING("result")));
    }

    cond = TAILQ_NEXT(res, next);
    if (cond) {
      res = TAILQ_NEXT(cond, next);
      if (!res) {
        res = cond;
        cond = NULL;
      }
    } else {
      res = NULL;
    }
  }

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static ava_string ava_intr_if_to_string(const ava_intr_if* this) {
  ava_string accum;
  size_t i;

  accum = AVA_ASCII9_STRING("if");
  for (i = 0; i < this->num_clauses; ++i) {
    accum = ava_string_concat(accum, AVA_ASCII9_STRING(" {"));
    if (this->clauses[i].condition) {
      accum = ava_string_concat(
        accum, ava_ast_node_to_string(this->clauses[i].condition));
    } else {
      accum = ava_string_concat(accum, AVA_ASCII9_STRING("else"));
    }
    accum = ava_string_concat(accum, AVA_ASCII9_STRING(" => "));
    accum = ava_string_concat(
      accum, ava_ast_node_to_string(this->clauses[i].result));
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("}"));
  }

  return accum;
}

static void ava_intr_if_postprocess(ava_intr_if* this) {
  size_t i;

  for (i = 0; i < this->num_clauses; ++i) {
    if (this->clauses[i].condition)
      ava_ast_node_postprocess(this->clauses[i].condition);
    ava_ast_node_postprocess(this->clauses[i].result);
  }
}

static void ava_intr_if_cg_common(
  ava_intr_if* this, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_uint endlbl, elselbl;
  size_t i;
  ava_pcode_register creg, dreg;

  endlbl = ava_codegen_genlabel(context);
  creg.type = ava_prt_int;
  creg.index = ava_codegen_push_reg(context, ava_prt_int, 1);
  dreg.type = ava_prt_data;
  dreg.index = ava_codegen_push_reg(context, ava_prt_data, 1);

  for (i = 0; i < this->num_clauses; ++i) {
    elselbl = ava_codegen_genlabel(context);
    if (this->clauses[i].condition) {
      ava_ast_node_cg_evaluate(this->clauses[i].condition, &dreg, context);
      AVA_PCXB(ld_reg, creg, dreg);
      AVA_PCXB(bool, creg, creg);
      ava_codegen_branch(context, &this->header.location,
                         creg, 0, ava_false, elselbl);
    }

    if (this->expression_form)
      ava_ast_node_cg_evaluate(this->clauses[i].result, dst, context);
    else
      ava_ast_node_cg_discard(this->clauses[i].result, context);

    ava_codegen_goto(context, &this->header.location, endlbl);

    if (this->clauses[i].condition)
      AVA_PCXB(label, elselbl);
  }

  /* If there's no explicit else result and this is expression-form, provide
   * one.
   */
  if (this->expression_form && this->clauses[this->num_clauses-1].condition)
    AVA_PCXB(ld_imm_vd, *dst, AVA_EMPTY_STRING);

  AVA_PCXB(label, endlbl);
  ava_codegen_pop_reg(context, ava_prt_data, 1);
  ava_codegen_pop_reg(context, ava_prt_int, 1);
}

static void ava_intr_if_cg_evaluate(
  ava_intr_if* this, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  if (!this->expression_form) {
    ava_codegen_error(context, (ava_ast_node*)this,
                      ava_error_statement_form_does_not_produce_a_value(
                        &this->header.location));
    return;
  }

  ava_intr_if_cg_common(this, dst, context);
}

static void ava_intr_if_cg_discard(
  ava_intr_if* this, ava_codegen_context* context
) {
  if (this->expression_form) {
    ava_codegen_error(context, (ava_ast_node*)this,
                      ava_error_expression_form_discarded(
                        &this->header.location));
    return;
  }

  ava_intr_if_cg_common(this, NULL, context);
}

static void ava_intr_if_cg_force(
  ava_intr_if* this, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_intr_if_cg_common(this, dst, context);
  if (!this->expression_form)
    AVA_PCXB(ld_imm_vd, *dst, AVA_EMPTY_STRING);
}
