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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/alloc.h"
#include "avalanche/lex.h"
#include "-hexes.h"
#include "gen-lex.h"

typedef struct {
  size_t line, column, line_offset, index;
} ava_lex_pos;

struct ava_lex_context_s {
  ava_string str;
  size_t strlen;
  ava_lex_pos p;

  /* Misc state that needs to be passed around */
  ava_bool is_independent;
  unsigned verbatim_depth;
  ava_string accum;
  char string_started_with;

  const char*restrict buffer;
  char tmpbuff[AVA_STR_TMPSZ];
  size_t buffer_off;
  /* The most recent 4 characters, excluding the one under the cursor, to
   * guarantee that, eg, string escaping doesn't have to go deep-diving in the
   * string to find what it's working on.
   *
   * The lowest-order byte is the most recent character.
   */
  unsigned prev_char;
};

static unsigned char ava_lex_get(ava_lex_context*);
static void ava_lex_consume(ava_lex_context*);
static ava_lex_status ava_lex_put_token(
  ava_lex_result* dst, ava_lex_token_type,
  const ava_lex_pos* begin, const ava_lex_pos* end,
  const ava_lex_context* lex);
static ava_lex_status ava_lex_put_token_str(
  ava_lex_result* dst, ava_lex_token_type, ava_string,
  const ava_lex_pos* begin, const ava_lex_pos* end);
static ava_lex_status ava_lex_put_error(
  ava_lex_result* dst,
  const ava_lex_pos* begin,
  const ava_lex_pos* end,
  const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
__attribute((format(printf,4,5)))
#endif
;

static ava_lex_status ava_lex_bareword(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_newline(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_left_paren(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_right_paren(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_left_bracket(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_right_bracket(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_left_brace(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_right_brace(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static void ava_lex_string_init(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_string_finish(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static void ava_lex_accum_verb(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static void ava_lex_accum_nl(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static void ava_lex_accum_esc(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static void ava_lex_accum_esc2(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static void ava_lex_accum_esc_off(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex,
  unsigned skip);
static ava_lex_status ava_lex_error_backslash_sequence(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_error_backslash_at_eof(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static void ava_lex_verb_init(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_verb_finish(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_error_illegal_chars(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);
static ava_lex_status ava_lex_error_need_separator(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex);

ava_bool ava_lex_token_type_is_simple(ava_lex_token_type type) {
  switch (type) {
  case ava_ltt_bareword:
  case ava_ltt_astring:
  case ava_ltt_verbatim:
    return ava_true;

  case ava_ltt_none:
    abort();

  default:
    return ava_false;
  }
}

ava_bool ava_lex_token_type_is_close_paren(ava_lex_token_type type) {
  switch (type) {
  case ava_ltt_close_paren:
  case ava_ltt_close_bracket:
  case ava_ltt_close_brace:
    return ava_true;

  case ava_ltt_none:
    abort();

  default:
    return ava_false;
  }
}

ava_lex_context* ava_lex_new(ava_string str) {
  ava_lex_context* lex = AVA_NEW(ava_lex_context);
  lex->str = str;
  lex->strlen = ava_string_length(str);
  lex->p.line = 1;
  lex->p.column = 1;
  lex->p.line_offset = 0;
  lex->p.index = 0;
  lex->buffer = ava_string_to_cstring_buff(lex->tmpbuff, str);

  lex->verbatim_depth = 0;

  return lex;
}

static unsigned char ava_lex_get(ava_lex_context* lex) {
  if (lex->buffer_off >= lex->strlen)
    return 0;

  return lex->buffer[lex->buffer_off];
}

static void ava_lex_consume(ava_lex_context* lex) {
  if (lex->buffer_off >= lex->strlen) return;

  lex->prev_char <<= 8;
  lex->prev_char |= lex->buffer[lex->buffer_off] & 0xFF;

  switch (lex->buffer[lex->buffer_off]) {
  case '\t':
    lex->p.column = (lex->p.column + 7) / 8 * 8 + 1;
    break;

  case '\n':
    ++lex->p.line;
    lex->p.column = 1;
    lex->p.line_offset = lex->buffer_off + 1;
    break;

  default:
    ++lex->p.column;
    break;
  }

  ++lex->buffer_off;
  ++lex->p.index;
}

static ava_lex_status ava_lex_put_token_str(
  ava_lex_result* dst,
  ava_lex_token_type type,
  ava_string str,
  const ava_lex_pos* begin,
  const ava_lex_pos* end
) {
  dst->type = type;
  dst->str = str;
  dst->line = begin->line;
  dst->column = begin->column;
  dst->index_start = begin->index;
  dst->index_end = end->index;
  dst->line_offset = begin->line_offset;
  return ava_ls_ok;
}

static ava_lex_status ava_lex_put_token(
  ava_lex_result* dst,
  ava_lex_token_type type,
  const ava_lex_pos* begin,
  const ava_lex_pos* end,
  const ava_lex_context* lex
) {
  return ava_lex_put_token_str(
    dst, type, ava_string_slice(lex->str, begin->index, end->index),
    begin, end);
}

static ava_lex_status ava_lex_put_error(
  ava_lex_result* dst,
  const ava_lex_pos* begin,
  const ava_lex_pos* end,
  const char* format, ...
) {
  va_list args;
  char message[256];

  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  dst->type = ava_ltt_none;
  dst->str = ava_string_of_cstring(message);
  dst->line = begin->line;
  dst->column = begin->column;
  dst->index_start = begin->index;
  dst->index_end = end->index;
  dst->line_offset = begin->line_offset;
  return ava_ls_error;
}

static ava_lex_status ava_lex_put_eof(ava_lex_result* dst,
                                      const ava_lex_context* lex) {
  dst->type = ava_ltt_none;
  dst->str = AVA_EMPTY_STRING;
  dst->line = lex->p.line;
  dst->column = lex->p.column;
  dst->index_start = dst->index_end = lex->p.index;
  dst->line_offset = lex->p.line_offset;
  return ava_ls_end_of_input;
}

static const ava_ulong is_independent_prefix[256/64] = {
  0x00000101FFFFFFFF,
  0x8800000108000000,
  0x0000000000000000,
  0x0000000000000000,
};

ava_lex_status ava_lex_lex(ava_lex_result* dst, ava_lex_context* lex) {
  ava_lex_context marker/*, ctxmarker not currently used by lexer */;
  ava_lex_pos start, frag_start;
  enum YYCONDTYPE cond = yycGround;

  ava_bool is_new_token = ava_true;
  ava_lex_status status = ava_ls_ok;

#define YYCTYPE unsigned char
#define YYPEEK() (ava_lex_get(lex))
#define YYSKIP() (ava_lex_consume(lex))
#define YYBACKUP() (marker = *lex)
#define YYBACKUPCTX() (ctxmarker = *lex)
#define YYRESTORE() (*lex = marker)
#define YYRESTORECTX() (*lex = ctxmarker)
#define YYLESSTHAN(n) (lex->strlen - lex->p.index < (n))
#define YYFILL(n) do {} while(0)
#define YYSETCONDITION(c) (cond = c)
#define YYGETCONDITION() (cond)
#define IGNORE() do { is_new_token = 1; goto continue_loop; } while (0)
#define ACCEPT(fun) do {                                \
    if (!status)                                        \
      status = fun(dst, &start, &frag_start, lex);      \
    goto end;                                           \
  } while (0)
#define ACCUM(fun) do {                                         \
    fun(&frag_start, lex);                                      \
    goto continue_loop;                                         \
  } while (0)
#define SET_ERROR(fun) do {                             \
    if (!status)                                        \
      status = fun(dst, &start, &frag_start, lex);      \
  } while (0)
#define DERROR(fun) do {                                \
    SET_ERROR(fun);                                     \
    goto continue_loop;                                 \
  } while (0)
#define ERROR(fun) do {                                 \
    SET_ERROR(fun);                                     \
    goto end;                                           \
  } while (0)
#define IND() do {                              \
    if (!lex->is_independent)                   \
      ERROR(ava_lex_error_need_separator);      \
  } while (0)
#define DIND() do {                             \
    if (!lex->is_independent)                   \
      SET_ERROR(ava_lex_error_need_separator);  \
  } while (0)

  lex->verbatim_depth = 0;
  lex->accum = AVA_EMPTY_STRING;

  /*
    General design notes:

    A logical token, at the point where an ACCEPT call occurs, extends from
    start to lex->p, inclusive.

    IGNORED strings like whitespace re-set the is_new_token flag so that the
    next iteration of the loop will reset start to the new position, then cause
    the loop to restart.

    In the ground state, ERRORs immediately terminate the loop.

    String and Verbatim literals are processed in multiple fragments,
    represented as some number of ACCUM calls (in a non-Ground state)
    terminated by an ACCEPT call that also transitions back to the Ground
    state. This is done both out of necessity (in the case of Verbatims, which
    are not regular) and for performance: The very common case of characters
    that need no processing at all can thus be handled by directly slicing the
    input string and appending it tho the accumulator.

    When in a non-Ground state, errors cannot simply terminate the loop, since
    another call to lex_lex() would attempt to parse whatever came after in the
    Ground state, which would likely produce a large number of incomprehensible
    errors. Instead, a deferred error is set with DERROR; this populates dst
    and sets the status flag to error, but then allows the loop to continue
    running. Only when the end of input is reached or ACCEPT is called will the
    actual error be returned.

    The handling of Verbatim literals deserves some extra description. The
    verbatim_depth field on the lexer maintains the current brace-nesting
    depth, which is updated whenever a \{ or \} token is encountered.
    Additionally, when the depth drops from 1 to 0, the state is switched back
    to Ground; additionally, the resulting depth on \} determines whether to
    ACCUM or ACCEPT the result.
   */

  while (lex->p.index < lex->strlen) {
    if (is_new_token) {
      unsigned prev = lex->prev_char & 0xFF;
      lex->is_independent = !!(
        is_independent_prefix[prev / 64] &
        (1LL << (prev % 64)));
      start = lex->p;
    }
    frag_start = lex->p;
    is_new_token = 0;

    /*!re2c

      WS = [ \t] ;
      NL = ("\r"? "\n" | "\r");
      LEGAL = [^\x00-\x08\x0b\x0c\x0e-\x1f\x7f] ;
      LEGALNL = LEGAL \ [\n\r] ;
      COM = ";" LEGALNL* ;
      BS = [\\] ;
      NS = LEGALNL \ [()\[\]{}\\;"`] \ WS ;
      SD = ["`] ;
      ESCT = ([\\"`'abefnrtv]|"x"[0-9a-fA-F]{2}) ;
      STRINGB = LEGALNL \ BS \ SD ;
      VERBB = LEGALNL \ BS ;
      ILLEGAL = [^] \ LEGAL ;
      ILLEGAL_STR = ((ILLEGAL \ [\x00])+ | "\x00") ;

      <Ground> NS+              { IND();  ACCEPT(ava_lex_bareword); }
      <Ground> WS+              {         IGNORE(); }
      <Ground> NL               {         ACCEPT(ava_lex_newline); }
      <Ground> BS WS* COM? NL   {         IGNORE(); }
      <Ground> BS WS+           { IND();  ACCEPT(ava_lex_newline); }
      <Ground> COM              {         IGNORE(); }
      <Ground> "("              {         ACCEPT(ava_lex_left_paren); }
      <Ground> ")"              {         ACCEPT(ava_lex_right_paren); }
      <Ground> "["              {         ACCEPT(ava_lex_left_bracket); }
      <Ground> "]"              {         ACCEPT(ava_lex_right_bracket); }
      <Ground> "{"              { IND();  ACCEPT(ava_lex_left_brace); }
      <Ground> "}"              {         ACCEPT(ava_lex_right_brace); }

      <Ground> '"' => String    { DIND(); ACCUM(ava_lex_string_init); }
      <Ground> '`' => String    {         ACCUM(ava_lex_string_init); }
      <String> SD => Ground     {         ACCEPT(ava_lex_string_finish); }
      <String> STRINGB+         {         ACCUM(ava_lex_accum_verb); }
      <String> NL               {         ACCUM(ava_lex_accum_nl); }
      <String> BS ESCT          {         ACCUM(ava_lex_accum_esc); }
      <String> BS [^]           {         DERROR(ava_lex_error_backslash_sequence); }
      <String> BS               {         DERROR(ava_lex_error_backslash_at_eof); }

      <Ground> BS "{" => Verb   { DIND(); ACCUM(ava_lex_verb_init); }
      <Verb>   BS "{"           { ++lex->verbatim_depth;
                                          ACCUM(ava_lex_accum_verb); }
      <Verb>   BS "}"           { if (--lex->verbatim_depth) {
                                          ACCUM(ava_lex_accum_verb);
                                  } else {
                                          cond = yycGround;
                                          ACCEPT(ava_lex_verb_finish);
                                  }
                                }
      <Verb> VERBB+             {         ACCUM(ava_lex_accum_verb); }
      <Verb> BS ";" ESCT        {         ACCUM(ava_lex_accum_esc2); }
      <Verb> BS ";" [^]         {         DERROR(ava_lex_error_backslash_sequence); }
      <Verb> BS ";"             {         DERROR(ava_lex_error_backslash_at_eof); }
      <Verb> BS                 {         ACCUM(ava_lex_accum_verb); }
      <Verb> NL                 {         ACCUM(ava_lex_accum_nl); }

      <Ground> BS [^]           {         ERROR(ava_lex_error_backslash_sequence); }
      <Ground> BS               {         ERROR(ava_lex_error_backslash_at_eof); }

      <String,Verb> ILLEGAL_STR {         DERROR(ava_lex_error_illegal_chars); }
      <Ground> ILLEGAL_STR      {         ERROR(ava_lex_error_illegal_chars); }
     */

    continue_loop:;
  }

  switch (cond) {
  case yycGround:
    if (status) goto end;
    return ava_lex_put_eof(dst, lex);

  case yycString:
    return ava_lex_put_error(dst, &start, &lex->p,
                             "unclosed string literal");

  case yycVerb:
    return ava_lex_put_error(dst, &start, &lex->p,
                             "unclosed verbatim literal "
                             "(nested %d levels at eof)",
                             lex->verbatim_depth);
  }

  end:
  return status;
}

static ava_lex_status ava_lex_bareword(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  return ava_lex_put_token(dst, ava_ltt_bareword, start, &lex->p, lex);
}

static ava_lex_status ava_lex_newline(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  return ava_lex_put_token_str(
    dst, ava_ltt_newline, AVA_ASCII9_STRING("\n"), start, &lex->p);
}

static ava_lex_status ava_lex_left_paren(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  ava_bool ws = lex->is_independent;

  return ava_lex_put_token(
    dst,
    ws? ava_ltt_begin_substitution : ava_ltt_begin_name_subscript,
    start, &lex->p, lex);
}

static ava_lex_status ava_lex_right_paren(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  return ava_lex_put_token(
    dst, ava_ltt_close_paren, start, &lex->p, lex);
}

static ava_lex_status ava_lex_left_bracket(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  ava_bool ws = lex->is_independent;

  return ava_lex_put_token(
    dst,
    ws? ava_ltt_begin_semiliteral : ava_ltt_begin_numeric_subscript,
    start, &lex->p, lex);
}

static ava_lex_status ava_lex_right_bracket(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  return ava_lex_put_token(
    dst, ava_ltt_close_bracket, start, &lex->p, lex);
}

static ava_lex_status ava_lex_left_brace(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  return ava_lex_put_token(
    dst, ava_ltt_begin_block, start, &lex->p, lex);
}

static ava_lex_status ava_lex_right_brace(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  return ava_lex_put_token(
    dst, ava_ltt_close_brace, start, &lex->p, lex);
}

static void ava_lex_string_init(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  lex->accum = AVA_EMPTY_STRING;
  lex->string_started_with = lex->prev_char & 0xFF;
}

static void ava_lex_accum_verb(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  lex->accum = ava_string_concat(
    lex->accum,
    ava_string_slice(lex->str, frag_start->index, lex->p.index));
}

static void ava_lex_accum_nl(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  lex->accum = ava_string_concat(
    lex->accum, AVA_ASCII9_STRING("\n"));
}

static const char ava_sc_escapes[128] = {
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0, 34,  0,  0,  0,  0, 39,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 92,  0,  0,  0,
  96,  7,  8,  0,  0, 27, 12,  0,  0,  0,  0,  0,  0,  0, 10,  0,
   0,  0, 13,  0,  9,  0, 11,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static void ava_lex_accum_esc_off(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex,
  unsigned skip
) {
  size_t len = lex->p.index - frag_start->index - skip;
  char ch;

  switch (len) {
  case 1:
    ch = ava_sc_escapes[lex->prev_char & 0x7F];
    break;

  case 3:
    ch = ava_hexes[(lex->prev_char >> 8) & 0x7F]*16 +
         ava_hexes[lex->prev_char & 0x7F];
    break;

  default: abort();
  }

  lex->accum = ava_string_concat(lex->accum, ava_string_of_char(ch));
}

static void ava_lex_accum_esc(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  return ava_lex_accum_esc_off(frag_start, lex, 1);
}

static void ava_lex_accum_esc2(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  return ava_lex_accum_esc_off(frag_start, lex, 2);
}

static ava_lex_status ava_lex_string_finish(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  char ending_char = lex->prev_char & 0xFF;
  ava_string result = lex->accum;
  ava_lex_token_type type;

  lex->accum = AVA_EMPTY_STRING;

  switch ((lex->string_started_with << 8) | ending_char) {
  case ('"' << 8) | '"': type = ava_ltt_astring; break;
  case ('`' << 8) | '"': type = ava_ltt_lstring; break;
  case ('"' << 8) | '`': type = ava_ltt_rstring; break;
  case ('`' << 8) | '`': type = ava_ltt_lrstring; break;
  default: abort();
  }

  return ava_lex_put_token_str(dst, type, result, start, &lex->p);
}

static ava_lex_status ava_lex_error_backslash_sequence(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  return ava_lex_put_error(dst, frag_start, &lex->p,
                           "invalid backslash sequence");
}

static ava_lex_status ava_lex_error_backslash_at_eof(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  return ava_lex_put_error(dst, frag_start, &lex->p,
                           "lone backslash at end of input");
}

static void ava_lex_verb_init(
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  lex->accum = AVA_EMPTY_STRING;
  lex->verbatim_depth = 1;
}

static ava_lex_status ava_lex_verb_finish(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  ava_string result = lex->accum;
  lex->accum = AVA_EMPTY_STRING;

  return ava_lex_put_token_str(dst, ava_ltt_verbatim, result, start, &lex->p);
}

static ava_lex_status ava_lex_error_illegal_chars(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  char chars[4], hex[20];
  unsigned i;
  size_t n;

  n = lex->p.index - start->index;
  ava_string_to_bytes(chars, lex->str,
                      start->index, start->index + (n > 4? 4 : n));
  for (i = 0; i < n && i < 4; ++i)
    snprintf(hex + 4*i, sizeof(hex) - 4*i, "\\x%02X", (unsigned)chars[i]);

  if (n > 4) {
    hex[16] = hex[17] = hex[18] = '.';
    hex[19] = 0;
  }

  return ava_lex_put_error(dst, start, &lex->p,
                           "encountered %d illegal character%s: %s",
                           (unsigned)n, n > 1? "s": "", hex);
}

static ava_lex_status ava_lex_error_need_separator(
  ava_lex_result* dst,
  const ava_lex_pos* start,
  const ava_lex_pos* frag_start,
  ava_lex_context* lex
) {
  return ava_lex_put_error(
    dst, start, &lex->p,
    "token must be separated from previous by whitespace");
}
