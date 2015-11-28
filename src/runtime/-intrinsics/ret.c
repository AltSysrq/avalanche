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
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/errors.h"
#include "fundamental.h"
#include "ret.h"

typedef struct {
  ava_ast_node header;
  ava_ast_node* value;
} ava_intr_ret;

static ava_string ava_intr_ret_to_string(const ava_intr_ret* node);
static void ava_intr_ret_postprocess(ava_intr_ret* node);
static void ava_intr_ret_cg_discard(
  ava_intr_ret* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_ret_vtable = {
  .name = "ret",
  .to_string = (ava_ast_node_to_string_f)ava_intr_ret_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_ret_postprocess,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_ret_cg_discard,
};

ava_macro_subst_result ava_intr_ret_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  ava_intr_ret* node;

  if (0 == ava_macsub_get_level(context))
    return ava_macsub_error_result(
      context, ava_error_ret_at_global_scope(&provoker->location));

  node = AVA_NEW(ava_intr_ret);
  node->header.v = &ava_intr_ret_vtable;
  node->header.location = provoker->location;
  node->header.context = context;
  if (TAILQ_NEXT(provoker, next)) {
    node->value = ava_macsub_run_units(
      context,
      TAILQ_NEXT(provoker, next),
      TAILQ_LAST(&statement->units, ava_parse_unit_list_s));
  } else {
    node->value = ava_intr_seq_to_node(
      ava_intr_seq_new(context, &provoker->location, ava_isrp_last));
  }

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)node },
  };
}

static ava_string ava_intr_ret_to_string(const ava_intr_ret* node) {
  return ava_strcat(
    AVA_ASCII9_STRING("ret "), ava_ast_node_to_string(node->value));
}

static void ava_intr_ret_postprocess(ava_intr_ret* node) {
  ava_ast_node_postprocess(node->value);
}

static void ava_intr_ret_cg_discard(
  ava_intr_ret* node, ava_codegen_context* context
) {
  ava_pcode_register reg;

  reg.type = ava_prt_data;
  reg.index = ava_codegen_push_reg(context, ava_prt_data, 1);
  ava_ast_node_cg_evaluate(node->value, &reg, context);
  ava_codegen_ret(context, &node->header.location, reg);
  ava_codegen_pop_reg(context, ava_prt_data, 1);
}
