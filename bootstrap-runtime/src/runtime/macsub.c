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
#include <assert.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/parser.h"
#include "avalanche/code-gen.h"
#include "avalanche/macsub.h"
#include "avalanche/symbol.h"
#include "avalanche/symtab.h"
#include "avalanche/varscope.h"
#include "-intrinsics/fundamental.h"
#include "../../../common/bsd.h"

#define CONTROL_MACRO_PRECEDENCE -1
#define FUNCTION_MACRO_PRECEDENCE (AVA_MAX_OPERATOR_MACRO_PRECEDENCE+1)
#define STRING_PSEUDOMACRO_PRECEDENCE 20

typedef struct ava_compenv_s ava_compenv;

typedef struct {
  ava_string last_seed;
  ava_string base_prefix;
  ava_string prefix;
  ava_integer generation;
} ava_macsub_gensym_status;

struct ava_macsub_context_s {
  ava_symtab* symbol_table;
  ava_compenv* compenv;
  ava_varscope* varscope;
  ava_compile_error_list* errors;
  ava_bool* panic;
  ava_string symbol_prefix;
  unsigned level;
  ava_symbol* context_var;

  ava_macsub_gensym_status* gensym;
};

typedef enum {
  ava_mrmr_not_macro,
  ava_mrmr_is_macro,
  ava_mrmr_ambiguous
} ava_macsub_resolve_macro_result;

static ava_ast_node* ava_macsub_run_one_nonempty_statement(
  ava_macsub_context* context,
  ava_parse_statement* statement,
  ava_bool* consumed_rest,
  ava_bool subst_even_on_singleton);

static ava_bool ava_macsub_is_macroid(
  signed* min_precedence, ava_symbol_type* found_type,
  ava_macsub_context* context, const ava_parse_unit* unit);

static ava_macsub_resolve_macro_result ava_macsub_resolve_macro(
  const ava_symbol** dst,
  ava_macsub_context* context,
  const ava_parse_unit* provoker,
  ava_symbol_type target_type,
  unsigned target_precedence);

static ava_string ava_macsub_error_to_string(const ava_ast_node* this);
static ava_ast_node* ava_macsub_error_to_lvalue(
  const ava_ast_node* this, ava_ast_node* producer, ava_ast_node** reader);

ava_macsub_context* ava_macsub_context_new(
  ava_symtab* symbol_table,
  ava_compenv* compenv,
  ava_compile_error_list* errors,
  ava_string symbol_prefix
) {
  ava_macsub_context* this;

  this = AVA_NEW(ava_macsub_context);
  this->symbol_table = symbol_table;
  this->compenv = compenv;
  this->varscope = ava_varscope_new();
  this->errors = errors;
  this->symbol_prefix = symbol_prefix;
  this->panic = ava_alloc_atomic_zero(sizeof(ava_bool));
  this->level = 0;
  this->gensym = AVA_NEW(ava_macsub_gensym_status);

  this->gensym->last_seed = AVA_EMPTY_STRING;
  this->gensym->base_prefix = AVA_EMPTY_STRING;
  this->gensym->prefix = AVA_EMPTY_STRING;
  this->gensym->generation = 0;

  return this;
}

ava_symtab* ava_macsub_get_symtab(
  const ava_macsub_context* context
) {
  return context->symbol_table;
}

ava_compenv* ava_macsub_get_compenv(
  const ava_macsub_context* context
) {
  return context->compenv;
}

ava_varscope* ava_macsub_get_varscope(
  const ava_macsub_context* context
) {
  return context->varscope;
}

void ava_macsub_import(
  ava_string* absolutised, ava_string* ambiguous,
  ava_macsub_context* context,
  ava_string old_prefix, ava_string new_prefix,
  ava_bool absolute, ava_bool is_strong
) {
  context->symbol_table = ava_symtab_import(
    absolutised, ambiguous, context->symbol_table,
    old_prefix, new_prefix,
    absolute, is_strong);
}

ava_compile_error_list* ava_macsub_get_errors(
  const ava_macsub_context* context
) {
  return context->errors;
}

ava_string ava_macsub_apply_prefix(
  const ava_macsub_context* context,
  ava_string simple_name
) {
  return ava_strcat(context->symbol_prefix, simple_name);
}

unsigned ava_macsub_get_level(const ava_macsub_context* context) {
  return context->level;
}

void ava_macsub_gensym_seed(ava_macsub_context* context,
                            const ava_compile_location* location) {
  ava_ulong hash, rem;
  ava_uint i;
  char buf[13];

  /* The prefix is determined by hashing the source file itself */
  if (ava_strcmp(context->gensym->last_seed, location->source)) {
    hash = ava_value_hash_semiconsistent(
      ava_value_of_string(location->source));
    /* Base-32-encode the hash */
    for (i = 0; i < sizeof(buf); ++i) {
      rem = hash % 32;
      hash /= 32;
      if (rem < 10)
        buf[i] = '0' + rem;
      else
        buf[i] = 'a' + rem - 10;
    }

    context->gensym->last_seed = location->source;
    context->gensym->base_prefix =
      ava_strcat(
        AVA_ASCII9_STRING("?["),
        ava_strcat(
          ava_string_of_bytes(buf, sizeof(buf)),
          AVA_ASCII9_STRING("];")));
  }

  ++context->gensym->generation;
  context->gensym->prefix = ava_strcat(
    context->gensym->base_prefix,
    ava_strcat(
      ava_to_string(ava_value_of_integer(context->gensym->generation)),
      AVA_ASCII9_STRING(";")));
}

ava_string ava_macsub_gensym(const ava_macsub_context* context,
                             ava_string key) {
  return ava_strcat(context->gensym->prefix, key);
}

ava_macsub_context* ava_macsub_context_push_major(
  const ava_macsub_context* parent,
  ava_string interfix
) {
  ava_macsub_context* this;

  this = AVA_CLONE(*parent);
  this->symbol_table = ava_symtab_new(parent->symbol_table);
  this->varscope = ava_varscope_new();
  this->symbol_prefix = ava_strcat(parent->symbol_prefix, interfix);
  this->level = parent->level + 1;

  return this;
}

ava_macsub_context* ava_macsub_context_push_minor(
  const ava_macsub_context* parent,
  ava_string interfix
) {
  ava_macsub_context* this;

  this = AVA_CLONE(*parent);
  this->symbol_prefix = ava_strcat(parent->symbol_prefix, interfix);

  return this;
}

ava_symbol* ava_macsub_get_context_var(const ava_macsub_context* context) {
  return context->context_var;
}

ava_macsub_context* ava_macsub_context_with_context_var(
  const ava_macsub_context* parent, ava_symbol* context_var
) {
  ava_macsub_context* this;

  assert(!context_var || ava_st_local_variable == context_var->type ||
         ava_st_global_variable == context_var->type);
  this = AVA_CLONE(*parent);
  this->context_var = context_var;

  return this;
}

ava_bool ava_macsub_put_symbol(
  ava_macsub_context* context,
  ava_symbol* symbol,
  const ava_compile_location* location
) {
  const ava_symbol* conflicting;

  if (context->level > 0 && ava_v_private != symbol->visibility) {
    ava_macsub_record_error(
      context, ava_error_non_private_definition_in_nested_scope(location));
  }

  conflicting = ava_symtab_put(context->symbol_table, symbol);
  if (conflicting)
    ava_macsub_record_error(
      context, ava_error_symbol_redefined(location, symbol->full_name));

  return NULL == conflicting;
}

ava_ast_node* ava_macsub_run(ava_macsub_context* context,
                             const ava_compile_location* start,
                             ava_parse_statement_list* statements,
                             ava_intr_seq_return_policy return_policy) {
  if (TAILQ_EMPTY(statements))
    return ava_intr_seq_to_node(
      ava_intr_seq_new(context, start, return_policy));

  return ava_macsub_run_from(context, start,
                             TAILQ_FIRST(statements), return_policy);
}

ava_ast_node* ava_macsub_run_contents(
  ava_macsub_context* context,
  const ava_parse_unit* container
) {
  switch (container->type) {
  case ava_put_block:
    return ava_macsub_run(context, &container->location,
                          (ava_parse_statement_list*)&container->v.statements,
                          ava_isrp_void);
  case ava_put_substitution:
    return ava_macsub_run_units(context, container, container);

  default: abort();
  }
}

ava_ast_node* ava_macsub_run_units(ava_macsub_context* context,
                                   const ava_parse_unit* first,
                                   const ava_parse_unit* last) {
  ava_parse_statement_list statement_list;
  ava_parse_statement statement;
  const ava_parse_unit* src;
  ava_parse_unit* unit;
  ava_bool keep_going, consumed_rest;

  TAILQ_INIT(&statement_list);
  TAILQ_INIT(&statement.units);
  TAILQ_INSERT_TAIL(&statement_list, &statement, next);
  consumed_rest = ava_false;

  for (src = first, keep_going = ava_true; keep_going;
       keep_going = src != last, src = TAILQ_NEXT(src, next)) {
    unit = ava_clone(src, sizeof(ava_parse_unit));
    TAILQ_INSERT_TAIL(&statement.units, unit, next);
  }

  return ava_macsub_run_one_nonempty_statement(
    context, &statement, &consumed_rest, ava_false);
}

ava_ast_node* ava_macsub_run_single(ava_macsub_context* context,
                                    const ava_compile_location* start,
                                    ava_parse_statement* orig) {
  ava_parse_statement_list list;
  ava_parse_statement statement;
  ava_ast_node* result;

  TAILQ_INIT(&list);
  TAILQ_SWAP(&statement.units, &orig->units, ava_parse_unit_s, next);
  TAILQ_INSERT_TAIL(&list, &statement, next);

  result = ava_macsub_run(context, start, &list, ava_isrp_only);

  TAILQ_SWAP(&statement.units, &orig->units, ava_parse_unit_s, next);
  return result;
}

ava_ast_node* ava_macsub_run_from(ava_macsub_context* context,
                                  const ava_compile_location* start,
                                  ava_parse_statement* statement,
                                  ava_intr_seq_return_policy return_policy) {
  ava_intr_seq* seq;
  ava_bool consumed_rest = ava_false, plural;

  plural = statement && TAILQ_NEXT(statement, next);
  seq = ava_intr_seq_new(context, start, return_policy);

  for (; statement && !consumed_rest;
       statement = TAILQ_NEXT(statement, next)) {
    if (TAILQ_EMPTY(&statement->units)) continue;

    ava_intr_seq_add(
      seq,
      ava_macsub_run_one_nonempty_statement(
        context, statement, &consumed_rest,
        ava_isrp_void == return_policy ||
        (ava_isrp_only == return_policy && plural)));
  }

  return ava_intr_seq_to_node(seq);
}

static ava_ast_node* ava_macsub_run_one_nonempty_statement(
  ava_macsub_context* context,
  ava_parse_statement* statement,
  ava_bool* consumed_rest,
  ava_bool subst_even_on_singleton
) {
  const ava_symbol* symbol;
  const ava_parse_unit* unit, * candidate;
  ava_macro_subst_result subst_result;
  ava_symbol_type macro_type = ava_st_other, candidate_type;
  signed precedence = -0x7f, candidate_precedence;
  ava_bool singleton;
  const ava_compile_location* init_location;

  assert(!TAILQ_EMPTY(&statement->units));
  init_location = &TAILQ_FIRST(&statement->units)->location;

  tail_call:

  ava_macsub_expand_expanders(context, &statement->units);

  /* Statement could become empty via expander expansion */
  if (TAILQ_EMPTY(&statement->units))
    return ava_intr_statement(context, statement, init_location);

  unit = TAILQ_FIRST(&statement->units);

  if (*context->panic)
    return ava_macsub_silent_error(&unit->location);

  /* If there is only one unit, no macro substitution is performed, even if
   * that unit would reference a macro.
   */
  singleton = !TAILQ_NEXT(unit, next);
  if (singleton && !subst_even_on_singleton) goto no_macsub;

  candidate_precedence = 0x7FFFF;
  candidate = NULL;
  candidate_type = ava_st_other;

  TAILQ_FOREACH(unit, &statement->units, next) {
    if (ava_macsub_is_macroid(&precedence, &macro_type, context, unit)) {
      if (precedence < candidate_precedence ||
          /* If even, last wins (effectively right-to-left first-wins),
           * otherwise first wins.
           */
          (!(precedence & 1) && precedence == candidate_precedence)) {
        candidate = unit;
        candidate_precedence = precedence;
        candidate_type = macro_type;
      }
    }
  }

  /* Singletons can never be anything but control macros */
  if (singleton && ava_st_control_macro != candidate_type)
    goto no_macsub;

  if (candidate) {
    unit = candidate;
    switch (ava_macsub_resolve_macro(&symbol, context, candidate,
                                     candidate_type, candidate_precedence)) {
    case ava_mrmr_ambiguous: goto ambiguous;
    case ava_mrmr_is_macro: goto macsub;
    case ava_mrmr_not_macro: abort();
    }
  }

  /* No more macro substitution possible */
  no_macsub:
  return ava_intr_statement(context, statement, &unit->location);

  ambiguous:
  /* TODO: Should indicate the conflicting symbols */
  return ava_macsub_error(
    context, ava_error_ambiguous_bareword(&unit->location));

  macsub:
  subst_result = (*symbol->v.macro.macro_subst)(
    symbol, context, statement, unit, consumed_rest);

  switch (subst_result.status) {
  case ava_mss_done:
    return subst_result.v.node;

  case ava_mss_again:
    assert(!*consumed_rest);
    TAILQ_SWAP(&statement->units, &subst_result.v.statement->units,
               ava_parse_unit_s, next);
    goto tail_call;

  default: abort();
  }
}

void ava_macsub_expand_expanders(ava_macsub_context* context,
                                 ava_parse_unit_list* units) {
  ava_parse_unit* unit, * unit_clone, * new, * tmp, * insert_point;
  ava_parse_statement tmp_statement, * res_statement;
  ava_macro_subst_result result;
  const ava_symbol* symbol;
  ava_bool ignore;

  tailcall:

  TAILQ_FOREACH(unit, units, next) {
    if (ava_put_expander != unit->type) continue;

    switch (ava_macsub_resolve_macro(
              &symbol, context, unit, ava_st_expander_macro, 0)) {
    case ava_mrmr_is_macro: break;
    case ava_mrmr_not_macro:
      ava_macsub_record_error(
        context, ava_error_no_such_expander(
          &unit->location, unit->v.string));
      goto delete_unit;

    case ava_mrmr_ambiguous:
      ava_macsub_record_error(
        context, ava_error_ambiguous_expander(
          &unit->location, unit->v.string));
      goto delete_unit;
    }

    unit_clone = AVA_CLONE(*unit);
    TAILQ_INIT(&tmp_statement.units);
    TAILQ_INSERT_TAIL(&tmp_statement.units, unit_clone, next);
    result = (*symbol->v.macro.macro_subst)(
      symbol, context, &tmp_statement, unit_clone, &ignore);
    switch (result.status) {
    case ava_mss_done:
      /* Assumed an error */
      break;

    case ava_mss_again:
      res_statement = result.v.statement;
      insert_point = unit;
      TAILQ_FOREACH_SAFE(new, &res_statement->units, next, tmp) {
        TAILQ_REMOVE(&res_statement->units, new, next);
        TAILQ_INSERT_AFTER(units, insert_point, new, next);
        insert_point = new;
      }
      break;
    }

    delete_unit:
    TAILQ_REMOVE(units, unit, next);
    goto tailcall;
  }
}

static const ava_symbol ava_macsub_string_pseudosymbol = {
  .type = ava_st_operator_macro,
  .level = 0,
  .visibility = ava_v_private,
  .v = {
    .macro = {
      .precedence = STRING_PSEUDOMACRO_PRECEDENCE,
      .macro_subst = ava_intr_string_pseudomacro,
      .userdata = NULL
    }
  }
};

static ava_bool ava_macsub_is_macroid(
  signed* min_precedence, ava_symbol_type* found_type,
  ava_macsub_context* context, const ava_parse_unit* unit
) {
  const ava_symbol** results;
  size_t num_results, i;
  signed precedence;
  ava_bool found, allow_cf;

  switch (unit->type) {
  case ava_put_lstring:
  case ava_put_rstring:
  case ava_put_lrstring:
    *min_precedence = STRING_PSEUDOMACRO_PRECEDENCE;
    *found_type = ava_st_operator_macro;
    return ava_true;

  case ava_put_bareword:
    /* Handled below */
    break;

  default:
    /* Definitely not a macro */
    return ava_false;
  }

  num_results = ava_symtab_get(
    &results, context->symbol_table, unit->v.string);
  found = ava_false;
  allow_cf = (NULL == TAILQ_PREV(unit, ava_parse_unit_list_s, next));
  for (i = 0; i < num_results; ++i) {
    switch (results[i]->type) {
    case ava_st_function_macro:
    case ava_st_control_macro:
    case ava_st_operator_macro:
      if (!allow_cf && ava_st_operator_macro != results[i]->type) break;

      precedence = (
        ava_st_function_macro == results[i]->type? FUNCTION_MACRO_PRECEDENCE :
        ava_st_control_macro == results[i]->type? CONTROL_MACRO_PRECEDENCE :
        (signed)results[i]->v.macro.precedence);

      if (!found || precedence < *min_precedence) {
        *min_precedence = precedence;
        *found_type = results[i]->type;
        found = ava_true;
      }
      break;

    default: break;
    }
  }

  return found;
}

static ava_macsub_resolve_macro_result ava_macsub_resolve_macro(
  const ava_symbol** dst,
  ava_macsub_context* context,
  const ava_parse_unit* provoker,
  ava_symbol_type target_type,
  unsigned target_precedence
) {
  const ava_symbol** results;
  size_t num_results, i;

  /* L-Strings, LR-Strings, and R-Strings are treated as precedence-10 operator
   * macros.
   */
  if (ava_st_operator_macro == target_type &&
      STRING_PSEUDOMACRO_PRECEDENCE == target_precedence &&
      (ava_put_lstring == provoker->type ||
       ava_put_rstring == provoker->type ||
       ava_put_lrstring == provoker->type)) {
    *dst = &ava_macsub_string_pseudosymbol;
    return ava_mrmr_is_macro;
  }

  if (ava_put_bareword != provoker->type &&
      ava_st_expander_macro != target_type)
    return ava_mrmr_not_macro;
  if (ava_put_expander != provoker->type &&
      ava_st_expander_macro == target_type)
    return ava_mrmr_not_macro;

  num_results = ava_symtab_get(
    &results, context->symbol_table, provoker->v.string);

  /* See if any result is a macro. If there is a matching macro, it must be
   * unambiguous; but if there is no possible macro that would be substituted
   * now, don't raise an error since this might not be a candidate for macro
   * substitution later.
   */
  for (i = 0; i < num_results; ++i) {
    if (target_type == results[i]->type &&
        (target_type != ava_st_operator_macro ||
         target_precedence == results[i]->v.macro.precedence)) {
      *dst = results[i];
      if (1 != num_results)
        return ava_mrmr_ambiguous;
      else
        return ava_mrmr_is_macro;
    }
  }

  return ava_mrmr_not_macro;
}

void ava_macsub_panic(ava_macsub_context* context) {
  *context->panic = ava_true;
}

static const ava_ast_node_vtable ava_macsub_error_vtable = {
  .name = "<error>",
  .to_string = ava_macsub_error_to_string,
  .to_lvalue = ava_macsub_error_to_lvalue,
};

void ava_macsub_record_error(ava_macsub_context* context,
                             ava_compile_error* error) {
  TAILQ_INSERT_TAIL(context->errors, error, next);
}

ava_ast_node* ava_macsub_error(ava_macsub_context* context,
                               ava_compile_error* error) {
  ava_macsub_record_error(context, error);
  return ava_macsub_silent_error(&error->location);
}

ava_ast_node* ava_macsub_silent_error(const ava_compile_location* location) {
  ava_ast_node* node;

  node = AVA_NEW(ava_ast_node);
  node->v = &ava_macsub_error_vtable;
  node->location = *location;
  return node;
}

ava_macro_subst_result ava_macsub_error_result(
  ava_macsub_context* context,
  ava_compile_error* error
) {
  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = {
      .node = ava_macsub_error(context, error)
    }
  };
}

ava_macro_subst_result ava_macsub_silent_error_result(
  const ava_compile_location* location
) {
  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v.node = ava_macsub_silent_error(location)
  };
}

static ava_string ava_macsub_error_to_string(const ava_ast_node* this) {
  return AVA_ASCII9_STRING("<error>");
}

static ava_ast_node* ava_macsub_error_to_lvalue(
  const ava_ast_node* this, ava_ast_node* producer, ava_ast_node** reader
) {
  *reader = (ava_ast_node*)this;
  return (ava_ast_node*)this;
}

ava_string ava_ast_node_to_string(const ava_ast_node* node) {
  return (*node->v->to_string)(node);
}

ava_ast_node* ava_ast_node_to_lvalue(
  const ava_ast_node* node, ava_ast_node* producer, ava_ast_node** reader
) {
  ava_ast_node* error;

  if (node->v->to_lvalue) {
    return (*node->v->to_lvalue)(node, producer, reader);
  } else {
    error = ava_macsub_error(
      node->context, ava_error_not_an_lvalue(
        &node->location, ava_string_of_cstring(node->v->name)));
    *reader = error;
    return error;
  }
}

void ava_ast_node_postprocess(ava_ast_node* node) {
  if (node->v->postprocess)
    (*node->v->postprocess)(node);
}

ava_bool ava_ast_node_get_constexpr(const ava_ast_node* node,
                                    ava_value* dst) {
  if (node->v->get_constexpr)
    return (*node->v->get_constexpr)(node, dst);
  else
    return ava_false;
}

ava_bool ava_ast_node_get_constexpr_spread(
  const ava_ast_node* node, ava_list_value* dst
) {
  if (node->v->get_constexpr_spread)
    return (*node->v->get_constexpr_spread)(node, dst);
  else
    return ava_false;
}

ava_string ava_ast_node_get_funname(const ava_ast_node* node) {
  if (node->v->get_funname)
    return (*node->v->get_funname)(node);
  else
    return AVA_ABSENT_STRING;
}

void ava_ast_node_cg_evaluate(ava_ast_node* node,
                              const struct ava_pcode_register_s* dst,
                              ava_codegen_context* context) {
  assert(ava_prt_data == dst->type || ava_prt_var == dst->type);

  ava_ast_node_cg_set_up(node, context);
  if (node->v->cg_evaluate)
    (*node->v->cg_evaluate)(node, dst, context);
  else
    ava_codegen_error(context, node,
                      ava_error_does_not_produce_a_value(
                        &node->location, ava_string_of_cstring(
                          node->v->name)));
  ava_ast_node_cg_tear_down(node, context);
}

void ava_ast_node_cg_spread(ava_ast_node* node,
                            const struct ava_pcode_register_s* dst,
                            ava_codegen_context* context) {
  assert(ava_prt_list == dst->type);
  assert(node->v->cg_spread);
  ava_ast_node_cg_set_up(node, context);
  (*node->v->cg_spread)(node, dst, context);
  ava_ast_node_cg_tear_down(node, context);
}

void ava_ast_node_cg_discard(ava_ast_node* node,
                             ava_codegen_context* context) {
  ava_ast_node_cg_set_up(node, context);
  if (node->v->cg_discard)
    (*node->v->cg_discard)(node, context);
  else
    ava_codegen_error(context, node,
                      ava_error_is_pure_but_would_discard(
                        &node->location, ava_string_of_cstring(
                          node->v->name)));
  ava_ast_node_cg_tear_down(node, context);
}

void ava_ast_node_cg_force(ava_ast_node* node,
                           const ava_pcode_register* dst,
                           ava_codegen_context* context) {
  ava_ast_node_cg_set_up(node, context);
  if (node->v->cg_force) {
    (*node->v->cg_force)(node, dst, context);
  } else if (node->v->cg_evaluate) {
    ava_ast_node_cg_evaluate(node, dst, context);
  } else {
    ava_ast_node_cg_discard(node, context);
    AVA_PCXB(ld_imm_vd, *dst, AVA_EMPTY_STRING);
  }
  ava_ast_node_cg_tear_down(node, context);
}

void ava_ast_node_cg_define(ava_ast_node* node,
                            ava_codegen_context* context) {
  if (!node) return;

  assert(node->v->cg_define);
  (*node->v->cg_define)(node, context);
}

void ava_ast_node_cg_set_up(ava_ast_node* node,
                            ava_codegen_context* context) {
  if (!node->setup_count++)
    if (node->v->cg_set_up)
      (*node->v->cg_set_up)(node, context);
}

void ava_ast_node_cg_tear_down(ava_ast_node* node,
                               ava_codegen_context* context) {
  if (!--node->setup_count)
    if (node->v->cg_tear_down)
      (*node->v->cg_tear_down)(node, context);
}
