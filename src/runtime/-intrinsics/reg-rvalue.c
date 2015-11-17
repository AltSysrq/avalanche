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
#include "../avalanche/string.h"
#include "../avalanche/macsub.h"
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
#include "reg-rvalue.h"

static ava_string ava_intr_reg_rvalue_to_string(
  const ava_intr_reg_rvalue* this);
static void ava_intr_reg_rvalue_cg_evaluate(
  ava_intr_reg_rvalue* this, const ava_pcode_register* dst,
  ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_reg_rvalue_vtable = {
  .name = "<register-pseudo-node>",
  .to_string = (ava_ast_node_to_string_f)ava_intr_reg_rvalue_to_string,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_reg_rvalue_cg_evaluate,
};

void ava_intr_reg_rvalue_init(ava_intr_reg_rvalue* this,
                              ava_macsub_context* context) {
  this->header.v = &ava_intr_reg_rvalue_vtable;
  this->header.context = context;
  memset(&this->header.location, 0, sizeof(this->header.location));
  this->header.location.filename = AVA_ASCII9_STRING("<none>");
}

static ava_string ava_intr_reg_rvalue_to_string(
  const ava_intr_reg_rvalue* this
) {
  AVA_STATIC_STRING(text, "<register-pseudo-node>");

  return text;
}

static void ava_intr_reg_rvalue_cg_evaluate(
  ava_intr_reg_rvalue* this, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  AVA_PCXB(ld_reg, *dst, this->reg);
}
