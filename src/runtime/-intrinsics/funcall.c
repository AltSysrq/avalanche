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
#include "../avalanche/value.h"
#include "../avalanche/errors.h"
#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"
#include "../avalanche/symbol.h"
#include "../avalanche/symtab.h"
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/varscope.h"
#include "fundamental.h"
#include "funcall.h"

typedef enum {
  /**
   * The function is known at compile time, as is the binding of all its
   * arguments.
   */
  ava_ift_static_bind,
  /**
   * The function is known at compile time, but the binding of its arguments is
   * not.
   */
  ava_ift_dynamic_bind,
  /**
   * The identity of the function (and therefore also the binding of its
   * arguments) is not known at compile time.
   */
  ava_ift_dynamic_invoke
} ava_intr_funcall_type;

typedef struct {
  ava_ast_node header;

  /* "parms" includes the function itself */
  size_t num_parms;
  ava_ast_node** parms;

  ava_symtab* symtab;

  /* The below fields are populated upon postprocessing */
  ava_intr_funcall_type type;
  /* For all types but dynamic_invoke, the function to invoke. */
  const ava_symbol* static_function;
  /* For afa_ift_static_bind */
  ava_function_bound_argument* bound_args;
  size_t* variadic_collection;
} ava_intr_funcall;

static ava_string ava_intr_funcall_to_string(const ava_intr_funcall* node);
static void ava_intr_funcall_postprocess(ava_intr_funcall* node);
static void ava_intr_funcall_cg_evaluate(
  ava_intr_funcall* node,
  const ava_pcode_register* dst,
  ava_codegen_context* context);
static void ava_intr_funcall_cg_discard(
  ava_intr_funcall* node, ava_codegen_context* context);

static void ava_intr_funcall_bind_parms(ava_intr_funcall* node);

static const ava_ast_node_vtable ava_intr_funcall_vtable = {
  .name = "function call",
  .to_string = (ava_ast_node_to_string_f)ava_intr_funcall_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_funcall_postprocess,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_funcall_cg_evaluate,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_funcall_cg_discard,
};

ava_ast_node* ava_intr_funcall_of(
  ava_macsub_context* context,
  const ava_parse_statement* statement
) {
  ava_intr_funcall* this;
  size_t num_parms, i;
  const ava_parse_unit* parm;

  num_parms = 0;
  TAILQ_FOREACH(parm, &statement->units, next)
    ++num_parms;

  assert(num_parms > 1);

  this = AVA_NEW(ava_intr_funcall);
  this->header.v = &ava_intr_funcall_vtable;
  this->header.location = ava_compile_location_span(
    &TAILQ_FIRST(&statement->units)->location,
    &TAILQ_LAST(&statement->units, ava_parse_unit_list_s)->location);
  this->header.context = context;

  this->symtab = ava_macsub_get_symtab(context);
  this->num_parms = num_parms;
  this->parms = ava_alloc(num_parms * sizeof(ava_ast_node*));
  i = 0;
  TAILQ_FOREACH(parm, &statement->units, next)
    this->parms[i++] = ava_macsub_run_units(context, parm, parm);

  return (ava_ast_node*)this;
}

ava_ast_node* ava_intr_funcall_make(
  ava_macsub_context* context,
  ava_ast_node*const* parms,
  size_t num_parms
) {
  ava_intr_funcall* this;

  assert(num_parms > 1);

  this = AVA_NEW(ava_intr_funcall);
  this->header.v = &ava_intr_funcall_vtable;
  this->header.location = ava_compile_location_span(
    &parms[0]->location, &parms[num_parms-1]->location);
  this->header.context = context;

  this->symtab = ava_macsub_get_symtab(context);
  this->num_parms = num_parms;
  this->parms = ava_clone(parms, num_parms * sizeof(ava_ast_node*));

  return (ava_ast_node*)this;
}

static ava_string ava_intr_funcall_to_string(const ava_intr_funcall* this) {
  ava_string accum;
  size_t i;

  accum = AVA_ASCII9_STRING("call { ");
  for (i = 0; i < this->num_parms; ++i) {
    accum = ava_string_concat(
      accum, ava_ast_node_to_string(this->parms[i]));
    accum = ava_string_concat(
      accum, AVA_ASCII9_STRING("; "));
  }
  accum = ava_string_concat(accum, AVA_ASCII9_STRING("}"));
  return accum;
}

static void ava_intr_funcall_postprocess(ava_intr_funcall* this) {
  size_t num_results, i;
  ava_string function_name;
  const ava_symbol* function_symbol, ** results;

  /* In case of early return, ensure type is set to something that makes the
   * whole structure valid.
   */
  this->type = ava_ift_dynamic_invoke;

  for (i = 0; i < this->num_parms; ++i)
    ava_ast_node_postprocess(this->parms[i]);

  function_name = ava_ast_node_get_funname(this->parms[0]);
  if (!ava_string_is_present(function_name)) {
    this->type = ava_ift_dynamic_invoke;
    return;
  }

  num_results = ava_symtab_get(&results, this->symtab, function_name);

  if (num_results > 1) {
    /* TODO: Indicate what the ambiguity is */
    ava_macsub_record_error(
      this->header.context, ava_error_ambiguous_function(
        &this->parms[0]->location, function_name));
    return;
  } else if (0 == num_results) {
    ava_macsub_record_error(
      this->header.context, ava_error_no_such_function(
        &this->parms[0]->location, function_name));
    return;
  }

  function_symbol = results[0];
  if (ava_st_global_function != function_symbol->type &&
      ava_st_local_function != function_symbol->type) {
    ava_macsub_record_error(
      this->header.context, ava_error_not_a_function(
        &this->parms[0]->location, function_name));
    return;
  }

  this->static_function = function_symbol;
  ava_varscope_ref_scope(ava_macsub_get_varscope(this->header.context),
                         function_symbol->v.var.scope);
  ava_intr_funcall_bind_parms(this);
}

static void ava_intr_funcall_bind_parms(ava_intr_funcall* this) {
  ava_function_parameter parms[this->num_parms-1];
  ava_function_bound_argument bound_args[
    this->static_function->v.var.fun.num_args];
  size_t variadic_collection[this->num_parms-1];
  ava_string bind_message;
  size_t i;

  for (i = 0; i < this->num_parms - 1; ++i) {
    if (this->parms[i+1]->v->cg_spread) {
      parms[i].type = ava_fpt_spread;
    } else if (ava_ast_node_get_constexpr(this->parms[i+1], &parms[i].value)) {
      parms[i].type = ava_fpt_static;
    } else {
      parms[i].type = ava_fpt_dynamic;
    }
  }

  switch (ava_function_bind(&this->static_function->v.var.fun,
                            this->num_parms - 1,
                            parms, bound_args, variadic_collection,
                            &bind_message)) {
  case ava_fbs_bound:
    this->type = ava_ift_static_bind;
    this->bound_args = ava_clone(bound_args, sizeof(bound_args));
    this->variadic_collection = ava_clone(
      variadic_collection, sizeof(variadic_collection));
    break;

  case ava_fbs_unknown:
  case ava_fbs_unpack:
    this->type = ava_ift_dynamic_bind;
    break;

  case ava_fbs_impossible:
    /* TODO: More precicely indicate the location of the error */
    ava_macsub_record_error(this->header.context,
                            ava_compile_error_new(
                              bind_message, &this->header.location));
    this->type = ava_ift_dynamic_bind;
    break;
  }
}

static void ava_intr_funcall_cg_discard(
  ava_intr_funcall* funcall,
  ava_codegen_context* context
) {
  ava_pcode_register ret_tmp;

  ret_tmp.type = ava_prt_data;
  ret_tmp.index = ava_codegen_push_reg(context, ava_prt_data, 1);
  ava_intr_funcall_cg_evaluate(funcall, &ret_tmp, context);
  ava_codegen_pop_reg(context, ava_prt_data, 1);
}

static void ava_intr_funcall_cg_evaluate(
  ava_intr_funcall* funcall,
  const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_pcode_register_index parm_base;
  ava_pcode_register parm_reg, list_reg;
  ava_uint i;
  ava_bool static_fun =
    ava_ift_dynamic_invoke != funcall->type;

  parm_reg.type = ava_prt_data;
  parm_base = ava_codegen_push_reg(
    context, ava_prt_data, funcall->num_parms - static_fun);

  if (static_fun)
    ava_ast_node_cg_define(funcall->static_function->definer, context);

  /* Evaluate all parms from left-to-right */
  for (i = 0; i < funcall->num_parms - static_fun; ++i) {
    parm_reg.index = parm_base + i;
    if (funcall->parms[i + static_fun]->v->cg_spread) {
      list_reg.type = ava_prt_list;
      list_reg.index = ava_codegen_push_reg(context, ava_prt_list, 1);
      ava_ast_node_cg_spread(
        funcall->parms[i + static_fun], &list_reg, context);
      AVA_PCXB(ld_reg, parm_reg, list_reg);
      ava_codegen_pop_reg(context, ava_prt_list, 1);
    } else {
      ava_ast_node_cg_evaluate(funcall->parms[i + static_fun],
                               &parm_reg, context);
    }
  }

  ava_codegen_set_location(context, &funcall->header.location);

  switch (funcall->type) {
  case ava_ift_static_bind: {
    ava_pcode_register_index arg_base;
    ava_pcode_register arg_reg, var_reg;
    const ava_varscope* localscope =
      ava_macsub_get_varscope(funcall->header.context);
    const ava_varscope* funscope = funcall->static_function->v.var.scope;
    size_t num_captures = ava_varscope_num_captures(funscope);
    const ava_symbol* captures[num_captures];

    arg_reg.type = ava_prt_data;
    arg_base = ava_codegen_push_reg(
      context, ava_prt_data,
      num_captures + funcall->static_function->v.var.fun.num_args);

    /* Copy captures */
    ava_varscope_get_vars(captures, funscope, num_captures);
    var_reg.type = ava_prt_var;
    for (i = 0; i < num_captures; ++i) {
      var_reg.index = ava_varscope_get_index(localscope, captures[i]);
      arg_reg.index = arg_base + i;
      AVA_PCXB(ld_reg, arg_reg, var_reg);
    }

    /* Map parms to args */
    for (i = 0; i < funcall->static_function->v.var.fun.num_args; ++i) {
      arg_reg.index = arg_base + i + num_captures;
      switch (funcall->bound_args[i].type) {
      case ava_fbat_implicit:
        AVA_PCXB(ld_imm_vd, arg_reg,
                 ava_to_string(funcall->bound_args[i].v.value));
        break;

      case ava_fbat_parameter:
        parm_reg.index = parm_base + funcall->bound_args[i].v.parameter_index;
        AVA_PCXB(ld_reg, arg_reg, parm_reg);
        break;

      case ava_fbat_collect: {
        ava_pcode_register accum, tmplist;
        size_t j;

        accum.type = ava_prt_list;
        accum.index = ava_codegen_push_reg(context, ava_prt_list, 2);
        tmplist.type = ava_prt_list;
        tmplist.index = accum.index + 1;

        if (0 == funcall->bound_args[i].v.collection_size) {
          AVA_PCXB(lempty, accum);
        } else {
          parm_reg.index = parm_base + funcall->variadic_collection[0];
          if (funcall->parms[
                funcall->variadic_collection[0] + 1]->v->cg_spread) {
            AVA_PCXB(ld_reg, accum, parm_reg);
          } else {
            AVA_PCXB(lempty, accum);
            AVA_PCXB(lappend, accum, accum, parm_reg);
          }

          for (j = 1; j < funcall->bound_args[i].v.collection_size; ++j) {
            parm_reg.index = parm_base + funcall->variadic_collection[j];
            if (funcall->parms[
                  funcall->variadic_collection[j] + 1]->v->cg_spread) {
              AVA_PCXB(ld_reg, tmplist, parm_reg);
              AVA_PCXB(lcat, accum, accum, tmplist);
            } else {
              AVA_PCXB(lappend, accum, accum, parm_reg);
            }
          }
        }

        AVA_PCXB(ld_reg, arg_reg, accum);

        ava_codegen_pop_reg(context, ava_prt_list, 2);
      } break;
      }
    }

    AVA_PCXB(invoke_ss, *dst, funcall->static_function->pcode_index,
             arg_base,
             funcall->static_function->v.var.fun.num_args + num_captures);

    ava_codegen_pop_reg(
      context, ava_prt_data,
      funcall->static_function->v.var.fun.num_args + num_captures);
  } break;

  case ava_ift_dynamic_bind: {
    ava_pcode_register_index preg_base;
    ava_pcode_register preg, var_reg;
    const ava_varscope* localscope =
      ava_macsub_get_varscope(funcall->header.context);
    const ava_varscope* funscope = funcall->static_function->v.var.scope;
    size_t num_captures = ava_varscope_num_captures(funscope);
    const ava_symbol* captures[num_captures];

    ava_codegen_set_location(context, &funcall->header.location);
    preg_base = ava_codegen_push_reg(
      context, ava_prt_parm, funcall->num_parms - 1 + num_captures);
    preg.type = ava_prt_parm;

    /* Copy captures */
    ava_varscope_get_vars(captures, funscope, num_captures);
    var_reg.type = ava_prt_var;
    for (i = 0; i < num_captures; ++i) {
      var_reg.index = ava_varscope_get_index(localscope, captures[i]);
      preg.index = preg_base + i;
      AVA_PCXB(ld_parm, preg, var_reg, ava_false);
    }

    /* Copy parms */
    for (i = 0; i < funcall->num_parms - 1; ++i) {
      preg.index = preg_base + i + num_captures;
      parm_reg.index = parm_base + i;
      AVA_PCXB(ld_parm, preg, parm_reg, !!funcall->parms[i+1]->v->cg_spread);
    }

    AVA_PCXB(invoke_sd, *dst, funcall->static_function->pcode_index,
             preg_base, funcall->num_parms - 1 + num_captures);
    ava_codegen_pop_reg(context, ava_prt_parm,
                        funcall->num_parms - 1 + num_captures);
  } break;

  case ava_ift_dynamic_invoke: {
    ava_pcode_register fun_reg;
    ava_pcode_register_index preg_base;
    ava_pcode_register preg;
    ava_pcode_register rlist_reg;
    ava_pcode_register tmp_reg;
    ava_uint normal_parm_offset;
    ava_bool extra_parm;

    ava_codegen_set_location(context, &funcall->header.location);

    fun_reg.type = ava_prt_function;
    fun_reg.index = ava_codegen_push_reg(context, ava_prt_function, 1);
    preg.type = ava_prt_parm;
    preg_base = ava_codegen_push_reg(context, ava_prt_parm, funcall->num_parms);

    if (!funcall->parms[0]->v->cg_spread) {
      /* Simple case: The function is a lone expression */
      parm_reg.index = parm_base;
      AVA_PCXB(ld_reg, fun_reg, parm_reg);
      normal_parm_offset = 1;
      extra_parm = ava_false;
    } else {
      /* The function to call comes from a spread.
       *
       * We can't just take the first parm and disassemble it, because if the
       * first parm is empty, the function will come from the *next* parm, and
       * so on.
       *
       * Instead, accumulate parms into a list until we encounter one that
       * isn't spread (which must be appended to the accumulation) or run out
       * of parameters, then disassemble that.
       */
      extra_parm = ava_true;
      list_reg.type = ava_prt_list;
      list_reg.index = ava_codegen_push_reg(context, ava_prt_list, 2);
      rlist_reg.type = ava_prt_list;
      rlist_reg.index = list_reg.index + 1;
      parm_reg.index = parm_base;
      /* Accumulatign leading spreads and first non-spread (if any) into a
       * single list.
       */
      AVA_PCXB(ld_reg, list_reg, parm_reg);
      for (normal_parm_offset = 1;
           normal_parm_offset < funcall->num_parms; ++normal_parm_offset) {
        parm_reg.index = parm_base + normal_parm_offset;
        if (funcall->parms[normal_parm_offset]->v->cg_spread) {
          AVA_PCXB(ld_reg, rlist_reg, parm_reg);
          AVA_PCXB(lcat, list_reg, list_reg, rlist_reg);
        } else {
          AVA_PCXB(lappend, list_reg, list_reg, parm_reg);
          ++normal_parm_offset;
          break;
        }
      }

      tmp_reg.type = ava_prt_data;
      tmp_reg.index = ava_codegen_push_reg(context, ava_prt_data, 1);
      /* The function is the first element in the list; error if empty */
      AVA_PCXB(lhead, tmp_reg, list_reg);
      AVA_PCXB(ld_reg, fun_reg, tmp_reg);

      /* The rest becomes the first spread argument to the function */
      AVA_PCXB(lbehead, list_reg, list_reg);
      AVA_PCXB(ld_reg, tmp_reg, list_reg);
      preg.index = preg_base + normal_parm_offset - 1;
      AVA_PCXB(ld_parm, preg, tmp_reg, ava_true);
      ava_codegen_pop_reg(context, ava_prt_data, 1);

      ava_codegen_pop_reg(context, ava_prt_list, 2);
    }

    /* Anything not consumed by the above forms the rest of the parms */
    for (i = normal_parm_offset; i < funcall->num_parms; ++i) {
      preg.index = preg_base + i;
      parm_reg.index = parm_base + i;
      AVA_PCXB(ld_parm, preg, parm_reg, !!funcall->parms[i]->v->cg_spread);
    }

    AVA_PCXB(invoke_dd, *dst, fun_reg,
             preg_base + normal_parm_offset - extra_parm,
             funcall->num_parms - normal_parm_offset + extra_parm);

    ava_codegen_pop_reg(context, ava_prt_parm, funcall->num_parms);
    ava_codegen_pop_reg(context, ava_prt_function, 1);
  } break;
  }

  ava_codegen_pop_reg(context, ava_prt_data, funcall->num_parms - static_fun);
}
