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
#include "test.c"

#include <string.h>
#include <stdio.h>

#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/alloc.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/parser.h"
#include "bsd.h"

defsuite(parser);

static ava_string stringify_unit(const ava_parse_unit* unit);
static ava_string stringify_statement(const ava_parse_statement* statement);
static void assert_looks_like(const char* expected, const ava_parse_unit* unit);

static ava_parse_unit* parse_successfully(const char* source) {
  AVA_STATIC_STRING(filename, "<test>");
  ava_parse_unit* dst = AVA_NEW(ava_parse_unit);
  ava_compile_error_list errors;

  if (!ava_parse(dst, &errors, ava_string_of_cstring(source),
                 filename, ava_true)) {
    fputs(ava_string_to_cstring(ava_error_list_to_string(
                                  &errors, 50, ava_false)),
          stderr);
    ck_abort_msg("Parse failed unexpectedly");
  }

  ck_assert(TAILQ_EMPTY(&errors));
  return dst;
}

static ava_parse_statement* parse_one_statement(const char* source) {
  ava_parse_unit* block;
  ava_parse_statement* statement;

  block = parse_successfully(source);
  ck_assert_int_eq(ava_put_block, block->type);
  statement = TAILQ_FIRST(&block->v.statements);
  ck_assert_ptr_ne(NULL, statement);
  ck_assert_ptr_eq(NULL, TAILQ_NEXT(statement, next));

  return statement;
}

static ava_parse_unit* parse_one_unit(const char* source) {
  ava_parse_statement* statement;
  ava_parse_unit* unit;

  statement = parse_one_statement(source);
  unit = TAILQ_FIRST(&statement->units);
  ck_assert_ptr_ne(NULL, unit);
  ck_assert_ptr_eq(NULL, TAILQ_NEXT(unit, next));

  return unit;
}

static void parse_failure(const char* source, const char* expected_error) {
  AVA_STATIC_STRING(filename, "<test>");
  ava_parse_unit dst;
  ava_compile_error_list errors;
  ava_compile_error* error;

  if (ava_parse(&dst, &errors, ava_string_of_cstring(source),
                filename, ava_true)) {
    ck_abort_msg("Parse succeeded unexpectedly");
  }

  fputs(ava_string_to_cstring(ava_error_list_to_string(&errors, 50, ava_false)),
        stderr);

  TAILQ_FOREACH(error, &errors, next) {
    if (strstr(ava_string_to_cstring(error->message), expected_error))
      goto found_expected;
  }

  ck_abort_msg("Error message not found: %s", expected_error);

  found_expected:;
}

static ava_string stringify_unit(const ava_parse_unit* unit) {
  ava_string accum;
  const ava_parse_statement* statement;
  const ava_parse_unit* child;
  ava_bool first;

  switch (unit->type) {
  case ava_put_bareword:
    accum = AVA_ASCII9_STRING("bareword:");
    accum = ava_strcat(accum, unit->v.string);
    return accum;

  case ava_put_expander:
    accum = AVA_ASCII9_STRING("expander:");
    accum = ava_strcat(accum, unit->v.string);
    return accum;

  case ava_put_astring:
    accum = AVA_ASCII9_STRING("astring:");
    accum = ava_strcat(accum, unit->v.string);
    return accum;

  case ava_put_lstring:
    accum = AVA_ASCII9_STRING("lstring:");
    accum = ava_strcat(accum, unit->v.string);
    return accum;

  case ava_put_rstring:
    accum = AVA_ASCII9_STRING("rstring:");
    accum = ava_strcat(accum, unit->v.string);
    return accum;

  case ava_put_lrstring:
    accum = AVA_ASCII9_STRING("lrstring:");
    accum = ava_strcat(accum, unit->v.string);
    return accum;

  case ava_put_verbatim:
    accum = AVA_ASCII9_STRING("verbatim:");
    accum = ava_strcat(accum, unit->v.string);
    return accum;

  case ava_put_substitution:
  case ava_put_block:
    accum = (ava_put_block == unit->type?
             AVA_ASCII9_STRING("{") :
             AVA_ASCII9_STRING("("));
    first = ava_true;
    TAILQ_FOREACH(statement, &unit->v.statements, next) {
      if (!first)
        accum = ava_strcat(accum, AVA_ASCII9_STRING("; "));
      accum = ava_strcat(accum, stringify_statement(statement));
      first = ava_false;
    }
    accum = ava_strcat(
      accum, ava_put_substitution == unit->type?
      AVA_ASCII9_STRING(")") : AVA_ASCII9_STRING("}"));
    return accum;

  case ava_put_semiliteral:
    accum = AVA_ASCII9_STRING("[");
    first = ava_true;
    TAILQ_FOREACH(child, &unit->v.units, next) {
      if (!first)
        accum = ava_strcat(accum, AVA_ASCII9_STRING(" "));
      accum = ava_strcat(accum, stringify_unit(child));
      first = ava_false;
    }
    accum = ava_strcat(accum, AVA_ASCII9_STRING("]"));
    return accum;

  case ava_put_spread:
    accum = AVA_ASCII9_STRING("\\*");
    accum = ava_strcat(accum, stringify_unit(unit->v.unit));
    return accum;
  }

  /* Unreachable */
  abort();
}

static ava_string stringify_statement(const ava_parse_statement* statement) {
  ava_string accum = AVA_EMPTY_STRING;
  const ava_parse_unit* unit;

  TAILQ_FOREACH(unit, &statement->units, next) {
    if (!ava_string_is_empty(accum))
      accum = ava_strcat(accum, AVA_ASCII9_STRING(" "));
    accum = ava_strcat(accum, stringify_unit(unit));
  }

  return accum;
}

static void assert_looks_like(const char* expected,
                              const ava_parse_unit* actual) {
  ck_assert_str_eq(expected, ava_string_to_cstring(stringify_unit(actual)));
}

static void parse_like(const char* expected, const char* source) {
  assert_looks_like(expected, parse_successfully(source));
}

deftest(parse_empty) {
  ava_parse_unit* result = parse_successfully("");

  ck_assert_int_eq(ava_put_block, result->type);
  ck_assert(TAILQ_EMPTY(&result->v.statements));
}

deftest(parse_effectively_empty) {
  ava_parse_unit* result = parse_successfully("\n\r\n  \n \\ \n");

  ck_assert_int_eq(ava_put_block, result->type);
  ck_assert(TAILQ_EMPTY(&result->v.statements));
}

deftest(parse_fail_closing_brace) {
  parse_failure("}foo", "Unexpected token");
}

deftest(parse_fail_closing_paren) {
  parse_failure(")", "Unexpected token");
}

deftest(parse_fail_lex_error) {
  parse_failure("\x01 foo \x02", "illegal character");
}

deftest(parse_single_simple_bareword) {
  ava_parse_unit* result = parse_one_unit("foo");

  ck_assert_int_eq(ava_put_bareword, result->type);
  ck_assert_str_eq("foo", ava_string_to_cstring(result->v.string));
}

deftest(parse_single_astring) {
  ava_parse_unit* result = parse_one_unit("\"foo\"");

  ck_assert_int_eq(ava_put_astring, result->type);
  ck_assert_str_eq("foo", ava_string_to_cstring(result->v.string));
}

deftest(parse_single_lstring) {
  ava_parse_unit* result = parse_one_unit("`foo\"");

  ck_assert_int_eq(ava_put_lstring, result->type);
  ck_assert_str_eq("foo", ava_string_to_cstring(result->v.string));
}

deftest(parse_single_rstring) {
  ava_parse_unit* result = parse_one_unit("\"foo`");

  ck_assert_int_eq(ava_put_rstring, result->type);
  ck_assert_str_eq("foo", ava_string_to_cstring(result->v.string));
}

deftest(parse_single_lrstring) {
  ava_parse_unit* result = parse_one_unit("`foo`");

  ck_assert_int_eq(ava_put_lrstring, result->type);
  ck_assert_str_eq("foo", ava_string_to_cstring(result->v.string));
}

deftest(parse_single_verbatim) {
  ava_parse_unit* result = parse_one_unit("\\{foo\\}");

  ck_assert_int_eq(ava_put_verbatim, result->type);
  ck_assert_str_eq("foo", ava_string_to_cstring(result->v.string));
}

deftest(parse_simple_multitoken_statement) {
  parse_like("{bareword:foo bareword:bar}", "foo bar");
}

deftest(parse_multiple_statements) {
  parse_like("{bareword:foo bareword:bar; bareword:baz bareword:quux}",
             "foo bar\nbaz quux\n");
}

deftest(variable_simplification_single_var_whole_word) {
  parse_like("{((bareword:#var# bareword:foo))}", "$foo");
}

deftest(variable_simplification_single_var_whole_word_trailing_dollar) {
  parse_like("{((bareword:#var# bareword:foo))}", "$foo$");
}

deftest(variable_simplification_single_var_prefixed) {
  parse_like("{(rstring:foo (bareword:#var# bareword:bar))}", "foo$bar");
}

deftest(variable_simplification_single_var_suffixed) {
  parse_like("{((bareword:#var# bareword:foo) lstring:bar)}", "$foo$bar");
}

deftest(variable_simplification_two_vars_interfix) {
  parse_like("{((bareword:#var# bareword:foo) lrstring:bar "
             "(bareword:#var# bareword:baz))}", "$foo$bar$baz");
}

deftest(variable_simplification_two_vars_juxt) {
  parse_like("{((bareword:#var# bareword:foo) lrstring: "
             "(bareword:#var# bareword:bar))}", "$foo$$bar$");
}

deftest(variable_simplification_maximal) {
  parse_like("{(rstring:a (bareword:#var# bareword:b) lrstring:c "
             "(bareword:#var# bareword:d) lstring:e)}", "a$b$c$d$e");
}

deftest(variable_simplification_rejects_empty_variable_name_in_middle) {
  parse_failure("foo$$bar", "Empty");
}

deftest(variable_simplification_rejects_empty_variable_name_at_end) {
  parse_failure("foo$", "Empty");
}

deftest(variable_simplification_context_variable) {
  parse_like("{((bareword:#var# bareword:$))}", "$");
}

deftest(basic_expander) {
  parse_like("{expander:foo}", "$$foo");
}

deftest(lone_double_dollar_not_expander) {
  parse_failure("$$", "Empty");
}

deftest(double_dollar_with_dollar_later_not_expander) {
  parse_failure("$$foo$bar", "Empty");
}

deftest(parse_empty_substitution) {
  ava_parse_unit* subst = parse_one_unit("()");

  ck_assert_int_eq(ava_put_substitution, subst->type);
  ck_assert(TAILQ_EMPTY(&subst->v.statements));
}

deftest(parse_simple_substitution) {
  parse_like("{(bareword:foo bareword:bar)}", "(foo bar)");
}

deftest(parse_unclosed_substitution) {
  parse_failure("(foo", "Unexpected end-of-input");
}

deftest(parse_incorrectly_closed_substitution) {
  parse_failure("(foo}", "Unexpected token");
}

deftest(parse_nested_substitution) {
  parse_like("{(bareword:foo (bareword:bar bareword:baz))}", "(foo (bar baz))");
}

deftest(parse_substitution_with_nls) {
  parse_like("{(bareword:foo bareword:bar)}", "(foo\nbar)");
}

deftest(group_tag_simplification_on_substitution) {
  parse_like("{(bareword:#substitution#plugh (bareword:foo bareword:bar))}",
             "(foo bar)plugh");
}

deftest(parse_empty_semiliteral) {
  ava_parse_unit* semi = parse_one_unit("[]");

  ck_assert_int_eq(ava_put_semiliteral, semi->type);
  ck_assert(TAILQ_EMPTY(&semi->v.units));
}

deftest(parse_simple_semiliteral) {
  parse_like("{[bareword:foo bareword:bar bareword:baz]}", "[foo bar baz]");
}

deftest(parse_nested_semiliteral) {
  parse_like("{[bareword:foo [bareword:bar bareword:baz]]}", "[foo [bar baz]]");
}

deftest(parse_semiliteral_with_nls) {
  parse_like("{[bareword:foo bareword:bar]}", "[foo\nbar]");
}

deftest(parse_unclosed_semiliteral) {
  parse_failure("[foo", "Unexpected end-of-input");
}

deftest(parse_incorrectly_closed_semiliteral) {
  parse_failure("[foo)", "Unexpected token");
}

deftest(group_tag_simplification_on_semiliteral) {
  parse_like("{(bareword:#semiliteral#plugh [bareword:foo bareword:bar])}",
             "[foo bar]plugh");
}

deftest(string_regrouping_leading_lstring) {
  parse_failure("[`foo\"]", "before L-");
}

deftest(string_regrouping_leading_lrstring) {
  parse_failure("[`foo` bar]", "LR-String");
}

deftest(string_regrouping_trailing_rstring) {
  parse_failure("[\"foo`]", "after R-");
}

deftest(string_regrouping_trailing_lrstring) {
  parse_failure("[foo `bar`]", "LR-String");
}

deftest(string_regrouping_isolated_lrstring) {
  parse_failure("[`foo`]", "LR-String");
}

deftest(string_regrouping_simple_lstring) {
  parse_like("{[(verbatim:foo lstring:bar) bareword:baz]}", "[foo `bar\" baz]");
}

deftest(string_regrouping_simple_rstring) {
  parse_like("{[bareword:foo (rstring:bar verbatim:baz)]}", "[foo \"bar` baz]");
}

deftest(string_regrouping_simple_lrstring) {
  parse_like("{[(verbatim:foo lrstring:bar verbatim:baz)]}", "[foo `bar` baz]");
}

deftest(string_regrouping_simple_lstring_with_leading_word) {
  parse_like("{[bareword:plugh "
             "(verbatim:foo lstring:bar) bareword:baz]}",
             "[plugh foo `bar\" baz]");
}

deftest(string_regrouping_simple_rstring_with_trailing_word) {
  parse_like("{[bareword:foo (rstring:bar verbatim:baz) bareword:plugh]}",
             "[foo \"bar` baz plugh]");
}

deftest(string_regrouping_simple_lrstring_with_surrounding_words) {
  parse_like("{[bareword:xyzzy (verbatim:foo lrstring:bar verbatim:baz) "
             "bareword:plugh]}", "[xyzzy foo `bar` baz plugh]");
}

deftest(string_regrouping_rs_bw_ls) {
  parse_like("{[(rstring:foo verbatim:bar lstring:baz)]}",
             "[\"foo` bar `baz\"]");
}

deftest(string_regrouping_rs_ls) {
  parse_like("{[(rstring:foo lstring:bar)]}", "[\"foo` `bar\"]");
}

deftest(string_regrouping_rs_rs_bw) {
  parse_like("{[(rstring:foo rstring:bar verbatim:baz)]}",
             "[\"foo` \"bar` baz]");
}

deftest(string_regrouping_bw_ls_ls) {
  parse_like("{[(verbatim:foo lstring:bar lstring:baz)]}",
             "[foo `bar\" `baz\"]");
}

deftest(string_regrouping_bw_lrs_bw_lrs_bw) {
  parse_like("{[(verbatim:a lrstring:b verbatim:c lrstring:d verbatim:e)]}",
             "[a `b` c `d` e]");
}

deftest(string_regrouping_bw_lrs_lrs_bw) {
  parse_like("{[(verbatim:a lrstring:b lrstring:c verbatim:d)]}",
             "[a `b` `c` d]");
}

deftest(string_regrouping_doesnt_change_type_of_non_bareword) {
  parse_like("{[(rstring:a astring:b)]}", "[\"a` \"b\"]");
}

deftest(parse_empty_block) {
  ava_parse_unit* block = parse_one_unit("{}");

  ck_assert_int_eq(ava_put_block, block->type);
  ck_assert(TAILQ_EMPTY(&block->v.statements));
}

deftest(parse_simple_block) {
  parse_like("{{bareword:foo bareword:bar}}", "{foo bar}");
}

deftest(parse_multistatement_block) {
  parse_like("{{bareword:foo bareword:bar; "
             "bareword:quux bareword:xyzzy}}",
             "{foo bar\nquux xyzzy}");
}

deftest(parse_unclosed_block) {
  parse_failure("{", "Unexpected end-of-input");
}

deftest(parse_misclosed_block) {
  parse_failure("{foo)", "Unexpected token");
}

deftest(group_tag_simplification_on_block) {
  parse_like("{(bareword:#block#plugh {bareword:foo})}", "{ foo }plugh");
}

deftest(simple_name_subscript) {
  parse_like("{(bareword:#name-subscript# bareword:## "
             "bareword:foo (bareword:bar))}",
             "foo(bar)");
}

deftest(empty_name_subscript) {
  parse_like("{(bareword:#name-subscript# bareword:## bareword:foo ())}",
             "foo()");
}

deftest(tagged_name_subscript) {
  parse_like("{(bareword:#name-subscript# bareword:#tag# "
             "bareword:foo (bareword:bar))}",
             "foo(bar)tag");
}

deftest(unclosed_name_subscript) {
  parse_failure("foo(bar", "Unexpected end-of-input");
}

deftest(misclosed_name_subscript) {
  parse_failure("foo(bar]", "Unexpected token");
}

deftest(simple_numeric_subscript) {
  parse_like("{(bareword:#numeric-subscript# bareword:## "
             "bareword:foo (bareword:bar))}",
             "foo[bar]");
}

deftest(empty_numeric_subscript) {
  parse_like("{(bareword:#numeric-subscript# bareword:## bareword:foo ())}",
             "foo[]");
}

deftest(tagged_numeric_subscript) {
  parse_like("{(bareword:#numeric-subscript# bareword:#tag# "
             "bareword:foo (bareword:bar))}",
             "foo[bar]tag");
}

deftest(unclosed_numeric_subscript) {
  parse_failure("foo[bar", "Unexpected end-of-input");
}

deftest(misclosed_numeric_subscript) {
  parse_failure("foo[bar)", "Unexpected token");
}

deftest(simple_string_subscript) {
  parse_like("{(bareword:#string-subscript# bareword:## "
             "bareword:foo (bareword:bar))}",
             "foo{bar}");
}

deftest(empty_string_subscript) {
  parse_like("{(bareword:#string-subscript# bareword:## bareword:foo ())}",
             "foo{}");
}

deftest(tagged_string_subscript) {
  parse_like("{(bareword:#string-subscript# bareword:#tag# "
             "bareword:foo (bareword:bar))}",
             "foo{bar}tag");
}

deftest(unclosed_string_subscript) {
  parse_failure("foo{bar", "Unexpected end-of-input");
}

deftest(misclosed_string_subscript) {
  parse_failure("foo{bar]", "Unexpected token");
}

deftest(chained_subscript) {
  parse_like("{(bareword:#string-subscript# bareword:## "
             "(bareword:#numeric-subscript# bareword:## "
             "(bareword:#name-subscript# bareword:## "
             "bareword:foo (bareword:bar)) "
             "(bareword:42)) "
             "(bareword:56))}",
             "foo(bar)[42]{56}");
}

deftest(spread_at_eof) {
  parse_failure("\\*", "C5057");
}

deftest(spread_followed_by_nl) {
  parse_failure("\\*\nfoo", "C5057");
}

deftest(spread_followed_by_close) {
  parse_failure("(\\*)", "C5057");
}

deftest(simple_spread) {
  parse_like("{\\*bareword:foo}", "\\*foo");
}

deftest(chained_spread) {
  parse_like("{\\*\\*bareword:foo}", "\\*\\*foo");
}

deftest(spread_over_subscript) {
  parse_like("{\\*(bareword:#numeric-subscript# bareword:## "
             "bareword:foo (bareword:42))}",
             "\\*foo[42]");
}

deftest(compound_spread_over_compound_subscript) {
  parse_like("{\\*\\*(bareword:#name-subscript# bareword:## "
             "(bareword:#numeric-subscript# bareword:## "
             "bareword:foo (bareword:42)) "
             "(bareword:bar))}",
             "\\*\\*foo[42](bar)");
}

deftest(spread_over_variable) {
  parse_like("{\\*((bareword:#var# bareword:foo))}", "\\*$foo");
}
