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
#include "../avalanche/macro-arg.h"
#include "reg-rvalue.h"
#include "eh.h"

typedef struct {
  ava_pcode_exception_type type;
  ava_compile_location location;
  ava_ast_node* lvalue;
  ava_ast_node* body;
} ava_intr_try_clause;

typedef struct {
  ava_ast_node header;

  ava_ast_node* body, * finally;
  ava_bool expression_form;
  /* So we can produce better diagnostics */
  ava_bool is_defer;
  ava_bool postprocessed;

  ava_intr_reg_rvalue exception_value;

  size_t num_catches;
  ava_intr_try_clause catches[];
} ava_intr_try;

static ava_intr_try* ava_intr_try_new(
  ava_macsub_context* context,
  const ava_parse_unit* provoker,
  size_t num_catches);

static ava_string ava_intr_try_to_string(const ava_intr_try* node);
static void ava_intr_try_postprocess(ava_intr_try* node);
static void ava_intr_try_cg_discard(
  ava_intr_try* node, ava_codegen_context* context);
static void ava_intr_try_cg_evaluate(
  ava_intr_try* node, const ava_pcode_register* dst,
  ava_codegen_context* context);
static void ava_intr_try_cg_force(
  ava_intr_try* node, const ava_pcode_register* dst,
  ava_codegen_context* context);

static void ava_intr_try_cg_common(
  ava_intr_try* node, const ava_pcode_register* dst,
  ava_codegen_context* context);

static void ava_intr_try_generate_finally(
  ava_intr_try* node, ava_codegen_context* context);

static void ava_intr_try_do_finally(
  ava_codegen_context* context, const ava_compile_location* location,
  ava_intr_try* node);

static void ava_intr_try_put_yrt(
  ava_codegen_context* context, const ava_compile_location* location,
  void* ignored);

static void ava_intr_try_finally_barrier(
  ava_codegen_context* context, const ava_compile_location* location,
  void* ignored);

static const ava_ast_node_vtable ava_intr_try_vtable = {
  .name = "try/defer",
  .to_string = (ava_ast_node_to_string_f)ava_intr_try_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_try_postprocess,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_try_cg_discard,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_try_cg_evaluate,
  .cg_force = (ava_ast_node_cg_force_f)ava_intr_try_cg_force,
};

static ava_intr_try* ava_intr_try_new(
  ava_macsub_context* context,
  const ava_parse_unit* provoker,
  size_t num_catches
) {
  ava_intr_try* this;

  this = ava_alloc(sizeof(ava_intr_try) +
                   sizeof(ava_intr_try_clause) * num_catches);
  this->header.v = &ava_intr_try_vtable;
  this->header.context = context;
  this->header.location = provoker->location;
  this->num_catches = num_catches;
  ava_intr_reg_rvalue_init(&this->exception_value, context);

  return this;
}

ava_macro_subst_result ava_intr_try_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  const ava_string ct_catch = AVA_ASCII9_STRING("catch");
  AVA_STATIC_STRING(ct_workaround, "workaround");
  AVA_STATIC_STRING(ct_on_any_bad_format, "on-any-bad-format");
  AVA_STATIC_STRING(ct_workaround_undefined, "workaround-undefined");
  AVA_STATIC_STRING(
    expected_keywords,
    "catch, workaround, on-any-bad-format, workaround-undefined, or finally");

  ava_intr_try* this;
  size_t num_clauses, clause;
  const ava_parse_unit* unit, * body_unit;
  const ava_parse_unit* catch_type_unit, * exlv_unit;
  const ava_parse_unit* finally_unit;
  ava_ast_node* ignore_reader;
  ava_string catch_type, finally_kw;

  /* Every catch clause is exactly 3 units. The first 2 units are the provoker
   * and the main body. The last two might be finally and its body; division by
   * 3 will discount those.
   */
  num_clauses = 0;
  TAILQ_FOREACH(unit, &statement->units, next) ++num_clauses;
  num_clauses -= 2;
  num_clauses /= 3;

  this = ava_intr_try_new(context, provoker, num_clauses);

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_UNIT(body_unit, "body");

      if (ava_put_substitution == body_unit->type) {
        this->expression_form = ava_true;
      } else if (ava_put_block == body_unit->type) {
        this->expression_form = ava_false;
      } else {
        return ava_macsub_error_result(
          context, ava_error_macro_arg_must_be_substitution_or_block(
            &body_unit->location, AVA_ASCII9_STRING("body")));
      }
      this->body = ava_macsub_run_contents(context, body_unit);

      for (clause = 0; clause < num_clauses; ++clause) {
        AVA_MACRO_ARG_CURRENT_UNIT(catch_type_unit, "catch clause type");
        AVA_MACRO_ARG_BAREWORD(catch_type, "catch clause type");
        AVA_MACRO_ARG_UNIT(exlv_unit, "exception lvalue");
        AVA_MACRO_ARG_UNIT(body_unit, "catch body");

        this->catches[clause].location = catch_type_unit->location;

        if (ava_string_equal(ct_catch, catch_type))
          this->catches[clause].type = ava_pet_user_exception;
        else if (ava_string_equal(ct_workaround, catch_type))
          this->catches[clause].type = ava_pet_error_exception;
        else if (ava_string_equal(ct_on_any_bad_format, catch_type))
          this->catches[clause].type = ava_pet_format_exception;
        else if (ava_string_equal(ct_workaround_undefined, catch_type))
          this->catches[clause].type = ava_pet_undefined_behaviour_exception;
        else
          return ava_macsub_error_result(
            context, ava_error_bad_macro_keyword(
              &catch_type_unit->location,
              self->full_name, catch_type, expected_keywords));

        if (ava_put_substitution == body_unit->type ||
            ava_put_block == body_unit->type) {
          if (this->expression_form !=
              (ava_put_substitution == body_unit->type))
            return ava_macsub_error_result(
              context, ava_error_structure_inconsistent_result_form(
                &body_unit->location));
        } else {
          return ava_macsub_error_result(
            context, ava_error_macro_arg_must_be_substitution_or_block(
              &body_unit->location, AVA_ASCII9_STRING("body")));
        }

        this->catches[clause].lvalue = ava_ast_node_to_lvalue(
          ava_macsub_run_units(context, exlv_unit, exlv_unit),
          (ava_ast_node*)&this->exception_value, &ignore_reader);
        this->catches[clause].body =
          ava_macsub_run_contents(context, body_unit);
      }

      if (AVA_MACRO_ARG_HAS_ARG()) {
        AVA_MACRO_ARG_CURRENT_UNIT(finally_unit, "finally");
        AVA_MACRO_ARG_BAREWORD(finally_kw, "finally");
        AVA_MACRO_ARG_UNIT(body_unit, "body");

        if (!ava_string_equal(AVA_ASCII9_STRING("finally"), finally_kw))
          return ava_macsub_error_result(
            context, ava_error_bad_macro_keyword(
              &finally_unit->location,
              self->full_name, finally_kw, AVA_ASCII9_STRING("finally")));

        if (ava_put_block != body_unit->type)
          return ava_macsub_error_result(
            context, ava_error_macro_arg_must_be_block(
              &body_unit->location, AVA_ASCII9_STRING("body")));

        this->finally = ava_macsub_run_contents(context, body_unit);
      }
    }
  }

  if (!this->num_catches && !this->finally)
    return ava_macsub_error_result(
      context, ava_error_try_without_catch_or_finally(
        &provoker->location));

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static ava_string ava_intr_try_to_string(const ava_intr_try* node) {
  ava_string accum;
  size_t catch;

  accum = AVA_ASCII9_STRING("try ");
  accum = ava_strcat(accum, ava_ast_node_to_string(node->body));
  for (catch = 0; catch < node->num_catches; ++catch) {
    accum = ava_strcat(accum, AVA_ASCII9_STRING(" catch "));
    accum = ava_strcat(
      accum,
      ava_to_string(ava_value_of_integer(node->catches[catch].type)));
    accum = ava_strcat(accum, AVA_ASCII9_STRING(" "));
    accum = ava_strcat(
      accum, ava_ast_node_to_string(node->catches[catch].body));
  }

  if (node->finally) {
    accum = ava_strcat(accum, AVA_ASCII9_STRING(" finally "));
    accum = ava_strcat(accum, ava_ast_node_to_string(node->finally));
  }

  return accum;
}

static void ava_intr_try_postprocess(ava_intr_try* node) {
  size_t catch;

  if (node->postprocessed) return;
  node->postprocessed = ava_true;

  ava_ast_node_postprocess(node->body);
  for (catch = 0; catch < node->num_catches; ++catch) {
    ava_ast_node_postprocess(node->catches[catch].lvalue);
    ava_ast_node_postprocess(node->catches[catch].body);
  }
  if (node->finally)
    ava_ast_node_postprocess(node->finally);
}

static void ava_intr_try_cg_discard(
  ava_intr_try* node, ava_codegen_context* context
) {
  if (node->expression_form) {
    ava_codegen_error(context, (ava_ast_node*)node,
                      ava_error_expression_form_discarded(
                        &node->header.location));
    return;
  }

  ava_intr_try_cg_common(node, NULL, context);
}

static void ava_intr_try_cg_evaluate(
  ava_intr_try* node, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  if (!node->expression_form) {
    if (node->is_defer) {
      ava_codegen_error(context, (ava_ast_node*)node,
                        ava_error_does_not_produce_a_value(
                          &node->header.location,
                          AVA_ASCII9_STRING("defer")));
    } else {
      ava_codegen_error(context, (ava_ast_node*)node,
                        ava_error_statement_form_does_not_produce_a_value(
                          &node->header.location));
    }
    return;
  }

  ava_intr_try_cg_common(node, dst, context);
}

static void ava_intr_try_cg_force(
  ava_intr_try* node, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_intr_try_cg_common(node, dst, context);
  if (!node->expression_form)
    AVA_PCXB(ld_imm_vd, *dst, AVA_EMPTY_STRING);
}

static void ava_intr_try_cg_common(
  ava_intr_try* node, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_codegen_jprot do_finally_jprot, yrt_jprot;
  ava_uint join_label, start_catch_label = ~0u, finally_label = ~0u, next_label;
  ava_pcode_register ex_type;
  size_t catch;

  ava_codegen_set_location(context, &node->header.location);

  if (node->finally) {
    ava_codegen_push_jprot(
      &do_finally_jprot, context,
      (ava_codegen_jprot_exit_f)ava_intr_try_do_finally,
      node);
    finally_label = ava_codegen_genlabel(context);
    AVA_PCXB(try, ava_true, finally_label);
  }

  join_label = ava_codegen_genlabel(context);

  if (node->num_catches) {
    ava_codegen_push_jprot(
      &yrt_jprot, context,
      ava_intr_try_put_yrt, NULL);
    start_catch_label = ava_codegen_genlabel(context);
    AVA_PCXB(try, ava_false, start_catch_label);
  }

  if (node->expression_form)
    ava_ast_node_cg_evaluate(node->body, dst, context);
  else
    ava_ast_node_cg_discard(node->body, context);

  /* Implicit yrt(s) */
  ava_codegen_goto(context, &node->header.location, join_label);

  if (node->num_catches) {
    AVA_PCXB(label, start_catch_label);

    ex_type.type = ava_prt_int;
    ex_type.index = ava_codegen_push_reg(context, ava_prt_int, 1);
    node->exception_value.reg.type = ava_prt_data;
    node->exception_value.reg.index =
      ava_codegen_push_reg(context, ava_prt_data, 1);

    AVA_PCXB(ex_type, ex_type);
    AVA_PCXB(ex_value, node->exception_value.reg);

    for (catch = 0; catch < node->num_catches; ++catch) {
      ava_codegen_set_location(context, &node->catches[catch].location);
      next_label = ava_codegen_genlabel(context);
      ava_codegen_branch(context, &node->catches[catch].location,
                         ex_type, node->catches[catch].type,
                         ava_true, next_label);
      ava_ast_node_cg_discard(node->catches[catch].lvalue, context);

      if (node->expression_form)
        ava_ast_node_cg_evaluate(node->catches[catch].body, dst, context);
      else
        ava_ast_node_cg_discard(node->catches[catch].body, context);

      ava_codegen_goto(context, &node->header.location, join_label);
      AVA_PCXB(label, next_label);
    }

    AVA_PCXB0(rethrow);

    ava_codegen_pop_reg(context, ava_prt_data, 1);
    ava_codegen_pop_reg(context, ava_prt_int, 1);
    ava_codegen_pop_jprot(context);
  }

  if (node->finally) {
    AVA_PCXB(label, finally_label);
    ava_intr_try_generate_finally(node, context);
    AVA_PCXB0(rethrow);
  }

  AVA_PCXB(label, join_label);
  if (node->finally)
    ava_codegen_pop_jprot(context);
}

static void ava_intr_try_generate_finally(
  ava_intr_try* node, ava_codegen_context* context
) {
  ava_codegen_jprot jprot;

  ava_codegen_push_jprot(&jprot, context, ava_intr_try_finally_barrier, node);
  ava_ast_node_cg_discard(node->finally, context);
  ava_codegen_pop_jprot(context);
}

static void ava_intr_try_do_finally(
  ava_codegen_context* context, const ava_compile_location* location,
  ava_intr_try* node
) {
  ava_intr_try_generate_finally(node, context);
  AVA_PCXB0(yrt);
}

static void ava_intr_try_put_yrt(
  ava_codegen_context* context, const ava_compile_location* location,
  void* ignored
) {
  AVA_PCXB0(yrt);
}

static void ava_intr_try_finally_barrier(
  ava_codegen_context* context, const ava_compile_location* location,
  void* node
) {
  if (location) {
    ava_codegen_error(
      context, node, ava_error_jump_out_of_finally(location));
  }
}

ava_macro_subst_result ava_intr_defer_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  const ava_parse_unit* finally_begin, * finally_end;
  ava_intr_try* this;

  finally_begin = TAILQ_NEXT(provoker, next);
  assert(finally_begin);
  finally_end = TAILQ_LAST(&statement->units, ava_parse_unit_list_s);

  this = ava_intr_try_new(context, provoker, 0);
  this->expression_form = ava_false;
  this->is_defer = ava_true;
  if (finally_begin == finally_end && ava_put_block == finally_begin->type) {
    this->finally = ava_macsub_run_contents(context, finally_begin);
  } else {
    this->finally = ava_macsub_run_units(context, finally_begin, finally_end);
  }
  this->body = ava_macsub_run_from(
    context, &this->header.location,
    TAILQ_NEXT(statement, next), ava_isrp_void);

  *consumed_other_statements = ava_true;
  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

typedef struct {
  ava_ast_node header;

  ava_pcode_exception_type type;
  ava_ast_node* value;

  ava_bool postprocessed;
} ava_intr_throw;

static ava_string ava_intr_throw_to_string(const ava_intr_throw* node);
static void ava_intr_throw_postprocess(ava_intr_throw* node);
static void ava_intr_throw_cg_discard(
  ava_intr_throw* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_throw_vtable = {
  .name = "throw statement",
  .to_string = (ava_ast_node_to_string_f)ava_intr_throw_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_throw_postprocess,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_throw_cg_discard,
};

ava_macro_subst_result ava_intr_throw_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  AVA_STATIC_STRING(undefined_behaviour_str, "undefined-behaviour");
  const ava_string user_str = AVA_ASCII9_STRING("user");
  const ava_string error_str = AVA_ASCII9_STRING("error");
  const ava_string format_str = AVA_ASCII9_STRING("format");
  AVA_STATIC_STRING(types, "user, error, format, or undefined-behaviour");

  const ava_parse_unit* type_unit = NULL, * value_unit = NULL;
  ava_string type_str = AVA_EMPTY_STRING;
  ava_intr_throw* this;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_CURRENT_UNIT(type_unit, "type");
      AVA_MACRO_ARG_BAREWORD(type_str, "type");
      AVA_MACRO_ARG_UNIT(value_unit, "value");
    }
  }

  this = AVA_NEW(ava_intr_throw);
  this->header.context = context;
  this->header.location = provoker->location;
  this->header.v = &ava_intr_throw_vtable;

  if (ava_string_equal(user_str, type_str))
    this->type = ava_pet_user_exception;
  else if (ava_string_equal(error_str, type_str))
    this->type = ava_pet_error_exception;
  else if (ava_string_equal(format_str, type_str))
    this->type = ava_pet_format_exception;
  else if (ava_string_equal(undefined_behaviour_str, type_str))
    this->type = ava_pet_undefined_behaviour_exception;
  else
    return ava_macsub_error_result(
      context, ava_error_bad_macro_keyword(
        &type_unit->location, self->full_name, type_str, types));

  this->value = ava_macsub_run_units(context, value_unit, value_unit);

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static ava_string ava_intr_throw_to_string(const ava_intr_throw* node) {
  AVA_STATIC_STRING(undefined_behaviour_str, "undefined-behaviour");
  const ava_string user_str = AVA_ASCII9_STRING("user");
  const ava_string error_str = AVA_ASCII9_STRING("error");
  const ava_string format_str = AVA_ASCII9_STRING("format");

  ava_string accum, type_str;

  accum = AVA_ASCII9_STRING("throw ");
  switch (node->type) {
  case ava_pet_user_exception:
    type_str = user_str; break;
  case ava_pet_error_exception:
    type_str = error_str; break;
  case ava_pet_format_exception:
    type_str = format_str; break;
  case ava_pet_undefined_behaviour_exception:
    type_str = undefined_behaviour_str;
  default: abort();
  }

  accum = ava_strcat(accum, type_str);
  accum = ava_strcat(accum, AVA_ASCII9_STRING(" "));
  accum = ava_strcat(accum, ava_ast_node_to_string(node->value));
  return accum;
}

static void ava_intr_throw_postprocess(ava_intr_throw* node) {
  if (node->postprocessed) return;

  node->postprocessed = ava_true;
  ava_ast_node_postprocess(node->value);
}

static void ava_intr_throw_cg_discard(
  ava_intr_throw* node, ava_codegen_context* context
) {
  ava_pcode_register reg;

  reg.type = ava_prt_data;
  reg.index = ava_codegen_push_reg(context, ava_prt_data, 1);
  ava_ast_node_cg_evaluate(node->value, &reg, context);
  AVA_PCXB(throw, node->type, reg);
  ava_codegen_pop_reg(context, ava_prt_data, 1);
}
