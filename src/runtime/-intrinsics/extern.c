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

#include <stdlib.h>

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
#include "../avalanche/alloc.h"
#include "../avalanche/string.h"
#include "../avalanche/value.h"
#include "../avalanche/list.h"
#include "../avalanche/function.h"
#include "../avalanche/name-mangle.h"
#include "../avalanche/symbol-table.h"
#include "../avalanche/macsub.h"
#include "../avalanche/macro-arg.h"
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/exception.h"
#include "extern.h"

typedef struct {
  ava_ast_node header;

  ava_string self_name;
  ava_symbol* symbol;
  ava_bool defined;
} ava_intr_extern;

static ava_string ava_intr_extern_to_string(const ava_intr_extern*);
static void ava_intr_extern_cg_define(
  ava_intr_extern* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_extern_vtable = {
  .name = "extern",
  .to_string = (ava_ast_node_to_string_f)ava_intr_extern_to_string,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_extern_cg_define, /*sic*/
  .cg_define = (ava_ast_node_cg_define_f)ava_intr_extern_cg_define,
};

ava_macro_subst_result ava_intr_extern_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  ava_exception_handler handler;
  const ava_parse_unit* ava_name_unit, * prototype_first_unit = NULL;
  ava_string ava_name, native_name;
  ava_value prototype_list = ava_value_of_string(AVA_ASCII9_STRING("1"));
  const ava_function* prototype;
  ava_symbol* definition;
  ava_intr_extern* this;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_CURRENT_UNIT(ava_name_unit, "ava-name");
      AVA_MACRO_ARG_BAREWORD(ava_name, "ava-name");
      AVA_MACRO_ARG_STRINGOID(native_name, "native-name");
      AVA_MACRO_ARG_CURRENT_UNIT(prototype_first_unit, "prototype");
      AVA_MACRO_ARG_FOR_REST {
        ava_value arg;
        AVA_MACRO_ARG_LITERAL(arg, "prototype element");
        prototype_list = ava_list_append(prototype_list, arg);
      }
    }
  }

  ava_try(handler) {
    prototype = ava_function_of_value(prototype_list);
  } ava_catch (handler, ava_format_exception) {
    AVA_STATIC_STRING(message, "Invalid function prototype: ");
    return ava_macsub_error_result(
      context, ava_string_concat(
        message, ava_to_string(handler.value)),
      &prototype_first_unit->location);
  } ava_catch_all {
    ava_rethrow(&handler);
  }

  definition = AVA_NEW(ava_symbol);
  this = AVA_NEW(ava_intr_extern);

  this->header.v = &ava_intr_extern_vtable;
  this->header.location = provoker->location;
  this->header.context = context;
  this->self_name = self->full_name;
  this->symbol = definition;
  this->defined = ava_false;

  definition->type = ava_st_global_function;
  definition->level = 0; /* always global */
  definition->visibility = *(const ava_visibility*)self->v.macro.userdata;
  definition->definer = (ava_ast_node*)this;
  definition->full_name = ava_macsub_apply_prefix(context, ava_name);
  definition->v.var.is_mutable = ava_false;
  definition->v.var.name.scheme = ava_nms_none;
  definition->v.var.name.name = native_name;
  definition->v.var.fun = *prototype;

  ava_macsub_put_symbol(context, definition, &ava_name_unit->location);

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static ava_string ava_intr_extern_to_string(const ava_intr_extern* this) {
  ava_list_value accum = ava_empty_list();
  ava_value prototype = ava_value_of_function(&this->symbol->v.var.fun);

  accum = ava_list_append(accum, ava_value_of_string(this->self_name));
  accum = ava_list_append(accum, ava_value_of_string(this->symbol->full_name));
  accum = ava_list_append(
    accum, ava_value_of_string(this->symbol->v.var.name.name));
  accum = ava_list_append(
    accum, ava_list_slice(prototype, 1, ava_list_length(prototype)));
  return ava_to_string(accum.v);
}

static void ava_intr_extern_cg_define(
  ava_intr_extern* this,
  ava_codegen_context* context
) {
  if (this->defined) return;

  ava_codegen_set_global_location(context, &this->header.location);
  this->symbol->pcode_index =
    AVA_PCGB(ext_fun, this->symbol->v.var.name,
             &this->symbol->v.var.fun);
  ava_codegen_export(context, this->symbol);

  this->defined = ava_true;
}
