/*-
 * Copyright (c) 2015, 2016, Jason Lingle
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
#include "../avalanche/defs.h"
#include "../avalanche/alloc.h"
#include "../avalanche/string.h"
#include "../avalanche/value.h"
#include "../avalanche/list.h"
#include "../avalanche/integer.h"
#include "../avalanche/function.h"
#include "../avalanche/name-mangle.h"
#include "../avalanche/symbol.h"
#include "../avalanche/macsub.h"
#include "../avalanche/macro-arg.h"
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/varscope.h"
#include "../avalanche/exception.h"
#include "../avalanche/errors.h"
#include "variable.h"
#include "fundamental.h"
#include "defun.h"
#include "bsd.h"

#define LAMBDA_ARGS 4

typedef struct {
  ava_ast_node header;
  ava_string self_name;

  ava_macsub_context* subcontext;
  ava_symbol* symbol;

  ava_ast_node* body;

  ava_bool defined;
} ava_intr_fun;

typedef struct {
  const ava_function* result;
  const ava_parse_unit* unit;
} ava_intr_fun_prototype_conv_data;

static const ava_parse_unit* first_unit(const ava_parse_statement_list* list);
static ava_bool ava_intr_fun_is_named(ava_string* name);
static ava_bool ava_intr_fun_is_def_begin(const ava_parse_unit* unit);

static ava_string ava_intr_fun_to_string(const ava_intr_fun* this);
static void ava_intr_fun_postprocess(ava_intr_fun* this);
static void ava_intr_fun_cg_define(
  ava_intr_fun* this, ava_codegen_context* context);

static void ava_intr_fun_codegen(
  ava_intr_fun* this, ava_codegen_context* context);
static void ava_intr_fun_try_convert_prototype(void* conv_data);

static const ava_ast_node_vtable ava_intr_fun_vtable = {
  .name = "function declaration",
  .to_string = (ava_ast_node_to_string_f)ava_intr_fun_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_fun_postprocess,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_fun_cg_define, /* sic */
  .cg_define = (ava_ast_node_cg_define_f)ava_intr_fun_cg_define,
};

ava_macro_subst_result ava_intr_fun_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  const ava_parse_unit* name_unit, * arg_begin, * arg_unit;
  const ava_parse_unit* body_begin, * prev_unit;
  const ava_parse_unit* linkage_name_unit = NULL, * prototype_unit = NULL;
  ava_macsub_context* subcontext;
  ava_string name, abs, amb;
  ava_bool is_expression_form;
  size_t num_args, arg_ix;
  const ava_function* fun;
  ava_function* mutfun;
  ava_argument_spec* argspecs;
  ava_intr_fun* this;
  ava_bool has_nonoptional_arg = ava_false, expect_valid = ava_true;
  ava_bool has_varargs = ava_false, has_varshape = ava_false;
  ava_bool last_was_varshape = ava_false, is_varshape;
  ava_intr_fun_prototype_conv_data prototype_conv_data;
  ava_exception caught;

  name_unit = TAILQ_NEXT(provoker, next);
  if (!name_unit)
    return ava_macsub_error_result(
      context, ava_error_macro_arg_missing(
        &provoker->location, self->full_name, AVA_ASCII9_STRING("name")));

  if (ava_put_bareword != name_unit->type)
    return ava_macsub_error_result(
      context, ava_error_macro_arg_must_be_bareword(
        &name_unit->location, AVA_ASCII9_STRING("name")));

  name = name_unit->v.string;
  subcontext = ava_macsub_context_push_major(
    context, ava_strcat(name, AVA_ASCII9_STRING("\\")));
  ava_macsub_import(&abs, &amb, subcontext,
                    ava_macsub_apply_prefix(subcontext, AVA_EMPTY_STRING),
                    AVA_EMPTY_STRING,
                    ava_true, ava_true);

  arg_unit = TAILQ_NEXT(prev_unit = name_unit, next);
  if (arg_unit && ava_put_astring == arg_unit->type) {
    linkage_name_unit = arg_unit;
    arg_unit = TAILQ_NEXT(prev_unit = arg_unit, next);
  }
  if (arg_unit && ava_put_astring == arg_unit->type) {
    prototype_unit = arg_unit;
    arg_unit = TAILQ_NEXT(prev_unit = arg_unit, next);
  }

  if (linkage_name_unit && ava_macsub_get_level(context)) {
    ava_macsub_record_error(
      context, ava_error_overrides_on_nested_function(
        &linkage_name_unit->location));
    linkage_name_unit = prototype_unit = NULL;
  }

  if (linkage_name_unit &&
      !ava_string_is_empty(linkage_name_unit->v.string) &&
      ava_v_private == *(const ava_visibility*)self->v.macro.userdata) {
    ava_macsub_record_error(
      context, ava_error_linkage_name_on_non_linked(
        &linkage_name_unit->location));
    linkage_name_unit = NULL;
  }

  for (arg_begin = arg_unit, num_args = 0;
       arg_unit && !ava_intr_fun_is_def_begin(arg_unit);
       arg_unit = TAILQ_NEXT(prev_unit = arg_unit, next))
    ++num_args;

  body_begin = arg_unit;
  if (body_begin && ava_put_bareword == body_begin->type) {
    is_expression_form = ava_true;
    body_begin = TAILQ_NEXT(prev_unit = body_begin, next);
  } else {
    is_expression_form = ava_false;
    assert(!body_begin || ava_put_block == body_begin->type);
  }
  if (!body_begin)
    return ava_macsub_error_result(
      context, ava_error_function_without_body(&prev_unit->location));

  if (!is_expression_form && TAILQ_NEXT(body_begin, next))
    return ava_macsub_error_result(
      context, ava_error_garbage_after_function_body(
        &TAILQ_NEXT(body_begin, next)->location));

  if (0 == num_args)
    return ava_macsub_error_result(
      context, ava_error_defun_without_args(
        &(arg_unit? arg_unit : name_unit)->location));

  fun = NULL;
  if (prototype_unit) {
    prototype_conv_data.unit = prototype_unit;
    if (ava_catch(&caught, ava_intr_fun_try_convert_prototype,
                  &prototype_conv_data)) {
      ava_macsub_record_error(
        context, ava_error_invalid_function_prototype(
          &prototype_unit->location,
          ava_exception_get_value(&caught)));
    } else if (num_args != prototype_conv_data.result->num_args) {
      ava_macsub_record_error(
        context, ava_error_prototype_override_wrong_arg_count(
          &prototype_unit->location));
    } else {
      fun = prototype_conv_data.result;
    }
  }

  /* If there is a prototype override, this array ends up getting discarded,
   * but the code is simpler if it always has a location to write back to, and
   * it's not worth optimising for the case where there is a prototype
   * override.
   */
  argspecs = ava_alloc(num_args * sizeof(ava_argument_spec));
  if (!fun) {
    fun = mutfun = AVA_NEW(ava_function);
    mutfun->address = (void(*)())1;
    mutfun->calling_convention = ava_cc_ava;
    mutfun->num_args = num_args;
    mutfun->args = argspecs;
  }

  for (arg_unit = arg_begin, arg_ix = 0;
       arg_ix < num_args;
       arg_unit = TAILQ_NEXT(arg_unit, next), ++arg_ix) {
    ava_string arg_name;
    ava_value arg_default = ava_value_of_string(AVA_EMPTY_STRING);
    ava_argument_binding_type arg_type;
    ava_symbol* var;
    const ava_parse_unit* subunit, * arg_name_unit = arg_unit, * error_unit;

    switch (arg_unit->type) {
    case ava_put_bareword:
      has_nonoptional_arg = ava_true;
      arg_name = arg_unit->v.string;
      if (ava_intr_fun_is_named(&arg_name)) {
        arg_type = ava_abt_named;
        is_varshape = ava_true;
      } else {
        arg_type = ava_abt_pos;
        is_varshape = ava_false;
      }
      break;

    case ava_put_substitution:
      has_nonoptional_arg = ava_true;
      is_varshape = ava_false;
      arg_name = AVA_EMPTY_STRING;
      arg_type = ava_abt_empty;
      subunit = first_unit(&arg_unit->v.statements);
      if (subunit)
        ava_macsub_record_error(
          context, ava_error_defun_nonempty_empty(
            &subunit->location));
      break;

    case ava_put_spread:
      has_nonoptional_arg = ava_true;
      is_varshape = ava_true;
      subunit = arg_unit->v.unit;
      if (subunit->type != ava_put_bareword) {
        arg_name = AVA_EMPTY_STRING;
        arg_type = ava_abt_varargs;
        ava_macsub_record_error(
          context, ava_error_defun_varargs_name_must_be_simple(
            &subunit->location));
      } else {
        arg_name_unit = subunit;
        arg_name = arg_name_unit->v.string;
        arg_type = ava_abt_varargs;
        if (ava_intr_fun_is_named(&arg_name))
          ava_macsub_record_error(
            context, ava_error_defun_varargs_name_must_be_simple(
              &arg_name_unit->location));
      }
      break;

    case ava_put_semiliteral:
      is_varshape = ava_true;
      subunit = TAILQ_FIRST(&arg_unit->v.units);
      if (!subunit) {
        ava_macsub_record_error(
          context, ava_error_defun_optional_empty(&arg_unit->location));
        arg_type = ava_abt_pos_default;
        arg_name = AVA_EMPTY_STRING;
      } else if (ava_put_spread == subunit->type) {
        ava_macsub_record_error(
          context, ava_error_defun_varargs_in_optional(
            &subunit->location));
        arg_type = ava_abt_pos_default;
        arg_name = AVA_EMPTY_STRING;
      } else if (ava_put_bareword != subunit->type) {
        ava_macsub_record_error(
          context, ava_error_macro_arg_must_be_bareword(
            &subunit->location, AVA_ASCII9_STRING("arg name")));
        arg_type = ava_abt_pos_default;
        arg_name = AVA_EMPTY_STRING;
      } else {
        arg_name_unit = subunit;
        arg_name = subunit->v.string;
        if (ava_intr_fun_is_named(&arg_name))
          arg_type = ava_abt_named_default;
        else
          arg_type = ava_abt_pos_default;

        subunit = TAILQ_NEXT(subunit, next);
        if (subunit) {
          if (!ava_macro_arg_literal(
                &arg_default, &error_unit, subunit))
            ava_macsub_record_error(
              context, ava_error_macro_arg_must_be_literal(
                &error_unit->location, AVA_ASCII9_STRING("default")));

          subunit = TAILQ_NEXT(subunit, next);
          if (subunit)
            ava_macsub_record_error(
              context, ava_error_defun_extra_tokens_after_default(
                &subunit->location));
        }
      }
      break;

    default:
      ava_macsub_record_error(
        context, ava_error_defun_invalid_arg(&arg_unit->location));
      arg_name = AVA_EMPTY_STRING;
      arg_type = ava_abt_pos;
      is_varshape = ava_false;
      break;
    }

    argspecs[arg_ix].binding.type = arg_type;
    argspecs[arg_ix].binding.name =
      ava_strcat(AVA_ASCII9_STRING("-"), arg_name);
    argspecs[arg_ix].binding.value = arg_default;

    var = AVA_NEW(ava_symbol);
    var->type = ava_st_local_variable;
    var->level = ava_macsub_get_level(subcontext);
    var->visibility = ava_v_private;
    var->full_name = ava_macsub_apply_prefix(subcontext, arg_name);
    var->v.var.is_mutable = ava_true;
    var->v.var.name.scheme = ava_nms_ava;
    var->v.var.name.name = var->full_name;

    if (!ava_string_is_empty(arg_name))
      expect_valid &= ava_macsub_put_symbol(
        subcontext, var, &arg_name_unit->location);

    ava_varscope_put_local(ava_macsub_get_varscope(subcontext), var);

    if (is_varshape && has_varshape && !last_was_varshape && expect_valid) {
      expect_valid = ava_false;
      ava_macsub_record_error(
        context, ava_error_defun_discontiguous_varshape(&arg_name_unit->location));
    }

    if (has_varargs && is_varshape && expect_valid) {
      expect_valid = ava_false;
      ava_macsub_record_error(
        context, ava_error_defun_varshape_after_varargs(&arg_name_unit->location));
    }

    has_varshape |= is_varshape;
    last_was_varshape = is_varshape;
    has_varargs |= ava_abt_varargs == arg_type;
  }

  if (!has_nonoptional_arg)
    ava_macsub_record_error(
      context, ava_error_defun_no_explicit_args(&name_unit->location));

  this = AVA_NEW(ava_intr_fun);
  this->header.v = &ava_intr_fun_vtable;
  this->header.location = provoker->location;
  this->header.context = context;
  this->self_name = self->full_name;
  this->subcontext = subcontext;
  this->symbol = AVA_NEW(ava_symbol);
  this->symbol->type = ava_macsub_get_level(context)?
    ava_st_local_function : ava_st_global_function;
  this->symbol->level = ava_macsub_get_level(context);
  this->symbol->visibility = *(const ava_visibility*)self->v.macro.userdata;
  this->symbol->definer = (ava_ast_node*)this;
  this->symbol->full_name = ava_macsub_apply_prefix(context, name);
  this->symbol->v.var.is_mutable = ava_false;
  if (linkage_name_unit &&
      !ava_string_is_empty(linkage_name_unit->v.string)) {
    this->symbol->v.var.name.scheme = ava_nms_none;
    this->symbol->v.var.name.name = linkage_name_unit->v.string;
  } else {
    this->symbol->v.var.name.scheme = ava_nms_ava;
    this->symbol->v.var.name.name = this->symbol->full_name;
  }
  this->symbol->v.var.fun = *fun;
  this->symbol->v.var.scope = ava_macsub_get_varscope(subcontext);

  ava_macsub_put_symbol(context, this->symbol, &name_unit->location);

  if (is_expression_form) {
    this->body = ava_macsub_run_units(
      subcontext, body_begin, TAILQ_LAST(&statement->units, ava_parse_unit_list_s));
  } else {
    this->body = ava_macsub_run(
      subcontext, &body_begin->location,
      (ava_parse_statement_list*)&body_begin->v.statements, ava_isrp_void);
  }

  {
    ava_string msg;
    if (expect_valid && !ava_function_is_valid(&msg, fun)) {
      ava_macsub_record_error(
        context, ava_error_invalid_function_prototype(
          &provoker->location, ava_value_of_string(msg)));
    }
  }

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static ava_bool ava_intr_fun_is_def_begin(const ava_parse_unit* unit) {
  switch (unit->type) {
  case ava_put_block:
    return ava_true;
  case ava_put_bareword:
    return 0 == ava_strcmp(AVA_ASCII9_STRING("="), unit->v.string);
  default:
    return ava_false;
  }
}

static const ava_parse_unit* first_unit(const ava_parse_statement_list* list) {
  ava_parse_unit_list* units;

  if (TAILQ_EMPTY(list)) return NULL;

  units = &TAILQ_FIRST(list)->units;
  if (TAILQ_EMPTY(units)) return NULL;

  return TAILQ_FIRST(units);
}

static void ava_intr_fun_try_convert_prototype(void* vdata) {
  ava_intr_fun_prototype_conv_data* data = vdata;

  data->result = ava_function_of_value(
    ava_value_of_string(
      ava_strcat(
        AVA_ASCII9_STRING("1 "),
        data->unit->v.string)));
}

static ava_bool ava_intr_fun_is_named(ava_string* name) {
  if (ava_string_is_empty(*name)) return ava_false;

  if ('-' == ava_string_index(*name, 0)) {
    *name = ava_string_slice(*name, 1, ava_strlen(*name));
    return ava_true;
  } else {
    return ava_false;
  }
}

static ava_string ava_intr_fun_to_string(const ava_intr_fun* this) {
  ava_string accum;

  accum = this->self_name;
  accum = ava_strcat(accum, AVA_ASCII9_STRING("["));
  accum = ava_strcat(
    accum, ava_to_string(ava_value_of_function(&this->symbol->v.var.fun)));
  accum = ava_strcat(accum, AVA_ASCII9_STRING("] = "));
  accum = ava_strcat(accum, ava_ast_node_to_string(this->body));
  return accum;
}

static void ava_intr_fun_postprocess(ava_intr_fun* this) {
  ava_ast_node_postprocess(this->body);
}

static void ava_intr_fun_cg_define(
  ava_intr_fun* this, ava_codegen_context* context
) {
  ava_pcx_builder* body_builder;
  size_t num_vars = ava_varscope_num_vars(this->symbol->v.var.scope), i;
  const ava_symbol* var_symbols[num_vars];
  ava_list_value vars;
  ava_function* prototype;
  ava_argument_spec* argspecs;
  size_t num_captures;

  if (this->defined) return;
  this->defined = ava_true;

  ava_varscope_get_vars(var_symbols, this->symbol->v.var.scope, num_vars);
  vars = ava_empty_list();
  for (i = 0; i < num_vars; ++i)
    vars = ava_list_append(
      vars, ava_value_of_string(var_symbols[i]->full_name));

  /* Modify the function's prototype to include captured variables */
  num_captures = ava_varscope_num_captures(this->symbol->v.var.scope);
  prototype = AVA_CLONE(this->symbol->v.var.fun);
  prototype->args = argspecs =
    ava_alloc(sizeof(ava_argument_spec) *
              (prototype->num_args + num_captures));
  prototype->num_args += num_captures;
  for (i = 0; i < num_captures; ++i) {
    argspecs[i].binding.type = ava_abt_pos;
  }
  memcpy(argspecs + num_captures, this->symbol->v.var.fun.args,
         sizeof(ava_argument_spec) * this->symbol->v.var.fun.num_args);

  ava_codegen_set_global_location(context, &this->header.location);
  this->symbol->pcode_index = AVA_PCGB(
    fun, ava_v_private != this->symbol->visibility,
    this->symbol->v.var.name, prototype, vars, &body_builder);
  ava_codegen_export(context, this->symbol);

  ava_intr_fun_codegen(this, ava_codegen_context_new(context, body_builder));
}

static void ava_intr_fun_codegen(
  ava_intr_fun* this, ava_codegen_context* context
) {
  ava_pcode_register reg;

  ava_codegen_set_location(context, &this->header.location);

  reg.type = ava_prt_data;
  reg.index = ava_codegen_push_reg(context, ava_prt_data, 1);
  ava_ast_node_cg_force(this->body, &reg, context);
  AVA_PCXB(ret, reg);
  ava_codegen_pop_reg(context, ava_prt_data, 1);
}

ava_ast_node* ava_intr_lambda_expr(
  ava_macsub_context* context,
  ava_parse_unit* lambda
) {
  ava_macsub_context* subcontext;
  ava_string name, abs, amb;
  ava_function* fun;
  ava_argument_spec* argspecs;
  ava_intr_fun* definition;
  ava_intr_seq* seq;
  ava_symbol* symbol;
  size_t i;

  ava_macsub_gensym_seed(context, &lambda->location);
  name = ava_macsub_gensym(context, AVA_ASCII9_STRING("{}\\"));

  subcontext = ava_macsub_context_push_major(context, name);
  ava_macsub_import(&abs, &amb, subcontext,
                    ava_macsub_apply_prefix(subcontext, AVA_EMPTY_STRING),
                    AVA_EMPTY_STRING,
                    ava_true, ava_true);

  fun = AVA_NEW(ava_function);
  fun->address = (void(*)())1;
  fun->calling_convention = ava_cc_ava;
  fun->num_args = LAMBDA_ARGS;
  fun->args = argspecs = ava_alloc(sizeof(ava_argument_spec) * LAMBDA_ARGS);
  for (i = 0; i < LAMBDA_ARGS; ++i) {
    argspecs[i].binding.type = i? ava_abt_pos_default : ava_abt_pos;
    argspecs[i].binding.value = ava_value_of_string(AVA_EMPTY_STRING);

    symbol = AVA_NEW(ava_symbol);
    symbol->type = ava_st_local_variable;
    symbol->level = ava_macsub_get_level(subcontext);
    symbol->full_name = ava_macsub_apply_prefix(
      subcontext, ava_to_string(ava_value_of_integer(i + 1)));
    symbol->v.var.is_mutable = ava_true;
    symbol->v.var.name.scheme = ava_nms_ava;
    symbol->v.var.name.name = symbol->full_name;
    ava_macsub_put_symbol(subcontext, symbol, &lambda->location);
    ava_varscope_put_local(ava_macsub_get_varscope(subcontext), symbol);
  }

  symbol = AVA_NEW(ava_symbol);
  definition = AVA_NEW(ava_intr_fun);
  symbol->type = ava_st_local_function; /* even if global, consider local */
  symbol->level = ava_macsub_get_level(context);
  symbol->full_name = ava_macsub_apply_prefix(context, name);
  symbol->definer = (ava_ast_node*)definition;
  symbol->v.var.is_mutable = ava_false;
  symbol->v.var.name.scheme = ava_nms_ava;
  symbol->v.var.name.name = symbol->full_name;
  symbol->v.var.fun = *fun;
  symbol->v.var.scope = ava_macsub_get_varscope(subcontext);
  ava_macsub_put_symbol(context, symbol, &lambda->location);

  definition->header.v = &ava_intr_fun_vtable;
  definition->header.location = lambda->location;
  definition->header.context = context;
  definition->self_name = AVA_ASCII9_STRING("{}");
  definition->subcontext = subcontext;
  definition->symbol = symbol;

  definition->body = ava_macsub_run(
    subcontext, &lambda->location,
    &lambda->v.statements, ava_isrp_only);

  seq = ava_intr_seq_new(context, &lambda->location, ava_isrp_last);
  ava_intr_seq_add(seq, (ava_ast_node*)definition);
  ava_intr_seq_add(seq, ava_intr_var_read_new(
                     context, symbol, &lambda->location));
  return ava_intr_seq_to_node(seq);
}
