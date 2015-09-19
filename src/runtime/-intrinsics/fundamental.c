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
#include "../avalanche/exception.h"
#include "../avalanche/list.h"
#include "../avalanche/list-proj.h"
#include "../avalanche/errors.h"
#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"
#include "../avalanche/symbol.h"
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
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
static ava_ast_node* ava_intr_seq_to_lvalue(
  const ava_intr_seq* node, ava_ast_node* producer, ava_ast_node** reader);
static void ava_intr_seq_postprocess(const ava_intr_seq* node);
static ava_bool ava_intr_seq_get_constexpr(const ava_intr_seq* node,
                                           ava_value* dst);
static void ava_intr_seq_cg_common(
  ava_intr_seq* node, const ava_pcode_register* dst,
  ava_codegen_context* context, ava_bool force);
static void ava_intr_seq_cg_evaluate(
  ava_intr_seq* node, const ava_pcode_register* dst,
  ava_codegen_context* context);
static void ava_intr_seq_cg_discard(
  ava_intr_seq* node, ava_codegen_context* context);
static void ava_intr_seq_cg_force(
  ava_intr_seq* node, const ava_pcode_register* dst,
  ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_seq_vtable = {
  .name = "statement sequence",
  .to_string = (ava_ast_node_to_string_f)ava_intr_seq_to_string,
  .to_lvalue = (ava_ast_node_to_lvalue_f)ava_intr_seq_to_lvalue,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_seq_postprocess,
  .get_constexpr = (ava_ast_node_get_constexpr_f)ava_intr_seq_get_constexpr,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_seq_cg_evaluate,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_seq_cg_discard,
  .cg_force = (ava_ast_node_cg_force_f)ava_intr_seq_cg_force,
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

static ava_ast_node* ava_intr_seq_to_lvalue(
  const ava_intr_seq* seq, ava_ast_node* producer, ava_ast_node** reader
) {
  AVA_STATIC_STRING(non_expression, "Non-expression");
  ava_intr_seq_entry* first;

  if (ava_isrp_void == seq->return_policy)
    return ava_macsub_error(
      seq->header.context, ava_error_not_an_lvalue(
        &seq->header.location, non_expression));

  if (STAILQ_EMPTY(&seq->children))
    return ava_macsub_error(
      seq->header.context, ava_error_empty_sequence_as_lvalue(
        &seq->header.location));

  first = STAILQ_FIRST(&seq->children);
  if (STAILQ_NEXT(first, next))
    return ava_macsub_error(
      seq->header.context, ava_error_multi_sequence_as_lvalue(
        &seq->header.location));

  return ava_ast_node_to_lvalue(first->node, producer, reader);
}

static void ava_intr_seq_postprocess(const ava_intr_seq* seq) {
  ava_intr_seq_entry* e;

  STAILQ_FOREACH(e, &seq->children, next)
    ava_ast_node_postprocess(e->node);
}

static ava_bool ava_intr_seq_get_constexpr(const ava_intr_seq* seq,
                                           ava_value* dst) {
  ava_value empty = ava_value_of_string(AVA_EMPTY_STRING);
  ava_intr_seq_entry* e;
  unsigned count = 0;

  *dst = empty;

  /* All constituent parts must be a constexpr */
  STAILQ_FOREACH(e, &seq->children, next) {
    if (!ava_ast_node_get_constexpr(e->node, dst))
      return ava_false;
    ++count;
  }

  /* This is a constexpr. See if the return policy has us return the last value
   * evaluated.
   */
  switch (seq->return_policy) {
  case ava_isrp_void:
    /* Not an expression */
    return ava_false;

  case ava_isrp_only:
    if (count > 1)
      *dst = empty;
    break;

  case ava_isrp_last:
    break;
  }

  return ava_true;
}

static void ava_intr_seq_cg_common(
  ava_intr_seq* seq, const ava_pcode_register* dst,
  ava_codegen_context* context, ava_bool force
) {
  AVA_STATIC_STRING(empty_expression, "Empty expression");
  ava_intr_seq_entry* e;

  STAILQ_FOREACH(e, &seq->children, next) {
    if (dst && !STAILQ_NEXT(e, next))
      (force? ava_ast_node_cg_force : ava_ast_node_cg_evaluate)(
        e->node, dst, context);
    else
      ava_ast_node_cg_discard(e->node, context);
  }

  if (STAILQ_EMPTY(&seq->children) && dst) {
    AVA_PCXB(ld_imm_vd, *dst, AVA_EMPTY_STRING);
  }

  if (STAILQ_EMPTY(&seq->children) && !dst) {
    ava_codegen_error(
      context, (ava_ast_node*)seq, ava_error_is_pure_but_would_discard(
        &seq->header.location, empty_expression));
  }
}

static void ava_intr_seq_cg_evaluate(
  ava_intr_seq* seq, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  AVA_STATIC_STRING(block_or_declaration, "Block or declaration");
  const ava_pcode_register* child_dst = NULL;

  switch (seq->return_policy) {
  case ava_isrp_void:
    if (dst) {
      ava_codegen_error(
        context, (ava_ast_node*)seq, ava_error_does_not_produce_a_value(
          &seq->header.location, block_or_declaration));
    }
    child_dst = NULL;
    break;

  case ava_isrp_last:
    child_dst = dst;
    break;

  case ava_isrp_only:
    if (!STAILQ_FIRST(&seq->children) ||
        STAILQ_NEXT(STAILQ_FIRST(&seq->children), next))
      child_dst = NULL;
    else
      child_dst = dst;
    break;
  }

  ava_intr_seq_cg_common(seq, child_dst, context, ava_false);

  if (dst && dst != child_dst)
    AVA_PCXB(ld_imm_vd, *dst, AVA_EMPTY_STRING);
}

static void ava_intr_seq_cg_discard(
  ava_intr_seq* seq, ava_codegen_context* context
) {
  ava_intr_seq_cg_common(seq, NULL, context, ava_false);
}

static void ava_intr_seq_cg_force(
  ava_intr_seq* seq, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_intr_seq_cg_common(seq, dst, context, ava_true);
}

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
  ava_parse_statement* statement;
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

  statement = AVA_NEW(ava_parse_statement);
  TAILQ_INIT(&statement->units);

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
      return ava_macsub_error_result(
        context, ava_error_lstring_missing_left_expr(
          &provoker->location));

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
      return ava_macsub_error_result(
        context, ava_error_rstring_missing_right_expr(
          &provoker->location));

    right_subexpr = ava_parse_subst_of_nonempty_statement(ss);
  }

  /* If not left-valent, copy everything from the old statement to the head of
   * the new one.
   */
  if (!left_valent) {
    TAILQ_FOREACH(src_unit, &orig_statement->units, next) {
      if (provoker == src_unit) break;

      unit = ava_clone(src_unit, sizeof(ava_parse_unit));
      TAILQ_INSERT_TAIL(&statement->units, unit, next);
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

  TAILQ_INSERT_TAIL(&statement->units, subexpr, next);

  /* If not right-valent, copy everything that remains */
  if (!right_valent) {
    for (src_unit = TAILQ_NEXT(provoker, next); src_unit;
         src_unit = TAILQ_NEXT(src_unit, next)) {
      unit = ava_clone(src_unit, sizeof(ava_parse_unit));
      TAILQ_INSERT_TAIL(&statement->units, unit, next);
    }
  }

  result.status = ava_mss_again;
  result.v.statement = statement;
  return result;
}

static ava_string ava_intr_empty_expr_to_string(const ava_ast_node* node);
static void ava_intr_empty_expr_cg_evaluate(
  ava_ast_node* node, const ava_pcode_register* dst,
  ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_empty_expr_vtable = {
  .name = "empty expression",
  .to_string = ava_intr_empty_expr_to_string,
  .cg_evaluate = ava_intr_empty_expr_cg_evaluate
};

static ava_string ava_intr_empty_expr_to_string(const ava_ast_node* node) {
  return AVA_ASCII9_STRING("<empty>");
}

static void ava_intr_empty_expr_cg_evaluate(
  ava_ast_node* node, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  AVA_PCXB(ld_imm_vd, *dst, AVA_EMPTY_STRING);
}

typedef struct {
  ava_ast_node header;

  ava_string value;
  ava_bool is_bareword;
} ava_intr_string_expr;

static ava_string ava_intr_string_expr_to_string(
  const ava_intr_string_expr* node);
static ava_ast_node* ava_intr_string_expr_to_lvalue(
  const ava_intr_string_expr* node,
  ava_ast_node* producer, ava_ast_node** reader);
static ava_bool ava_intr_string_expr_get_constexpr(
  const ava_intr_string_expr* node, ava_value* dst);
static ava_string ava_intr_string_expr_get_funname(
  const ava_intr_string_expr* node);
static void ava_intr_string_expr_cg_evaluate(
  const ava_intr_string_expr* node,
  const ava_pcode_register* dst,
  ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_string_expr_vtable = {
  .name = "bareword or string",
  .to_string = (ava_ast_node_to_string_f)ava_intr_string_expr_to_string,
  .to_lvalue = (ava_ast_node_to_lvalue_f)ava_intr_string_expr_to_lvalue,
  .get_constexpr = (ava_ast_node_get_constexpr_f)
    ava_intr_string_expr_get_constexpr,
  .get_funname = (ava_ast_node_get_funname_f)ava_intr_string_expr_get_funname,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_string_expr_cg_evaluate,
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
  const ava_intr_string_expr* node,
  ava_ast_node* producer, ava_ast_node** reader
) {
  if (!node->is_bareword)
    return ava_macsub_error(
      node->header.context, ava_error_string_as_lvalue(
        &node->header.location));

  return ava_intr_variable_lvalue(
    node->header.context, node->value, &node->header.location,
    producer, reader);
}

static ava_bool ava_intr_string_expr_get_constexpr(
  const ava_intr_string_expr* node, ava_value* dst
) {
  *dst = ava_value_of_string(node->value);
  return ava_true;
}

static ava_string ava_intr_string_expr_get_funname(
  const ava_intr_string_expr* node
) {
  if (node->is_bareword)
    return node->value;
  else
    return AVA_ABSENT_STRING;
}

static void ava_intr_string_expr_cg_evaluate(
  const ava_intr_string_expr* node,
  const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_codegen_set_location(context, &node->header.location);
  AVA_PCXB(ld_imm_vd, *dst, node->value);
}

typedef struct {
  ava_ast_node header;

  size_t num_elements;
  ava_ast_node* elements[];
} ava_intr_semilit;

static ava_ast_node* ava_intr_semilit_of(
  ava_macsub_context* context, ava_parse_unit* semilit);

static ava_string ava_intr_semilit_to_string(
  const ava_intr_semilit* node);
/* TODO: support lvalues in semiliterals */
static void ava_intr_semilit_postprocess(
  ava_intr_semilit* node);
static ava_bool ava_intr_semilit_get_constexpr(
  const ava_intr_semilit* node, ava_value* dst);
static void ava_intr_semilit_cg_evaluate(
  ava_intr_semilit* node, const ava_pcode_register* dst,
  ava_codegen_context* context);
static void ava_intr_semilit_cg_discard(
  ava_intr_semilit* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_semilit_vtable = {
  .name = "semiliteral",
  .to_string = (ava_ast_node_to_string_f)ava_intr_semilit_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_semilit_postprocess,
  .get_constexpr = (ava_ast_node_get_constexpr_f)ava_intr_semilit_get_constexpr,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_semilit_cg_evaluate,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_semilit_cg_discard,
};

static ava_ast_node* ava_intr_semilit_of(
  ava_macsub_context* context, ava_parse_unit* unit
) {
  ava_intr_semilit* node;
  size_t num_elements, i;
  ava_parse_unit* subunit;

  num_elements = 0;
  TAILQ_FOREACH(subunit, &unit->v.units, next)
    ++num_elements;

  node = ava_alloc(sizeof(ava_intr_semilit) +
                   sizeof(ava_ast_node*) * num_elements);
  node->header.v = &ava_intr_semilit_vtable;
  node->header.context = context;
  node->header.location = unit->location;
  node->num_elements = num_elements;

  i = 0;
  TAILQ_FOREACH(subunit, &unit->v.units, next)
    node->elements[i++] = ava_intr_unit(context, subunit);

  return (ava_ast_node*)node;
}

static ava_string ava_intr_semilit_to_string(
  const ava_intr_semilit* node
) {
  ava_string accum;
  size_t i;

  accum = AVA_ASCII9_STRING("[");
  for (i = 0; i < node->num_elements; ++i) {
    if (i)
      accum = ava_string_concat(accum, AVA_ASCII9_STRING(" "));

    accum = ava_string_concat(
      accum, ava_ast_node_to_string(node->elements[i]));
  }

  return ava_string_concat(accum, AVA_ASCII9_STRING("]"));
}

static void ava_intr_semilit_postprocess(
  ava_intr_semilit* node
) {
  size_t i;

  for (i = 0; i < node->num_elements; ++i)
    ava_ast_node_postprocess(node->elements[i]);
}

static ava_bool ava_intr_semilit_get_constexpr(
  const ava_intr_semilit* node, ava_value* dst
) {
  ava_list_value accum = ava_empty_list(), sublist;
  ava_value elt;
  size_t i;

  for (i = 0; i < node->num_elements; ++i) {
    if (node->elements[i]->v->cg_spread) {
      if (ava_ast_node_get_constexpr_spread(node->elements[i], &sublist)) {
          accum = ava_list_concat(accum, sublist);
      } else {
        return ava_false;
      }
    } else {
      if (ava_ast_node_get_constexpr(node->elements[i], &elt)) {
        accum = ava_list_append(accum, elt);
      } else {
        return ava_false;
      }
    }
  }

  *dst = accum.v;
  return ava_true;
}

static void ava_intr_semilit_cg_evaluate(
  ava_intr_semilit* node, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_pcode_register accum, tmplist, eltreg;
  ava_value constexpr;
  size_t i;

  if (ava_ast_node_get_constexpr((ava_ast_node*)node, &constexpr)) {
    AVA_PCXB(ld_imm_vd, *dst, ava_to_string(constexpr));
  } else {
    accum.type = ava_prt_list;
    accum.index = ava_codegen_push_reg(context, ava_prt_list, 2);
    tmplist.type = ava_prt_list;
    tmplist.index = accum.index + 1;
    eltreg.type = ava_prt_data;
    eltreg.index = ava_codegen_push_reg(context, ava_prt_data, 1);

    AVA_PCXB(lempty, accum);
    for (i = 0; i < node->num_elements; ++i) {
      if (node->elements[i]->v->cg_spread) {
        ava_ast_node_cg_spread(node->elements[i], &tmplist, context);
        ava_codegen_set_location(context, &node->elements[i]->location);
        AVA_PCXB(lcat, accum, accum, tmplist);
      } else {
        ava_ast_node_cg_evaluate(node->elements[i], &eltreg, context);
        ava_codegen_set_location(context, &node->elements[i]->location);
        AVA_PCXB(lappend, accum, accum, eltreg);
      }
    }

    AVA_PCXB(ld_reg, *dst, accum);
    ava_codegen_pop_reg(context, ava_prt_data, 1);
    ava_codegen_pop_reg(context, ava_prt_list, 2);
  }
}

static void ava_intr_semilit_cg_discard(
  ava_intr_semilit* node, ava_codegen_context* context
) {
  ava_codegen_error(
    context, (ava_ast_node*)node,
    ava_error_would_discard_semilit(&node->header.location));
}

typedef struct {
  ava_ast_node header;
  ava_ast_node* child;
} ava_intr_spread;

static ava_ast_node* ava_intr_spread_of(
  ava_macsub_context* context, ava_parse_unit* unit);

static ava_string ava_intr_spread_to_string(const ava_intr_spread* node);
static void ava_intr_spread_postprocess(ava_intr_spread* node);
static ava_bool ava_intr_spread_get_constexpr_spread(
  const ava_intr_spread* node, ava_list_value* dst);
static void ava_intr_spread_cg_evaluate(
  ava_intr_spread* node, const ava_pcode_register* dst,
  ava_codegen_context* context);
static void ava_intr_spread_cg_spread(
  ava_intr_spread* node, const ava_pcode_register* dst,
  ava_codegen_context* context);
static void ava_intr_spread_cg_discard(
  ava_intr_spread* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_spread_vtable = {
  .name = "spread",
  .to_string = (ava_ast_node_to_string_f)ava_intr_spread_to_string,
  .postprocess = (ava_ast_node_postprocess_f)ava_intr_spread_postprocess,
  .get_constexpr_spread =
    (ava_ast_node_get_constexpr_spread_f)ava_intr_spread_get_constexpr_spread,
  .cg_evaluate = (ava_ast_node_cg_evaluate_f)ava_intr_spread_cg_evaluate,
  .cg_spread = (ava_ast_node_cg_spread_f)ava_intr_spread_cg_spread,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_spread_cg_discard,
};

static ava_ast_node* ava_intr_spread_of(
  ava_macsub_context* context, ava_parse_unit* unit
) {
  ava_parse_unit_list dummy_list;
  ava_intr_spread* node = AVA_NEW(ava_intr_spread);
  node->header.v = &ava_intr_spread_vtable;
  node->header.context = context;
  node->header.location = unit->location;
  /* The unit needs to be in a valid list to be traversed */
  TAILQ_INIT(&dummy_list);
  TAILQ_INSERT_TAIL(&dummy_list, unit->v.unit, next);
  node->child = ava_macsub_run_units(context, unit->v.unit, unit->v.unit);

  return (ava_ast_node*)node;
}

static ava_string ava_intr_spread_to_string(const ava_intr_spread* node) {
  return ava_string_concat(AVA_ASCII9_STRING("\\*"),
                           ava_ast_node_to_string(node->child));
}

static void ava_intr_spread_postprocess(ava_intr_spread* node) {
  ava_ast_node_postprocess(node->child);
}

static ava_bool ava_intr_spread_get_constexpr_spread(
  const ava_intr_spread* node, ava_list_value* dst
) {
  ava_exception_handler handler;
  ava_list_value sublist;
  ava_value child;
  ava_bool ret;

  ava_try (handler) {
    if (node->child->v->cg_spread) {
      if (ava_ast_node_get_constexpr_spread(node->child, &sublist)) {
        *dst = ava_list_proj_flatten(sublist);
        ret = ava_true;
      } else {
        ret = ava_false;
      }
    } else {
      if (ava_ast_node_get_constexpr(node->child, &child)) {
        *dst = ava_list_value_of(child);
        ret = ava_true;
      } else {
        ret = ava_false;
      }
    }
  } ava_catch (handler, ava_format_exception) {
    return ava_false;
  } ava_catch_all {
    ava_rethrow(&handler);
  }

  return ret;
}

static void ava_intr_spread_cg_evaluate(
  ava_intr_spread* node, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_codegen_error(
    context, (ava_ast_node*)node, ava_error_spread_cannot_be_used_here(
      &node->header.location));
}

static void ava_intr_spread_cg_spread(
  ava_intr_spread* node, const ava_pcode_register* dst,
  ava_codegen_context* context
) {
  ava_pcode_register tmp;

  if (node->child->v->cg_spread) {
    ava_ast_node_cg_spread(node->child, dst, context);
    AVA_PCXB(lflatten, *dst, *dst);
  } else {
    tmp.type = ava_prt_data;
    tmp.index = ava_codegen_push_reg(context, ava_prt_data, 1);
    ava_ast_node_cg_evaluate(node->child, &tmp, context);
    AVA_PCXB(ld_reg, *dst, tmp);
    ava_codegen_pop_reg(context, ava_prt_data, 1);
  }
}

static void ava_intr_spread_cg_discard(
  ava_intr_spread* node, ava_codegen_context* context
) {
  ava_codegen_error(
    context, (ava_ast_node*)node, ava_error_spread_cannot_be_used_here(
      &node->header.location));
}

ava_ast_node* ava_intr_unit(ava_macsub_context* context,
                            ava_parse_unit* unit) {
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
    return ava_macsub_error(context, ava_error_lstring_missing_left_expr(
                              &unit->location));
  case ava_put_rstring:
    return ava_macsub_error(context, ava_error_rstring_missing_right_expr(
                              &unit->location));

  case ava_put_spread:
    return ava_intr_spread_of(context, unit);

  case ava_put_substitution:
    return ava_macsub_run(
      context, &unit->location,
      &unit->v.statements,
      ava_isrp_last);

  case ava_put_semiliteral:
    return ava_intr_semilit_of(context, unit);

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
