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
#include <string.h>
#include <assert.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/lex.h"
#include "avalanche/parser.h"

typedef enum {
  ava_purr_ok = 0,
  ava_purr_nonunit,
  ava_purr_eof,
  ava_purr_fatal_error
} ava_parse_unit_read_result;

typedef struct {
  ava_lex_context* lex;
  ava_string source;
  ava_string filename;
} ava_parse_context;

static ava_parse_unit_read_result ava_parse_unit_read(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  ava_lex_result* end_token,
  const ava_parse_context* context);

static ava_parse_unit_read_result ava_parse_block_content(
  ava_parse_unit* block_unit,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  ava_bool is_top_level,
  const ava_lex_result* first_token);

static void ava_parse_simplify_group_tag(
  ava_parse_unit* block_unit,
  const ava_parse_context* context,
  const ava_lex_result* closing_token);

static void ava_parse_unexpected_token(
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token);

static void ava_parse_unexpected_eof(
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* eof);

static void ava_parse_error_on_lex(
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* lexed,
  ava_string message);

static void ava_parse_error_on_lex_off(
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* lexed,
  ava_string message,
  size_t off_begin, size_t off_end);

static void ava_parse_error_on_unit(
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_parse_unit* unit,
  ava_string message);

static void ava_parse_location_from_lex(
  ava_compile_location* dst,
  const ava_parse_context* context,
  const ava_lex_result* lexed);
static void ava_parse_location_from_lex_off(
  ava_compile_location* dst,
  const ava_parse_context* context,
  const ava_lex_result* lexed,
  size_t off_begin, size_t off_end);

static ava_parse_unit_read_result ava_parse_bareword(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token);
static ava_parse_unit_read_result ava_parse_stringoid(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token);
static ava_parse_unit_read_result ava_parse_substitution(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token);
static ava_parse_unit_read_result ava_parse_name_subscript(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token);
static ava_parse_unit_read_result ava_parse_semiliteral(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token);
static ava_parse_unit_read_result ava_parse_numeric_subscript(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token);
static ava_parse_unit_read_result ava_parse_block(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token);
static ava_parse_unit_read_result ava_parse_string_subscript(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token);
static ava_parse_unit_read_result ava_parse_substitution_body(
  ava_parse_unit** dst, ava_lex_result* last_token,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* first_token,
  ava_lex_token_type closing_token_type);
static ava_parse_unit_read_result ava_parse_expression_list(
  ava_parse_unit_list* dst,
  ava_lex_result* last_token,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  ava_lex_token_type closing_token_type);
static ava_parse_unit_read_result ava_parse_regroup_semiliteral_strings(
  ava_parse_unit* unit,
  ava_compile_error_list* errors,
  const ava_parse_context* context);
static ava_parse_unit_read_result ava_parse_subscript(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* first_token,
  ava_string prefix,
  ava_lex_token_type closing_token_type);

ava_bool ava_parse(ava_parse_unit* dst,
                   ava_compile_error_list* errors,
                   ava_string source, ava_string filename) {
  ava_parse_context context = {
    .lex = ava_lex_new(source),
    .source = source,
    .filename = filename,
  };
  ava_lex_result pseudo_first_token = {
    .type = ava_ltt_none,
    .str = AVA_EMPTY_STRING,
    .line = 1,
    .column = 0,
    .index_start = 0,
    .index_end = 0,
    .line_offset = 0
  };

  TAILQ_INIT(errors);

  ava_parse_block_content(dst, errors, &context, ava_true, &pseudo_first_token);

  return TAILQ_EMPTY(errors);
}

static ava_parse_unit_read_result ava_parse_unit_read(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  ava_lex_result* lexed,
  const ava_parse_context* context
) {
  ava_lex_status lex_status;

  for (;;) {
    lex_status = ava_lex_lex(lexed, context->lex);
    switch (lex_status) {
    case ava_ls_end_of_input:
      return ava_purr_eof;

    case ava_ls_error:
      ava_parse_error_on_lex(errors, context, lexed, lexed->str);
      continue;

    case ava_ls_ok:
      break;
    }

    switch (lexed->type) {
    case ava_ltt_bareword:
      return ava_parse_bareword(dst, errors, context, lexed);

    case ava_ltt_astring:
    case ava_ltt_lstring:
    case ava_ltt_rstring:
    case ava_ltt_lrstring:
    case ava_ltt_verbatim:
      return ava_parse_stringoid(dst, errors, context, lexed);

    case ava_ltt_begin_substitution:
      return ava_parse_substitution(dst, errors, context, lexed);

    case ava_ltt_begin_name_subscript:
      return ava_parse_name_subscript(dst, errors, context, lexed);

    case ava_ltt_begin_semiliteral:
      return ava_parse_semiliteral(dst, errors, context, lexed);

    case ava_ltt_begin_numeric_subscript:
      return ava_parse_numeric_subscript(dst, errors, context, lexed);

    case ava_ltt_begin_block:
      return ava_parse_block(dst, errors, context, lexed);

    case ava_ltt_begin_string_subscript:
      return ava_parse_string_subscript(dst, errors, context, lexed);

    case ava_ltt_newline:
    case ava_ltt_close_paren:
    case ava_ltt_close_bracket:
    case ava_ltt_close_brace:
      return ava_purr_nonunit;

    case ava_ltt_none: abort();
    }
  }
}

static ava_parse_unit_read_result ava_parse_block_content(
  ava_parse_unit* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  ava_bool is_top_level,
  const ava_lex_result* first_token
) {
  ava_bool beginning_of_statement = ava_true;
  ava_parse_unit_read_result result;
  ava_parse_statement* statement;
  ava_lex_result token;

  dst->type = ava_put_block;
  dst->location.filename = context->filename;
  dst->location.source = context->source;
  dst->location.line_offset = 0;
  dst->location.start_line = first_token->line;
  dst->location.start_column = first_token->column;
  TAILQ_INIT(&dst->v.statements);

  for (;;) {
    statement = TAILQ_LAST(&dst->v.statements, ava_parse_statement_list_s);
    if (beginning_of_statement) {
      if (!statement || !TAILQ_EMPTY(&statement->units)) {
        statement = AVA_NEW(ava_parse_statement);
        TAILQ_INIT(&statement->units);
        TAILQ_INSERT_TAIL(&dst->v.statements, statement, next);
      }
      beginning_of_statement = ava_false;
    }

    result = ava_parse_unit_read(&statement->units, errors, &token, context);

    switch (result) {
    case ava_purr_ok:
      break;

    case ava_purr_fatal_error:
      return ava_purr_fatal_error;

    case ava_purr_nonunit:
      if (ava_ltt_newline == token.type) {
        beginning_of_statement = ava_true;
      } else if (!is_top_level || ava_ltt_close_brace != token.type) {
        ava_parse_unexpected_token(errors, context, &token);
        return ava_purr_fatal_error;
      } else {
        ava_parse_simplify_group_tag(dst, context, &token);
        return ava_purr_ok;
      }
      break;

    case ava_purr_eof:
      if (is_top_level) {
        return ava_purr_ok;
      } else {
        ava_parse_unexpected_eof(errors, context, &token);
        return ava_purr_fatal_error;
      }
    }
  }
}

static void ava_parse_unexpected_token(
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token
) {
  AVA_STATIC_STRING(message, "Unexpected token: ");

  ava_parse_error_on_lex(errors, context, token,
                         ava_string_concat(message, token->str));
}

static void ava_parse_unexpected_eof(
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* eof
) {
  AVA_STATIC_STRING(message, "Unexpected end-of-input");

  ava_parse_error_on_lex(errors, context, eof, message);
}

static void ava_parse_error_on_lex(
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* lexed,
  ava_string message
) {
  ava_parse_error_on_lex_off(
    errors, context, lexed, message,
    0, lexed->index_end - lexed->index_start);
}

static void ava_parse_error_on_lex_off(
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* lexed,
  ava_string message,
  size_t off_begin, size_t off_end
) {
  ava_compile_error* error = AVA_NEW(ava_compile_error);
  error->message = message;
  ava_parse_location_from_lex_off(&error->location, context, lexed,
                                  off_begin, off_end);
  TAILQ_INSERT_TAIL(errors, error, next);
}

static void ava_parse_error_on_unit(
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_parse_unit* unit,
  ava_string message
) {
  ava_compile_error* error = AVA_NEW(ava_compile_error);
  error->message = message;
  error->location = unit->location;
  TAILQ_INSERT_TAIL(errors, error, next);
}

static void ava_parse_location_from_lex(
  ava_compile_location* dst,
  const ava_parse_context* context,
  const ava_lex_result* lexed
) {
  ava_parse_location_from_lex_off(
    dst, context, lexed, 0, lexed->index_end - lexed->index_start);
}

static void ava_parse_location_from_lex_off(
  ava_compile_location* dst,
  const ava_parse_context* context,
  const ava_lex_result* lexed,
  size_t off_begin, size_t off_end
) {
  dst->filename = context->filename;
  dst->source = context->source;
  dst->line_offset = lexed->line_offset;
  dst->start_line = lexed->line;
  dst->end_line = lexed->line;
  dst->start_column = lexed->column + off_begin;
  dst->end_column = lexed->column + off_end;
}

static void ava_parse_simplify_group_tag(
  ava_parse_unit* unit,
  const ava_parse_context* context,
  const ava_lex_result* closing_token
) {
  AVA_STATIC_STRING(substitution_base, "#substitution#");
  AVA_STATIC_STRING(semiliteral_base, "#semiliteral#");
  AVA_STATIC_STRING(block_base, "#block#");
  ava_string base;
  ava_parse_unit* orig, * bareword;

  if (1 == ava_string_length(closing_token->str))
    /* No tag */
    return;

  orig = AVA_NEW(ava_parse_unit);
  bareword = AVA_NEW(ava_parse_unit);

  *orig = *unit;
  bareword->type = ava_put_bareword;
  ava_parse_location_from_lex(&bareword->location, context, closing_token);

  switch (unit->type) {
  case ava_put_substitution:
    base = substitution_base;
    break;

  case ava_put_semiliteral:
    base = semiliteral_base;
    break;

  case ava_put_block:
    base = block_base;
    break;

  default:
    abort();
  }

  bareword->v.string = ava_string_concat(
    base, ava_string_slice(closing_token->str, 1,
                           ava_string_length(closing_token->str)));

  unit->type = ava_put_substitution;
  TAILQ_INIT(&unit->v.units);
  TAILQ_INSERT_TAIL(&unit->v.units, bareword, next);
  TAILQ_INSERT_TAIL(&unit->v.units, orig, next);
}

static ava_parse_unit_read_result ava_parse_bareword(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token
) {
  AVA_STATIC_STRING(empty_varname_message,
                    "Empty variable name");

  char strtmp[AVA_STR_TMPSZ];
  const char* content;
  size_t strlen, begin, end, i;
  ava_parse_unit* unit, * subunit, * varword;
  ava_bool has_dollar;
  ava_bool in_var;

  strlen = ava_string_length(token->str);
  content = ava_string_to_cstring_buff(strtmp, token->str);

  has_dollar = ava_false;
  for (i = 0; i < strlen && !has_dollar; ++i)
    has_dollar |= '$' == content[i];

  if (!has_dollar) {
    unit = AVA_NEW(ava_parse_unit);
    unit->type = ava_put_bareword;
    ava_parse_location_from_lex(&unit->location, context, token);
    unit->v.string = token->str;
    TAILQ_INSERT_TAIL(dst, unit, next);
    return ava_purr_ok;
  }

  /* Else, interpolated bareword */
  unit = AVA_NEW(ava_parse_unit);
  unit->type = ava_put_substitution;
  ava_parse_location_from_lex(&unit->location, context, token);
  TAILQ_INIT(&unit->v.units);

  in_var = ava_false;
  for (end = begin = 0; end <= strlen; ++end) {
    if ('$' == content[end] || 0 == content[end]) {
      /* Variable names cannot be empty */
      if (end == begin && in_var) {
        ava_parse_error_on_lex_off(errors, context, token,
                                   empty_varname_message,
                                   begin, end);
      }

      /* If in a variable, always produce a substitution expression for that
       * variable.
       */
      if (in_var) {
        subunit = AVA_NEW(ava_parse_unit);
        subunit->type = ava_put_substitution;
        ava_parse_location_from_lex_off(
          &subunit->location, context, token, begin, end);
        TAILQ_INIT(&subunit->v.units);

        varword = AVA_NEW(ava_parse_unit);
        varword->type = ava_put_bareword;
        ava_parse_location_from_lex_off(
          &varword->location, context, token, begin, end);
        varword->v.string = AVA_ASCII9_STRING("#var#");
        TAILQ_INSERT_TAIL(&subunit->v.units, varword, next);

        varword = AVA_NEW(ava_parse_unit);
        varword->type = ava_put_bareword;
        ava_parse_location_from_lex_off(
          &varword->location, context, token, begin, end);
        varword->v.string = ava_string_slice(token->str, begin, end);
        TAILQ_INSERT_TAIL(&subunit->v.units, varword, next);

        TAILQ_INSERT_TAIL(&unit->v.units, subunit, next);

      /* Otherwise produce a string. Omit empty strings at the beginning or
       * end of the bareword.
       */
      } else if (end > begin || (begin != 0 && end != strlen)) {
        subunit = AVA_NEW(ava_parse_unit);

        if (begin > 0 && end < strlen)
          subunit->type = ava_put_lrstring;
        else if (begin > 0)
          subunit->type = ava_put_lstring;
        else if (end < strlen)
          subunit->type = ava_put_rstring;
        else
          /* Nominally an A-String, but we should never hit this case, since
           * such barewords are left as such.
           */
          abort();

        ava_parse_location_from_lex_off(
          &subunit->location, context, token, begin, end);
        subunit->v.string = ava_string_slice(token->str, begin, end);
        TAILQ_INSERT_TAIL(&unit->v.units, subunit, next);
      }

      in_var = !in_var;
      begin = end + 1;
    } /* end if delimiter */
  }

  TAILQ_INSERT_TAIL(dst, unit, next);
  return ava_purr_ok;
}

static ava_parse_unit_read_result ava_parse_stringoid(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* token
) {
  ava_parse_unit* unit;

  unit = AVA_NEW(ava_parse_unit);
  switch (token->type) {
  case ava_ltt_astring:  unit->type = ava_put_astring;  break;
  case ava_ltt_lstring:  unit->type = ava_put_lstring;  break;
  case ava_ltt_rstring:  unit->type = ava_put_rstring;  break;
  case ava_ltt_lrstring: unit->type = ava_put_lrstring; break;
  case ava_ltt_verbatim: unit->type = ava_put_verbatim; break;
  default: abort();
  }

  ava_parse_location_from_lex(&unit->location, context, token);
  unit->v.string = token->str;

  TAILQ_INSERT_TAIL(dst, unit, next);
  return ava_purr_ok;
}

static ava_parse_unit_read_result ava_parse_substitution(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* first_token
) {
  ava_parse_unit* substitution;
  ava_lex_result last_token;
  ava_parse_unit_read_result status;

  status = ava_parse_substitution_body(
    &substitution, &last_token, errors, context,
    first_token, ava_ltt_close_paren);
  if (status == ava_purr_ok)
    ava_parse_simplify_group_tag(substitution, context, &last_token);

  TAILQ_INSERT_TAIL(dst, substitution, next);

  return status;
}

static ava_parse_unit_read_result ava_parse_substitution_body(
  ava_parse_unit** dst, ava_lex_result* last_token,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* first_token,
  ava_lex_token_type closing_token_type
) {
  ava_parse_unit* unit;
  ava_parse_statement* statement;
  ava_parse_unit_read_result result;

  unit = AVA_NEW(ava_parse_unit);
  unit->type = ava_put_substitution;
  ava_parse_location_from_lex(&unit->location, context, first_token);
  TAILQ_INIT(&unit->v.statements);

  statement = AVA_NEW(ava_parse_statement);
  TAILQ_INIT(&statement->units);

  result = ava_parse_expression_list(
    &statement->units, last_token, errors, context, closing_token_type);

  if (!TAILQ_EMPTY(&statement->units))
    TAILQ_INSERT_TAIL(&unit->v.statements, statement, next);

  *dst = unit;
  return result;
}

static ava_parse_unit_read_result ava_parse_expression_list(
  ava_parse_unit_list* dst,
  ava_lex_result* last_token,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  ava_lex_token_type closing_token_type
) {
  ava_parse_unit_read_result result;

  for (;;) {
    result = ava_parse_unit_read(dst, errors, last_token, context);

    switch (result) {
    case ava_purr_ok: continue;
    case ava_purr_fatal_error: return ava_purr_fatal_error;
    case ava_purr_eof:
      ava_parse_unexpected_eof(errors, context, last_token);
      return ava_purr_fatal_error;

    case ava_purr_nonunit:
      if (ava_ltt_newline == last_token->type) {
        continue;
      } else if (closing_token_type != last_token->type) {
        ava_parse_unexpected_token(errors, context, last_token);
        return ava_purr_fatal_error;
      } else {
        return ava_purr_ok;
      }
    }
  }
}

static ava_parse_unit_read_result ava_parse_semiliteral(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* first_token
) {
  ava_parse_unit* unit;
  ava_parse_unit_read_result result;
  ava_lex_result last_token;

  unit = AVA_NEW(ava_parse_unit);
  unit->type = ava_put_semiliteral;
  ava_parse_location_from_lex(&unit->location, context, first_token);
  TAILQ_INIT(&unit->v.units);

  result = ava_parse_expression_list(
    &unit->v.units, &last_token, errors, context, ava_ltt_close_bracket);

  if (ava_purr_ok == result)
    result = ava_parse_regroup_semiliteral_strings(
      unit, errors, context);

  if (ava_purr_ok == result)
    ava_parse_simplify_group_tag(unit, context, &last_token);

  TAILQ_INSERT_TAIL(dst, unit, next);
  return result;
}

static ava_parse_unit_read_result ava_parse_regroup_semiliteral_strings(
  ava_parse_unit* unit,
  ava_compile_error_list* errors,
  const ava_parse_context* context
) {
  AVA_STATIC_STRING(lstring_at_start_message,
                    "L-String or LR-String at beginning of semiliteral");
  AVA_STATIC_STRING(rstring_at_end_message,
                    "R-String or LR-String at end of semiliteral");

  ava_parse_unit * it, * begin, * end, * after_end, * m, * next;
  ava_parse_unit* wrapper;
  ava_parse_statement* statement;

  for (it = TAILQ_FIRST(&unit->v.units); it; it = TAILQ_NEXT(it, next)) {
    if (ava_put_lstring == it->type ||
        ava_put_lrstring == it->type ||
        ava_put_rstring == it->type) {
      if (ava_put_lstring == it->type || ava_put_lrstring == it->type) {
        begin = TAILQ_PREV(it, ava_parse_unit_list_s, next);
        if (!begin) {
          ava_parse_error_on_unit(errors, context, it,
                                  lstring_at_start_message);
          goto next_it;
        }
      } else {
        begin = it;
      }

      end = begin;
      /* Search forward until we find no reason to continue advancing the end
       * pointer.
       */
      do {
        after_end = TAILQ_NEXT(end, next);

        /* If the current (inclusive) end is an R-String or LR-String, move end
         * to the following unit.
         */
        if (ava_put_rstring == end->type || ava_put_lrstring == end->type)  {
          if (!after_end) {
            ava_parse_error_on_unit(errors, context, it,
                                    rstring_at_end_message);
            goto next_it;
          }

          end = after_end;
        }

        /* If the unit following the inclusive end is present and is an
         * L-String or LR-String, advance end.
         */
        if (after_end && (ava_put_lstring == after_end->type ||
                          ava_put_lrstring == after_end->type)) {
          end = after_end;
        }
      } while (end == after_end);

      /* Wrap begin..end inclusive in a Substitution */
      wrapper = AVA_NEW(ava_parse_unit);
      wrapper->type = ava_put_substitution;
      wrapper->location = begin->location;
      TAILQ_INIT(&wrapper->v.statements);
      statement = AVA_NEW(ava_parse_statement);
      TAILQ_INIT(&statement->units);
      TAILQ_INSERT_TAIL(&wrapper->v.statements, statement, next);

      TAILQ_INSERT_BEFORE(begin, wrapper, next);

      /* Hypothetically this could be done without a loop, but the necessary
       * API isn't exposed by queue(2).
       *
       * Performance doesn't really matter here, though.
       */
      for (m = begin; m != after_end; m = next) {
        next = TAILQ_NEXT(m, next);

        /* If an unsubstituted bareword, change to a verbatim */
        if (ava_put_bareword == m->type)
          m->type = ava_put_bareword;

        TAILQ_REMOVE(&unit->v.units, m, next);
        TAILQ_INSERT_TAIL(&statement->units, m, next);
      }

      it = wrapper;
    } /* end if {L,R,LR}-String */

    next_it:;
  }

  return ava_purr_ok;
}

static ava_parse_unit_read_result ava_parse_block(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* first_token
) {
  ava_parse_unit* unit;
  ava_parse_unit_read_result result;

  unit = AVA_NEW(ava_parse_unit);
  result = ava_parse_block_content(
    unit, errors, context, ava_false, first_token);

  TAILQ_INSERT_TAIL(dst, unit, next);
  return result;
}

static ava_parse_unit_read_result ava_parse_name_subscript(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* first_token
) {
  AVA_STATIC_STRING(prefix, "#name-subscript#");

  return ava_parse_subscript(
    dst, errors, context, first_token,
    prefix, ava_ltt_close_paren);
}

static ava_parse_unit_read_result ava_parse_numeric_subscript(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* first_token
) {
  AVA_STATIC_STRING(prefix, "#numeric-subscript#");

  return ava_parse_subscript(
    dst, errors, context, first_token,
    prefix, ava_ltt_close_bracket);
}

static ava_parse_unit_read_result ava_parse_string_subscript(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* first_token
) {
  AVA_STATIC_STRING(prefix, "#string-subscript#");

  return ava_parse_subscript(
    dst, errors, context, first_token,
    prefix, ava_ltt_close_brace);
}

static ava_parse_unit_read_result ava_parse_subscript(
  ava_parse_unit_list* dst,
  ava_compile_error_list* errors,
  const ava_parse_context* context,
  const ava_lex_result* first_token,
  ava_string prefix,
  ava_lex_token_type closing_token_type
) {
  ava_parse_unit* unit, * bareword, * base, * subscript;
  ava_parse_statement* statement, * substatement;
  ava_lex_result last_token;
  ava_parse_unit_read_result result;
  ava_string tag;
  unsigned tag_off;

  /* A subscript with no preceding unit is syntactically impossible. If this
   * happens, assume that an earlier syntax error resulted in this situation,
   * so don't report any errors.
   */
  if (TAILQ_EMPTY(dst)) {
    assert(!TAILQ_EMPTY(errors));
    return ava_purr_fatal_error;
  }

  substatement = AVA_NEW(ava_parse_statement);
  TAILQ_INIT(&substatement->units);
  result = ava_parse_expression_list(
    &substatement->units, &last_token, errors, context, closing_token_type);

  if (ava_purr_ok == result) {
    tag = ava_string_concat(
      AVA_ASCII9_STRING("#"),
      ava_string_concat(
        ava_string_slice(
          last_token.str,
          1, ava_string_length(last_token.str)),
        AVA_ASCII9_STRING("#")));
    tag_off = 1;
  } else {
    tag = AVA_ASCII9_STRING("##");
    tag_off = 0;
  }

  unit = AVA_NEW(ava_parse_unit);
  unit->type = ava_put_substitution;
  ava_parse_location_from_lex(&unit->location, context, first_token);
  TAILQ_INIT(&unit->v.statements);

  base = TAILQ_LAST(dst, ava_parse_unit_list_s);
  assert(base);
  TAILQ_REMOVE(dst, base, next);

  statement = AVA_NEW(ava_parse_statement);
  TAILQ_INIT(&statement->units);
  TAILQ_INSERT_TAIL(&unit->v.statements, statement, next);

  bareword = AVA_NEW(ava_parse_unit);
  bareword->type = ava_put_bareword;
  ava_parse_location_from_lex(&bareword->location, context, first_token);
  bareword->v.string = prefix;
  TAILQ_INSERT_TAIL(&statement->units, bareword, next);

  bareword = AVA_NEW(ava_parse_unit);
  bareword->type = ava_put_bareword;
  ava_parse_location_from_lex_off(
    &bareword->location, context, &last_token,
    tag_off, last_token.index_end - last_token.index_start);
  bareword->v.string = tag;
  TAILQ_INSERT_TAIL(&statement->units, bareword, next);

  TAILQ_INSERT_TAIL(&statement->units, base, next);

  subscript = AVA_NEW(ava_parse_unit);
  subscript->type = ava_put_substitution;
  subscript->location = unit->location;
  TAILQ_INIT(&subscript->v.statements);
  TAILQ_INSERT_TAIL(&subscript->v.statements, substatement, next);
  TAILQ_INSERT_TAIL(&statement->units, subscript, next);

  TAILQ_INSERT_TAIL(dst, unit, next);
  return result;
}
