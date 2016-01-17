/*-
 * Copyright (c) 2016, Jason Lingle
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

#include <stdlib.h>

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
#include "../avalanche/macsub.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/pcode.h"
#include "funmac.h"
#include "structops.h"
#include "../-internal-defs.h"

#define ARG_POS { .binding.type = ava_abt_pos }
#define MACRO(name) AVA_DEF_MACRO_SUBST(ava_intr_##name##_subst) {      \
    return ava_funmac_subst(&ava_intr_##name##_type,                    \
                            self, context, statement, provoker);        \
  }

static const ava_argument_spec ava_intr_esoterica_empty_argspecs[] = {
  { .binding.type = ava_abt_empty }
};
static const ava_function ava_intr_esoterica_empty_prototype = {
  .args = ava_intr_esoterica_empty_argspecs,
  .num_args = ava_lenof(ava_intr_esoterica_empty_argspecs),
};

/******************** S.get-sp ********************/

static void ava_intr_S_get_sp_cg_evaluate(
  void* userdata, void* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  ava_ast_node*const* args);

static const ava_funmac_type ava_intr_S_get_sp_type = {
  .prototype = &ava_intr_esoterica_empty_prototype,
  .cg_evaluate = ava_intr_S_get_sp_cg_evaluate,
};

static void ava_intr_S_get_sp_cg_evaluate(
  void* userdata, void* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  ava_ast_node*const* args
) {
  ava_codegen_set_location(context, location);
  AVA_PCXB(S_get_sp, *dst);
}

MACRO(S_get_sp)

/******************** S.set-sp ********************/

static const ava_argument_spec ava_intr_S_set_sp_argspecs[] = { ARG_POS };
static const ava_function ava_intr_S_set_sp_prototype = {
  .args = ava_intr_S_set_sp_argspecs,
  .num_args = ava_lenof(ava_intr_S_set_sp_argspecs),
};

static void ava_intr_S_set_sp_cg_discard(
  void* userdata, void* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  ava_ast_node*const* args);

static const ava_funmac_type ava_intr_S_set_sp_type = {
  .prototype = &ava_intr_S_set_sp_prototype,
  .cg_discard = ava_intr_S_set_sp_cg_discard,
};

static void ava_intr_S_set_sp_cg_discard(
  void* userdata, void* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  ava_ast_node*const* args
) {
  ava_pcode_register reg;

  reg.type = ava_prt_data;
  reg.index = ava_codegen_push_reg(context, ava_prt_data, 1);
  ava_ast_node_cg_evaluate(args[0], &reg, context);
  ava_codegen_set_location(context, location);
  AVA_PCXB(S_set_sp, reg);
  ava_codegen_pop_reg(context, ava_prt_data, 1);
}

MACRO(S_set_sp)

/******************** S.cpu-pause ********************/

static void ava_intr_S_cpu_pause_cg_discard(
  void* userdata, void* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  ava_ast_node*const* args);

static const ava_funmac_type ava_intr_S_cpu_pause_type = {
  .prototype = &ava_intr_esoterica_empty_prototype,
  .cg_discard = ava_intr_S_cpu_pause_cg_discard,
};

static void ava_intr_S_cpu_pause_cg_discard(
  void* userdata, void* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  ava_ast_node*const* args
) {
  ava_codegen_set_location(context, location);
  AVA_PCXB0(cpu_pause);
}

MACRO(S_cpu_pause)
