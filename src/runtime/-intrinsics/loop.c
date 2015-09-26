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
#include <string.h>
#include <stddef.h>

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
#include "loop.h"

typedef enum {
  ava_ilct_each = 0,
  ava_ilct_for,
  ava_ilct_while,
  ava_ilct_do,
  ava_ilct_collect
} ava_intr_loop_clause_type;

typedef struct {
  ava_intr_loop_clause_type type;

  /* For code-generation, the label at which this clause's update stage can be
   * found.
   */
  ava_uint update_start_label;

  union {
    struct {
      size_t num_lvalues;
      ava_ast_node** lvalues;
      ava_ast_node* rvalue;

      /* Code generation */
      ava_pcode_register reg_list, reg_index, reg_length;
    } each;

    struct {
      ava_ast_node* init, * cond, * update;
    } vor;

    struct {
      ava_ast_node* cond;
      ava_bool invert;
    } vhile;

    struct {
      ava_ast_node* body;
      ava_bool is_expression;
    } doo;

    struct {
      /* If NULL, use loop iteration value */
      ava_ast_node* expression;
    } collect;
  } v;
} ava_intr_loop_clause;

typedef struct {
  ava_ast_node header;

  ava_ast_node* else_clause;
  ava_bool else_is_expression;

  /* Writing into the lvalues for the each clause requires a pseudo-AST-node to
   * serve as the value source.
   *
   * each_pnode is this pseudo-node. It simply produces the value stored in the
   * each_data_reg D-register. Thus, the process for setting an each lvalue is
   *   - Extract value from the list
   *   - Write into each_data_reg
   *   - cg_discard the lvalue
   */
  ava_ast_node each_pnode;
  ava_pcode_register each_data_reg;

  size_t num_clauses;
  ava_intr_loop_clause clauses[];
} ava_intr_loop;

static ava_string ava_intr_loop_to_string(const ava_intr_loop* this);
static void ava_intr_loop_postprocess(ava_intr_loop* this);
static void ava_intr_loop_cg_evaluate(
  ava_intr_loop* this, const ava_pcode_register* dst,
  ava_codegen_context* context);
static void ava_intr_loop_cg_discard(
  ava_intr_loop* this, ava_codegen_context* context);

static ava_string ava_intr_leach_pnode_to_string(const ava_ast_node* node);
static void ava_intr_leach_pnode_cg_evaluate(
  ava_ast_node* node, const ava_pcode_register* dst,
  ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_loop_vtable = {
  .name = "loop",
  .to_string = (ava_ast_node_to_string_f)ava_intr_loop_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_loop_postprocess,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_loop_cg_evaluate,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_loop_cg_discard,
};

static const ava_ast_node_vtable ava_intr_leach_pnode_vtable = {
  .name = "loop-each-clause",
  .to_string = ava_intr_leach_pnode_to_string,
  .cg_evaluate = ava_intr_leach_pnode_cg_evaluate,
};

ava_macro_subst_result ava_intr_loop_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  AVA_STATIC_STRING(loop_clause_type, "loop clause type");
  AVA_STATIC_STRING(collecting_str, "collecting");

  const ava_parse_unit* clause_id_unit, * unit;
  ava_string clause_id;
  ava_intr_loop* this;
  size_t num_clauses;
  ava_bool while_invert;

  /* Allocate space enough for every clause to be one unit */
  num_clauses = 0;
  for (unit = provoker; unit; unit = TAILQ_NEXT(unit, next))
    ++num_clauses;

  this = ava_alloc(sizeof(ava_intr_loop) +
                   num_clauses * sizeof(ava_intr_loop_clause));
  this->header.v = &ava_intr_loop_vtable;
  this->header.location = provoker->location;
  this->header.context = context;
  this->each_pnode.v = &ava_intr_leach_pnode_vtable;
  this->each_pnode.location = provoker->location;
  this->each_pnode.context = context;

  for (clause_id_unit = provoker, num_clauses = 0; clause_id_unit;
       clause_id_unit = TAILQ_NEXT(unit, next), ++num_clauses) {
    if (provoker == clause_id_unit) {
      clause_id = ava_string_of_cstring(self->v.macro.userdata);
    } else if (ava_put_block == clause_id_unit->type) {
      /* Implicit "do" block */
      unit = clause_id_unit;
      goto add_do_clause;
    } else if (ava_put_bareword != clause_id_unit->type) {
      return ava_macsub_error_result(
        context, ava_error_macro_arg_must_be_bareword(
          &clause_id_unit->location, loop_clause_type));
    } else {
      clause_id = clause_id_unit->v.string;
    }

    switch (ava_string_to_ascii9(clause_id)) {
    case AVA_ASCII9('e','a','c','h'): {
      const ava_parse_unit* in_unit = NULL, * list_unit;
      size_t num_lvalues = 0;
      ava_ast_node* reader;

      for (unit = TAILQ_NEXT(clause_id_unit, next);
           unit;
           unit = TAILQ_NEXT(unit, next)) {
        if (ava_put_bareword == unit->type &&
            0 == ava_strcmp(AVA_ASCII9_STRING("in"), unit->v.string)) {
          in_unit = unit;
          break;
        }

        ++num_lvalues;
      }

      if (!in_unit)
        return ava_macsub_error_result(
          context, ava_error_loop_each_without_in(&clause_id_unit->location));

      if (in_unit == TAILQ_NEXT(clause_id_unit, next))
        return ava_macsub_error_result(
          context, ava_error_loop_each_without_lvalues(&in_unit->location));

      list_unit = TAILQ_NEXT(in_unit, next);
      if (!list_unit)
        return ava_macsub_error_result(
          context, ava_error_loop_each_without_list(&in_unit->location));

      this->clauses[num_clauses].type = ava_ilct_each;
      this->clauses[num_clauses].v.each.num_lvalues = num_lvalues;
      this->clauses[num_clauses].v.each.lvalues =
        ava_alloc(sizeof(ava_ast_node*) * num_lvalues);

      for (unit = TAILQ_NEXT(clause_id_unit, next), num_lvalues = 0;
           unit != in_unit;
           unit = TAILQ_NEXT(unit, next), ++num_lvalues)
        this->clauses[num_clauses].v.each.lvalues[num_lvalues] =
          ava_ast_node_to_lvalue(
            ava_macsub_run_units(context, unit, unit),
            &this->each_pnode, &reader);

      this->clauses[num_clauses].v.each.rvalue =
        ava_macsub_run_units(context, list_unit, list_unit);

      unit = list_unit;
    } break;

    case AVA_ASCII9('f','o','r'): {
      const ava_parse_unit* init_unit, * cond_unit, * update_unit;

      init_unit = TAILQ_NEXT(clause_id_unit, next);
      if (!init_unit)
        return ava_macsub_error_result(
          context, ava_error_loop_for_without_init(&clause_id_unit->location));

      if (ava_put_block != init_unit->type)
        return ava_macsub_error_result(
          context, ava_error_loop_for_init_not_block(&init_unit->location));

      cond_unit = TAILQ_NEXT(init_unit, next);
      if (!cond_unit)
        return ava_macsub_error_result(
          context, ava_error_loop_for_without_cond(&clause_id_unit->location));

      if (ava_put_substitution != cond_unit->type)
        return ava_macsub_error_result(
          context, ava_error_loop_for_cond_not_subst(&cond_unit->location));

      update_unit = TAILQ_NEXT(cond_unit, next);
      if (!update_unit)
        return ava_macsub_error_result(
          context, ava_error_loop_for_without_update(&clause_id_unit->location));

      if (ava_put_block != update_unit->type)
        return ava_macsub_error_result(
          context, ava_error_loop_for_update_not_block(&update_unit->location));

      this->clauses[num_clauses].type = ava_ilct_for;
      this->clauses[num_clauses].v.vor.init = ava_macsub_run_contents(
        context, init_unit);
      this->clauses[num_clauses].v.vor.cond = ava_macsub_run_units(
        context, cond_unit, cond_unit);
      this->clauses[num_clauses].v.vor.update = ava_macsub_run_contents(
        context, update_unit);
      unit = update_unit;
    } break;

    case AVA_ASCII9('w','h','i','l','e'):
      while_invert = ava_false;
      goto add_while_clause;

    case AVA_ASCII9('u','n','t','i','l'):
      while_invert = ava_true;
      goto add_while_clause;

    add_while_clause: {
      unit = TAILQ_NEXT(clause_id_unit, next);

      if (!unit)
        return ava_macsub_error_result(
          context, ava_error_loop_while_without_cond(
            &clause_id_unit->location, clause_id));

      if (ava_put_substitution != unit->type)
        return ava_macsub_error_result(
          context, ava_error_loop_while_cond_not_subst(
            &unit->location, clause_id));

      this->clauses[num_clauses].type = ava_ilct_while;
      this->clauses[num_clauses].v.vhile.cond =
        ava_macsub_run_units(context, unit, unit);
      this->clauses[num_clauses].v.vhile.invert = while_invert;
      break;
      } break;

    case AVA_ASCII9('d','o'): {
      unit = TAILQ_NEXT(clause_id_unit, next);
      if (!unit)
        return ava_macsub_error_result(
          context, ava_error_loop_do_without_body(
            &clause_id_unit->location, clause_id));

      if (ava_put_block != unit->type &&
          ava_put_substitution != unit->type)
        return ava_macsub_error_result(
          context, ava_error_loop_do_body_not_block_or_subst(
            &unit->location, clause_id));

    add_do_clause:
      this->clauses[num_clauses].type = ava_ilct_do;
      this->clauses[num_clauses].v.doo.is_expression =
        ava_put_substitution == unit->type;
      this->clauses[num_clauses].v.doo.body =
        ava_macsub_run_contents(context, unit);
    } break;

    case AVA_ASCII9('c','o','l','l','e','c','t'): {
      unit = TAILQ_NEXT(clause_id_unit, next);
      if (!unit)
        return ava_macsub_error_result(
          context, ava_error_loop_collect_without_value(
            &clause_id_unit->location));

      this->clauses[num_clauses].type = ava_ilct_collect;
      this->clauses[num_clauses].v.collect.expression =
        ava_macsub_run_units(context, unit, unit);
    } break;

    case AVA_ASCII9('e','l','s','e'): goto exit_clause_loop;

    default: {
      if (0 == ava_strcmp(collecting_str, clause_id)) {
        this->clauses[num_clauses].type = ava_ilct_collect;
        this->clauses[num_clauses].v.collect.expression = NULL;
        unit = clause_id_unit;
      } else {
        return ava_macsub_error_result(
          context, ava_error_bad_loop_clause_id(
            &clause_id_unit->location, clause_id));
      }
    } break;
    }
  }

  exit_clause_loop:

  this->num_clauses = num_clauses;

  unit = clause_id_unit;
  if (unit) {
    /* Else clause */
    assert(ava_put_bareword == unit->type);
    assert(0 == ava_strcmp(AVA_ASCII9_STRING("else"), unit->v.string));

    if (!TAILQ_NEXT(unit, next))
      return ava_macsub_error_result(
        context, ava_error_loop_do_without_body(
          &unit->location, AVA_ASCII9_STRING("else")));

    unit = TAILQ_NEXT(unit, next);
    if (ava_put_block != unit->type && ava_put_substitution != unit->type)
      return ava_macsub_error_result(
        context, ava_error_loop_do_body_not_block_or_subst(
          &unit->location, AVA_ASCII9_STRING("else")));

    this->else_is_expression = ava_put_substitution == unit->type;
    this->else_clause = ava_macsub_run_contents(context, unit);

    unit = TAILQ_NEXT(unit, next);
    if (unit)
      return ava_macsub_error_result(
        context, ava_error_loop_garbage_after_else(
          &unit->location));
  }

  assert(!unit);

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static ava_string ava_intr_loop_to_string(const ava_intr_loop* loop) {
  ava_string accum;
  size_t clause, i;

  accum = AVA_ASCII9_STRING("loop");
  for (clause = 0; clause < loop->num_clauses; ++clause) {
    switch (loop->clauses[clause].type) {
    case ava_ilct_each:
      accum = ava_string_concat(accum, AVA_ASCII9_STRING(" each ["));
      for (i = 0; i < loop->clauses[clause].v.each.num_lvalues; ++i) {
        if (i)
          accum = ava_string_concat(accum, AVA_ASCII9_STRING(", "));
        accum = ava_string_concat(
          accum, ava_ast_node_to_string(
            loop->clauses[clause].v.each.lvalues[i]));
      }
      accum = ava_string_concat(accum, AVA_ASCII9_STRING("] = "));
      accum = ava_string_concat(
        accum, ava_ast_node_to_string(loop->clauses[clause].v.each.rvalue));
      break;

    case ava_ilct_for:
      accum = ava_string_concat(accum, AVA_ASCII9_STRING(" for ("));
      accum = ava_string_concat(
        accum, ava_ast_node_to_string(loop->clauses[clause].v.vor.init));
      accum = ava_string_concat(accum, AVA_ASCII9_STRING("; "));
      accum = ava_string_concat(
        accum, ava_ast_node_to_string(loop->clauses[clause].v.vor.cond));
      accum = ava_string_concat(accum, AVA_ASCII9_STRING("; "));
      accum = ava_string_concat(
        accum, ava_ast_node_to_string(loop->clauses[clause].v.vor.update));
      accum = ava_string_concat(accum, AVA_ASCII9_STRING(")"));
      break;

    case ava_ilct_while:
      if (loop->clauses[clause].v.vhile.invert)
        accum = ava_string_concat(accum, AVA_ASCII9_STRING(" until "));
      else
        accum = ava_string_concat(accum, AVA_ASCII9_STRING(" while "));
      accum = ava_string_concat(
        accum, ava_ast_node_to_string(loop->clauses[clause].v.vhile.cond));
      break;

    case ava_ilct_do:
      accum = ava_string_concat(accum, AVA_ASCII9_STRING(" do "));
      if (loop->clauses[clause].v.doo.is_expression)
        accum = ava_string_concat(accum, AVA_ASCII9_STRING("("));
      else
        accum = ava_string_concat(accum, AVA_ASCII9_STRING("{"));
      accum = ava_string_concat(
        accum, ava_ast_node_to_string(loop->clauses[clause].v.doo.body));
      if (loop->clauses[clause].v.doo.is_expression)
        accum = ava_string_concat(accum, AVA_ASCII9_STRING(")"));
      else
        accum = ava_string_concat(accum, AVA_ASCII9_STRING("}"));
      break;

    case ava_ilct_collect:
      accum = ava_string_concat(accum, AVA_ASCII9_STRING(" collect "));
      if (loop->clauses[clause].v.collect.expression)
        accum = ava_string_concat(
          accum, ava_ast_node_to_string(
            loop->clauses[clause].v.collect.expression));
      else
        accum = ava_string_concat(accum, AVA_ASCII9_STRING("<>"));
      break;
    }
  }

  if (loop->else_clause) {
    accum = ava_string_concat(accum, AVA_ASCII9_STRING(" else "));
    if (loop->else_is_expression)
      accum = ava_string_concat(accum, AVA_ASCII9_STRING("("));
    else
      accum = ava_string_concat(accum, AVA_ASCII9_STRING("{"));
    accum = ava_string_concat(
      accum, ava_ast_node_to_string(loop->else_clause));
    if (loop->else_is_expression)
      accum = ava_string_concat(accum, AVA_ASCII9_STRING(")"));
    else
      accum = ava_string_concat(accum, AVA_ASCII9_STRING("}"));
  }

  return accum;
}

static void ava_intr_loop_postprocess(ava_intr_loop* loop) {
  size_t clause, i;

  for (clause = 0; clause < loop->num_clauses; ++clause) {
    switch (loop->clauses[clause].type) {
    case ava_ilct_each:
      for (i = 0; i < loop->clauses[clause].v.each.num_lvalues; ++i)
        ava_ast_node_postprocess(loop->clauses[clause].v.each.lvalues[i]);
      ava_ast_node_postprocess(loop->clauses[clause].v.each.rvalue);
      break;

    case ava_ilct_for:
      ava_ast_node_postprocess(loop->clauses[clause].v.vor.init);
      ava_ast_node_postprocess(loop->clauses[clause].v.vor.cond);
      ava_ast_node_postprocess(loop->clauses[clause].v.vor.update);
      break;

    case ava_ilct_while:
      ava_ast_node_postprocess(loop->clauses[clause].v.vhile.cond);
      break;

    case ava_ilct_do:
      ava_ast_node_postprocess(loop->clauses[clause].v.doo.body);
      break;

    case ava_ilct_collect:
      if (loop->clauses[clause].v.collect.expression)
        ava_ast_node_postprocess(loop->clauses[clause].v.collect.expression);
      break;
    }
  }

  if (loop->else_clause)
    ava_ast_node_postprocess(loop->else_clause);
}

static void ava_intr_loop_cg_evaluate(
  ava_intr_loop* loop, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  size_t clause, i;
  ava_pcode_register accum, iterval;
  ava_uint iterate_label, completion_label;

  accum.type = ava_prt_data;
  accum.index = ava_codegen_push_reg(context, ava_prt_data, 2);
  iterval.type = ava_prt_data;
  iterval.index = accum.index + 1;
  iterate_label = ava_codegen_genlabel(context);
  completion_label = ava_codegen_genlabel(context);

  /* Allocate registers and generate labels */
  for (clause = 0; clause < loop->num_clauses; ++clause) {
    loop->clauses[clause].update_start_label = ava_codegen_genlabel(context);
    switch (loop->clauses[clause].type) {
    case ava_ilct_each:
      loop->clauses[clause].v.each.reg_list.type = ava_prt_list;
      loop->clauses[clause].v.each.reg_list.index =
        ava_codegen_push_reg(context, ava_prt_list, 1);
      loop->clauses[clause].v.each.reg_index.type = ava_prt_int;
      loop->clauses[clause].v.each.reg_index.index =
        ava_codegen_push_reg(context, ava_prt_int, 2);
      loop->clauses[clause].v.each.reg_length.type = ava_prt_int;
      loop->clauses[clause].v.each.reg_length.index =
        loop->clauses[clause].v.each.reg_index.index + 1;
      break;

    case ava_ilct_for:
    case ava_ilct_while:
    case ava_ilct_do:
    case ava_ilct_collect:
      break;
    }
  }

  /* Initialisation phase */
  AVA_PCXB(ld_imm_vd, accum, AVA_EMPTY_STRING);
  for (clause = 0; clause < loop->num_clauses; ++clause) {
    switch (loop->clauses[clause].type) {
    case ava_ilct_each: {
      ava_pcode_register tmp;

      tmp.type = ava_prt_data;
      tmp.index = ava_codegen_push_reg(context, ava_prt_data, 1);
      ava_ast_node_cg_evaluate(
        loop->clauses[clause].v.each.rvalue, &tmp, context);
      AVA_PCXB(ld_reg, loop->clauses[clause].v.each.reg_list, tmp);
      AVA_PCXB(llength, loop->clauses[clause].v.each.reg_length,
               loop->clauses[clause].v.each.reg_list);
      AVA_PCXB(ld_imm_i, loop->clauses[clause].v.each.reg_index, 0);
      ava_codegen_pop_reg(context, ava_prt_data, 1);
    } break;

    case ava_ilct_for: {
      ava_ast_node_cg_discard(loop->clauses[clause].v.vor.init, context);
    } break;

    case ava_ilct_while:
    case ava_ilct_do:
    case ava_ilct_collect:
      break;
    }
  }

  /* Iteration phase */
  AVA_PCXB(label, iterate_label);
  AVA_PCXB(ld_imm_vd, iterval, AVA_EMPTY_STRING);
  for (clause = 0; clause < loop->num_clauses; ++clause) {
    switch (loop->clauses[clause].type) {
    case ava_ilct_each: {
      AVA_STATIC_STRING(exception_type, "bad-list-multiplicity");
      ava_pcode_register cmp;

      cmp.type = ava_prt_int;
      cmp.index = ava_codegen_push_reg(context, ava_prt_int, 1);
      /* if (index >= length) goto completion; */
      AVA_PCXB(icmp, cmp, loop->clauses[clause].v.each.reg_index,
               loop->clauses[clause].v.each.reg_length);
      AVA_PCXB(branch, cmp, -1, ava_true, completion_label);
      ava_codegen_pop_reg(context, ava_prt_int, 1);

      for (i = 0; i < loop->clauses[clause].v.each.num_lvalues; ++i) {
        loop->each_data_reg.type = ava_prt_data;
        loop->each_data_reg.index =
          ava_codegen_push_reg(context, ava_prt_data, 1);

        AVA_PCXB(lindex, loop->each_data_reg,
                 loop->clauses[clause].v.each.reg_list,
                 loop->clauses[clause].v.each.reg_index,
                 exception_type,
                 ava_error_bad_list_multiplicity());
        AVA_PCXB(iadd_imm, loop->clauses[clause].v.each.reg_index,
                 loop->clauses[clause].v.each.reg_index, +1);
        ava_ast_node_cg_discard(
          loop->clauses[clause].v.each.lvalues[i], context);

        ava_codegen_pop_reg(context, ava_prt_data, 1);
      }
    } break;

    case ava_ilct_for: {
      ava_pcode_register condres, tmp;

      condres.type = ava_prt_int;
      condres.index = ava_codegen_push_reg(context, ava_prt_int, 1);
      tmp.type = ava_prt_data;
      tmp.index = ava_codegen_push_reg(context, ava_prt_data, 1);

      ava_ast_node_cg_evaluate(
        loop->clauses[clause].v.vor.cond, &tmp, context);
      AVA_PCXB(ld_reg, condres, tmp);
      AVA_PCXB(branch, condres, 0, ava_false, completion_label);

      ava_codegen_pop_reg(context, ava_prt_data, 1);
      ava_codegen_pop_reg(context, ava_prt_int, 1);
    } break;

    case ava_ilct_while: {
      ava_pcode_register condres, tmp;

      condres.type = ava_prt_int;
      condres.index = ava_codegen_push_reg(context, ava_prt_int, 1);
      tmp.type = ava_prt_data;
      tmp.index = ava_codegen_push_reg(context, ava_prt_data, 1);

      ava_ast_node_cg_evaluate(
        loop->clauses[clause].v.vhile.cond, &tmp, context);
      AVA_PCXB(ld_reg, condres, tmp);
      AVA_PCXB(branch, condres, 0, loop->clauses[clause].v.vhile.invert,
               completion_label);

      ava_codegen_pop_reg(context, ava_prt_data, 1);
      ava_codegen_pop_reg(context, ava_prt_int, 1);
    } break;

    case ava_ilct_do: {
      if (loop->clauses[clause].v.doo.is_expression)
        ava_ast_node_cg_evaluate(
          loop->clauses[clause].v.doo.body, &iterval, context);
      else
        ava_ast_node_cg_discard(
          loop->clauses[clause].v.doo.body, context);
    } break;

    case ava_ilct_collect: {
      ava_pcode_register ltmp, etmp;

      ltmp.type = ava_prt_list;
      ltmp.index = ava_codegen_push_reg(context, ava_prt_list, 1);
      etmp.type = ava_prt_data;
      etmp.index = ava_codegen_push_reg(context, ava_prt_data, 1);

      if (loop->clauses[clause].v.collect.expression)
        ava_ast_node_cg_evaluate(
          loop->clauses[clause].v.collect.expression, &etmp, context);
      else
        AVA_PCXB(ld_reg, etmp, iterval);

      AVA_PCXB(ld_reg, ltmp, accum);
      AVA_PCXB(lappend, ltmp, ltmp, etmp);
      AVA_PCXB(ld_reg, accum, ltmp);

      ava_codegen_pop_reg(context, ava_prt_data, 1);
      ava_codegen_pop_reg(context, ava_prt_list, 1);
    } break;
    }
  }

  /* Update phase */
  for (clause = loop->num_clauses - 1; clause < loop->num_clauses; --clause) {
    AVA_PCXB(label, loop->clauses[clause].update_start_label);
    switch (loop->clauses[clause].type) {
    case ava_ilct_each:
      break;

    case ava_ilct_for: {
      ava_ast_node_cg_discard(loop->clauses[clause].v.vor.update, context);
    } break;

    case ava_ilct_while:
    case ava_ilct_do:
    case ava_ilct_collect:
      break;
    }
  }
  AVA_PCXB(goto, iterate_label);

  /* Completion phase */
  AVA_PCXB(label, completion_label);
  if (loop->else_clause) {
    if (loop->else_is_expression)
      ava_ast_node_cg_evaluate(loop->else_clause, &accum, context);
    else
      ava_ast_node_cg_discard(loop->else_clause, context);
  }

  /* Deallocate registers */
  for (clause = loop->num_clauses - 1; clause < loop->num_clauses; --clause) {
    switch (loop->clauses[clause].type) {
    case ava_ilct_each:
      ava_codegen_pop_reg(context, ava_prt_int, 2);
      ava_codegen_pop_reg(context, ava_prt_list, 1);
      break;

    case ava_ilct_for:
    case ava_ilct_while:
    case ava_ilct_do:
    case ava_ilct_collect:
      break;
    }
  }

  if (dst)
    AVA_PCXB(ld_reg, *dst, accum);

  ava_codegen_pop_reg(context, ava_prt_data, 2);
}

static void ava_intr_loop_cg_discard(
  ava_intr_loop* loop, ava_codegen_context* context
) {
  ava_intr_loop_cg_evaluate(loop, NULL, context);
}

static ava_string ava_intr_leach_pnode_to_string(const ava_ast_node* node) {
  return AVA_ASCII9_STRING("<each-lv>");
}

static void ava_intr_leach_pnode_cg_evaluate(
  ava_ast_node* node, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  const ava_intr_loop* loop = (const ava_intr_loop*)
    ((char*)node - offsetof(ava_intr_loop, each_pnode));
  AVA_PCXB(ld_reg, *dst, loop->each_data_reg);
}
