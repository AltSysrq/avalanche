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
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/name-mangle.h"
#include "avalanche/function.h"
#include "avalanche/list.h"
#include "avalanche/parser.h"
#include "avalanche/macsub.h"
#include "avalanche/symbol.h"
#include "avalanche/pcode.h"
#include "avalanche/code-gen.h"

struct ava_codegen_context_s {
  ava_compile_error_list* errors;
  ava_pcx_builder* builder;
  ava_string last_src_filename;
  ava_integer last_src_line;

  ava_pcode_register_index register_stacks[ava_prt_function+1];
};

static ava_codegen_context* ava_codegen_context_alloc(
  ava_pcx_builder* builder,
  ava_compile_error_list* errors);

void ava_codegen_error(ava_codegen_context* context,
                       const ava_ast_node* node,
                       ava_compile_error* error) {
  TAILQ_INSERT_TAIL(context->errors, error, next);
}

static ava_codegen_context* ava_codegen_context_alloc(
  ava_pcx_builder* builder,
  ava_compile_error_list* errors
) {
  ava_codegen_context* this;

  this = AVA_NEW(ava_codegen_context);
  this->builder = builder;
  this->errors = errors;
  this->last_src_filename = AVA_EMPTY_STRING;
  this->last_src_line = 0;
  return this;
}

ava_codegen_context* ava_codegen_context_new(
  const ava_codegen_context* parent,
  ava_pcx_builder* builder
) {
  return ava_codegen_context_alloc(builder, parent->errors);
}


ava_pcode_register_index ava_codegen_push_reg(
  ava_codegen_context* context,
  ava_pcode_register_type register_type,
  ava_uint count
) {
  ava_pcode_register_index ret;

  ret = context->register_stacks[register_type];
  context->register_stacks[register_type] += count;
  ava_pcxb_push(context->builder, register_type, count);
  return ret;
}

void ava_codegen_pop_reg(
  ava_codegen_context* context,
  ava_pcode_register_type register_type,
  ava_uint count
) {
  assert(count <= context->register_stacks[register_type]);
  context->register_stacks[register_type] -= count;
  ava_pcxb_pop(context->builder, register_type, count);
}

void ava_codegen_set_location(
  ava_codegen_context* context,
  const ava_compile_location* location
) {
  if (ava_strcmp(context->last_src_filename, location->filename)) {
    ava_pcxb_src_file(context->builder, location->filename);
    context->last_src_filename = location->filename;
  }

  if (context->last_src_line != location->start_line) {
    ava_pcxb_src_line(context->builder, location->start_line);
    context->last_src_line = location->start_line;
  }
}

void ava_codegen_set_global_location(
  ava_codegen_context* context,
  const ava_compile_location* location
) {
  ava_pcg_builder* builder = ava_pcx_builder_get_parent(context->builder);

  ava_pcgb_src_file(builder, location->filename);
  ava_pcgb_src_line(builder, location->start_line);
}

void ava_codegen_export(
  ava_codegen_context* context, const ava_symbol* symbol
) {
  switch (symbol->visibility) {
  case ava_v_private: break;
  case ava_v_internal:
    AVA_PCGB(export, symbol->pcode_index, ava_false, symbol->full_name);
    break;

  case ava_v_public:
    AVA_PCGB(export, symbol->pcode_index, ava_true, symbol->full_name);
    break;
  }
}

ava_pcode_global_list* ava_codegen_run(
  ava_ast_node* root,
  ava_compile_error_list* errors
) {
  ava_pcg_builder* global_builder = ava_pcg_builder_new();
  ava_pcx_builder* init_builder;
  ava_uint init_function;
  const ava_function* init_prototype;
  ava_demangled_name init_name;
  ava_list_value init_vars;
  ava_codegen_context* context;

  init_name.scheme = ava_nms_ava;
  init_name.name = AVA_ASCII9_STRING("\\init");
  init_prototype = ava_function_of_value(
    ava_value_of_cstring("1 ava pos"));
  init_vars = ava_list_of_values(
    (ava_value[]) { ava_empty_list().v }, 1);

  ava_pcgb_src_file(global_builder, root->location.filename);
  ava_pcgb_src_line(global_builder, root->location.start_line);
  init_function = ava_pcgb_fun(
    global_builder,
    ava_false, init_name, init_prototype, init_vars,
    &init_builder);
  ava_pcgb_init(global_builder, init_function);

  context = ava_codegen_context_alloc(init_builder, errors);
  ava_ast_node_cg_discard(root, context);

  return ava_pcg_builder_get(global_builder);
}

ava_pcx_builder* ava_codegen_get_builder(const ava_codegen_context* context) {
  return context->builder;
}
