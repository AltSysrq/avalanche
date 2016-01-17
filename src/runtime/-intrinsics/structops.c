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

#include <stdlib.h>

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
#include "../avalanche/alloc.h"
#include "../avalanche/string.h"
#include "../avalanche/macsub.h"
#include "../avalanche/symtab.h"
#include "../avalanche/symbol.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/struct.h"
#include "../avalanche/function.h"
#include "../avalanche/pcode.h"
#include "../avalanche/exception.h"
#include "../avalanche/macro-arg.h"
#include "../avalanche/errors.h"
#include "funmac.h"
#include "reg-rvalue.h"
#include "structops.h"
#include "../-internal-defs.h"

#define SXT ava_intr_structop_get_index(instance, context)
#define FIELD (instance->field_ix)
#define MACRO(name) AVA_DEF_MACRO_SUBST(ava_intr_S_##name##_subst) {    \
    return ava_funmac_subst(&ava_intr_S_##name##_type,                  \
                            self, context, statement, provoker);        \
  }

#define DO_SXT_LOOKUP() do {                            \
    if (!ava_intr_structop_look_struct_up(              \
          instance, context, args->sxt))                \
      return;                                           \
  } while (0)

#define DO_FIELD_LOOKUP() do {                          \
    if (!ava_intr_structop_look_field_up(               \
          *instance, context, args->field))             \
      return;                                           \
  } while (0)

#define REQUIRE_COMPOSABLE(trigger) do {                                \
    if (!(*instance)->sxt->is_composable)                               \
      ava_macsub_record_error(                                          \
        context, ava_error_cannot_operate_array_of_noncomposable(       \
          &trigger->location, (*instance)->struct_sym->full_name));     \
  } while (0)

#define REQUIRE_TAIL(trigger) do {                                      \
    if (!ava_intr_structop_has_tail((*instance)->sxt))                  \
      ava_macsub_record_error(                                          \
        context, ava_error_tail_operation_on_struct_without_tail(       \
          &trigger->location, (*instance)->struct_sym->full_name));     \
  } while (0)

#define ARG_POS { .binding.type = ava_abt_pos }
#define ARG_BOOL(...)                                                   \
  { .binding = { .type = ava_abt_bool, .name = AVA_ASCII9_INIT(__VA_ARGS__) } }
#define ARG_NAME_OPT(...)                                       \
  { .binding = { .type = ava_abt_named_default,                 \
                 .name = AVA_ASCII9_INIT(__VA_ARGS__) } }

#define ARG_MOVE ARG_BOOL('-','m','o','v','e')
#define ARG_TAIL ARG_NAME_OPT('-','t')
#define ARG_VOLATILE ARG_BOOL('-','v','o','l','a','t','i','l','e')
#define ARG_ORDER ARG_NAME_OPT('-','o','r','d','e','r')
#define ARG_UNATOMIC ARG_BOOL('-','u','n','a','t','o','m','i','c')
#define ARG_INT ARG_BOOL('-','i','n','t')
#define ARG_PTR ARG_BOOL('-','p','t','r')

typedef struct {
  /**
   * The symbol of the struct this instance is targetted at.
   */
  const ava_symbol* struct_sym;
  /**
   * Convenience for struct_sym->v.sxt.def
   */
  const ava_struct* sxt;
  /**
   * The index of the field this instance is operating.
   */
  size_t field_ix;
  /**
   * Convenience for (sxt->fields + field_ix), or NULL if there is no field.
   */
  const ava_struct_field* field;
  /**
   * Scratch node used for operations that work with lvalues.
   */
  ava_intr_reg_rvalue reg_rvalue;
} ava_intr_structop_instance;

static ava_bool ava_intr_structop_look_struct_up(
  ava_intr_structop_instance** dst,
  ava_macsub_context* context,
  const ava_ast_node* node);

static const ava_symbol* ava_intr_structop_look_struct_sym_up(
  ava_macsub_context* context,
  ava_string name,
  const ava_compile_location* location);

static ava_bool ava_intr_structop_look_field_up(
  ava_intr_structop_instance* dst,
  ava_macsub_context* context,
  const ava_ast_node* node);

static ava_bool ava_intr_structop_has_tail(
  const ava_struct* sxt);

static size_t ava_intr_structop_get_index(
  const ava_intr_structop_instance* this,
  ava_codegen_context* context);

static ava_bool ava_intr_structop_check_order_valid(
  ava_macsub_context* context,
  ava_ast_node* order);

static ava_bool ava_intr_structop_check_rmw_op_valid(
  ava_macsub_context* context,
  ava_ast_node* rmw_op);

static void ava_intr_structop_check_atomic_sanity(
  const ava_intr_structop_instance* this,
  ava_macsub_context* context,
  ava_bool require_atomic,
  ava_ast_node* order, ava_ast_node* unatomic,
  const ava_compile_location* field_location);

static void ava_intr_structop_check_hybrid_sanity(
  const ava_intr_structop_instance* this,
  ava_macsub_context* context,
  ava_ast_node* hy_int, ava_ast_node* hy_ptr,
  const ava_compile_location* location);

static void ava_intr_structop_convert_order(
  ava_pcode_memory_order* dst,
  const ava_ast_node* order);

static void ava_intr_structop_convert_rmw_op(
  ava_pcode_rmw_op* dst,
  const ava_ast_node* order);

static ava_bool ava_intr_structop_look_struct_up(
  ava_intr_structop_instance** dst,
  ava_macsub_context* context,
  const ava_ast_node* node
) {
  ava_value name_value;
  ava_string name;

  *dst = AVA_NEW(ava_intr_structop_instance);

  if (!ava_ast_node_get_constexpr(node, &name_value)) {
    ava_macsub_record_error(
      context, ava_error_macro_arg_not_constexpr(
        &node->location, AVA_ASCII9_STRING("struct")));
    return ava_false;
  }
  name = ava_to_string(name_value);

  (*dst)->struct_sym = ava_intr_structop_look_struct_sym_up(
    context, name, &node->location);
  if ((*dst)->struct_sym) {
    (*dst)->sxt = (*dst)->struct_sym->v.sxt.def;
    return ava_true;
  } else {
    return ava_false;
  }
}

static const ava_symbol* ava_intr_structop_look_struct_sym_up(
  ava_macsub_context* context,
  ava_string name,
  const ava_compile_location* location
) {
  const ava_symbol** results;
  size_t num_results;

  num_results = ava_symtab_get(
    &results, ava_macsub_get_symtab(context), name);

  if (!num_results) {
    ava_macsub_record_error(
      context, ava_error_no_such_struct(location, name));
    return NULL;
  }

  if (num_results > 1) {
    ava_macsub_record_error(
      context, ava_error_ambiguous_struct(
        location, name, num_results,
        results[0]->full_name, results[1]->full_name));
    return NULL;
  }

  if (ava_st_struct != results[0]->type) {
    ava_macsub_record_error(
      context, ava_error_symbol_not_a_struct(
        location,
        results[0]->full_name, ava_symbol_type_name(results[0])));
    return NULL;
  }

  return results[0];
}

static ava_bool ava_intr_structop_look_field_up(
  ava_intr_structop_instance* dst,
  ava_macsub_context* context,
  const ava_ast_node* node
) {
  size_t i;
  ava_value name_value;
  ava_string name;

  if (!ava_ast_node_get_constexpr(node, &name_value)) {
    ava_macsub_record_error(
      context, ava_error_macro_arg_not_constexpr(
        &node->location, AVA_ASCII9_STRING("field")));
    return ava_false;
  }
  name = ava_to_string(name_value);

  for (i = 0; i < dst->sxt->num_fields; ++i) {
    if (ava_string_equal(name, dst->sxt->fields[i].name)) {
      dst->field_ix = i;
      dst->field = dst->sxt->fields + i;
      return ava_true;
    }
  }

  ava_macsub_record_error(
    context, ava_error_struct_field_not_found(
      &node->location, dst->struct_sym->full_name, name));
  return ava_false;
}

static ava_bool ava_intr_structop_has_tail(
  const ava_struct* sxt
) {
  return sxt->num_fields > 0 &&
    ava_sft_tail == sxt->fields[sxt->num_fields-1].type;
}

static size_t ava_intr_structop_get_index(
  const ava_intr_structop_instance* this,
  ava_codegen_context* context
) {
  if (this->struct_sym->definer)
    ava_ast_node_cg_define(this->struct_sym->definer, context);
  return this->struct_sym->pcode_index;
}

static void ava_intr_structop_try_parse_memory_order(void* v) {
  (void)ava_pcode_parse_memory_order(*(const ava_value*)v);
}

static ava_bool ava_intr_structop_is_valid_memory_order(ava_value v) {
  ava_exception caught;

  return !ava_catch(&caught, ava_intr_structop_try_parse_memory_order, &v);
}

static ava_bool ava_intr_structop_check_order_valid(
  ava_macsub_context* context,
  ava_ast_node* order
) {
  ava_value order_value;

  if (order) {
    if (!ava_ast_node_get_constexpr(order, &order_value)) {
      ava_macsub_record_error(
        context, ava_error_macro_arg_not_constexpr(
          &order->location, AVA_ASCII9_STRING("order")));
      return ava_false;
    } else if (!ava_intr_structop_is_valid_memory_order(order_value)) {
      ava_macsub_record_error(
        context, ava_error_unknown_memory_order(
          &order->location, ava_to_string(order_value)));
      return ava_false;
    }
  }

  return ava_true;
}

static void ava_intr_structop_try_parse_rmw_op(void* v) {
  (void)ava_pcode_parse_rmw_op(*(const ava_value*)v);
}

static ava_bool ava_intr_structop_is_valid_rmw_op(ava_value v) {
  ava_exception caught;

  return !ava_catch(&caught, ava_intr_structop_try_parse_rmw_op, &v);
}

static ava_bool ava_intr_structop_check_rmw_op_valid(
  ava_macsub_context* context,
  ava_ast_node* op
) {
  ava_value op_value;

  if (op) {
    if (!ava_ast_node_get_constexpr(op, &op_value)) {
      ava_macsub_record_error(
        context, ava_error_macro_arg_not_constexpr(
          &op->location, AVA_ASCII9_STRING("operation")));
      return ava_false;
    } else if (!ava_intr_structop_is_valid_rmw_op(op_value)) {
      ava_macsub_record_error(
        context, ava_error_unknown_rmw_op(
          &op->location, ava_to_string(op_value)));
      return ava_false;
    }
  }

  return ava_true;
}

static void ava_intr_structop_check_atomic_sanity(
  const ava_intr_structop_instance* this,
  ava_macsub_context* context,
  ava_bool require_atomic,
  ava_ast_node* order, ava_ast_node* unatomic,
  const ava_compile_location* field_location
) {
  if ((ava_sft_int == this->field->type && this->field->v.vint.is_atomic) ||
      (ava_sft_ptr == this->field->type && this->field->v.vptr.is_atomic)) {
    if (order && unatomic) {
      ava_macsub_record_error(
        context, ava_error_nonatomic_operation_cannot_have_memory_order(
          &order->location));
    }

    ava_intr_structop_check_order_valid(context, order);
  } else {
    if (require_atomic) {
      ava_macsub_record_error(
        context, ava_error_operation_only_legal_on_atomic_fields(
          field_location));
    } else if (unatomic) {
      ava_macsub_record_error(
        context, ava_error_already_unatomic(&unatomic->location));
    } else if (order) {
      ava_macsub_record_error(
        context, ava_error_nonatomic_operation_cannot_have_memory_order(
          &order->location));
    }
  }
}

static void ava_intr_structop_check_hybrid_sanity(
  const ava_intr_structop_instance* this,
  ava_macsub_context* context,
  ava_ast_node* hy_int, ava_ast_node* hy_ptr,
  const ava_compile_location* location
) {
  if ((ava_sft_hybrid == this->field->type && (!!hy_int == !!hy_ptr)) ||
      (ava_sft_hybrid != this->field->type && (hy_int || hy_ptr))) {
    ava_macsub_record_error(
      context, ava_error_struct_invalid_hybrid_flags(
        (hy_int? &hy_int->location :
         hy_ptr? &hy_ptr->location :
         location)));
  }
}

static void ava_intr_structop_convert_order(
  ava_pcode_memory_order* dst,
  const ava_ast_node* order
) {
  ava_value order_val;

  if (order) {
    if (!ava_ast_node_get_constexpr(order, &order_val))
      abort();

    *dst = ava_pcode_parse_memory_order(order_val);
  } else {
    *dst = ava_pmo_unordered;
  }
}

static void ava_intr_structop_convert_rmw_op(
  ava_pcode_rmw_op* dst,
  const ava_ast_node* rmw_op
) {
  ava_value rmw_op_val;

  if (!ava_ast_node_get_constexpr(rmw_op, &rmw_op_val))
    abort();

  *dst = ava_pcode_parse_rmw_op(rmw_op_val);
}

/******************** S.new ********************/

typedef struct {
  ava_ast_node* sxt, * on_stack, * zero, * atomic, * precise, * tail, * array;
} ava_intr_S_new_args;

static void ava_intr_S_new_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_new_args* args);
static void ava_intr_S_new_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_new_args* args);

static const ava_argument_spec ava_intr_S_new_argspecs[] = {
  ARG_POS,
  { .binding = {
      .type = ava_abt_bool,
      .name = AVA_ASCII9_INIT('-','s') } },
  { .binding = {
      .type = ava_abt_bool,
      .name = AVA_ASCII9_INIT('-','z') } },
  { .binding = {
      .type = ava_abt_bool,
      .name = AVA_ASCII9_INIT('-','a','t','o','m','i','c') } },
  { .binding = {
      .type = ava_abt_bool,
      .name = AVA_ASCII9_INIT('-','p','r','e','c','i','s','e') } },
  ARG_TAIL,
  { .binding = {
      .type = ava_abt_named_default,
      .name = AVA_ASCII9_INIT('-','n') } },
};

static const ava_function ava_intr_S_new_prototype = {
  .args = ava_intr_S_new_argspecs,
  .num_args = ava_lenof(ava_intr_S_new_argspecs),
};

static const ava_funmac_type ava_intr_S_new_type = {
  .prototype = &ava_intr_S_new_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_new_accept,
  .cg_evaluate = (ava_funmac_cg_evaluate_f)ava_intr_S_new_cg_evaluate,
};

static void ava_intr_S_new_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_new_args* args
) {
  DO_SXT_LOOKUP();
  if (args->array)
    REQUIRE_COMPOSABLE(args->array);

  if (args->tail)
    REQUIRE_TAIL(args->tail);
}

static void ava_intr_S_new_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_new_args* args
) {
  ava_pcode_register counti, countv;

  counti.type = ava_prt_int;
  countv.type = ava_prt_data;
  if (args->array || args->tail) {
    counti.index = ava_codegen_push_reg(context, ava_prt_int, 1);
    countv.index = ava_codegen_push_reg(context, ava_prt_data, 1);

    if (args->array)
      ava_ast_node_cg_evaluate(args->array, &countv, context);
    else
      ava_ast_node_cg_evaluate(args->tail, &countv, context);

    ava_codegen_set_location(context, location);
    AVA_PCXB(ld_reg_d, counti, countv);

    ava_codegen_pop_reg(context, ava_prt_data, 1);
  }

  if (args->on_stack) {
    if (args->array) {
      AVA_PCXB(S_new_sa, *dst, SXT, counti, !!args->zero);
    } else if (args->tail) {
      AVA_PCXB(S_new_st, *dst, SXT, counti, !!args->zero);
    } else {
      AVA_PCXB(S_new_s, *dst, SXT, !!args->zero);
    }
  } else {
    if (args->array) {
      AVA_PCXB(S_new_ha, *dst, SXT, counti,
               !!args->zero, !!args->atomic, !!args->precise);
    } else if (args->tail) {
      AVA_PCXB(S_new_ht, *dst, SXT, counti,
               !!args->zero, !!args->atomic, !!args->precise);
    } else {
      AVA_PCXB(S_new_h, *dst, SXT,
               !!args->zero, !!args->atomic, !!args->precise);
    }
  }

  if (args->array || args->tail) {
    ava_codegen_pop_reg(context, ava_prt_int, 1);
  }
}

MACRO(new)

/******************** S.cpy ********************/

typedef struct {
  ava_ast_node* sxt, * move, * tail, * dst, * src;
} ava_intr_S_cpy_args;

static void ava_intr_S_cpy_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_cpy_args* args);
static void ava_intr_S_cpy_cg_discard(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_cpy_args* args);

static const ava_argument_spec ava_intr_S_cpy_argspecs[] = {
  ARG_POS, ARG_MOVE, ARG_TAIL, ARG_POS, ARG_POS,
};

static const ava_function ava_intr_S_cpy_prototype = {
  .args = ava_intr_S_cpy_argspecs,
  .num_args = ava_lenof(ava_intr_S_cpy_argspecs),
};

static const ava_funmac_type ava_intr_S_cpy_type = {
  .prototype = &ava_intr_S_cpy_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_cpy_accept,
  .cg_discard = (ava_funmac_cg_evaluate_f)ava_intr_S_cpy_cg_discard,
};

static void ava_intr_S_cpy_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_cpy_args* args
) {
  DO_SXT_LOOKUP();
  if (args->tail)
    REQUIRE_TAIL(args->tail);
}

static void ava_intr_S_cpy_cg_discard(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* ignore,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_cpy_args* args
) {
  ava_pcode_register dst, src, tailv, taili;

  dst.type = ava_prt_data;
  dst.index = ava_codegen_push_reg(context, ava_prt_data, 2);
  src.type = ava_prt_data;
  src.index = dst.index + 1;

  if (args->tail) {
    taili.type = ava_prt_int;
    taili.index = ava_codegen_push_reg(context, ava_prt_int, 1);
    tailv.type = ava_prt_data;
    tailv.index = ava_codegen_push_reg(context, ava_prt_data, 1);

    ava_ast_node_cg_evaluate(args->tail, &tailv, context);
    ava_codegen_set_location(context, location);
    AVA_PCXB(ld_reg_d, taili, tailv);
    ava_codegen_pop_reg(context, ava_prt_data, 1);
  }

  ava_ast_node_cg_evaluate(args->dst, &dst, context);
  ava_ast_node_cg_evaluate(args->src, &src, context);

  ava_codegen_set_location(context, location);

  if (args->tail) {
    AVA_PCXB(S_cpy_t, dst, src, taili, SXT, !args->move);
  } else {
    AVA_PCXB(S_cpy, dst, src, SXT, !args->move);
  }

  if (args->tail) {
    ava_codegen_pop_reg(context, ava_prt_int, 1);
  }
  ava_codegen_pop_reg(context, ava_prt_data, 2);
}

MACRO(cpy)

/******************** S.arraycpy ********************/

typedef struct {
  ava_ast_node* sxt, * move, * dst, * dstoff, * src, * srcoff, * count;
} ava_intr_S_arraycpy_args;

static void ava_intr_S_arraycpy_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_arraycpy_args* args);
static void ava_intr_S_arraycpy_cg_discard(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_arraycpy_args* args);

static const ava_argument_spec ava_intr_S_arraycpy_argspecs[] = {
  ARG_POS, ARG_MOVE, ARG_POS, ARG_POS, ARG_POS, ARG_POS, ARG_POS,
};

static const ava_function ava_intr_S_arraycpy_prototype = {
  .args = ava_intr_S_arraycpy_argspecs,
  .num_args = ava_lenof(ava_intr_S_arraycpy_argspecs),
};

static const ava_funmac_type ava_intr_S_arraycpy_type = {
  .prototype = &ava_intr_S_arraycpy_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_arraycpy_accept,
  .cg_discard = (ava_funmac_cg_evaluate_f)ava_intr_S_arraycpy_cg_discard,
};

static void ava_intr_S_arraycpy_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_arraycpy_args* args
) {
  DO_SXT_LOOKUP();
  REQUIRE_COMPOSABLE(args->sxt);
}

static void ava_intr_S_arraycpy_cg_discard(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* ignore,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_arraycpy_args* args
) {
  ava_pcode_register dst, dstoffv, src, srcoffv, countv;
  ava_pcode_register dstoffi, srcoffi, counti;

  dst.type = ava_prt_data;
  dst.index = ava_codegen_push_reg(context, ava_prt_data, 2);
  src.type = ava_prt_data;
  src.index = dst.index + 1;

  dstoffv.type = ava_prt_data;
  dstoffv.index = ava_codegen_push_reg(context, ava_prt_data, 3);
  srcoffv.type = ava_prt_data;
  srcoffv.index = dstoffv.index + 1;
  countv.type = ava_prt_data;
  countv.index = dstoffv.index + 2;

  ava_ast_node_cg_evaluate(args->dst, &dst, context);
  ava_ast_node_cg_evaluate(args->dstoff, &dstoffv, context);
  ava_ast_node_cg_evaluate(args->src, &src, context);
  ava_ast_node_cg_evaluate(args->srcoff, &srcoffv, context);
  ava_ast_node_cg_evaluate(args->count, &countv, context);

  ava_codegen_set_location(context, location);
  dstoffi.type = ava_prt_int;
  dstoffi.index = ava_codegen_push_reg(context, ava_prt_int, 3);
  srcoffi.type = ava_prt_int;
  srcoffi.index = dstoffi.index + 1;
  counti.type = ava_prt_int;
  counti.index = dstoffi.index + 2;

  AVA_PCXB(ld_reg_d, dstoffi, dstoffv);
  AVA_PCXB(ld_reg_d, srcoffi, srcoffv);
  AVA_PCXB(ld_reg_d, counti, countv);
  ava_codegen_pop_reg(context, ava_prt_data, 3);

  AVA_PCXB(S_cpy_a, dst, dstoffi, src, srcoffi, counti, SXT, !args->move);

  ava_codegen_pop_reg(context, ava_prt_int, 3);
  ava_codegen_pop_reg(context, ava_prt_data, 2);
}

MACRO(arraycpy)

/******************** S.get ********************/

typedef struct {
  ava_ast_node* sxt, * src, * field, * order, * unatomic, * volatil,
              * hy_int, * hy_ptr;
} ava_intr_S_get_args;

static void ava_intr_S_get_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_get_args* args);
static void ava_intr_S_get_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_get_args* args);

static const ava_argument_spec ava_intr_S_get_argspecs[] = {
  ARG_POS, ARG_POS, ARG_POS, ARG_ORDER, ARG_UNATOMIC, ARG_VOLATILE,
  ARG_INT, ARG_PTR,
};

static const ava_function ava_intr_S_get_prototype = {
  .args = ava_intr_S_get_argspecs,
  .num_args = ava_lenof(ava_intr_S_get_argspecs),
};

static const ava_funmac_type ava_intr_S_get_type = {
  .prototype = &ava_intr_S_get_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_get_accept,
  .cg_evaluate = (ava_funmac_cg_evaluate_f)ava_intr_S_get_cg_evaluate,
};

static void ava_intr_S_get_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_get_args* args
) {
  DO_SXT_LOOKUP();
  DO_FIELD_LOOKUP();

  ava_intr_structop_check_atomic_sanity(
    *instance, context, ava_false, args->order, args->unatomic,
    &args->field->location);
  ava_intr_structop_check_hybrid_sanity(
    *instance, context, args->hy_int, args->hy_ptr, location);

  if (args->volatil &&
      (ava_sft_compose == (*instance)->field->type ||
       ava_sft_array == (*instance)->field->type ||
       ava_sft_tail == (*instance)->field->type))
    ava_macsub_record_error(
      context, ava_error_non_memory_access_cannot_be_volatile(
        &args->volatil->location));
}

static void ava_intr_S_get_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_get_args* args
) {
  ava_pcode_register tmp, src;
  ava_pcode_memory_order order;

  src.type = ava_prt_data;
  src.index = ava_codegen_push_reg(context, ava_prt_data, 1);
  ava_ast_node_cg_evaluate(args->src, &src, context);

  ava_intr_structop_convert_order(&order, args->order);

  ava_codegen_set_location(context, location);

  switch (instance->field->type) {
  case ava_sft_int:
    tmp.type = ava_prt_int;
    tmp.index = ava_codegen_push_reg(context, ava_prt_int, 1);
    if (args->unatomic || !instance->field->v.vint.is_atomic)
      AVA_PCXB(S_i_ld, tmp, src, SXT, FIELD, !!args->volatil);
    else
      AVA_PCXB(S_ia_ld, tmp, src, SXT, FIELD, !!args->volatil, order);
    AVA_PCXB(ld_reg_u, *dst, tmp);
    ava_codegen_pop_reg(context, ava_prt_int, 1);
    break;

  case ava_sft_real:
    AVA_PCXB(S_r_ld, *dst, src, SXT, FIELD, !!args->volatil);
    break;

  case ava_sft_ptr:
    if (args->unatomic || !instance->field->v.vptr.is_atomic)
      AVA_PCXB(S_p_ld, *dst, src, SXT, FIELD, !!args->volatil);
    else
      AVA_PCXB(S_pa_ld, *dst, src, SXT, FIELD, !!args->volatil, order);
    break;

  case ava_sft_hybrid:
    if (args->hy_int) {
      tmp.type = ava_prt_int;
      tmp.index = ava_codegen_push_reg(context, ava_prt_int, 1);
      AVA_PCXB(S_hi_ld, tmp, src, SXT, FIELD, !!args->volatil);
      AVA_PCXB(ld_reg_u, *dst, tmp);
      ava_codegen_pop_reg(context, ava_prt_int, 1);
    } else {
      AVA_PCXB(S_p_ld, *dst, src, SXT, FIELD, !!args->volatil);
    }
    break;

  case ava_sft_value:
    AVA_PCXB(S_v_ld, *dst, src, SXT, FIELD, !!args->volatil);
    break;

  case ava_sft_compose:
  case ava_sft_array:
  case ava_sft_tail:
    AVA_PCXB(S_gfp, *dst, src, SXT, FIELD);
    break;
  }

  ava_codegen_pop_reg(context, ava_prt_data, 1);
}

MACRO(get)

/******************** S.set ********************/

typedef struct {
  ava_ast_node* sxt, * dst, * field, * order, * unatomic,
              * volatil, * hy_int, * hy_ptr, * value;
} ava_intr_S_set_args;

static void ava_intr_S_set_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_set_args* args);
static void ava_intr_S_set_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_set_args* args);

static const ava_argument_spec ava_intr_S_set_argspecs[] = {
  ARG_POS, ARG_POS, ARG_POS, ARG_ORDER, ARG_UNATOMIC,
  ARG_VOLATILE, ARG_INT, ARG_PTR, ARG_POS,
};

static const ava_function ava_intr_S_set_prototype = {
  .args = ava_intr_S_set_argspecs,
  .num_args = ava_lenof(ava_intr_S_set_argspecs),
};

static const ava_funmac_type ava_intr_S_set_type = {
  .prototype = &ava_intr_S_set_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_set_accept,
  .cg_evaluate = (ava_funmac_cg_evaluate_f)ava_intr_S_set_cg_evaluate,
  .cg_discard  = (ava_funmac_cg_evaluate_f)ava_intr_S_set_cg_evaluate,
};

static void ava_intr_S_set_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_set_args* args
) {
  DO_SXT_LOOKUP();
  DO_FIELD_LOOKUP();

  ava_intr_structop_check_atomic_sanity(
    *instance, context, ava_false, args->order, args->unatomic,
    &args->field->location);
  ava_intr_structop_check_hybrid_sanity(
    *instance, context, args->hy_int, args->hy_ptr, location);

  if (ava_sft_compose == (*instance)->field->type ||
       ava_sft_array == (*instance)->field->type ||
       ava_sft_tail == (*instance)->field->type)
    ava_macsub_record_error(
      context, ava_error_struct_composed_field_cannot_be_set(
        &args->field->location));
}

static void ava_intr_S_set_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* eval_dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_set_args* args
) {
  ava_pcode_register dst, value, tmp;
  ava_pcode_memory_order order;

  ava_intr_structop_convert_order(&order, args->order);

  dst.type = ava_prt_data;
  dst.index = ava_codegen_push_reg(context, ava_prt_data, 1);

  if (eval_dst) {
    value = *eval_dst;
  } else {
    value.type = ava_prt_data;
    value.index = ava_codegen_push_reg(context, ava_prt_data, 1);
  }

  ava_ast_node_cg_evaluate(args->dst, &dst, context);
  ava_ast_node_cg_evaluate(args->value, &value, context);

  ava_codegen_set_location(context, location);

  switch (instance->field->type) {
  case ava_sft_int:
    tmp.type = ava_prt_int;
    tmp.index = ava_codegen_push_reg(context, ava_prt_int, 1);
    AVA_PCXB(ld_reg_d, tmp, value);
    if (args->unatomic || !instance->field->v.vint.is_atomic)
      AVA_PCXB(S_i_st, dst, SXT, FIELD, tmp, !!args->volatil);
    else
      AVA_PCXB(S_ia_st, dst, SXT, FIELD, tmp, !!args->volatil, order);
    ava_codegen_pop_reg(context, ava_prt_int, 1);
    break;

  case ava_sft_real:
    AVA_PCXB(S_r_st, dst, SXT, FIELD, value, !!args->volatil);
    break;

  case ava_sft_ptr:
    if (args->unatomic || !instance->field->v.vptr.is_atomic)
      AVA_PCXB(S_p_st, dst, SXT, FIELD, value, !!args->volatil);
    else
      AVA_PCXB(S_pa_st, dst, SXT, FIELD, value, !!args->volatil, order);
    break;

  case ava_sft_hybrid:
    if (args->hy_int) {
      tmp.type = ava_prt_int;
      tmp.index = ava_codegen_push_reg(context, ava_prt_int, 1);
      AVA_PCXB(ld_reg_d, tmp, value);
      AVA_PCXB(S_hi_st, dst, SXT, FIELD, tmp, !!args->volatil);
      ava_codegen_pop_reg(context, ava_prt_int, 1);
    } else {
      AVA_PCXB(S_p_st, dst, SXT, FIELD, value, !!args->volatil);
    }
    break;

  case ava_sft_value:
    AVA_PCXB(S_v_st, dst, SXT, FIELD, value, !!args->volatil);
    break;

  case ava_sft_compose:
  case ava_sft_array:
  case ava_sft_tail:
    /* Should be unreachable */
    abort();
  }

  if (!eval_dst) {
    ava_codegen_pop_reg(context, ava_prt_data, 1);
  }
  ava_codegen_pop_reg(context, ava_prt_data, 1);
}

MACRO(set)

/******************** S.is-int ********************/

typedef struct {
  ava_ast_node* sxt, * src, * field, * volatil;
} ava_intr_S_is_int_args;

static void ava_intr_S_is_int_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_is_int_args* args);
static void ava_intr_S_is_int_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_is_int_args* args);

static const ava_argument_spec ava_intr_S_is_int_argspecs[] = {
  ARG_POS, ARG_POS, ARG_POS, ARG_VOLATILE,
};

static const ava_function ava_intr_S_is_int_prototype = {
  .args = ava_intr_S_is_int_argspecs,
  .num_args = ava_lenof(ava_intr_S_is_int_argspecs),
};

static const ava_funmac_type ava_intr_S_is_int_type = {
  .prototype = &ava_intr_S_is_int_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_is_int_accept,
  .cg_evaluate = (ava_funmac_cg_evaluate_f)ava_intr_S_is_int_cg_evaluate,
};

static void ava_intr_S_is_int_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_is_int_args* args
) {
  DO_SXT_LOOKUP();
  DO_FIELD_LOOKUP();

  if (ava_sft_hybrid != (*instance)->field->type)
    ava_macsub_record_error(
      context, ava_error_is_int_on_non_hybrid(&args->field->location));
}

static void ava_intr_S_is_int_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_is_int_args* args
) {
  ava_pcode_register src, tmp;

  src.type = ava_prt_data;
  src.index = ava_codegen_push_reg(context, ava_prt_data, 1);
  tmp.type = ava_prt_int;
  tmp.index = ava_codegen_push_reg(context, ava_prt_int, 1);

  ava_ast_node_cg_evaluate(args->src, &src, context);
  ava_codegen_set_location(context, location);
  AVA_PCXB(S_hy_intp, tmp, src, SXT, FIELD, !!args->volatil);
  AVA_PCXB(ld_reg_u, *dst, tmp);

  ava_codegen_pop_reg(context, ava_prt_int, 1);
  ava_codegen_pop_reg(context, ava_prt_data, 1);
}

MACRO(is_int)

/******************** S.cas ********************/

typedef struct {
  ava_ast_node* sxt, * dst, * field, * volatil, * order, * forder, * weak,
              * actual_lvalue, * old, * new;
} ava_intr_S_cas_args;

static void ava_intr_S_cas_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_cas_args* args);
static void ava_intr_S_cas_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_cas_args* args);

static const ava_argument_spec ava_intr_S_cas_argspecs[] = {
  ARG_POS, ARG_POS, ARG_POS, ARG_VOLATILE, ARG_ORDER,
  { .binding = {
      .type = ava_abt_named_default,
      .name = AVA_ASCII9_INIT('-','f','o','r','d','e','r') } },
  { .binding = {
      .type = ava_abt_bool,
      .name = AVA_ASCII9_INIT('-','w','e','a','k') } },
  { .binding = {
      .type = ava_abt_named_default,
      .name = AVA_ASCII9_INIT('-','o','l','d') } },
  ARG_POS, ARG_POS,
};

static const ava_function ava_intr_S_cas_prototype = {
  .args = ava_intr_S_cas_argspecs,
  .num_args = ava_lenof(ava_intr_S_cas_argspecs),
};

static const ava_funmac_type ava_intr_S_cas_type = {
  .prototype = &ava_intr_S_cas_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_cas_accept,
  .cg_evaluate = (ava_funmac_cg_evaluate_f)ava_intr_S_cas_cg_evaluate,
  .cg_discard  = (ava_funmac_cg_evaluate_f)ava_intr_S_cas_cg_evaluate,
};

static void ava_intr_S_cas_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_cas_args* args
) {
  ava_ast_node* ignore;

  DO_SXT_LOOKUP();
  DO_FIELD_LOOKUP();

  ava_intr_reg_rvalue_init(&(*instance)->reg_rvalue, context);

  ava_intr_structop_check_atomic_sanity(
    *instance, context, ava_true, args->order, NULL,
    &args->field->location);
  ava_intr_structop_check_order_valid(context, args->forder);

  if (args->actual_lvalue)
    args->actual_lvalue = ava_ast_node_to_lvalue(
      args->actual_lvalue, (ava_ast_node*)&(*instance)->reg_rvalue, &ignore);
}

static void ava_intr_S_cas_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* success_dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_cas_args* args
) {
  ava_pcode_register dst, oldv, newv, actualv, oldt, newt, actualt;
  ava_pcode_register success;
  ava_pcode_memory_order success_order, failure_order;

  ava_intr_structop_convert_order(&success_order, args->order);
  ava_intr_structop_convert_order(
    &failure_order, args->forder?
    args->forder : args->order);

  dst.type = oldv.type = newv.type = actualv.type = ava_prt_data;
  dst.index = ava_codegen_push_reg(context, ava_prt_data, 4);
  oldv.index = dst.index + 1;
  newv.index = dst.index + 2;
  actualv.index = dst.index + 3;
  if (ava_sft_int == instance->field->type) {
    oldt.type = newt.type = actualt.type = ava_prt_int;
    oldt.index = ava_codegen_push_reg(context, ava_prt_int, 3);
    newt.index = oldt.index + 1;
    actualt.index = oldt.index + 2;
  } else {
    oldt = oldv;
    newt = newv;
    actualt = actualv;
  }

  success.type = ava_prt_int;
  success.index = ava_codegen_push_reg(context, ava_prt_int, 1);

  ava_ast_node_cg_evaluate(args->dst, &dst, context);

  if (args->actual_lvalue)
    ava_ast_node_cg_set_up(args->actual_lvalue, context);

  ava_ast_node_cg_evaluate(args->old, &oldv, context);
  ava_ast_node_cg_evaluate(args->new, &newv, context);

  ava_codegen_set_location(context, location);

  if (ava_sft_int == instance->field->type) {
    AVA_PCXB(ld_reg_d, oldt, oldv);
    AVA_PCXB(ld_reg_d, newt, newv);
    AVA_PCXB(S_ia_cas, success, actualt, dst, SXT, FIELD,
             oldt, newt, !!args->volatil, !!args->weak,
             success_order, failure_order);
  } else {
    AVA_PCXB(S_pa_cas, success, actualt, dst, SXT, FIELD,
             oldt, newt, !!args->volatil, !!args->weak,
             success_order, failure_order);
  }

  if (args->actual_lvalue) {
    if (ava_sft_int == instance->field->type)
      AVA_PCXB(ld_reg_u, actualv, actualt);

    instance->reg_rvalue.reg = actualv;
    ava_ast_node_cg_discard(args->actual_lvalue, context);
    ava_ast_node_cg_tear_down(args->actual_lvalue, context);
  }

  if (success_dst)
    AVA_PCXB(ld_reg_u, *success_dst, success);

  ava_codegen_pop_reg(context, ava_prt_int, 1);
  if (ava_sft_int == instance->field->type)
    ava_codegen_pop_reg(context, ava_prt_int, 3);
  ava_codegen_pop_reg(context, ava_prt_data, 4);
}

MACRO(cas)

/******************** S.rmw ********************/

typedef struct {
  ava_ast_node* sxt, * dst, * field, * volatil, * order, * operation, * value;
} ava_intr_S_rmw_args;

static void ava_intr_S_rmw_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_rmw_args* args);
static void ava_intr_S_rmw_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* actual_dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_rmw_args* args);

static const ava_argument_spec ava_intr_S_rmw_argspecs[] = {
  ARG_POS, ARG_POS, ARG_POS, ARG_VOLATILE, ARG_ORDER, ARG_POS, ARG_POS,
};

static const ava_function ava_intr_S_rmw_prototype= {
  .args = ava_intr_S_rmw_argspecs,
  .num_args = ava_lenof(ava_intr_S_rmw_argspecs),
};

static const ava_funmac_type ava_intr_S_rmw_type = {
  .prototype = &ava_intr_S_rmw_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_rmw_accept,
  .cg_evaluate = (ava_funmac_cg_evaluate_f)ava_intr_S_rmw_cg_evaluate,
  .cg_discard  = (ava_funmac_cg_evaluate_f)ava_intr_S_rmw_cg_evaluate,
};

static void ava_intr_S_rmw_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_rmw_args* args
) {
  ava_pcode_rmw_op op;

  DO_SXT_LOOKUP();
  DO_FIELD_LOOKUP();

  ava_intr_structop_check_atomic_sanity(
    *instance, context, ava_true, args->order, NULL,
    &args->field->location);
  if (!ava_intr_structop_check_rmw_op_valid(context, args->operation))
    return;

  ava_intr_structop_convert_rmw_op(&op, args->operation);
  if (op != ava_pro_xchg && ava_sft_ptr == (*instance)->field->type)
    ava_macsub_record_error(
      context, ava_error_non_xchg_rmw_on_ptr(&args->operation->location));
}

static void ava_intr_S_rmw_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* actual_dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_rmw_args* args
) {
  ava_pcode_register dst, valv, valt, actualt;
  ava_pcode_rmw_op op;
  ava_pcode_memory_order order;

  ava_intr_structop_convert_rmw_op(&op, args->operation);
  ava_intr_structop_convert_order(&order, args->order);

  dst.type = valv.type = ava_prt_data;
  dst.index = ava_codegen_push_reg(context, ava_prt_data, 2);
  valv.index = dst.index + 1;

  if (ava_sft_int == instance->field->type) {
    valt.type = actualt.type = ava_prt_int;
    valt.index = ava_codegen_push_reg(context, ava_prt_int, 2);
    actualt.index = valt.index + 1;
  } else {
    valt = valv;
    actualt.type = ava_prt_data;
    actualt.index = ava_codegen_push_reg(context, ava_prt_data, 1);
  }

  ava_ast_node_cg_evaluate(args->dst, &dst, context);
  ava_ast_node_cg_evaluate(args->value, &valv, context);

  ava_codegen_set_location(context, location);
  if (ava_sft_int == instance->field->type) {
    AVA_PCXB(ld_reg_d, valt, valv);
    AVA_PCXB(S_ia_rmw, actualt, dst, SXT, FIELD, valt,
             op, !!args->volatil, order);
  } else {
    AVA_PCXB(S_pa_xch, actualt, dst, SXT, FIELD, valt,
             !!args->volatil, order);
  }

  if (actual_dst) {
    if (ava_sft_int == instance->field->type) {
      AVA_PCXB(ld_reg_u, *actual_dst, actualt);
    } else {
      AVA_PCXB(ld_reg_s, *actual_dst, actualt);
    }
  }

  if (ava_sft_int == instance->field->type)
    ava_codegen_pop_reg(context, ava_prt_int, 2);
  else
    ava_codegen_pop_reg(context, ava_prt_data, 1);

  ava_codegen_pop_reg(context, ava_prt_data, 2);
}

MACRO(rmw)

/******************** S.ix ********************/

typedef struct {
  ava_ast_node* sxt, * base, * offset;
} ava_intr_S_ix_args;

static void ava_intr_S_ix_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_ix_args* args);
static void ava_intr_S_ix_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_ix_args* args);

static const ava_argument_spec ava_intr_S_ix_argspecs[] = {
  ARG_POS, ARG_POS, ARG_POS,
};

static const ava_function ava_intr_S_ix_prototype = {
  .args = ava_intr_S_ix_argspecs,
  .num_args = ava_lenof(ava_intr_S_ix_argspecs),
};

static const ava_funmac_type ava_intr_S_ix_type = {
  .prototype = &ava_intr_S_ix_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_ix_accept,
  .cg_evaluate = (ava_funmac_cg_evaluate_f)ava_intr_S_ix_cg_evaluate,
};

static void ava_intr_S_ix_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_ix_args* args
) {
  DO_SXT_LOOKUP();
  REQUIRE_COMPOSABLE(args->sxt);
}

static void ava_intr_S_ix_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_ix_args* args
) {
  ava_pcode_register base, offsetv, offseti;

  base.type = offsetv.type = ava_prt_data;
  base.index = ava_codegen_push_reg(context, ava_prt_data, 2);
  offsetv.index = base.index + 1;
  offseti.type = ava_prt_int;
  offseti.index = ava_codegen_push_reg(context, ava_prt_int, 1);

  ava_ast_node_cg_evaluate(args->base, &base, context);
  ava_ast_node_cg_evaluate(args->offset, &offsetv, context);

  ava_codegen_set_location(context, location);
  AVA_PCXB(ld_reg_d, offseti, offsetv);
  AVA_PCXB(S_gap, *dst, base, SXT, offseti);

  ava_codegen_pop_reg(context, ava_prt_int, 1);
  ava_codegen_pop_reg(context, ava_prt_data, 2);
}

MACRO(ix)

/******************** S.sizeof, S.alignof ********************/

typedef struct {
  ava_ast_node* sxt;
} ava_intr_S_sizing_args;

static void ava_intr_S_sizing_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_sizing_args* args);
static void ava_intr_S_sizing_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_sizing_args* args);

static const ava_argument_spec ava_intr_S_sizing_argspecs[] = { ARG_POS };
static const ava_function ava_intr_S_sizing_prototype = {
  .args = ava_intr_S_sizing_argspecs,
  .num_args = ava_lenof(ava_intr_S_sizing_argspecs),
};

static const ava_funmac_type ava_intr_S_sizeof_type = {
  .prototype = &ava_intr_S_sizing_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_sizing_accept,
  .cg_evaluate = (ava_funmac_cg_evaluate_f)ava_intr_S_sizing_cg_evaluate,
  .userdata = (void*)&ava_intr_S_sizeof_type,
};
static const ava_funmac_type ava_intr_S_alignof_type = {
  .prototype = &ava_intr_S_sizing_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_sizing_accept,
  .cg_evaluate = (ava_funmac_cg_evaluate_f)ava_intr_S_sizing_cg_evaluate,
  .userdata = (void*)&ava_intr_S_alignof_type,
};

static void ava_intr_S_sizing_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_sizing_args* args
) {
  DO_SXT_LOOKUP();
}

static void ava_intr_S_sizing_cg_evaluate(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_sizing_args* args
) {
  ava_pcode_register tmp;

  ava_codegen_set_location(context, location);
  tmp.type = ava_prt_int;
  tmp.index = ava_codegen_push_reg(context, ava_prt_int, 1);
  if (&ava_intr_S_sizeof_type == userdata)
    AVA_PCXB(S_sizeof, tmp, SXT);
  else
    AVA_PCXB(S_alignof, tmp, SXT);
  AVA_PCXB(ld_reg_u, *dst, tmp);
  ava_codegen_pop_reg(context, ava_prt_int, 1);
}

MACRO(sizeof)
MACRO(alignof)

/******************** S.membar ********************/

typedef struct {
  ava_ast_node* order;
} ava_intr_S_membar_args;

static void ava_intr_S_membar_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_membar_args* args);
static void ava_intr_S_membar_cg_discard(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_membar_args* args);

static const ava_argument_spec ava_intr_S_membar_argspecs[] = { ARG_POS };
static const ava_function ava_intr_S_membar_prototype = {
  .args = ava_intr_S_membar_argspecs,
  .num_args = ava_lenof(ava_intr_S_membar_argspecs),
};

static const ava_funmac_type ava_intr_S_membar_type = {
  .prototype = &ava_intr_S_membar_prototype,
  .accept = (ava_funmac_accept_f)ava_intr_S_membar_accept,
  .cg_discard = (ava_funmac_cg_evaluate_f)ava_intr_S_membar_cg_discard,
};

static void ava_intr_S_membar_accept(
  void* userdata, ava_intr_structop_instance** instance,
  ava_macsub_context* context,
  const ava_compile_location* location,
  ava_intr_S_membar_args* args
) {
  ava_intr_structop_check_order_valid(context, args->order);
}

static void ava_intr_S_membar_cg_discard(
  void* userdata, ava_intr_structop_instance* instance,
  const ava_pcode_register* dst,
  ava_codegen_context* context,
  const ava_compile_location* location,
  const ava_intr_S_membar_args* args
) {
  ava_pcode_memory_order order;

  ava_intr_structop_convert_order(&order, args->order);
  ava_codegen_set_location(context, location);
  AVA_PCXB(S_membar, order);
}

MACRO(membar)

/******************** S.static ********************/

AVA_STATIC_STRING(ava_intr_S_static_thread_local_option,
                  "-thread-local");

typedef enum {
  ava_isst_scalar = 0, ava_isst_array, ava_isst_tail
} ava_intr_S_static_type;

typedef struct {
  ava_ast_node header;

  const ava_symbol* struct_sym;
  ava_symbol* var_sym;

  ava_intr_S_static_type type;
  ava_integer array_length;
  ava_bool thr_local;

  ava_bool defined;
} ava_intr_S_static;

static ava_string ava_intr_S_static_to_string(const ava_intr_S_static* node);
static void ava_intr_S_static_cg_define(
  ava_intr_S_static* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_S_static_vtable = {
  .name = "static strangelet declaration",
  .to_string = (ava_ast_node_to_string_f)ava_intr_S_static_to_string,
  .cg_define = (ava_ast_node_cg_define_f)ava_intr_S_static_cg_define,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_S_static_cg_define, /*sic*/
};

AVA_DEF_MACRO_SUBST(ava_intr_S_static_subst) {
  const ava_parse_unit* sxt_unit = NULL, * name_unit = NULL;
  const ava_parse_unit* linkage_name_unit = NULL;
  const ava_parse_unit* option_unit, * length_unit;
  ava_string sxt = AVA_ABSENT_STRING, name = AVA_ABSENT_STRING;
  ava_string linkage_name = AVA_ABSENT_STRING, option, length_str;
  ava_intr_S_static_type array_type;
  ava_intr_S_static* node;

  node = AVA_NEW(ava_intr_S_static);

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_CURRENT_UNIT(sxt_unit, "struct");
      AVA_MACRO_ARG_BAREWORD(sxt, "struct");

      node->struct_sym = ava_intr_structop_look_struct_sym_up(
        context, sxt, &sxt_unit->location);
      if (!node->struct_sym)
        return ava_macsub_silent_error_result(&provoker->location);

      AVA_MACRO_ARG_CURRENT_UNIT(name_unit, "name");
      AVA_MACRO_ARG_BAREWORD(name, "name");

      if (AVA_MACRO_ARG_HAS_ARG()) {
        AVA_MACRO_ARG_CURRENT_UNIT(linkage_name_unit, "linkage-name");
        if (ava_put_astring != linkage_name_unit->type) {
          linkage_name_unit = NULL;
        } else {
          linkage_name = linkage_name_unit->v.string;
          AVA_MACRO_ARG_CONSUME();
        }
      }

      while (AVA_MACRO_ARG_HAS_ARG()) {
        AVA_MACRO_ARG_CURRENT_UNIT(option_unit, "option");
        AVA_MACRO_ARG_BAREWORD(option, "option");

        if (ava_string_equal(ava_intr_S_static_thread_local_option, option)) {
          if (node->thr_local) {
            ava_macsub_record_error(
              context, ava_error_macro_arg_given_more_than_once(
                &option_unit->location, ava_intr_S_static_thread_local_option));
          } else {
            node->thr_local = ava_true;
          }
        } else if (ava_string_equal(AVA_ASCII9_STRING("-n"), option) ||
                   ava_string_equal(AVA_ASCII9_STRING("-t"), option)) {
          AVA_MACRO_ARG_CURRENT_UNIT(length_unit, "length");
          AVA_MACRO_ARG_BAREWORD(length_str, "length");

          if (ava_string_equal(AVA_ASCII9_STRING("-n"), option))
            array_type = ava_isst_array;
          else
            array_type = ava_isst_tail;

          if (!node->struct_sym->v.sxt.def->is_composable &&
              ava_isst_array == array_type) {
            ava_macsub_record_error(
              context, ava_error_cannot_operate_array_of_noncomposable(
                &option_unit->location, node->struct_sym->full_name));
          } else if (!ava_intr_structop_has_tail(node->struct_sym->v.sxt.def) &&
                     ava_isst_tail == array_type) {
            ava_macsub_record_error(
              context, ava_error_tail_operation_on_struct_without_tail(
                &option_unit->location, node->struct_sym->full_name));
          } else if (array_type == node->type) {
            ava_macsub_record_error(
              context, ava_error_macro_arg_given_more_than_once(
                &option_unit->location, option_unit->v.string));
          } else if (!ava_integer_try_parse(
                       &node->array_length, length_str, -1) ||
                     node->array_length < 0 ||
                     node->array_length !=
                     (ava_integer)(size_t)node->array_length) {
            ava_macsub_record_error(
              context, ava_error_macro_arg_not_an_integer(
                &length_unit->location, AVA_ASCII9_STRING("length")));
          } else {
            node->type = array_type;
          }
        } else {
          AVA_STATIC_STRING(expected_options,
                            "one of -thread-local, -t, or -n");
          return ava_macsub_error_result(
            context, ava_error_bad_macro_keyword(
              &option_unit->location,
              self->full_name, option, expected_options));
        }
      }
    }
  }

  if (linkage_name_unit &&
      ava_v_private == *(const ava_visibility*)self->v.macro.userdata) {
    ava_macsub_record_error(
      context, ava_error_linkage_name_on_non_linked(
        &linkage_name_unit->location));
  }

  node->header.v = &ava_intr_S_static_vtable;
  node->header.context = context;
  node->header.location = provoker->location;
  node->var_sym = AVA_NEW(ava_symbol);
  node->var_sym->type = ava_st_global_variable;
  node->var_sym->level = ava_macsub_get_level(context);
  node->var_sym->visibility = *(const ava_visibility*)self->v.macro.userdata;
  node->var_sym->definer = (ava_ast_node*)node;
  node->var_sym->full_name = ava_macsub_apply_prefix(context, name);
  node->var_sym->v.var.is_mutable = ava_false;
  if (ava_string_is_present(linkage_name)) {
    node->var_sym->v.var.name.scheme = ava_nms_none;
    node->var_sym->v.var.name.name = linkage_name;
  } else {
    node->var_sym->v.var.name.scheme = ava_nms_ava;
    node->var_sym->v.var.name.name = node->var_sym->full_name;
  }

  ava_macsub_put_symbol(context, node->var_sym, &name_unit->location);

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v.node = (ava_ast_node*)node
  };
}

static ava_string ava_intr_S_static_to_string(const ava_intr_S_static* node) {
  ava_string accum;

  switch (node->var_sym->visibility) {
  case ava_v_private:  accum = AVA_ASCII9_STRING("S.static "); break;
  case ava_v_internal: accum = AVA_ASCII9_STRING("S.Static "); break;
  case ava_v_public:   accum = AVA_ASCII9_STRING("S.STATIC "); break;
  }

  accum = ava_strcat(accum, node->struct_sym->full_name);
  accum = ava_strcat(accum, AVA_ASCII9_STRING(" "));
  accum = ava_strcat(accum, node->var_sym->full_name);
  switch (node->type) {
  case ava_isst_scalar: break;

  case ava_isst_tail:
    accum = ava_strcat(accum, AVA_ASCII9_STRING(" -t "));
    accum = ava_strcat(accum, ava_to_string(
                         ava_value_of_integer(node->array_length)));
    break;

  case ava_isst_array:
    accum = ava_strcat(accum, AVA_ASCII9_STRING(" -n "));
    accum = ava_strcat(accum, ava_to_string(
                         ava_value_of_integer(node->array_length)));
    break;
  }

  if (node->thr_local) {
    accum = ava_strcat(accum, AVA_ASCII9_STRING(" "));
    accum = ava_strcat(accum, ava_intr_S_static_thread_local_option);
  }

  return accum;
}

static void ava_intr_S_static_cg_define(
  ava_intr_S_static* node, ava_codegen_context* context
) {
  size_t ix;

  if (node->defined) return;
  node->defined = ava_true;

  ava_ast_node_cg_define(node->struct_sym->definer, context);

  switch (node->type) {
  case ava_isst_scalar:
    ix = AVA_PCGB(S_bss, node->struct_sym->pcode_index,
                  node->var_sym->visibility > ava_v_private,
                  node->var_sym->v.var.name,
                  node->thr_local);
    break;

  case ava_isst_array:
    ix = AVA_PCGB(S_bss_a, node->struct_sym->pcode_index,
                  node->var_sym->visibility > ava_v_private,
                  node->var_sym->v.var.name,
                  node->thr_local, node->array_length);
    break;

  case ava_isst_tail:
    ix = AVA_PCGB(S_bss_t, node->struct_sym->pcode_index,
                  node->var_sym->visibility > ava_v_private,
                  node->var_sym->v.var.name,
                  node->thr_local, node->array_length);
    break;
  }

  node->var_sym->pcode_index = ix;
  ava_codegen_export(context, node->var_sym);
}
