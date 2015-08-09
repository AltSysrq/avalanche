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
#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"
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

  ava_macsub_saved_symbol_table* saved_symtab;

  /* The below fields are populated upon postprocessing */
  ava_intr_funcall_type type;
  /* For all types but dynamic_invoke.
   * The function does not have an address or FFI.
   */
  ava_demangled_name static_function_name;
  const ava_function* static_function;
  /* For afa_ift_static_bind */
  ava_function_bound_argument* bound_args;
  size_t* variadic_collection;
} ava_intr_funcall;

static ava_string ava_intr_funcall_to_string(const ava_intr_funcall* node);
static void ava_intr_funcall_postprocess(ava_intr_funcall* node);
static void ava_intr_funcall_bind_parms(ava_intr_funcall* node);

static const ava_ast_node_vtable ava_intr_funcall_vtable = {
  .name = "function call",
  .to_string = (ava_ast_node_to_string_f)ava_intr_funcall_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_funcall_postprocess,
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

  this->saved_symtab = ava_macsub_save_symbol_table(
    context, &TAILQ_FIRST(&statement->units)->location);
  this->num_parms = num_parms;
  this->parms = ava_alloc(num_parms * sizeof(ava_ast_node*));
  i = 0;
  TAILQ_FOREACH(parm, &statement->units, next)
    this->parms[i] = ava_macsub_run_units(context, parm, parm);

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
  AVA_STATIC_STRING(ambiguous_message, "Function name is ambiguous: ");
  AVA_STATIC_STRING(not_found_message, "No such function: ");
  AVA_STATIC_STRING(not_a_function_message, "Not a function: ");

  const ava_symbol_table* symtab;
  size_t i;
  ava_string function_name;
  const ava_symbol* function_symbol;
  ava_symbol_table_get_result get_result;

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

  symtab = ava_macsub_get_saved_symbol_table(this->saved_symtab);
  get_result = ava_symbol_table_get(symtab, function_name);
  switch (get_result.status) {
  case ava_stgs_ok: break;
  case ava_stgs_ambiguous_weak:
    /* TODO: Indicate what the ambiguity is */
    ava_macsub_record_error(this->header.context,
                            ava_string_concat(ambiguous_message, function_name),
                            &this->parms[0]->location);
    return;

  case ava_stgs_not_found:
    ava_macsub_record_error(this->header.context,
                            ava_string_concat(not_found_message, function_name),
                            &this->parms[0]->location);
    return;
  }

  function_symbol = get_result.symbol;
  if (ava_st_global_function != function_symbol->type &&
      ava_st_local_function != function_symbol->type) {
    ava_macsub_record_error(this->header.context,
                            ava_string_concat(not_a_function_message,
                                              function_name),
                            &this->parms[0]->location);
    return;
  }

  this->static_function_name = function_symbol->v.var.name;
  this->static_function = &function_symbol->v.var.fun;
  ava_intr_funcall_bind_parms(this);
}

static void ava_intr_funcall_bind_parms(ava_intr_funcall* this) {
  ava_function_parameter parms[this->num_parms-1];
  ava_function_bound_argument bound_args[this->static_function->num_args];
  size_t variadic_collection[this->num_parms-1];
  ava_string bind_message;
  size_t i;

  for (i = 0; i < this->num_parms - 1; ++i) {
    if (this->parms[i+1]->v->is_spread) {
      parms[i].type = ava_fpt_spread;
    } else if (ava_ast_node_get_constexpr(this->parms[i+1], &parms[i].value)) {
      parms[i].type = ava_fpt_static;
    } else {
      parms[i].type = ava_fpt_dynamic;
    }
  }

  switch (ava_function_bind(this->static_function,
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
                            bind_message,
                            &this->header.location);
    this->type = ava_ift_dynamic_bind;
    break;
  }
}
