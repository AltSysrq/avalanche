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
#include "test.c"

#include <stdlib.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/alloc.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/parser.h"
#include "runtime/avalanche/macsub.h"
#include "bsd.h"

#include "parser-utils.h"

/* Tests for the macro substitution algorithm itself; intrinsics like function
 * calls are not tested here.
 *
 * The only actual macro invoked is a dummy macro in this file, which doesn't
 * implement any macro primitives except for stringification. Handling of
 * bareword and sequence intrinsics is also necessarily covered by this test,
 * since it is essentially impossible not to.
 */

defsuite(macsub);

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

static void test_macsub(const char* expected, const char* input) {
  ava_parse_unit root;
  ava_ast_node* ast;

  ck_assert(ava_parse(&root, &errors, ava_string_of_cstring(input),
                      AVA_ASCII9_STRING("<test>")));
  ast = ava_macsub_run(context, &root.location, &root.v.statements,
                       ava_isrp_void);

  dump_errors(&errors);

  ck_assert_str_eq(expected,
                   ava_string_to_cstring((*ast->v->to_string)(ast)));
}

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

deftest(empty_input) {
  test_macsub("seq(void) { }", "");
}

deftest(lone_bareword) {
  test_macsub("seq(void) { bareword:foo }", "foo");
}

deftest(lone_string) {
  test_macsub("seq(void) { string:foo }", "\"foo\"");
  test_macsub("seq(void) { string:foo }", "\\{foo\\}");
}

deftest(simple_multi_statement) {
  test_macsub("seq(void) { bareword:foo; bareword:bar }", "foo \\ bar");
}

deftest(simple_control_macro) {
  defmacro("macro", ava_st_control_macro, 0, ava_false);
  test_macsub("seq(void) { macro { right = bareword:foo; } }",
              "macro foo");
}

deftest(simple_function_macro) {
  defmacro("macro", ava_st_function_macro, 0, ava_false);
  test_macsub("seq(void) { macro { right = bareword:foo; } }",
              "macro foo");
}

deftest(simple_operator_macro_prefix) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  test_macsub("seq(void) { + { right = bareword:2; } }",
              "+ 2");
}

deftest(simple_operator_macro_suffix) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  test_macsub("seq(void) { + { left = bareword:1; } }",
              "1 +");
}

deftest(simple_operator_macro_interfix) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  test_macsub("seq(void) { + { left = bareword:1; "
              "right = bareword:2; } }", "1 + 2");
}

deftest(multi_operator_precedence) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  defmacro("*", ava_st_operator_macro, 20, ava_false);
  test_macsub("seq(void) { + { left = * { "
              "left = bareword:a; right = bareword:b; }; "
              "right = * { "
              "left = bareword:c; right = bareword:d; }; } }",
              "a * b + c * d");
}

deftest(left_to_right_operator_associativity) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  test_macsub("seq(void) { + { left = + { "
              "left = bareword:a; right = bareword:b; }; "
              "right = bareword:c; } }",
              "a + b + c");
}

deftest(right_to_left_operator_associativity) {
  defmacro("**", ava_st_operator_macro, 33, ava_false);
  test_macsub("seq(void) { ** { left = bareword:a; "
              "right = ** { left = bareword:b; "
              "right = bareword:c; }; } }",
              "a ** b ** c");
}

deftest(control_macro_contains_operators) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  defmacro("ret", ava_st_control_macro, 0, ava_false);
  test_macsub("seq(void) { "
              "ret { right = + { "
              "left = bareword:a; right = bareword:b; }; } }",
              "ret a + b");
}

deftest(operator_contains_function_macros) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  defmacro("f", ava_st_function_macro, 0, ava_false);
  test_macsub("seq(void) { "
              "+ { left = f { right = bareword:a; }; "
              "right = f { right = bareword:b; }; } }",
              "f a + f b");
}

deftest(isolated_function_macro_not_invoked) {
  defmacro("foo", ava_st_function_macro, 0, ava_false);
  test_macsub("seq(void) { bareword:foo }", "foo");
}

deftest(isolated_operator_macro_not_invoked) {
  defmacro("+", ava_st_operator_macro, 10, ava_false);
  test_macsub("seq(void) { bareword:+ }", "+");
}

deftest(isolated_control_macro_not_invoked) {
  defmacro("foo", ava_st_control_macro, 0, ava_false);
  test_macsub("seq(void) { bareword:foo }", "foo");
}

deftest(ambiguous_possible_macro_results_in_error) {
  defmacro("a.foo", ava_st_operator_macro, 10, ava_false);
  defmacro("b.foo", ava_st_operator_macro, 10, ava_false);
  ck_assert_int_eq(
    ava_stis_ok,
    ava_symbol_table_import(symbol_table,
                            AVA_ASCII9_STRING("a."),
                            AVA_EMPTY_STRING,
                            ava_false, ava_false));
  ck_assert_int_eq(
    ava_stis_ok,
    ava_symbol_table_import(symbol_table,
                            AVA_ASCII9_STRING("b."),
                            AVA_EMPTY_STRING,
                            ava_false, ava_false));

  test_macsub_fail("seq(void) { <error> }",
                   "ambiguous",
                   "a foo bar");
}

deftest(macro_consuming_rest_of_scope) {
  defmacro("macro", ava_st_control_macro, 0, ava_false);
  defmacro("defer", ava_st_control_macro, 0, ava_true);

  test_macsub("seq(void) { defer { "
              "right = string:; "
              "next = seq(void) { "
              "macro { right = bareword:foo; } }; } }",
              "defer \"\"\nmacro foo");
}

deftest(push_major_scope) {
  ava_macsub_context* inner;
  ava_symbol_table* inner_scope;

  inner = ava_macsub_context_push_major(
    context, AVA_ASCII9_STRING("inner."));
  inner_scope = ava_macsub_get_current_symbol_table(inner);

  ck_assert_ptr_ne(context, inner);
  ck_assert_ptr_ne(symbol_table, inner_scope);
  ck_assert_int_eq(1, ava_macsub_get_level(inner));

  ck_assert_int_eq(
    ava_stis_ok,
    ava_symbol_table_put(inner_scope,
                         AVA_ASCII9_STRING("foo"),
                         inner));
  ck_assert_int_eq(
    ava_stgs_not_found,
    ava_symbol_table_get(symbol_table, AVA_ASCII9_STRING("foo")).status);

  ck_assert_str_eq(
    "inner.foo", ava_string_to_cstring(
      ava_macsub_apply_prefix(inner, AVA_ASCII9_STRING("foo"))));
}

deftest(push_minor_scope) {
  ava_macsub_context* inner;
  ava_symbol_table* inner_scope;

  inner = ava_macsub_context_push_minor(
    context, AVA_ASCII9_STRING("inner."));
  inner_scope = ava_macsub_get_current_symbol_table(inner);

  ck_assert_ptr_ne(context, inner);
  ck_assert_ptr_ne(symbol_table, inner_scope);
  ck_assert_int_eq(0, ava_macsub_get_level(inner));

  ck_assert_int_eq(
    ava_stis_ok,
    ava_symbol_table_put(inner_scope,
                         AVA_ASCII9_STRING("foo"),
                         inner));
  ck_assert_int_eq(
    ava_stgs_ok,
    ava_symbol_table_get(symbol_table, AVA_ASCII9_STRING("foo")).status);

  ck_assert_str_eq(
    "inner.foo", ava_string_to_cstring(
      ava_macsub_apply_prefix(inner, AVA_ASCII9_STRING("foo"))));
}

deftest(save_apply_imports_no_conflict) {
  const ava_symbol_table* restored;
  ava_macsub_saved_symbol_table* saved;

  ava_compile_location location = {
    .filename = AVA_EMPTY_STRING,
    .source = AVA_EMPTY_STRING,
    .line_offset = 0,
    .start_line = 1,
    .end_line = 1,
    .start_column = 1,
    .end_column = 1,
  };

  ava_symbol_table_import(
    symbol_table, AVA_ASCII9_STRING("foo."), AVA_EMPTY_STRING,
    ava_false, ava_false);
  saved = ava_macsub_save_symbol_table(context, &location);

  defmacro("foo.bar", ava_st_control_macro, 0, ava_false);

  restored = ava_macsub_get_saved_symbol_table(saved);
  ck_assert_int_eq(ava_stgs_ok, ava_symbol_table_get(
                     restored, AVA_ASCII9_STRING("bar")).status);
  ck_assert_ptr_eq(NULL, TAILQ_FIRST(&errors));

  /* Assert memoised */
  ck_assert_ptr_eq(restored, ava_macsub_get_saved_symbol_table(saved));
}


deftest(save_apply_imports_strong_conflict) {
  const ava_symbol_table* restored;
  ava_macsub_saved_symbol_table* saved;

  ava_compile_location location = {
    .filename = AVA_EMPTY_STRING,
    .source = AVA_EMPTY_STRING,
    .line_offset = 0,
    .start_line = 1,
    .end_line = 1,
    .start_column = 1,
    .end_column = 1,
  };

  ava_symbol_table_import(
    symbol_table, AVA_ASCII9_STRING("foo."), AVA_EMPTY_STRING,
    ava_true, ava_false);
  ava_symbol_table_import(
    symbol_table, AVA_ASCII9_STRING("xyzzy."), AVA_EMPTY_STRING,
    ava_true, ava_false);
  saved = ava_macsub_save_symbol_table(context, &location);

  defmacro("foo.bar", ava_st_control_macro, 0, ava_false);
  defmacro("xyzzy.bar", ava_st_control_macro, 0, ava_false);

  restored = ava_macsub_get_saved_symbol_table(saved);
  ck_assert_ptr_ne(NULL, restored);
  ck_assert_ptr_ne(NULL, TAILQ_FIRST(&errors));
}

deftest(save_apply_imports_multiple) {
  const ava_symbol_table* restored[2];
  ava_macsub_saved_symbol_table* saved[2];

  ava_compile_location location = {
    .filename = AVA_EMPTY_STRING,
    .source = AVA_EMPTY_STRING,
    .line_offset = 0,
    .start_line = 1,
    .end_line = 1,
    .start_column = 1,
    .end_column = 1,
  };

  ava_symbol_table_import(
    symbol_table, AVA_ASCII9_STRING("foo."), AVA_EMPTY_STRING,
    ava_false, ava_false);
  saved[0] = ava_macsub_save_symbol_table(context, &location);
  ava_symbol_table_import(
    symbol_table, AVA_ASCII9_STRING("xyzzy."), AVA_EMPTY_STRING,
    ava_false, ava_false);
  saved[1] = ava_macsub_save_symbol_table(context, &location);

  defmacro("foo.bar", ava_st_control_macro, 0, ava_false);
  defmacro("xyzzy.quux", ava_st_control_macro, 0, ava_false);

  restored[0] = ava_macsub_get_saved_symbol_table(saved[0]);
  restored[1] = ava_macsub_get_saved_symbol_table(saved[1]);

  ck_assert_ptr_eq(NULL, TAILQ_FIRST(&errors));

  ck_assert_int_eq(ava_stgs_ok, ava_symbol_table_get(
                     restored[0], AVA_ASCII9_STRING("bar")).status);
  ck_assert_int_eq(ava_stgs_not_found, ava_symbol_table_get(
                     restored[0], AVA_ASCII9_STRING("quux")).status);
  ck_assert_int_eq(ava_stgs_ok, ava_symbol_table_get(
                     restored[1], AVA_ASCII9_STRING("bar")).status);
  ck_assert_int_eq(ava_stgs_ok, ava_symbol_table_get(
                     restored[1], AVA_ASCII9_STRING("quux")).status);
}
