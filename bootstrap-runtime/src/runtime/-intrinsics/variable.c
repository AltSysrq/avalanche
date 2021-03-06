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
#include "../avalanche/defs.h"
#include "../avalanche/alloc.h"
#include "../avalanche/string.h"
#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"
#include "../avalanche/symbol.h"
#include "../avalanche/symtab.h"
#include "../avalanche/macro-arg.h"
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/varscope.h"
#include "variable.h"

typedef struct {
  ava_ast_node header;
  const ava_symbol* var;
  ava_symtab* symtab;
  ava_string name;
  ava_bool postprocessed;
} ava_intr_var_read;

typedef struct {
  ava_ast_node header;
  const ava_symbol* var;
  ava_symbol* owned_var;
  ava_ast_node* producer;
  ava_bool defined;
} ava_intr_var_write;

/* Pseudo-node to propagate lvalue reader into the context var before
 * delegating to another node.
 */
typedef struct {
  ava_ast_node header;
  ava_ast_node* delegate;
  ava_ast_node* reader;
  ava_bool postprocessed;
} ava_intr_var_ucs;

/* Pseudo-node used to define global variables created with ava_var_gen(). */
typedef struct {
  ava_ast_node header;
  ava_symbol* symbol;
  ava_bool defined;
} ava_intr_genvar_definer;

typedef enum {
  ava_vc_lower, ava_vc_mixed, ava_vc_upper
} ava_var_casing;

static ava_string ava_intr_var_read_to_string(const ava_intr_var_read* node);
static ava_ast_node* ava_intr_var_read_to_lvalue(
  const ava_intr_var_read* node, ava_ast_node* producer,
  ava_ast_node** reader);
static void ava_intr_var_read_postprocess(ava_intr_var_read* node);
static void ava_intr_var_read_cg_evaluate(
  ava_intr_var_read* node, const ava_pcode_register* dst,
  ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_var_read_vtable = {
  .name = "variable read",
  .to_string = (ava_ast_node_to_string_f)ava_intr_var_read_to_string,
  .to_lvalue = (ava_ast_node_to_lvalue_f)ava_intr_var_read_to_lvalue,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_var_read_postprocess,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_var_read_cg_evaluate,
};

static ava_string ava_intr_var_write_to_string(const ava_intr_var_write* node);
static void ava_intr_var_write_postprocess(ava_intr_var_write* node);
static void ava_intr_var_write_cg_evaluate(
  ava_intr_var_write* node, const ava_pcode_register* dst,
  ava_codegen_context* context);
static void ava_intr_var_write_cg_discard(
  ava_intr_var_write* node, ava_codegen_context* context);
static void ava_intr_var_write_cg_define(
  ava_intr_var_write* node, ava_codegen_context* context);
static void ava_intr_var_write_cg_set_up(
  ava_intr_var_write* node, ava_codegen_context* context);
static void ava_intr_var_write_cg_tear_down(
  ava_intr_var_write* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_var_write_vtable = {
  .name = "variable write",
  .to_string = (ava_ast_node_to_string_f)ava_intr_var_write_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_var_write_postprocess,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_var_write_cg_evaluate,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_var_write_cg_discard,
  .cg_define = (ava_ast_node_cg_define_f)ava_intr_var_write_cg_define,
  .cg_set_up = (ava_ast_node_cg_set_up_f)ava_intr_var_write_cg_set_up,
  .cg_tear_down = (ava_ast_node_cg_tear_down_f)ava_intr_var_write_cg_tear_down,
};

static ava_string ava_intr_var_ucs_to_string(const ava_intr_var_ucs* node);
static void ava_intr_var_ucs_postprocess(ava_intr_var_ucs* node);
static void ava_intr_var_ucs_cg_evaluate(
  ava_intr_var_ucs* node, const ava_pcode_register* dst,
  ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_var_ucs_vtable = {
  .name = "variable update expression",
  .to_string = (ava_ast_node_to_string_f)ava_intr_var_ucs_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_var_ucs_postprocess,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_var_ucs_cg_evaluate,
};

static ava_string ava_intr_genvar_definer_to_string(
  const ava_intr_genvar_definer* node);
static void ava_intr_genvar_definer_cg_define(
  ava_intr_genvar_definer* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_genvar_definer_vtable = {
  .name = "generated global variable definer",
  .to_string = (ava_ast_node_to_string_f)ava_intr_genvar_definer_to_string,
  .cg_define = (ava_ast_node_cg_define_f)ava_intr_genvar_definer_cg_define,
};

static ava_var_casing ava_var_casing_of(ava_string name);
static ava_visibility ava_var_visibility_of(
  ava_var_casing casing, ava_uint level);
static ava_bool ava_var_is_casing_mutable(ava_var_casing casing);
static ava_intr_var_ucs* ava_intr_var_ucs_new(ava_ast_node* delegate);
static ava_intr_genvar_definer* ava_intr_genvar_definer_new(
  ava_macsub_context* context, const ava_compile_location* location,
  ava_symbol* sym);

ava_ast_node* ava_intr_variable_lvalue(
  ava_macsub_context* context,
  ava_string name,
  const ava_compile_location* location,
  ava_ast_node* producer,
  ava_ast_node** reader
) {
  const ava_symbol* symbol, ** results;
  ava_symbol* new_symbol;
  ava_uint level = ava_macsub_get_level(context);
  ava_intr_var_write* definer;
  ava_var_casing casing;
  size_t num_results, i;

  num_results = ava_symtab_get(
    &results, ava_macsub_get_symtab(context), name);

  symbol = NULL;
  for (i = 0; i < num_results; ++i) {
    /* Global symbols when we aren't global can be ignored */
    if (0 == results[i]->level && 0 != level) continue;

    if (!symbol) {
      symbol = results[i];
    } else {
      return ava_macsub_error(
        context, ava_error_ambiguous_var(location, name));
    }
  }

  definer = AVA_NEW(ava_intr_var_write);

  if (symbol) {
    switch (symbol->type) {
    case ava_st_global_variable: break;
    case ava_st_local_variable: break;

    case ava_st_global_function:
    case ava_st_local_function:
      return ava_macsub_error(
        context, ava_error_assignment_to_function(
          location, symbol->full_name));

    case ava_st_control_macro:
    case ava_st_operator_macro:
    case ava_st_function_macro:
      return ava_macsub_error(
        context, ava_error_assignment_to_macro(
          location, symbol->full_name));

    default:
      return ava_macsub_error(
        context, ava_error_assignment_to_other(
          location, symbol->full_name,
          ava_symbol_type_name(symbol)));
    }

    if (!symbol->v.var.is_mutable) {
      return ava_macsub_error(
        context, ava_error_assignment_to_readonly_var(
          location, symbol->full_name));
    }

    if (symbol->level != level) {
      return ava_macsub_error(
        context, ava_error_assignment_to_closed_var(
          location, name));
    }

    new_symbol = NULL;
  } else {
    casing = ava_var_casing_of(name);

    symbol = new_symbol = AVA_NEW(ava_symbol);
    new_symbol->type = level?
      ava_st_local_variable : ava_st_global_variable;
    new_symbol->level = level;
    new_symbol->visibility = ava_var_visibility_of(casing, level);
    new_symbol->definer = (ava_ast_node*)definer;
    new_symbol->full_name = ava_macsub_apply_prefix(context, name);
    new_symbol->v.var.is_mutable = ava_var_is_casing_mutable(casing);
    new_symbol->v.var.name.scheme = ava_nms_ava;
    new_symbol->v.var.name.name = new_symbol->full_name;
    ava_macsub_put_symbol(context, new_symbol, location);
    ava_varscope_put_local(ava_macsub_get_varscope(context), new_symbol);
  }

  definer->header.v = &ava_intr_var_write_vtable;
  definer->header.location = *location;
  definer->header.context = context;
  definer->var = symbol;
  definer->owned_var = new_symbol;
  definer->producer = producer;

  *reader = ava_intr_var_read_new(context, symbol, location);

  return (ava_ast_node*)definer;
}

static ava_var_casing ava_var_casing_of(ava_string name) {
  ava_str_tmpbuff tmp;
  const char* str;
  ava_bool has_upper = ava_false, has_lower = ava_false;
  size_t strlen, i;

  str = ava_string_to_cstring_buff(tmp, name);
  strlen = ava_strlen(name);

  for (i = 0; i < strlen; ++i) {
    has_upper |= (str[i] >= 'A' && str[i] <= 'Z');
    has_lower |= (str[i] >= 'a' && str[i] <= 'z');
  }

  if (has_upper && has_lower)
    return ava_vc_mixed;
  else if (has_upper)
    return ava_vc_upper;
  else
    return ava_vc_lower;
}

static ava_visibility ava_var_visibility_of(ava_var_casing casing,
                                            ava_uint level) {
  if (level)
    return ava_v_private;

  switch (casing) {
  case ava_vc_lower: return ava_v_private;
  case ava_vc_mixed: return ava_v_internal;
  case ava_vc_upper: return ava_v_public;
  }

  /* unreachable */
  abort();
}

static ava_bool ava_var_is_casing_mutable(ava_var_casing casing) {
  switch (casing) {
  case ava_vc_lower: return ava_true;
  case ava_vc_mixed: return ava_false;
  case ava_vc_upper: return ava_false;
  }

  /* unreachable */
  abort();
}

ava_ast_node* ava_intr_var_read_new(
  ava_macsub_context* context, const ava_symbol* symbol,
  const ava_compile_location* location
) {
  ava_intr_var_read* this = AVA_NEW(ava_intr_var_read);
  this->header.v = &ava_intr_var_read_vtable;
  this->header.context = context;
  this->header.location = *location;
  this->var = symbol;
  this->postprocessed = ava_true;
  return (ava_ast_node*)this;
}

ava_macro_subst_result ava_intr_var_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  ava_intr_var_read* this = AVA_NEW(ava_intr_var_read);

  this->header.v = &ava_intr_var_read_vtable;
  this->header.context = context;
  this->header.location = provoker->location;
  this->symtab = ava_macsub_get_symtab(context);

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_BAREWORD(this->name, "variable name");
    }
  }

  if (ava_string_equal(AVA_ASCII9_STRING("$"), this->name)) {
    this->var = ava_macsub_get_context_var(context);
    if (!this->var) {
      return ava_macsub_error_result(
        context, ava_error_no_context_var(&this->header.location));
    }
  }

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static ava_string ava_intr_var_read_to_string(const ava_intr_var_read* node) {
  ava_string accum;

  accum = AVA_ASCII9_STRING("var-read(");
  if (node->var)
    accum = ava_strcat(accum, node->var->full_name);
  else
    accum = ava_strcat(accum, node->name);
  accum = ava_strcat(accum, AVA_ASCII9_STRING(")"));

  return accum;
}

static ava_ast_node* ava_intr_var_read_to_lvalue(
  const ava_intr_var_read* node, ava_ast_node* producer,
  ava_ast_node** reader
) {
  ava_ast_node* error = ava_macsub_error(
    node->header.context,
    ava_error_assignment_to_var_read(&node->header.location));
  *reader = error;
  return error;
}

static void ava_intr_var_read_postprocess(ava_intr_var_read* node) {
  const ava_symbol** results;
  size_t num_results;

  if (node->postprocessed) return;
  node->postprocessed = ava_true;

  /* Variable already provided if this is the reader of an lvalue or if this is
   * the context variable.
   */
  if (!node->var) {
    num_results = ava_symtab_get(&results, node->symtab, node->name);

    if (0 == num_results) {
      ava_macsub_record_error(node->header.context,
                              ava_error_no_such_var(
                                &node->header.location, node->name));
      return;
    } else if (num_results > 1) {
      ava_macsub_record_error(node->header.context,
                              ava_error_ambiguous_var(
                                &node->header.location, node->name));
      return;
    } else {
      node->var = results[0];
    }
  }

  switch (node->var->type) {
  case ava_st_global_variable:
    break;

  case ava_st_local_variable:
    ava_varscope_ref_var(
      ava_macsub_get_varscope(node->header.context), node->var);
    break;

  case ava_st_global_function:
  case ava_st_local_function:
    ava_varscope_ref_scope(
      ava_macsub_get_varscope(node->header.context), node->var->v.var.scope);
    break;

  case ava_st_control_macro:
  case ava_st_operator_macro:
  case ava_st_function_macro:
    ava_macsub_record_error(node->header.context,
                            ava_error_use_of_macro_as_var(
                              &node->header.location, node->var->full_name));
    return;

  default:
    ava_macsub_record_error(
      node->header.context,
      ava_error_use_of_other_as_var(
        &node->header.location, node->var->full_name,
        ava_symbol_type_name(node->var)));
  }
}

static void ava_intr_var_read_cg_evaluate(
  ava_intr_var_read* node, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_pcode_register var_reg, fun_reg, data_reg;
  ava_pcode_register_index data_base;

  switch (node->var->type) {
  case ava_st_global_variable:
  case ava_st_global_function:
    ava_ast_node_cg_define(node->var->definer, context);
    ava_codegen_set_location(context, &node->header.location);
    AVA_PCXB(ld_glob, *dst, node->var->pcode_index);
    break;

  case ava_st_local_variable:
    var_reg.type = ava_prt_var;
    var_reg.index = ava_varscope_get_index(
      ava_macsub_get_varscope(node->header.context), node->var);
    ava_codegen_set_location(context, &node->header.location);
    AVA_PCXB(ld_reg_s, *dst, var_reg);
    break;

  case ava_st_local_function: {
    const ava_varscope* localscope =
      ava_macsub_get_varscope(node->header.context);
    const ava_varscope* funscope = node->var->v.var.scope;
    size_t i, num_captures = ava_varscope_num_captures(funscope);
    const ava_symbol* captures[num_captures];

    ava_ast_node_cg_define(node->var->definer, context);
    ava_codegen_set_location(context, &node->header.location);

    if (num_captures) {
      ava_varscope_get_vars(captures, funscope, num_captures);

      fun_reg.type = ava_prt_function;
      fun_reg.index = ava_codegen_push_reg(context, ava_prt_function, 1);
      data_reg.type = ava_prt_data;
      data_base = ava_codegen_push_reg(context, ava_prt_data, num_captures + 1);

      data_reg.index = data_base + num_captures;
      ava_codegen_set_location(context, &node->header.location);
      AVA_PCXB(ld_glob, data_reg, node->var->pcode_index);
      AVA_PCXB(ld_reg_d, fun_reg, data_reg);

      var_reg.type = ava_prt_var;
      for (i = 0; i < num_captures; ++i) {
        var_reg.index = ava_varscope_get_index(localscope, captures[i]);
        data_reg.index = data_base + i;
        AVA_PCXB(ld_reg_s, data_reg, var_reg);
      }

      AVA_PCXB(partial, fun_reg, fun_reg, data_base, num_captures);
      AVA_PCXB(ld_reg_u, *dst, fun_reg);

      ava_codegen_pop_reg(context, ava_prt_data, num_captures + 1);
      ava_codegen_pop_reg(context, ava_prt_function, 1);
    } else {
      AVA_PCXB(ld_glob, *dst, node->var->pcode_index);
    }
  } break;

  default:
    /* unreachable */
    abort();
  }
}

static ava_string ava_intr_var_write_to_string(const ava_intr_var_write* node) {
  AVA_STATIC_STRING(base, "var-write(");
  ava_string accum;

  accum = base;
  accum = ava_strcat(accum, node->var->full_name);
  accum = ava_strcat(accum, AVA_ASCII9_STRING(" = "));
  accum = ava_strcat(accum, ava_ast_node_to_string(node->producer));
  accum = ava_strcat(accum, AVA_ASCII9_STRING(")"));
  return accum;
}

static void ava_intr_var_write_postprocess(ava_intr_var_write* node) {
  ava_ast_node_postprocess(node->producer);
}

static void ava_intr_var_write_cg_evaluate(
  ava_intr_var_write* node, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_pcode_register reg;

  ava_ast_node_cg_define(node->var->definer, context);

  if (ava_st_local_variable != node->var->type) {
    reg.type = ava_prt_data;
    reg.index = ava_codegen_push_reg(context, ava_prt_data, 1);
  } else {
    reg.type = ava_prt_var;
    reg.index = ava_varscope_get_index(
      ava_macsub_get_varscope(node->header.context), node->var);
  }
  ava_ast_node_cg_evaluate(node->producer, &reg, context);

  ava_codegen_set_location(context, &node->header.location);
  if (ava_st_global_variable == node->var->type) {
    AVA_PCXB(set_glob, node->var->pcode_index, reg);
  } else {
    assert(ava_st_local_variable == node->var->type);
    /* Nothing else to do with the var */
  }

  if (dst) AVA_PCXB(ld_reg_s, *dst, reg);

  if (ava_st_local_variable != node->var->type)
    ava_codegen_pop_reg(context, ava_prt_data, 1);
}

static void ava_intr_var_write_cg_discard(
  ava_intr_var_write* node, ava_codegen_context* context
) {
  ava_intr_var_write_cg_evaluate(node, NULL, context);
}

static void ava_intr_var_write_cg_define(
  ava_intr_var_write* node, ava_codegen_context* context
) {
  if (!node->defined && node->owned_var &&
      ava_st_global_variable == node->owned_var->type) {
    node->defined = ava_true;

    ava_codegen_set_global_location(context, &node->header.location);
    node->owned_var->pcode_index =
      AVA_PCGB(var, ava_v_private != node->owned_var->visibility,
               node->owned_var->v.var.name);
    ava_codegen_export(context, node->owned_var);
  }
}

static void ava_intr_var_write_cg_set_up(
  ava_intr_var_write* node, ava_codegen_context* context
) {
  ava_ast_node_cg_set_up(node->producer, context);
}

static void ava_intr_var_write_cg_tear_down(
  ava_intr_var_write* node, ava_codegen_context* context
) {
  ava_ast_node_cg_tear_down(node->producer, context);
}

ava_macro_subst_result ava_intr_set_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  const ava_parse_unit* target_unit = NULL, * expression_unit = NULL;
  ava_ast_node* target, * expression, * ignore_reader, * result, ** reader;
  ava_intr_var_ucs* ucs;
  ava_symbol* context_var;
  ava_macsub_context* expr_context;
  ava_bool is_update;

  is_update = !!self->v.macro.userdata;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_UNIT(target_unit, "target");
      AVA_MACRO_ARG_UNIT(expression_unit, "expression");
    }
  }

  if (is_update) {
    ava_macsub_gensym_seed(context, &provoker->location);
    context_var = ava_var_gen(
      context, AVA_ASCII9_STRING("$"), &provoker->location);
    expr_context = ava_macsub_context_with_context_var(context, context_var);
  } else {
    expr_context = context;
  }

  target = ava_macsub_run_units(context, target_unit, target_unit);
  expression = ava_macsub_run_units(
    expr_context, expression_unit, expression_unit);
  if (is_update) {
    ucs = ava_intr_var_ucs_new(expression);
    expression = (ava_ast_node*)ucs;
    reader = &ucs->reader;
  } else {
    reader = &ignore_reader;
  }
  result = ava_ast_node_to_lvalue(target, expression, reader);

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = result },
  };
}

static ava_string ava_intr_var_ucs_to_string(const ava_intr_var_ucs* node) {
  ava_string accum;

  accum = AVA_ASCII9_STRING("(ucs ");
  accum = ava_strcat(accum, ava_ast_node_to_string(node->reader));
  accum = ava_strcat(accum, AVA_ASCII9_STRING(") "));
  accum = ava_strcat(accum, ava_ast_node_to_string(node->delegate));
  return accum;
}

static void ava_intr_var_ucs_postprocess(ava_intr_var_ucs* node) {
  ava_symbol* context_var;

  if (node->postprocessed) return;
  node->postprocessed = ava_true;

  context_var = ava_macsub_get_context_var(node->header.context);

  if (ava_st_local_variable == context_var->type) {
    ava_varscope_ref_var(ava_macsub_get_varscope(node->header.context),
                         context_var);
  }
  ava_ast_node_postprocess(node->reader);
  ava_ast_node_postprocess(node->delegate);
}

static void ava_intr_var_ucs_cg_evaluate(
  ava_intr_var_ucs* node, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_pcode_register reg;
  ava_symbol* context_var;

  context_var = ava_macsub_get_context_var(node->header.context);

  if (ava_st_local_variable == context_var->type) {
    reg.type = ava_prt_var;
    reg.index = ava_varscope_get_index(
      ava_macsub_get_varscope(node->header.context),
      context_var);

    ava_ast_node_cg_evaluate(node->reader, &reg, context);
  } else {
    reg.type = ava_prt_data;
    reg.index = ava_codegen_push_reg(context, ava_prt_data, 1);
    ava_ast_node_cg_evaluate(node->reader, &reg, context);
    ava_ast_node_cg_define(context_var->definer, context);
    AVA_PCXB(set_glob, context_var->pcode_index, reg);
    ava_codegen_pop_reg(context, ava_prt_data, 1);
  }

  ava_ast_node_cg_evaluate(node->delegate, dst, context);
}

static ava_intr_var_ucs* ava_intr_var_ucs_new(ava_ast_node* delegate) {
  ava_intr_var_ucs* this;

  this = AVA_NEW(ava_intr_var_ucs);
  this->header.context = delegate->context;
  this->header.location = delegate->location;
  this->header.v = &ava_intr_var_ucs_vtable;
  this->delegate = delegate;

  return this;
}

ava_symbol* ava_var_gen(
  ava_macsub_context* context, ava_string key,
  const ava_compile_location* location
) {
  unsigned level;
  ava_string name;
  ava_symbol* symbol;

  name = ava_macsub_gensym(context, key);
  level = ava_macsub_get_level(context);

  symbol = AVA_NEW(ava_symbol);
  symbol->type = level?
    ava_st_local_variable : ava_st_global_variable;
  symbol->level = ava_macsub_get_level(context);
  symbol->visibility = ava_v_private;
  symbol->definer = level? NULL :
    (ava_ast_node*)ava_intr_genvar_definer_new(context, location, symbol);
  symbol->full_name = ava_macsub_apply_prefix(context, name);
  symbol->v.var.is_mutable = ava_true;
  symbol->v.var.name.scheme = ava_nms_ava;
  symbol->v.var.name.name = symbol->full_name;
  ava_macsub_put_symbol(context, symbol, location);
  if (level)
    ava_varscope_put_local(ava_macsub_get_varscope(context), symbol);

  return symbol;
}

static ava_string ava_intr_genvar_definer_to_string(
  const ava_intr_genvar_definer* node
) {
  AVA_STATIC_STRING(prefix, "<definer for ");

  return ava_strcat(
    prefix, ava_strcat(
      node->symbol->full_name, AVA_ASCII9_STRING(">")));
}

static void ava_intr_genvar_definer_cg_define(
  ava_intr_genvar_definer* node, ava_codegen_context* context
) {
  if (!node->defined) {
    node->defined = ava_true;

    ava_codegen_set_global_location(context, &node->header.location);
    node->symbol->pcode_index =
      AVA_PCGB(var, ava_false, node->symbol->v.var.name);
  }
}

static ava_intr_genvar_definer* ava_intr_genvar_definer_new(
  ava_macsub_context* context, const ava_compile_location* location,
  ava_symbol* sym
) {
  ava_intr_genvar_definer* this;

  this = AVA_NEW(ava_intr_genvar_definer);
  this->header.context = context;
  this->header.location = *location;
  this->header.v = &ava_intr_genvar_definer_vtable;
  this->symbol = sym;
  this->defined = ava_false;


  return this;
}
