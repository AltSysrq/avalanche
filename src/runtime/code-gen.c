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

typedef SLIST_HEAD(,ava_codegen_jprot_s) ava_codegen_jprot_stack;
typedef SLIST_HEAD(,ava_codegen_symlabel_s) ava_codegen_symlabel_stack;
typedef SLIST_HEAD(,ava_codegen_symreg_s) ava_codegen_symreg_stack;

struct ava_codegen_context_s {
  ava_compile_error_list* errors;
  ava_pcx_builder* builder;
  ava_compile_location last_location;

  ava_pcode_register_index register_stacks[ava_prt_function+1];

  ava_uint next_label;
  ava_codegen_symlabel_stack symlabels;
  ava_codegen_symreg_stack symregs;
  ava_codegen_jprot_stack jprots;
};

static ava_codegen_context* ava_codegen_context_alloc(
  ava_pcx_builder* builder,
  ava_compile_error_list* errors);
static ava_bool ava_codegen_crosses_jprot(
  const ava_codegen_context* context, ava_uint target);

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
  this->last_location.filename = AVA_EMPTY_STRING;
  this->last_location.line_offset = 0;
  this->last_location.start_line = 0;
  this->last_location.end_line = 0;
  this->last_location.start_column = 0;
  this->last_location.end_column = 0;
  this->next_label = 0;
  SLIST_INIT(&this->symlabels);
  SLIST_INIT(&this->symregs);
  SLIST_INIT(&this->jprots);
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
  if (ava_strcmp(context->last_location.filename, location->filename) ||
      context->last_location.line_offset != location->line_offset ||
      context->last_location.start_line != location->start_line ||
      context->last_location.end_line != location->end_line ||
      context->last_location.start_column != location->start_column ||
      context->last_location.end_column != location->end_column) {
    ava_pcxb_src_pos(context->builder, location->filename,
                     location->line_offset,
                     location->start_line, location->end_line,
                     location->start_column, location->end_column);
    context->last_location = *location;
  }
}

void ava_codegen_set_global_location(
  ava_codegen_context* context,
  const ava_compile_location* location
) {
  ava_pcg_builder* builder = ava_pcx_builder_get_parent(context->builder);

  ava_pcgb_src_pos(builder, location->filename,
                   location->line_offset,
                   location->start_line, location->end_line,
                   location->start_column, location->end_column);
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

void ava_codegen_push_jprot(ava_codegen_jprot* jprot,
                            ava_codegen_context* context,
                            ava_codegen_jprot_exit_f exit,
                            void* userdata) {
  jprot->ordinal = ava_codegen_genlabel(context);
  jprot->exit = exit;
  jprot->userdata = userdata;
  SLIST_INSERT_HEAD(&context->jprots, jprot, next);
}

void ava_codegen_pop_jprot(ava_codegen_context* context) {
  ava_codegen_jprot* jprot;

  jprot = SLIST_FIRST(&context->jprots);
  SLIST_REMOVE_HEAD(&context->jprots, next);

  (*jprot->exit)(context, NULL, jprot->userdata);
}

void ava_codegen_push_symlabel(
  ava_codegen_symlabel* symlabel,
  ava_codegen_context* context,
  const ava_codegen_symlabel_name* name,
  ava_uint label
) {
  symlabel->name = name;
  symlabel->label = label;
  SLIST_INSERT_HEAD(&context->symlabels, symlabel, next);
}

void ava_codegen_pop_symlabel(ava_codegen_context* context) {
  SLIST_REMOVE_HEAD(&context->symlabels, next);
}

ava_uint ava_codegen_get_symlabel(const ava_codegen_context* context,
                                  const ava_codegen_symlabel_name* name) {
  const ava_codegen_symlabel* symlabel;

  SLIST_FOREACH(symlabel, &context->symlabels, next)
    if (name == symlabel->name)
      return symlabel->label;

  return AVA_LABEL_NONE;
}

void ava_codegen_push_symreg(
  ava_codegen_symreg* elt,
  ava_codegen_context* context,
  const ava_codegen_symreg_name* name,
  ava_pcode_register reg
) {
  elt->name = name;
  elt->reg = reg;
  SLIST_INSERT_HEAD(&context->symregs, elt, next);
}

void ava_codegen_pop_symreg(ava_codegen_context* context) {
  SLIST_REMOVE_HEAD(&context->symregs, next);
}

const ava_pcode_register* ava_codegen_get_symreg(
  const ava_codegen_context* context,
  const ava_codegen_symreg_name* name
) {
  const ava_codegen_symreg* symreg;

  SLIST_FOREACH(symreg, &context->symregs, next)
    if (name == symreg->name)
      return &symreg->reg;

  return NULL;
}

ava_uint ava_codegen_genlabel(ava_codegen_context* context) {
  return context->next_label++;
}

static ava_bool ava_codegen_crosses_jprot(
  const ava_codegen_context* context, ava_uint target
) {
  return !SLIST_EMPTY(&context->jprots) &&
    SLIST_FIRST(&context->jprots)->ordinal > target;
}

void ava_codegen_branch(ava_codegen_context* context,
                        const ava_compile_location* location,
                        ava_pcode_register key,
                        ava_integer value,
                        ava_bool invert,
                        ava_uint target) {
  ava_uint tmplbl;

  if (ava_codegen_crosses_jprot(context, target)) {
    tmplbl = ava_codegen_genlabel(context);
    /* Need to run protector exits. Direct the non-taken path around them. */
    ava_codegen_branch(context, location, key, value, !invert, tmplbl);
    ava_codegen_goto(context, location, target);
    AVA_PCXB(label, tmplbl);
  } else {
    AVA_PCXB(branch, key, value, invert, target);
  }
}

void ava_codegen_goto(ava_codegen_context* context,
                      const ava_compile_location* location,
                      ava_uint target) {
  ava_codegen_jprot* jprot;

  if (ava_codegen_crosses_jprot(context, target)) {
    /* Temporarily pop the jprot off the stack in case it also does an early
     * return.
     */
    jprot = SLIST_FIRST(&context->jprots);
    SLIST_REMOVE_HEAD(&context->jprots, next);
    (*jprot->exit)(context, location, jprot->userdata);
    /* Invoke any other exits needed; otherwise, just jump to the label
     * directly.
     */
    ava_codegen_goto(context, location, target);
    SLIST_INSERT_HEAD(&context->jprots, jprot, next);
  } else {
    AVA_PCXB(goto, target);
  }
}

void ava_codegen_ret(ava_codegen_context* context,
                     const ava_compile_location* location,
                     ava_pcode_register value) {
  ava_codegen_jprot* jprot;

  if (SLIST_EMPTY(&context->jprots)) {
    AVA_PCXB(ret, value);
  } else {
    /* Invoke all protector exits. As with goto, they must be temporarily
     * popped in case they themselves trigger an early exit.
     */
    jprot = SLIST_FIRST(&context->jprots);
    SLIST_REMOVE_HEAD(&context->jprots, next);
    (*jprot->exit)(context, location, jprot->userdata);
    ava_codegen_ret(context, location, value);
    SLIST_INSERT_HEAD(&context->jprots, jprot, next);
  }
}

ava_pcode_global_list* ava_codegen_run(
  ava_ast_node* root,
  ava_list_value implicit_packages,
  ava_compile_error_list* errors
) {
  ava_pcg_builder* global_builder = ava_pcg_builder_new();
  ava_pcx_builder* init_builder;
  ava_uint init_function;
  const ava_function* init_prototype;
  ava_demangled_name init_name;
  ava_list_value init_vars;
  ava_codegen_context* context;
  size_t i, n;

  init_name.scheme = ava_nms_ava;
  init_name.name = AVA_ASCII9_STRING("\\init");
  init_prototype = ava_function_of_value(
    ava_value_of_cstring("1 ava pos"));
  init_vars = ava_list_of_values(
    (ava_value[]) { ava_empty_list().v }, 1);

  ava_pcgb_src_pos(global_builder, root->location.filename,
                   root->location.line_offset,
                   root->location.start_line, root->location.end_line,
                   root->location.start_column, root->location.end_column);

  n = ava_list_length(implicit_packages);
  for (i = 0; i < n; ++i)
    ava_pcgb_load_pkg(
      global_builder, ava_to_string(ava_list_index(implicit_packages, i)));

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
