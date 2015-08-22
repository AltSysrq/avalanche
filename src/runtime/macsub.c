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
#include <assert.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/parser.h"
#include "avalanche/code-gen.h"
#include "avalanche/macsub.h"
#include "-intrinsics/fundamental.h"
#include "../bsd.h"

#define STRING_PSEUDOMACRO_PRECEDENCE 10

struct ava_macsub_saved_symbol_table_s {
  const ava_symbol_table* orig_table;
  size_t num_imports;

  const ava_import_list* imports;
  ava_compile_location location;
  ava_compile_error_list* errors;

  ava_symbol_table* new_table;

  SPLAY_ENTRY(ava_macsub_saved_symbol_table_s) tree;
};

static signed ava_compare_macsub_saved_symbol_table(
  const ava_macsub_saved_symbol_table* a,
  const ava_macsub_saved_symbol_table* b);

SPLAY_HEAD(ava_macsub_saved_symbol_table_tree, ava_macsub_saved_symbol_table_s);
SPLAY_PROTOTYPE(ava_macsub_saved_symbol_table_tree,
                ava_macsub_saved_symbol_table_s,
                tree, ava_compare_macsub_saved_symbol_table);
SPLAY_GENERATE(ava_macsub_saved_symbol_table_tree,
               ava_macsub_saved_symbol_table_s,
               tree, ava_compare_macsub_saved_symbol_table);

struct ava_macsub_context_s {
  ava_symbol_table* symbol_table;
  ava_compile_error_list* errors;
  ava_string symbol_prefix;
  unsigned level;

  struct ava_macsub_saved_symbol_table_tree saved_tables;
};

typedef enum {
  ava_mrmr_not_macro,
  ava_mrmr_is_macro,
  ava_mrmr_ambiguous
} ava_macsub_resolve_macro_result;

static ava_ast_node* ava_macsub_run_one_nonempty_statement(
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  ava_bool* consumed_rest);

static ava_macsub_resolve_macro_result ava_macsub_resolve_macro(
  const ava_symbol** dst,
  ava_macsub_context* context,
  const ava_parse_unit* provoker,
  ava_symbol_type target_type,
  unsigned target_precedence);

static ava_string ava_macsub_error_to_string(const ava_ast_node* this);
static ava_ast_node* ava_macsub_error_to_lvalue(const ava_ast_node* this);

static signed ava_compare_macsub_saved_symbol_table(
  const ava_macsub_saved_symbol_table* a,
  const ava_macsub_saved_symbol_table* b
) {
  signed cmp;

  if ((cmp = (a->orig_table < b->orig_table) - (a->orig_table > b->orig_table)))
    return cmp;

  return (a->num_imports < b->num_imports) - (a->num_imports > b->num_imports);
}

ava_macsub_context* ava_macsub_context_new(
  ava_symbol_table* symbol_table,
  ava_compile_error_list* errors,
  ava_string symbol_prefix
) {
  ava_macsub_context* this;

  this = AVA_NEW(ava_macsub_context);
  this->symbol_table = symbol_table;
  this->errors = errors;
  this->symbol_prefix = symbol_prefix;
  this->level = 0;
  SPLAY_INIT(&this->saved_tables);

  return this;
}

ava_symbol_table* ava_macsub_get_current_symbol_table(
  const ava_macsub_context* context
) {
  return context->symbol_table;
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
  return ava_string_concat(context->symbol_prefix, simple_name);
}

unsigned ava_macsub_get_level(const ava_macsub_context* context) {
  return context->level;
}

ava_macsub_context* ava_macsub_context_push_major(
  const ava_macsub_context* parent,
  ava_string interfix
) {
  ava_macsub_context* this;

  this = AVA_NEW(ava_macsub_context);
  this->symbol_table = ava_symbol_table_new(parent->symbol_table, ava_false);
  this->errors = parent->errors;
  this->symbol_prefix = ava_string_concat(parent->symbol_prefix, interfix);
  this->level = parent->level + 1;
  SPLAY_INIT(&this->saved_tables);

  return this;
}

ava_macsub_context* ava_macsub_context_push_minor(
  const ava_macsub_context* parent,
  ava_string interfix
) {
  ava_macsub_context* this;

  this = AVA_NEW(ava_macsub_context);
  this->symbol_table = ava_symbol_table_new(parent->symbol_table, ava_true);
  this->errors = parent->errors;
  this->symbol_prefix = ava_string_concat(parent->symbol_prefix, interfix);
  this->level = parent->level;
  SPLAY_INIT(&this->saved_tables);

  return this;
}

ava_macsub_saved_symbol_table* ava_macsub_save_symbol_table(
  ava_macsub_context* context,
  const ava_compile_location* location
) {
  ava_macsub_saved_symbol_table exemplar, * ret;

  exemplar.orig_table = context->symbol_table;
  exemplar.num_imports = ava_symbol_table_count_imports(context->symbol_table);

  ret = SPLAY_FIND(ava_macsub_saved_symbol_table_tree,
                   &context->saved_tables, &exemplar);
  if (!ret) {
    ret = AVA_NEW(ava_macsub_saved_symbol_table);
    ret->orig_table = exemplar.orig_table;
    ret->num_imports = exemplar.num_imports;
    ret->imports = ava_symbol_table_get_imports(ret->orig_table);
    ret->location = *location;
    ret->errors = context->errors;
    ret->new_table = NULL;
    SPLAY_INSERT(ava_macsub_saved_symbol_table_tree,
                 &context->saved_tables, ret);
  }

  return ret;
}

const ava_symbol_table* ava_macsub_get_saved_symbol_table(
  ava_macsub_saved_symbol_table* saved
) {
  AVA_STATIC_STRING(redefined_strong_local_message,
                    "Later import resulted in strong name conflict.");
  ava_compile_error* error;

  if (!saved->new_table) {
    switch (ava_symbol_table_apply_imports(
              &saved->new_table,
              saved->orig_table, saved->imports)) {
    case ava_stis_ok:
    case ava_stis_no_symbols_imported:
      break;

    case ava_stis_redefined_strong_local:
      /* TODO: Errors need to indicate where they came from */
      error = AVA_NEW(ava_compile_error);
      error->message = redefined_strong_local_message;
      error->location = saved->location;
      TAILQ_INSERT_TAIL(saved->errors, error, next);
      break;
    }
  }

  return saved->new_table;
}

ava_ast_node* ava_macsub_run(ava_macsub_context* context,
                             const ava_compile_location* start,
                             const ava_parse_statement_list* statements,
                             ava_intr_seq_return_policy return_policy) {
  if (TAILQ_EMPTY(statements))
    return ava_intr_seq_to_node(
      ava_intr_seq_new(context, start, return_policy));

  return ava_macsub_run_from(context, start,
                             TAILQ_FIRST(statements), return_policy);
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

  for (src = first, keep_going = ava_true; keep_going;
       keep_going = src != last, src = TAILQ_NEXT(src, next)) {
    unit = ava_clone(src, sizeof(ava_parse_unit));
    TAILQ_INSERT_TAIL(&statement.units, unit, next);
  }

  return ava_macsub_run_one_nonempty_statement(
    context, &statement, &consumed_rest);
}

ava_ast_node* ava_macsub_run_single(ava_macsub_context* context,
                                    const ava_compile_location* start,
                                    const ava_parse_statement* orig) {
  ava_parse_statement_list list;
  ava_parse_statement statement;

  TAILQ_INIT(&list);
  statement = *orig;
  TAILQ_INSERT_TAIL(&list, &statement, next);

  return ava_macsub_run(context, start, &list, ava_isrp_only);
}

ava_ast_node* ava_macsub_run_from(ava_macsub_context* context,
                                  const ava_compile_location* start,
                                  const ava_parse_statement* statement,
                                  ava_intr_seq_return_policy return_policy) {
  ava_intr_seq* seq;
  ava_bool consumed_rest = ava_false;

  seq = ava_intr_seq_new(context, start, return_policy);

  for (; statement && !consumed_rest;
       statement = TAILQ_NEXT(statement, next)) {
    if (TAILQ_EMPTY(&statement->units)) continue;

    ava_intr_seq_add(seq, ava_macsub_run_one_nonempty_statement(
                       context, statement, &consumed_rest));
  }

  return ava_intr_seq_to_node(seq);
}

static ava_ast_node* ava_macsub_run_one_nonempty_statement(
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  ava_bool* consumed_rest
) {
  AVA_STATIC_STRING(ambiguous_message, "Bareword is ambiguous.");

  const ava_symbol* symbol;
  const ava_parse_unit* unit;
  ava_macro_subst_result subst_result;
  unsigned precedence;
  ava_bool rtl;

  tail_call:

  assert(!TAILQ_EMPTY(&statement->units));

  unit = TAILQ_FIRST(&statement->units);

  /* If there is only one unit, no macro substitution is performed, even if
   * that unit would reference a macro.
   */
  /* TODO: It may be desirable to permit isolated control macros to be invoked
   * in certain contexts, such as statement top-level. This would, for example,
   * permit a lone `ret` bareword return from a function, instead of needing to
   * write `ret ()`.
   */
  if (!TAILQ_NEXT(unit, next)) goto no_macsub;

  switch (ava_macsub_resolve_macro(&symbol, context, unit,
                                   ava_st_control_macro, 0)) {
  case ava_mrmr_ambiguous: goto ambiguous;
  case ava_mrmr_is_macro: goto macsub;
  case ava_mrmr_not_macro: break;
  }

  for (precedence = 0; precedence <= AVA_MAX_OPERATOR_MACRO_PRECEDENCE;
       ++precedence) {
    rtl = (precedence & 1);

    for (unit = rtl? TAILQ_FIRST(&statement->units) :
                     TAILQ_LAST(&statement->units, ava_parse_unit_list_s);
         unit;
         unit = rtl? TAILQ_NEXT(unit, next) :
                     TAILQ_PREV(unit, ava_parse_unit_list_s, next)) {
      switch (ava_macsub_resolve_macro(&symbol, context, unit,
                                       ava_st_operator_macro, precedence)) {
      case ava_mrmr_ambiguous: goto ambiguous;
      case ava_mrmr_is_macro: goto macsub;
      case ava_mrmr_not_macro: break;
      }
    }
  }

  unit = TAILQ_FIRST(&statement->units);

  switch (ava_macsub_resolve_macro(&symbol, context, unit,
                                   ava_st_function_macro, 0)) {
  case ava_mrmr_ambiguous: goto ambiguous;
  case ava_mrmr_is_macro: goto macsub;
  case ava_mrmr_not_macro: break;
  }

  /* No more macro substitution possible */
  no_macsub:
  return ava_intr_statement(context, statement, &unit->location);

  ambiguous:
  /* TODO: Should indicate the conflicting symbols */
  return ava_macsub_error(context, ambiguous_message, &unit->location);

  macsub:
  subst_result = (*symbol->v.macro.macro_subst)(
    symbol, context, statement, unit, consumed_rest);

  switch (subst_result.status) {
  case ava_mss_done:
    return subst_result.v.node;

  case ava_mss_again:
    assert(!*consumed_rest);
    statement = &subst_result.v.statement;
    goto tail_call;

  default: abort();
  }
}

static const ava_symbol ava_macsub_string_pseudosymbol = {
  .type = ava_st_operator_macro,
  .level = 0,
  .visibility = ava_v_private,
  .v = {
    .macro = {
      .precedence = 10,
      .macro_subst = ava_intr_string_pseudomacro,
      .userdata = NULL
    }
  }
};

static ava_macsub_resolve_macro_result ava_macsub_resolve_macro(
  const ava_symbol** dst,
  ava_macsub_context* context,
  const ava_parse_unit* provoker,
  ava_symbol_type target_type,
  unsigned target_precedence
) {
  ava_symbol_table_get_result gotten;
  const ava_symbol* sym;

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

  if (ava_put_bareword != provoker->type) return ava_mrmr_not_macro;

  gotten = ava_symbol_table_get(context->symbol_table, provoker->v.string);

  switch (gotten.status) {
  case ava_stgs_ok:
    sym = gotten.symbol;
    if (target_type == sym->type &&
        target_precedence == sym->v.macro.precedence) {
      *dst = sym;
      return ava_mrmr_is_macro;
    } else {
      return ava_mrmr_not_macro;
    }
    break;

  case ava_stgs_not_found:
    return ava_mrmr_not_macro;

  case ava_stgs_ambiguous_weak:
    /* TODO This should probably only be an error if at least one of the
     * symbols was a macro, since otherwise there is no ambiguity and the unit
     * would be treated as a bareword string either way (at least as far as we
     * care here).
     */
    return ava_mrmr_ambiguous;
  }

  /* unreachable */
  abort();
}

static const ava_ast_node_vtable ava_macsub_error_vtable = {
  .name = "<error>",
  .to_string = ava_macsub_error_to_string,
  .to_lvalue = ava_macsub_error_to_lvalue,
};

void ava_macsub_record_error(ava_macsub_context* context,
                             ava_string message,
                             const ava_compile_location* location) {
  ava_compile_error* error;

  error = AVA_NEW(ava_compile_error);
  error->message = message;
  error->location = *location;
  TAILQ_INSERT_TAIL(context->errors, error, next);
}

ava_ast_node* ava_macsub_error(ava_macsub_context* context,
                               ava_string message,
                               const ava_compile_location* location) {
  ava_ast_node* node;

  ava_macsub_record_error(context, message, location);

  node = AVA_NEW(ava_ast_node);
  node->v = &ava_macsub_error_vtable;
  node->location = *location;
  return node;
}

ava_macro_subst_result ava_macsub_error_result(
  ava_macsub_context* context,
  ava_string message,
  const ava_compile_location* location
) {
  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = {
      .node = ava_macsub_error(context, message, location)
    }
  };
}

static ava_string ava_macsub_error_to_string(const ava_ast_node* this) {
  return AVA_ASCII9_STRING("<error>");
}

static ava_ast_node* ava_macsub_error_to_lvalue(const ava_ast_node* this) {
  return (ava_ast_node*)this;
}

ava_string ava_ast_node_to_string(const ava_ast_node* node) {
  return (*node->v->to_string)(node);
}

ava_ast_node* ava_ast_node_to_lvalue(const ava_ast_node* node) {
  AVA_STATIC_STRING(error_suffix, " cannot be used as lvalue");

  if (node->v->to_lvalue) {
    return (*node->v->to_lvalue)(node);
  } else {
    return ava_macsub_error(
      node->context, ava_string_concat(
        ava_string_of_cstring(node->v->name), error_suffix),
      &node->location);
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

ava_string ava_ast_node_get_funname(const ava_ast_node* node) {
  if (node->v->get_funname)
    return (*node->v->get_funname)(node);
  else
    return AVA_ABSENT_STRING;
}

void ava_ast_node_cg_evaluate(ava_ast_node* node,
                              const struct ava_pcode_register_s* dst,
                              ava_codegen_context* context) {
  AVA_STATIC_STRING(does_not_produce_value,
                    " does not produce a value.");

  if (node->v->cg_evaluate)
    (*node->v->cg_evaluate)(node, dst, context);
  else
    ava_codegen_error(context, node,
                      ava_string_concat(
                        ava_string_of_cstring(node->v->name),
                        does_not_produce_value));
}

void ava_ast_node_cg_discard(ava_ast_node* node,
                             ava_codegen_context* context) {
  AVA_STATIC_STRING(is_pure, " is pure, but value would be discarded.");

  if (node->v->cg_discard)
    (*node->v->cg_discard)(node, context);
  else
    ava_codegen_error(context, node,
                      ava_string_concat(
                        ava_string_of_cstring(node->v->name),
                        is_pure));
}

void ava_ast_node_cg_define(ava_ast_node* node,
                            ava_codegen_context* context) {
  assert(node->v->cg_define);
  (*node->v->cg_define)(node, context);
}
