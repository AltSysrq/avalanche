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
#error "Don't include avalanche/parser.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_PARSER_H_
#define AVA_RUNTIME_PARSER_H_

#include "defs.h"
#include "string.h"
#include "errors.h"

/**
 * @file
 *
 * Provides facilities for parsing a string containing presumed Avalanche
 * source into a simplified AST.
 *
 * The parser does not perform macro substitution, but does perform all steps
 * directly described in Syntax II in the spec.
 */

/**
 * The possible simplified AST unit types.
 */
typedef enum {
  /**
   * A syntax unit corresponding to a single Bareword token.
   */
  ava_put_bareword,
  /**
   * A syntax unit corresponding to a single A-String token.
   */
  ava_put_astring,
  /**
   * A syntax unit corresponding to a single L-String token.
   */
  ava_put_lstring,
  /**
   * A syntax unit corresponding to a single R-String token.
   */
  ava_put_rstring,
  /**
   * A syntax unit corresponding to a single LR-String token.
   */
  ava_put_lrstring,
  /**
   * A syntax unit holding units between a Begin-Substitution token and its
   * matching Close-Paren token.
   */
  ava_put_substitution,
  /**
   * A syntax unit holding units between a Begin-Semiliteral token and its
   * matching Close-Bracket token.
   */
  ava_put_semiliteral,
  /**
   * A syntax unit holding units between a Begin-Block token and its matching
   * Close-Brace token.
   */
  ava_put_block,
  /**
   * A syntax unit corresponding to a single Verbatim token.
   */
  ava_put_verbatim,
  /**
   * A syntax unit spreading another syntax unit.
   */
  ava_put_spread
} ava_parse_unit_type;

typedef struct ava_parse_unit_s ava_parse_unit;
/**
 * A list of ava_parse_units.
 */
typedef TAILQ_HEAD(ava_parse_unit_list_s, ava_parse_unit_s) ava_parse_unit_list;
typedef struct ava_parse_statement_s ava_parse_statement;
/**
 * A list of ava_parse_statements.
 */
typedef TAILQ_HEAD(ava_parse_statement_list_s, ava_parse_statement_s) ava_parse_statement_list;

/**
 * A single syntax unit as produced by the simplified-AST parser.
 */
struct ava_parse_unit_s {
  /**
   * The type of this unit.
   */
  ava_parse_unit_type type;

  /**
   * The location of this syntax unit.
   */
  ava_compile_location location;

  /**
   * The content of this unit. Which field to use is dictated by the type
   * field.
   */
  union {
    /**
     * The string content of a Bareword, A-String, L-String, R-String,
     * LR-String, or Verbatim syntax unit.
     */
    ava_string string;
    /**
     * The list of statements within a Block or Substitution syntax unit.
     */
    ava_parse_statement_list statements;
    /**
     * The list of units/elements within a Semiliteral syntax unit.
     */
    ava_parse_unit_list units;
    /**
     * The unit spread by a Spread unit.
     */
    ava_parse_unit* unit;
  } v;

  TAILQ_ENTRY(ava_parse_unit_s) next;
};

/**
 * A single statement within a Block or Substitution.
 */
struct ava_parse_statement_s {
  /**
   * The units comprising this statement.
   */
  ava_parse_unit_list units;
  TAILQ_ENTRY(ava_parse_statement_s) next;
};

/**
 * Attempts to parse the given string into a simplified AST.
 *
 * @param dst Out-pointer for the result on success. On success, it will be a
 * Block syntax unit containing the full parse result. On failure, contents are
 * undefined.
 * @param errors Out-pointer for the resultg on failure. On failure, contains a
 * list of all errors encountered, in the order they were encountered. On
 * success, contents are undefined.
 * @param source The source code to parse.
 * @param filename The name of the file being parsed.
 * @param init_root Whether to initialise dst. If false, *dst is assumed to
 * already be a Block syntax unit. This permits concatenating the parse of
 * multiple files into one tree.
 * @return Whether the parse succeeded.
 */
ava_bool ava_parse(ava_parse_unit* dst,
                   ava_compile_error_list* errors,
                   ava_string source, ava_string filename,
                   ava_bool init_root);

/**
 * Constructs a parse unit which is a substitution containing only the given
 * statement, which must not be empty.
 */
ava_parse_unit* ava_parse_subst_of_nonempty_statement(
  ava_parse_statement* statement);

/**
 * Returns a compile location representing the span between the beginning of
 * begin and the end of end.
 *
 * Assumes both locations are in the same source file.
 */
ava_compile_location ava_compile_location_span(
  const ava_compile_location* begin, const ava_compile_location* end);

#endif /* AVA_RUNTIME_PARSER_H_ */
