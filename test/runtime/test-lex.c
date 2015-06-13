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

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/lex.h"

defsuite(lex);

static ava_lex_context* lexer;

static void start(const char* str) {
  lexer = ava_lex_new(ava_string_of_cstring(str));
}

static void lex(ava_lex_token_type type, const char* str) {
  ava_lex_result result;
  ava_lex_status status;

  status = ava_lex_lex(&result, lexer);
  ck_assert_int_eq(ava_ls_ok, status);
  ck_assert_int_eq(type, result.type);
  ck_assert_str_eq(str, ava_string_to_cstring(result.str));
}

static void error(void) {
  ava_lex_result result;

  ck_assert_int_eq(ava_ls_error, ava_lex_lex(&result, lexer));
}

static void end(void) {
  ava_lex_result result;
  ava_lex_status status;

  status = ava_lex_lex(&result, lexer);
  ck_assert_int_eq(ava_ls_end_of_input, status);
}

deftest(empty_string_is_empty) {
  start("");
  end();
  /* Make sure it doesn't segfault if lexing past the end happens more than
   * once */
  end();
}

deftest(isolated_bareword) {
  start("avalanche");
  lex(ava_ltt_bareword, "avalanche");
  end();
}

deftest(bareword_surrounded_by_whitespace) {
  start(" \tavalanche\t ");
  lex(ava_ltt_bareword, "avalanche");
  end();
}

deftest(line_feed) {
  start("\n");
  lex(ava_ltt_newline, "\n");
  end();
}

deftest(carraige_return) {
  start("\r");
  lex(ava_ltt_newline, "\n");
  end();
}

deftest(crlf) {
  start("\r\n");
  lex(ava_ltt_newline, "\n");
  end();
}

deftest(horizontal_metrics) {
  ava_lex_result result;

  start("  foo\t bar");

  ck_assert_int_eq(ava_ls_ok, ava_lex_lex(&result, lexer));
  ck_assert_int_eq(3, result.column);
  ck_assert_int_eq(1, result.line);
  ck_assert_int_eq(2, result.index_start);
  ck_assert_int_eq(5, result.index_end);
  ck_assert_int_eq(0, result.line_offset);

  ck_assert_int_eq(ava_ls_ok, ava_lex_lex(&result, lexer));
  ck_assert_int_eq(10, result.column);
  ck_assert_int_eq(1, result.line);
  ck_assert_int_eq(7, result.index_start);
  ck_assert_int_eq(10, result.index_end);
  ck_assert_int_eq(0, result.line_offset);

  end();
}

deftest(vertical_metrics) {
  ava_lex_result result;

  start("  foo\r\n bar");

  ck_assert_int_eq(ava_ls_ok, ava_lex_lex(&result, lexer));
  ck_assert_int_eq(3, result.column);
  ck_assert_int_eq(1, result.line);
  ck_assert_int_eq(2, result.index_start);
  ck_assert_int_eq(5, result.index_end);
  ck_assert_int_eq(0, result.line_offset);

  ck_assert_int_eq(ava_ls_ok, ava_lex_lex(&result, lexer));
  ck_assert_int_eq(6, result.column);
  ck_assert_int_eq(1, result.line);
  ck_assert_int_eq(5, result.index_start);
  ck_assert_int_eq(7, result.index_end);
  ck_assert_int_eq(0, result.line_offset);

  ck_assert_int_eq(ava_ls_ok, ava_lex_lex(&result, lexer));
  ck_assert_int_eq(2, result.column);
  ck_assert_int_eq(2, result.line);
  ck_assert_int_eq(8, result.index_start);
  ck_assert_int_eq(11, result.index_end);
  ck_assert_int_eq(7, result.line_offset);

  end();
}

deftest(synthetic_newline) {
  start("foo \\ bar");
  lex(ava_ltt_bareword, "foo");
  lex(ava_ltt_newline, "\n");
  lex(ava_ltt_bareword, "bar");
  end();
}

deftest(escaped_newline) {
  start("foo\\\nbar");
  lex(ava_ltt_bareword, "foo");
  lex(ava_ltt_bareword, "bar");
  end();
}

deftest(comment_to_eof) {
  start("foo ; this is a comment");
  lex(ava_ltt_bareword, "foo");
  end();
}

deftest(command_to_newline) {
  start("foo ; comment\nbar");
  lex(ava_ltt_bareword, "foo");
  lex(ava_ltt_newline, "\n");
  lex(ava_ltt_bareword, "bar");
  end();
}

deftest(cannot_escape_comment_end) {
  start("foo ; comment\\\nbar");
  lex(ava_ltt_bareword, "foo");
  lex(ava_ltt_newline, "\n");
  lex(ava_ltt_bareword, "bar");
  end();
}

deftest(escape_newline_before_comment) {
  start("foo \\ ; comment\nbar");
  lex(ava_ltt_bareword, "foo");
  lex(ava_ltt_bareword, "bar");
  end();
}

deftest(parentheses) {
  start(" ()() [][] {}{}");
  lex(ava_ltt_begin_substitution, "(");
  lex(ava_ltt_close_paren, ")");
  lex(ava_ltt_begin_name_subscript, "(");
  lex(ava_ltt_close_paren, ")");
  lex(ava_ltt_begin_semiliteral, "[");
  lex(ava_ltt_close_bracket, "]");
  lex(ava_ltt_begin_numeric_subscript, "[");
  lex(ava_ltt_close_bracket, "]");
  lex(ava_ltt_begin_block, "{");
  lex(ava_ltt_close_brace, "}");
  lex(ava_ltt_begin_string_subscript, "{");
  lex(ava_ltt_close_brace, "}");
  end();
}

deftest(close_parens_followed_by_word) {
  start("()a()b []c[]d {}e{}f");
  lex(ava_ltt_begin_substitution, "(");
  lex(ava_ltt_close_paren, ")a");
  lex(ava_ltt_begin_name_subscript, "(");
  lex(ava_ltt_close_paren, ")b");
  lex(ava_ltt_begin_semiliteral, "[");
  lex(ava_ltt_close_bracket, "]c");
  lex(ava_ltt_begin_numeric_subscript, "[");
  lex(ava_ltt_close_bracket, "]d");
  lex(ava_ltt_begin_block, "{");
  lex(ava_ltt_close_brace, "}e");
  lex(ava_ltt_begin_string_subscript, "{");
  lex(ava_ltt_close_brace, "}f");
  end();
}

deftest(whitespace_set_at_start_of_input) {
  start("()");
  lex(ava_ltt_begin_substitution, "(");
  lex(ava_ltt_close_paren, ")");
  end();
}

deftest(whitespace_cleared_after_bareword) {
  start("foo()");
  lex(ava_ltt_bareword, "foo");
  lex(ava_ltt_begin_name_subscript, "(");
  lex(ava_ltt_close_paren, ")");
  end();
}

deftest(whitespace_set_at_start_of_paren_inside) {
  start("(())(())");
  lex(ava_ltt_begin_substitution, "(");
  lex(ava_ltt_begin_substitution, "(");
  lex(ava_ltt_close_paren, ")");
  lex(ava_ltt_close_paren, ")");
  lex(ava_ltt_begin_name_subscript, "(");
  lex(ava_ltt_begin_substitution, "(");
  lex(ava_ltt_close_paren, ")");
  lex(ava_ltt_close_paren, ")");
  end();
}

deftest(whitespace_set_at_start_of_bracket_inside) {
  start("[[]][[]]");
  lex(ava_ltt_begin_semiliteral, "[");
  lex(ava_ltt_begin_semiliteral, "[");
  lex(ava_ltt_close_bracket, "]");
  lex(ava_ltt_close_bracket, "]");
  lex(ava_ltt_begin_numeric_subscript, "[");
  lex(ava_ltt_begin_semiliteral, "[");
  lex(ava_ltt_close_bracket, "]");
  lex(ava_ltt_close_bracket, "]");
  end();
}

deftest(whitespace_set_at_start_of_brace_inside) {
  start("{[]}");
  lex(ava_ltt_begin_block, "{");
  lex(ava_ltt_begin_semiliteral, "[");
  lex(ava_ltt_close_bracket, "]");
  lex(ava_ltt_close_brace, "}");
  end();
}

deftest(whitespace_set_after_physical_nl) {
  start("foo\n()");
  lex(ava_ltt_bareword, "foo");
  lex(ava_ltt_newline, "\n");
  lex(ava_ltt_begin_substitution, "(");
  lex(ava_ltt_close_paren, ")");
  end();
}

deftest(whitespace_set_after_synthetic_nl) {
  start("foo \\ ()");
  lex(ava_ltt_bareword, "foo");
  lex(ava_ltt_newline, "\n");
  lex(ava_ltt_begin_substitution, "(");
  lex(ava_ltt_close_paren, ")");
  end();
}

deftest(whitespace_set_after_escaped_nl) {
  start("foo\\\n()");
  lex(ava_ltt_bareword, "foo");
  lex(ava_ltt_begin_substitution, "(");
  lex(ava_ltt_close_paren, ")");
  end();
}

deftest(simple_string) {
  start("\"foo\"");
  lex(ava_ltt_astring, "foo");
  end();
}

deftest(string_types) {
  start("\"\" \"``\"``");
  lex(ava_ltt_astring, "");
  lex(ava_ltt_rstring, "");
  lex(ava_ltt_lstring, "");
  lex(ava_ltt_lrstring, "");
  end();
}

deftest(string_clears_whitespace) {
  start("\"foo bar\"()");
  lex(ava_ltt_astring, "foo bar");
  lex(ava_ltt_begin_name_subscript, "(");
  lex(ava_ltt_close_paren, ")");
  end();
}

deftest(string_may_contain_linefeed) {
  start("\"foo\r\nbar\"");
  lex(ava_ltt_astring, "foo\nbar");
  end();
}

deftest(string_single_char_escapes) {
  start("\""
        "\\a\\b\\e\\f\\n\\r\\t\\v"
        "\\\\"
        "\\\"" "\\'" "\\`"
        "\"");
  lex(ava_ltt_astring, "\a\b\033\f\n\r\t\v\\\"\'`");
  end();
}

deftest(string_hex_escapes) {
  start("\"\\x61\\x76\\x61\\x6c\\x61\\x6e\\x63\\x68\\x65\\x0a\"");
  lex(ava_ltt_astring, "avalanche\n");
  end();
}

deftest(empty_verbatim) {
  start("\\{\\}");
  lex(ava_ltt_verbatim, "");
  end();
}

deftest(simple_verbatim) {
  start("\\{foo\\\"\\}");
  lex(ava_ltt_verbatim, "foo\\\"");
  end();
}

deftest(multiline_verbatim) {
  start("\\{foo\r\nbar\\}");
  lex(ava_ltt_verbatim, "foo\nbar");
  end();
}

deftest(verbatim_doesnt_count_unprefixed_braces) {
  start("\\{{\\} \\{}\\}");
  lex(ava_ltt_verbatim, "{");
  lex(ava_ltt_verbatim, "}");
  end();
}

deftest(nested_verbatim_1) {
  start("\\{\\{foo\\}\\}");
  lex(ava_ltt_verbatim, "\\{foo\\}");
  end();
}

deftest(nested_verbatim_2) {
  start("\\{\\{foo\\{bar\\}baz\\{\\}\\}xyzzy\\}");
  lex(ava_ltt_verbatim, "\\{foo\\{bar\\}baz\\{\\}\\}xyzzy");
  end();
}

deftest(verbatim_single_char_escapes) {
  start("\\{"
        "\\;a\\;b\\;e\\;f\\;n\\;r\\;t\\;v"
        "\\;\\"
        "\\;\"\\;'\\;`"
        "\\}");
  lex(ava_ltt_verbatim, "\a\b\033\f\n\r\t\v\\\"'`");
  end();
}

deftest(verbatim_hex_escapes) {
  start("\\{\\;x61\\;x76\\;x61\\;x6c\\;x61\\;x6e\\;x63\\;x68\\;x65\\;x0a\\}");
  lex(ava_ltt_verbatim, "avalanche\n");
  end();
}

deftest(verbatim_containing_lone_backslash) {
  start("\\{\\\\}");
  lex(ava_ltt_verbatim, "\\");
  end();
}

deftest(verbatim_clears_whitespace) {
  start("\\{\\}()");
  lex(ava_ltt_verbatim, "");
  lex(ava_ltt_begin_name_subscript, "(");
  lex(ava_ltt_close_paren, ")");
  end();
}

deftest(error_on_illegal_char_in_ground) {
  start("foo\b\bbar");
  lex(ava_ltt_bareword, "foo");
  error();
  lex(ava_ltt_bareword, "bar");
  end();
}

deftest(error_on_illegal_char_in_string) {
  start("\"foo\b\bbar\" baz");
  error();
  lex(ava_ltt_bareword, "baz");
  end();
}

deftest(error_on_illegal_char_in_verb) {
  start("\\{foo\b\bbar\\} baz");
  error();
  lex(ava_ltt_bareword, "baz");
  end();
}

deftest(error_on_illegal_char_at_eof) {
  start("\1");
  error();
  end();
}

deftest(error_on_nul_at_eof) {
  lexer = ava_lex_new(ava_string_of_char(0));
  error();
  end();
}

deftest(error_on_unterminated_string_literal) {
  start("\"foo\n\nbar");
  error();
  end();
}

deftest(error_on_unterminated_verbatim) {
  start("\\{foo\\{bar\\}}");
  error();
  end();
}

deftest(error_on_illegal_backslash_in_ground) {
  start("foo\\b ar");
  lex(ava_ltt_bareword, "foo");
  error();
  lex(ava_ltt_bareword, "ar");
  end();
}

deftest(error_on_illegal_backslash_in_string) {
  start("\"foo\\! bar\" xyzzy");
  error();
  lex(ava_ltt_bareword, "xyzzy");
  end();
}

deftest(error_on_illegal_backslash_in_verbatim) {
  start("\\{foo\\;! bar\\} xyzzy");
  error();
  lex(ava_ltt_bareword, "xyzzy");
  end();
}

deftest(error_on_attempt_to_escape_nl_in_string) {
  start("\"foo\\\nbar\" xyzzy");
  error();
  lex(ava_ltt_bareword, "xyzzy");
  end();
}

deftest(error_on_attempt_to_escape_nl_in_verbatim) {
  start("\\{foo\\;\nbar\\} xyzzy");
  error();
  lex(ava_ltt_bareword, "xyzzy");
  end();
}

deftest(error_on_backslash_at_eof_in_ground) {
  start("\\");
  error();
  end();
}

deftest(error_on_backslash_at_eof_in_string) {
  start("\"\\");
  error();
  end();
}

deftest(error_on_backslash_at_eof_in_verbatim) {
  start("\\{\\;");
  error();
  end();
}

deftest(error_on_synthetic_newline_not_preceded_by_whitespace) {
  start("foo\\ bar");
  lex(ava_ltt_bareword, "foo");
  error();
  lex(ava_ltt_bareword, "bar");
  end();
}

deftest(error_on_nonindependent_bareword) {
  start("\"\"foo");
  lex(ava_ltt_astring, "");
  error();
  end();
}

deftest(error_on_nonindependent_astring) {
  start("a\"\" b");
  lex(ava_ltt_bareword, "a");
  error();
  lex(ava_ltt_bareword, "b");
  end();
}

deftest(astring_has_attached_end) {
  start("\"\"a");
  lex(ava_ltt_astring, "");
  error();
  end();
}

deftest(rstring_has_independent_end) {
  start("\"`a");
  lex(ava_ltt_rstring, "");
  lex(ava_ltt_bareword, "a");
  end();
}

deftest(lstring_can_be_attached) {
  start("a`b\"");
  lex(ava_ltt_bareword, "a");
  lex(ava_ltt_lstring, "b");
  end();
}
