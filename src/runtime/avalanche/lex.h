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
#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/lex.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_LEX_H_
#define AVA_RUNTIME_LEX_H_

#include "defs.h"
#include "string.h"

/******************** LEXICAL ANALYSIS ********************/

/**
 * Opaque struct storing the state of the lexical analyser.
 */
typedef struct ava_lex_context_s ava_lex_context;

/**
 * Describes the type of a lexed token.
 */
typedef enum {
  /**
   * The token is a simple string without any quoting.
   */
  ava_ltt_bareword,
  /**
   * The token is an actual or logical line break.
   */
  ava_ltt_newline,
  /**
   * The token is a string enclosed in double-quotes.
   */
  ava_ltt_astring,
  /**
   * The token is a string initiated with a back-quote and terminated with a
   * double-quote.
   */
  ava_ltt_lstring,
  /**
   * The token is a string initiated with a double-quote an terminated with a
   * back-quote.
   */
  ava_ltt_rstring,
  /**
   * Tke token is a string enclosed in back-quotes.
   */
  ava_ltt_lrstring,
  /**
   * The token is a left-parenthesis starting a substitution.
   */
  ava_ltt_begin_substitution,
  /**
   * The token is a left-parenthesis starting a name subscript.
   */
  ava_ltt_begin_name_subscript,
  /**
   * The token is a right-parenthesis.
   */
  ava_ltt_close_paren,
  /**
   * The token is a left-bracket starting a semiliteral.
   */
  ava_ltt_begin_semiliteral,
  /**
   * The token is a left-bracket starting a numeric subscript.
   */
  ava_ltt_begin_numeric_subscript,
  /**
   * The token is a right-bracket.
   */
  ava_ltt_close_bracket,
  /**
   * The token is a left-brace starting a block.
   */
  ava_ltt_begin_block,
  /**
   * The token is a left-brace starting a string subscript.
   */
  ava_ltt_begin_string_subscript,
  /**
   * The token is a right-brace.
   */
  ava_ltt_close_brace,
  /**
   * The token is a string enclosed in \{...\}.
   */
  ava_ltt_verbatim,
  /**
   * Not an actual token type; used when no token could be extracted due to
   * end-of-input or error.
   */
  ava_ltt_none
} ava_lex_token_type;

/**
 * Output type from the lexical analyser.
 */
typedef struct {
  /**
   * The type of token encountered.
   */
  ava_lex_token_type type;
  /**
   * IF type is not ava_ltt_none, the string content of the token, after escape
   * sequence substitution. (Eg, the token
   *    "foo\x41"
   * would have a str value of
   *    fooA
   * ).
   *
   * Otherwise, this contains the error message if an error was encountered, or
   * empty string if end-of-file.
   */
  ava_string str;
  /**
   * The line and column numbers where the start of this result was found. Both
   * are 1-based.
   */
  size_t line, column;
  /**
   * The indices between which the raw token can be found.
   */
  size_t index_start, index_end;
  /**
   * The byte offset within the original string at which the line on which this
   * token is found begins.
   */
  size_t line_offset;
} ava_lex_result;

/**
 * Indicates whether the lexer successfully extracted a token.
 *
 * Zero is the only success code; any true value indicates that no token was
 * extracted.
 */
typedef enum {
  /**
   * Success.
   */
  ava_ls_ok = 0,
  /**
   * No error occurred, but the lexer encountered the end of the input string
   * before extracting any new tokens.
   */
  ava_ls_end_of_input,
  /**
   * An error in the lexical syntax was encountered.
   */
  ava_ls_error
} ava_lex_status;

/**
 * Returns whether the given token type can be treated as a simple string.
 *
 * This assumes the caller assigns no special semantics to barewords.
 */
ava_bool ava_lex_token_type_is_simple(ava_lex_token_type);
/**
 * Returns whether the given token type is any of the three
 * close-parenthesis-like token types.
 */
ava_bool ava_lex_token_type_is_close_paren(ava_lex_token_type);

/**
 * Creates a new lexical analyser that will tokenise the given string.
 *
 * @return The lexical analyser, managed on the garbage-collected heap.
 */
ava_lex_context* ava_lex_new(ava_string);
/**
 * Obtains the next token from the lexical analyser.
 *
 * @param result The result in which to store output information. All fields of
 * this value are written regardless of the return status.
 * @param analyser The lexical analyser. The state of the analyser will be
 * advanced to the next token.
 * @return The status of this lexing attempt. It is safe to continue using the
 * lexer even after it returns an error.
 */
ava_lex_status ava_lex_lex(ava_lex_result* result, ava_lex_context* analyser);

#endif /* AVA_RUNTIME_LEX_H_ */
