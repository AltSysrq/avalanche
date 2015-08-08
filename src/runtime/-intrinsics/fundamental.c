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
#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"
#include "fundamental.h"
#include "funcall.h"
#include "variable.h"
#include "../../bsd.h"

typedef struct ava_intr_seq_entry_s {
  ava_ast_node* node;

  STAILQ_ENTRY(ava_intr_seq_entry_s) next;
} ava_intr_seq_entry;

struct ava_intr_seq_s {
  ava_ast_node header;

  ava_intr_seq_return_policy return_policy;
  STAILQ_HEAD(, ava_intr_seq_entry_s) children;
};

static ava_string ava_intr_seq_to_string(const ava_intr_seq* node);
static ava_ast_node* ava_intr_seq_to_lvalue(const ava_intr_seq* node);
static void ava_intr_seq_postprocess(const ava_intr_seq* node);

static const ava_ast_node_vtable ava_intr_seq_vtable = {
  .name = "statement sequence",
  .to_string = (ava_ast_node_to_string_f)ava_intr_seq_to_string,
  .to_lvalue = (ava_ast_node_to_lvalue_f)ava_intr_seq_to_lvalue,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_seq_postprocess,
};

ava_intr_seq* ava_intr_seq_new(
  ava_macsub_context* context,
  const ava_compile_location* start_location,
  ava_intr_seq_return_policy return_policy
) {
  ava_intr_seq* this;

  this = AVA_NEW(ava_intr_seq);
  this->header.v = &ava_intr_seq_vtable;
  this->header.location = *start_location;
  this->header.context = context;
  this->return_policy = return_policy;
  STAILQ_INIT(&this->children);

  return this;
}

void ava_intr_seq_add(ava_intr_seq* seq, ava_ast_node* node) {
  ava_intr_seq_entry* entry;

  entry = AVA_NEW(ava_intr_seq_entry);
  entry->node = node;
  STAILQ_INSERT_TAIL(&seq->children, entry, next);
}

ava_ast_node* ava_intr_seq_to_node(ava_intr_seq* seq) {
  return (ava_ast_node*)seq;
}

static ava_string ava_intr_seq_to_string(const ava_intr_seq* seq) {
  ava_string accum;
  const ava_intr_seq_entry* entry;

  accum = AVA_ASCII9_STRING("seq(");
  switch (seq->return_policy) {
  case ava_isrp_void:
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("void"));
    break;
  case ava_isrp_only:
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("only"));
    break;
  case ava_isrp_last:
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("last"));
    break;
  }
  accum = ava_string_concat(accum, AVA_ASCII9_STRING(") { "));

  STAILQ_FOREACH(entry, &seq->children, next) {
    accum = ava_string_concat(
      accum, (*entry->node->v->to_string)(entry->node));
    if (STAILQ_NEXT(entry, next))
      accum = ava_string_concat(accum, AVA_ASCII9_STRING("; "));
    else
      accum = ava_string_concat(accum, AVA_ASCII9_STRING(" "));
  }

  accum = ava_string_concat(accum, AVA_ASCII9_STRING("}"));
  return accum;
}

static ava_ast_node* ava_intr_seq_to_lvalue(const ava_intr_seq* seq) {
  AVA_STATIC_STRING(empty_message, "Empty sequence cannot be used as lvalue.");
  AVA_STATIC_STRING(multi_message, "Sequence with more than one statement "
                    "cannot be used as lvalue.");
  ava_intr_seq_entry* first;

  if (STAILQ_EMPTY(&seq->children))
    return ava_macsub_error(
      seq->header.context, empty_message, &seq->header.location);

  first = STAILQ_FIRST(&seq->children);
  if (STAILQ_NEXT(first, next))
    return ava_macsub_error(
      seq->header.context, multi_message, &seq->header.location);

  return ava_ast_node_to_lvalue(first->node);
}

static void ava_intr_seq_postprocess(const ava_intr_seq* seq) {
  ava_intr_seq_entry* e;

  STAILQ_FOREACH(e, &seq->children, next)
    ava_ast_node_postprocess(e);
}

AVA_STATIC_STRING(lstring_missing_left_expression,
                  "Expected expression before L- or LR-String.");
AVA_STATIC_STRING(rstring_missing_right_expression,
                  "Expected expression after R- or LR-String.");
ava_macro_subst_result ava_intr_string_pseudomacro(
  const ava_symbol* ignored,
  ava_macsub_context* context,
  const ava_parse_statement* orig_statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  AVA_STATIC_STRING(concat_function,
                    "org.ava-lang.avast:string-concat");

  ava_macro_subst_result result;
  const ava_parse_unit* src_unit;
  ava_parse_unit* nucleus, * unit, * bareword;
  ava_parse_unit* left_subexpr, * right_subexpr, * subexpr;
  ava_parse_statement statement;
  ava_parse_statement* ss;
  ava_bool left_valent, right_valent;

  nucleus = ava_clone(provoker, sizeof(ava_parse_unit));
  nucleus->type = ava_put_astring;

  switch (provoker->type) {
  case ava_put_lstring:
    left_valent = ava_true;
    right_valent = ava_false;
    break;

  case ava_put_rstring:
    left_valent = ava_false;
    right_valent = ava_true;
    break;

  case ava_put_lrstring:
    left_valent = ava_true;
    right_valent = ava_true;
    break;

  default: abort();
  }

  TAILQ_INIT(&statement.units);
  statement.next = orig_statement->next;

  /* Collect valent units into subexpressions */
  if (left_valent) {
    ss = AVA_NEW(ava_parse_statement);
    TAILQ_INIT(&ss->units);

    TAILQ_FOREACH(src_unit, &orig_statement->units, next) {
      if (provoker == src_unit) break;

      unit = ava_clone(src_unit, sizeof(ava_parse_unit));
      TAILQ_INSERT_TAIL(&ss->units, unit, next);
    }

    if (TAILQ_EMPTY(&ss->units))
      return ava_macsub_error_result(context, lstring_missing_left_expression,
                                     &provoker->location);

    left_subexpr = ava_parse_subst_of_nonempty_statement(ss);
  }

  if (right_valent) {
    ss = AVA_NEW(ava_parse_statement);
    TAILQ_INIT(&ss->units);

    for (src_unit = TAILQ_NEXT(provoker, next); src_unit;
         src_unit = TAILQ_NEXT(src_unit, next)) {
      unit = ava_clone(src_unit, sizeof(ava_parse_unit));
      TAILQ_INSERT_TAIL(&ss->units, unit, next);
    }

    if (TAILQ_EMPTY(&ss->units))
      return ava_macsub_error_result(context, rstring_missing_right_expression,
                                     &provoker->location);

    right_subexpr = ava_parse_subst_of_nonempty_statement(ss);
  }

  /* If not left-valent, copy everything from the old statement to the head of
   * the new one.
   */
  if (!left_valent) {
    TAILQ_FOREACH(src_unit, &orig_statement->units, next) {
      if (provoker == src_unit) break;

      unit = ava_clone(src_unit, sizeof(ava_parse_unit));
      TAILQ_INSERT_TAIL(&statement.units, unit, next);
    }
  }

  /* Create the subexpression with the concatenation proper */
  if (left_valent) {
    /* (%string-concat (<) @) */
    ss = AVA_NEW(ava_parse_statement);
    TAILQ_INIT(&ss->units);

    bareword = AVA_NEW(ava_parse_unit);
    bareword->type = ava_put_bareword;
    bareword->location = provoker->location;
    bareword->v.string = concat_function;
    TAILQ_INSERT_TAIL(&ss->units, bareword, next);
    TAILQ_INSERT_TAIL(&ss->units, left_subexpr, next);
    TAILQ_INSERT_TAIL(&ss->units, nucleus, next);

    subexpr = ava_parse_subst_of_nonempty_statement(ss);
  }

  /* An LR string is essentially an R-String containing an L-String-expression
   * nucleus.
   */
  if (left_valent && right_valent)
    nucleus = subexpr;

  if (right_valent) {
    /* (%string-concat @ (>))
     * (%string-concat (%string-concat (<) @) (>))
     */
    ss = AVA_NEW(ava_parse_statement);
    TAILQ_INIT(&ss->units);

    /* Note that this instance cannot be shared across the whole function since
     * an LR-String needs two of them.
     */
    bareword = AVA_NEW(ava_parse_unit);
    bareword->type = ava_put_bareword;
    bareword->location = provoker->location;
    bareword->v.string = concat_function;
    TAILQ_INSERT_TAIL(&ss->units, bareword, next);
    TAILQ_INSERT_TAIL(&ss->units, nucleus, next);
    TAILQ_INSERT_TAIL(&ss->units, right_subexpr, next);

    subexpr = ava_parse_subst_of_nonempty_statement(ss);
  }

  TAILQ_INSERT_TAIL(&statement.units, subexpr, next);

  /* If not right-valent, copy everything that remains */
  if (!right_valent) {
    for (src_unit = TAILQ_NEXT(provoker, next); src_unit;
         src_unit = TAILQ_NEXT(src_unit, next)) {
      unit = ava_clone(src_unit, sizeof(ava_parse_unit));
      TAILQ_INSERT_TAIL(&statement.units, unit, next);
    }
  }

  result.status = ava_mss_again;
  result.v.statement = statement;
  return result;
}

static ava_string ava_intr_empty_expr_to_string(const ava_ast_node* node);

static const ava_ast_node_vtable ava_intr_empty_expr_vtable = {
  .name = "empty expression",
  .to_string = ava_intr_empty_expr_to_string,
};

static ava_string ava_intr_empty_expr_to_string(const ava_ast_node* node) {
  return AVA_ASCII9_STRING("<empty>");
}

typedef struct {
  ava_ast_node header;

  ava_string value;
  ava_bool is_bareword;
} ava_intr_string_expr;

static ava_string ava_intr_string_expr_to_string(
  const ava_intr_string_expr* node);
static ava_ast_node* ava_intr_string_expr_to_lvalue(
  const ava_intr_string_expr* node);

static const ava_ast_node_vtable ava_intr_string_expr_vtable = {
  .name = "bareword or string",
  .to_string = (ava_ast_node_to_string_f)ava_intr_string_expr_to_string,
  .to_lvalue = (ava_ast_node_to_lvalue_f)ava_intr_string_expr_to_lvalue,
};

static ava_string ava_intr_string_expr_to_string(
  const ava_intr_string_expr* node
) {
  ava_string str;

  if (node->is_bareword)
    str = AVA_ASCII9_STRING("bareword:");
  else
    str = AVA_ASCII9_STRING("string:");

  return ava_string_concat(str, node->value);
}

static ava_ast_node* ava_intr_string_expr_to_lvalue(
  const ava_intr_string_expr* node
) {
  AVA_STATIC_STRING(not_a_bareword_message,
                    "string cannot be used as lvalue");

  if (!node->is_bareword)
    return ava_macsub_error(
      node->header.context, not_a_bareword_message, &node->header.location);

  return ava_intr_variable_lvalue(
    node->header.context, node->value, &node->header.location);
}

ava_ast_node* ava_intr_unit(ava_macsub_context* context,
                            const ava_parse_unit* unit) {
  ava_intr_string_expr* str;

  switch (unit->type) {
  case ava_put_bareword:
  case ava_put_astring:
  case ava_put_verbatim:
    str = AVA_NEW(ava_intr_string_expr);
    str->header.v = &ava_intr_string_expr_vtable;
    str->header.context = context;
    str->header.location = unit->location;
    str->value = unit->v.string;
    str->is_bareword = ava_put_bareword == unit->type;
    return (ava_ast_node*)str;

  case ava_put_lstring:
  case ava_put_lrstring:
    /* These happens if someone puts a non-atomic string alone in an expression
     * (which is therefore not eligble for macro substitution).
     */
    return ava_macsub_error(context, lstring_missing_left_expression,
                            &unit->location);
  case ava_put_rstring:
    return ava_macsub_error(context, rstring_missing_right_expression,
                            &unit->location);

  case ava_put_substitution:
    return ava_macsub_run(
      context, &unit->location,
      &unit->v.statements,
      ava_isrp_last);

  case ava_put_semiliteral:
    /* TODO */
    abort();

  case ava_put_block:
    /* TODO */
    abort();
  }

  /* unreachable */
  abort();
}

ava_ast_node* ava_intr_statement(ava_macsub_context* context,
                                 const ava_parse_statement* statement,
                                 const ava_compile_location* location) {
  ava_parse_unit* first;
  ava_ast_node* empty;

  if (TAILQ_EMPTY(&statement->units)) {
    empty = AVA_NEW(ava_ast_node);
    empty->v = &ava_intr_empty_expr_vtable;
    empty->context = context;
    empty->location = *location;
    return empty;
  }

  first = TAILQ_FIRST(&statement->units);

  if (TAILQ_NEXT(first, next))
    return ava_intr_funcall_of(context, statement);
  else
    return ava_intr_unit(context, first);
}
