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
#include "../avalanche/function.h"
#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"
#include "../avalanche/symbol.h"
#include "../avalanche/errors.h"
#include "funmac.h"

typedef struct {
  ava_ast_node header;

  ava_string name;
  ava_bool postprocessed;
  const ava_funmac_type* type;
  void* local_userdata;
  ava_ast_node* args[];
} ava_funmac;

static ava_string ava_funmac_to_string(const ava_funmac* node);
static void ava_funmac_postprocess(ava_funmac* node);
static void ava_funmac_cg_evaluate(
  ava_funmac* node, const struct ava_pcode_register_s* dst,
  struct ava_codegen_context_s* context);
static void ava_funmac_cg_discard(
  ava_funmac* node, struct ava_codegen_context_s* context);

/* There are three vtables for funmac, one for each combination of having
 * evaluate and discard. This simplifies the error handling for improper use,
 * in that it need not be duplicated here.
 */

static const ava_ast_node_vtable ava_funmac_vtable_ed = {
  .name = "function-like macro",
  .to_string = (ava_ast_node_to_string_f)ava_funmac_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_funmac_postprocess,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_funmac_cg_evaluate,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_funmac_cg_discard,
}, ava_funmac_vtable_e = {
  .name = "function-like macro",
  .to_string = (ava_ast_node_to_string_f)ava_funmac_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_funmac_postprocess,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_funmac_cg_evaluate,
}, ava_funmac_vtable_d = {
  .name = "function-like macro",
  .to_string = (ava_ast_node_to_string_f)ava_funmac_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_funmac_postprocess,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_funmac_cg_discard,
};

ava_macro_subst_result ava_funmac_subst(
  const ava_funmac_type* funmac_type,
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker
) {
  AVA_STATIC_STRING(unknown_bind_error_message,
                    "non-constant in place of argument name?");

  const size_t num_args = funmac_type->prototype->num_args;
  const ava_parse_unit* parm_unit;
  size_t num_parms, i;
  ava_ast_node** parm_nodes;
  ava_function_parameter* parms;
  ava_function_bound_argument bound_args[num_args];
  ava_string bind_error;
  ava_funmac* node;

  num_parms = 0;
  for (parm_unit = TAILQ_NEXT(provoker, next); parm_unit;
       parm_unit = TAILQ_NEXT(parm_unit, next))
    ++num_parms;

  parm_nodes = ava_alloc(sizeof(ava_ast_node*) * num_parms);
  parms = ava_alloc(sizeof(ava_function_parameter) * num_parms);

  for (parm_unit = TAILQ_NEXT(provoker, next), i = 0; parm_unit;
       parm_unit = TAILQ_NEXT(parm_unit, next), ++i) {
    parm_nodes[i] = ava_macsub_run_units(context, parm_unit, parm_unit);
    if (ava_ast_node_get_constexpr(parm_nodes[i], &parms[i].value)) {
      parms[i].type = ava_fpt_static;
    } else {
      parms[i].type = ava_fpt_dynamic;
    }
  }

  switch (ava_function_bind(
            funmac_type->prototype, num_parms, parms,
            bound_args, NULL, &bind_error)) {
  case ava_fbs_bound: break;

  case ava_fbs_impossible:
    return ava_macsub_error_result(
      context, ava_error_funmac_bind_impossible(
        &provoker->location,
        self->full_name, bind_error));

  case ava_fbs_unknown:
    return ava_macsub_error_result(
      context, ava_error_funmac_bind_impossible(
        &provoker->location,
        self->full_name, unknown_bind_error_message));

  case ava_fbs_unpack: abort(); /* unreachable */
  }

  assert(funmac_type->cg_evaluate || funmac_type->cg_discard);
  node = ava_alloc(sizeof(ava_funmac) + sizeof(ava_ast_node*) * num_args);
  node->header.v =
    funmac_type->cg_evaluate && funmac_type->cg_discard? &ava_funmac_vtable_ed :
    funmac_type->cg_evaluate ? &ava_funmac_vtable_e : &ava_funmac_vtable_d;
  node->header.context = context;
  node->header.location = provoker->location;
  node->name = self->full_name;
  node->type = funmac_type;

  for (i = 0; i < num_args; ++i) {
    switch (bound_args[i].type) {
    case ava_fbat_collect: abort(); /* unexpected */
    case ava_fbat_parameter:
      node->args[i] = parm_nodes[bound_args[i].v.parameter_index];
      break;

    case ava_fbat_implicit:
      /* Null check the attribute chain since statically declared function
       * prototypes in C can't specify a valid value at all.
       */
      if (ava_value_attr(bound_args[i].v.value) &&
          ava_string_equal(AVA_ASCII9_STRING("true"),
                           ava_to_string(bound_args[i].v.value)))
        node->args[i] = AVA_FUNMAC_TRUE;
      else
        node->args[i] = NULL;
      break;
    }
  }

  if (funmac_type->accept)
    (*funmac_type->accept)(funmac_type->userdata, &node->local_userdata,
                           context, &node->header.location, node->args);

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)node }
  };
}

static ava_string ava_funmac_to_string(const ava_funmac* node) {
  ava_string accum, arg;
  size_t i, n;

  n = node->type->prototype->num_args;

  accum = node->name;

  for (i = 0; i < n; ++i) {
    if (NULL == node->args[i])
      arg = AVA_ASCII9_STRING("<NULL>");
    else if (AVA_FUNMAC_TRUE == node->args[i])
      arg = AVA_ASCII9_STRING("<TRUE>");
    else
      arg = ava_ast_node_to_string(node->args[i]);

    accum = ava_strcat(accum, AVA_ASCII9_STRING(" "));
    accum = ava_strcat(accum, arg);
  }

  return accum;
}

static void ava_funmac_postprocess(ava_funmac* node) {
  size_t i, n;

  if (node->postprocessed) return;
  node->postprocessed = ava_true;

  n = node->type->prototype->num_args;
  for (i = 0; i < n; ++i) {
    if (node->args[i] > AVA_FUNMAC_TRUE)
      ava_ast_node_postprocess(node->args[i]);
  }
}

static void ava_funmac_cg_evaluate(
  ava_funmac* node, const struct ava_pcode_register_s* dst,
  struct ava_codegen_context_s* context
) {
  (*node->type->cg_evaluate)(
    node->type->userdata, node->local_userdata, dst, context,
    &node->header.location, node->args);
}

static void ava_funmac_cg_discard(
  ava_funmac* node, struct ava_codegen_context_s* context
) {
  (*node->type->cg_discard)(
    node->type->userdata, node->local_userdata, NULL, context,
    &node->header.location, node->args);
}
