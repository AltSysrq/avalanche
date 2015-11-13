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
#include "funcall.h"
#include "reg-rvalue.h"
#include "fundamental.h"
#include "subscript.h"

typedef struct {
  ava_ast_node header;

  ava_ast_node* getter_fun, * wither_fun;
  ava_ast_node* getter, * wither, * composite, * key;
  ava_ast_node* lvalue_producer;
  ava_intr_reg_rvalue* evaluated_composite;
  ava_intr_reg_rvalue* evaluated_key;
  ava_intr_reg_rvalue* evaluated_producer;

  ava_bool composite_is_bareword;
  ava_bool postprocessed;
} ava_intr_subscript;

static ava_string ava_intr_subscript_to_string(
  const ava_intr_subscript* this);
static ava_ast_node* ava_intr_subscript_to_lvalue(
  const ava_intr_subscript* this, ava_ast_node* producer,
  ava_ast_node** reader);
static void ava_intr_subscript_postprocess(
  ava_intr_subscript* this);
static void ava_intr_subscript_cg_set_up(
  ava_intr_subscript* this, ava_codegen_context* context);
static void ava_intr_subscript_cg_evaluate(
  ava_intr_subscript* this, const ava_pcode_register* dst,
  ava_codegen_context* context);
static void ava_intr_subscript_cg_tear_down(
  ava_intr_subscript* this, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_subscript_vtable = {
  .name = "subscript",
  .to_string = (ava_ast_node_to_string_f)ava_intr_subscript_to_string,
  .to_lvalue = (ava_ast_node_to_lvalue_f)ava_intr_subscript_to_lvalue,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_subscript_postprocess,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_subscript_cg_evaluate,
  .cg_set_up = (ava_ast_node_cg_set_up_f)ava_intr_subscript_cg_set_up,
  .cg_tear_down = (ava_ast_node_cg_tear_down_f)ava_intr_subscript_cg_tear_down,
};

ava_macro_subst_result ava_intr_subscript_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  ava_intr_subscript* this;
  const ava_parse_unit* type_unit, * composite_unit, * key_unit;
  ava_parse_unit* getter_name_unit, * wither_name_unit;
  ava_string type_prefix, type_suffix;
  ava_ast_node* getter_fun_parms[3];

  type_prefix = ava_string_of_cstring(self->v.macro.userdata);

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_CURRENT_UNIT(type_unit, "type");
      AVA_MACRO_ARG_BAREWORD(type_suffix, "type");
      AVA_MACRO_ARG_UNIT(composite_unit, "composite");
      AVA_MACRO_ARG_UNIT(key_unit, "index/key");
    }
  }

  getter_name_unit = AVA_CLONE(*type_unit);
  getter_name_unit->v.string = ava_string_concat(
    ava_string_concat(type_prefix, AVA_ASCII9_STRING("get")),
    type_suffix);
  wither_name_unit = AVA_CLONE(*type_unit);
  wither_name_unit->v.string = ava_string_concat(
    ava_string_concat(type_prefix, AVA_ASCII9_STRING("with")),
    type_suffix);

  this = AVA_NEW(ava_intr_subscript);
  this->header.v = &ava_intr_subscript_vtable;
  this->header.context = context;
  this->header.location = provoker->location;
  this->getter_fun = ava_intr_unit(context, getter_name_unit);
  this->wither_fun = ava_intr_unit(context, wither_name_unit);
  this->evaluated_composite =
    ava_alloc(sizeof(ava_intr_reg_rvalue) * 3);
  this->evaluated_key = this->evaluated_composite + 1;
  this->evaluated_producer = this->evaluated_composite + 2;
  ava_intr_reg_rvalue_init(this->evaluated_composite, context);
  ava_intr_reg_rvalue_init(this->evaluated_key, context);
  ava_intr_reg_rvalue_init(this->evaluated_producer, context);

  getter_fun_parms[0] = this->getter_fun;
  getter_fun_parms[1] = (ava_ast_node*)this->evaluated_composite;
  getter_fun_parms[2] = (ava_ast_node*)this->evaluated_key;
  this->getter = ava_intr_funcall_make(context, getter_fun_parms, 3);
  this->composite = ava_macsub_run_units(
    context, composite_unit, composite_unit);
  this->key = ava_macsub_run_units(context, key_unit, key_unit);
  this->composite_is_bareword =
    ava_parse_unit_is_essentially_bareword(composite_unit);

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static ava_string ava_intr_subscript_to_string(const ava_intr_subscript* this) {
  ava_string accum;

  accum = AVA_ASCII9_STRING("subscript");
  accum = ava_string_concat(accum, AVA_ASCII9_STRING(" get = "));
  accum = ava_string_concat(accum, ava_ast_node_to_string(this->getter));
  if (this->wither) {
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("; with = "));
    accum = ava_string_concat(accum, ava_ast_node_to_string(this->wither));
  }
  accum = ava_string_concat(accum, AVA_ASCII9_STRING("; comp = "));
  accum = ava_string_concat(accum, ava_ast_node_to_string(this->composite));
  accum = ava_string_concat(accum, AVA_ASCII9_STRING("; key = "));
  accum = ava_string_concat(accum, ava_ast_node_to_string(this->key));
  if (this->lvalue_producer) {
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("; lvp = "));
    accum = ava_string_concat(accum, ava_ast_node_to_string(
                                this->lvalue_producer));
  }
  return accum;
}

static ava_ast_node* ava_intr_subscript_to_lvalue(
  const ava_intr_subscript* rvalue, ava_ast_node* producer,
  ava_ast_node** reader
) {
  ava_intr_subscript* lvalue;
  ava_ast_node* wither_parms[4];

  lvalue = AVA_CLONE(*rvalue);

  wither_parms[0] = lvalue->wither_fun;
  wither_parms[1] = (ava_ast_node*)lvalue->evaluated_composite;
  wither_parms[2] = (ava_ast_node*)lvalue->evaluated_key;
  wither_parms[3] = (ava_ast_node*)lvalue->evaluated_producer;
  lvalue->wither = ava_intr_funcall_make(
    lvalue->header.context, wither_parms, 4);
  lvalue->lvalue_producer = producer;
  *reader = lvalue->getter;

  return ava_ast_node_to_lvalue(
    lvalue->composite, (ava_ast_node*)lvalue, &lvalue->composite);
}

static void ava_intr_subscript_postprocess(
  ava_intr_subscript* this
) {
  if (this->postprocessed) return;
  this->postprocessed = ava_true;

  ava_ast_node_postprocess(this->getter);
  if (this->wither)
    ava_ast_node_postprocess(this->wither);
  ava_ast_node_postprocess(this->composite);
  ava_ast_node_postprocess(this->key);
  if (this->lvalue_producer)
    ava_ast_node_postprocess(this->lvalue_producer);

  if (this->composite_is_bareword && !this->wither)
    ava_macsub_record_error(
      this->header.context,
      ava_error_subscripted_composite_is_bareword(
        &this->composite->location));
}

static void ava_intr_subscript_cg_set_up(
  ava_intr_subscript* this, ava_codegen_context* context
) {
  ava_pcode_register_index reg_base;

  reg_base = ava_codegen_push_reg(
    context, ava_prt_data, 2 + !!this->lvalue_producer);
  this->evaluated_composite->reg.type = ava_prt_data;
  this->evaluated_composite->reg.index = reg_base + 0;
  this->evaluated_key->reg.type = ava_prt_data;
  this->evaluated_key->reg.index = reg_base + 1;
  this->evaluated_producer->reg.type = ava_prt_data;
  this->evaluated_producer->reg.index = reg_base + 2;

  ava_ast_node_cg_evaluate(
    this->composite, &this->evaluated_composite->reg, context);
  ava_ast_node_cg_evaluate(
    this->key, &this->evaluated_key->reg, context);

  if (this->lvalue_producer)
    ava_ast_node_cg_set_up(this->lvalue_producer, context);
}

static void ava_intr_subscript_cg_evaluate(
  ava_intr_subscript* this, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  if (this->lvalue_producer)
    ava_ast_node_cg_evaluate(
      this->lvalue_producer, &this->evaluated_producer->reg, context);

  ava_ast_node_cg_evaluate(
    this->wither? this->wither : this->getter, dst, context);
}

static void ava_intr_subscript_cg_tear_down(
  ava_intr_subscript* this, ava_codegen_context* context
) {
  if (this->lvalue_producer)
    ava_ast_node_cg_tear_down(this->lvalue_producer, context);

  ava_codegen_pop_reg(context, ava_prt_data, 2 + !!this->lvalue_producer);
}
