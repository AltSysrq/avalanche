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
#ifndef MACRO_TEST_COMMON_H_
#define MACRO_TEST_COMMON_H_

#include <stdio.h>

#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/alloc.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/parser.h"
#include "runtime/avalanche/macsub.h"
#include "bsd.h"

static ava_compile_error_list errors;
static ava_symbol_table* symbol_table;
static ava_macsub_context* context;

defsetup {
  TAILQ_INIT(&errors);
  symbol_table = ava_symbol_table_new(NULL, ava_false);
  context = ava_macsub_context_new(symbol_table, &errors, AVA_EMPTY_STRING);
}

defteardown { }

typedef struct {
  ava_string name;
  ava_bool consume_later_statements;
} dummy_macro_properties;

typedef struct {
  ava_ast_node self;

  ava_string name;
  ava_ast_node* left, * right, * next;
} dummy_macro_node;

static ava_string stringify(const ava_ast_node* node) {
  return (*node->v->to_string)(node);
}

static ava_string dummy_macro_to_string(const dummy_macro_node* this) {
  ava_string accum = this->name;

  accum = ava_string_concat(accum, AVA_ASCII9_STRING(" { "));
  if (this->left) {
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("left = "));
    accum = ava_string_concat(accum, stringify(this->left));
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("; "));
  }
  if (this->right) {
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("right = "));
    accum = ava_string_concat(accum, stringify(this->right));
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("; "));
  }
  if (this->next) {
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("next = "));
    accum = ava_string_concat(accum, stringify(this->next));
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("; "));
  }
  accum = ava_string_concat(accum, AVA_ASCII9_STRING("}"));
  return accum;
}

static const ava_ast_node_vtable dummy_macro_vtable = {
  .to_string = (ava_ast_node_to_string_f)dummy_macro_to_string,
};

static ava_macro_subst_result dummy_macro_subst(
  const ava_symbol* symbol,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  const dummy_macro_properties* props = symbol->v.macro.userdata;
  dummy_macro_node* this;

  this = AVA_NEW(dummy_macro_node);
  this->self.v = &dummy_macro_vtable;
  this->self.location = provoker->location;
  this->self.context = context;
  this->name = props->name;

  if (provoker != TAILQ_FIRST(&statement->units)) {
    this->left = ava_macsub_run_units(
      context, TAILQ_FIRST(&statement->units),
      TAILQ_PREV(provoker, ava_parse_unit_list_s, next));
  }
  if (provoker != TAILQ_LAST(&statement->units, ava_parse_unit_list_s)) {
    this->right = ava_macsub_run_units(
      context, TAILQ_NEXT(provoker, next),
      TAILQ_LAST(&statement->units, ava_parse_unit_list_s));
  }

  if (props->consume_later_statements) {
    this->next = ava_macsub_run_from(
      context, &provoker->location, TAILQ_NEXT(statement, next),
      ava_isrp_void);
    *consumed_other_statements = ava_true;
  }

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static void test_macsub(const char* expected, const char* input) AVA_UNUSED;
static void test_macsub(const char* expected, const char* input) {
  ava_parse_unit root;
  ava_ast_node* ast;

  ck_assert(ava_parse(&root, &errors, ava_string_of_cstring(input),
                      AVA_ASCII9_STRING("<test>")));
  ast = ava_macsub_run(context, &root.location, &root.v.statements,
                       ava_isrp_void);

  fputs(ava_string_to_cstring(ava_error_list_to_string(&errors, 50, ava_false)),
        stderr);

  ck_assert_str_eq(expected,
                   ava_string_to_cstring((*ast->v->to_string)(ast)));
}

static void test_macsub_fail(
  const char* expected, const char* expected_error,
  const char* input) AVA_UNUSED;
static void test_macsub_fail(
  const char* expected, const char* expected_error,
  const char* input
) {
  const ava_compile_error* error;

  test_macsub(expected, input);

  TAILQ_FOREACH(error, &errors, next) {
    if (strstr(ava_string_to_cstring(error->message),
               expected_error))
      return;
  }

  ck_abort_msg("No message containing \"%s\" found",
               expected_error);
}

static void defmacro(const char* name,
                     ava_symbol_type type,
                     unsigned precedence,
                     ava_bool consume_later_statements) AVA_UNUSED;
static void defmacro(const char* name,
                     ava_symbol_type type,
                     unsigned precedence,
                     ava_bool consume_later_statements) {
  dummy_macro_properties* props;
  ava_symbol* symbol;

  props = AVA_NEW(dummy_macro_properties);
  props->name = ava_string_of_cstring(name);
  props->consume_later_statements = consume_later_statements;

  symbol = AVA_NEW(ava_symbol);
  symbol->type = type;
  symbol->level = 0;
  symbol->visibility = ava_v_public;
  symbol->v.macro.precedence = precedence;
  symbol->v.macro.macro_subst = dummy_macro_subst;
  symbol->v.macro.userdata = props;

  ck_assert_int_eq(ava_stps_ok, ava_symbol_table_put(
                     symbol_table, ava_string_of_cstring(name), symbol));
}

/* A dummy macro which stands in for a function.
 * Unlike the dummy above, it evaluates each syntax unit argument individually,
 * allowing it to stand in for any function invocation.
 */
typedef struct {
  ava_string name;
} funmacro_properties;

typedef struct {
  ava_ast_node self;

  ava_string name;
  unsigned num_args;
  ava_ast_node** args;
} funmacro_node;

static ava_string funmacro_to_string(const funmacro_node* this) {
  ava_string accum;
  unsigned i;

  accum = this->name;
  accum = ava_string_concat(
    accum, AVA_ASCII9_STRING(" { "));
  for (i = 0; i < this->num_args; ++i) {
    accum = ava_string_concat(accum, stringify(this->args[i]));
    accum = ava_string_concat(accum, AVA_ASCII9_STRING("; "));
  }
  accum = ava_string_concat(accum, AVA_ASCII9_STRING("}"));
  return accum;
}

static const ava_ast_node_vtable funmacro_vtable = {
  .to_string = (ava_ast_node_to_string_f)funmacro_to_string,
};

static ava_macro_subst_result funmacro_subst(
  const ava_symbol* symbol,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  const funmacro_properties* props = symbol->v.macro.userdata;
  unsigned num_args;
  const ava_parse_unit* arg;
  funmacro_node* this;

  num_args = 0;
  for (arg = TAILQ_NEXT(provoker, next); arg;
       arg = TAILQ_NEXT(arg, next))
    ++num_args;

  this = AVA_NEW(funmacro_node);
  this->num_args = num_args;
  this->args = ava_alloc(sizeof(ava_ast_node*) * num_args);
  this->self.v = &funmacro_vtable;
  this->self.location = provoker->location;
  this->self.context = context;
  this->name = props->name;

  num_args = 0;
  for (arg = TAILQ_NEXT(provoker, next); arg;
       arg = TAILQ_NEXT(arg, next))
    this->args[num_args++] = ava_macsub_run_units(context, arg, arg);

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static void defun(const char* name) AVA_UNUSED;
static void defun(const char* name) {
  funmacro_properties* props;
  ava_symbol* symbol;

  props = AVA_NEW(funmacro_properties);
  props->name = ava_string_of_cstring(name);

  symbol = AVA_NEW(ava_symbol);
  symbol->type = ava_st_function_macro;
  symbol->level = 0;
  symbol->visibility = ava_v_public;
  symbol->v.macro.precedence = 0;
  symbol->v.macro.macro_subst = funmacro_subst;
  symbol->v.macro.userdata = props;

  ck_assert_int_eq(ava_stps_ok, ava_symbol_table_put(
                     symbol_table, ava_string_of_cstring(name), symbol));
}

#endif /* MACRO_TEST_COMMON_H_ */
