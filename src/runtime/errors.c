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

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/integer.h"
#include "avalanche/errors.h"

static void position_caret(char* dst, size_t limit,
                           unsigned begin, unsigned end);
static ava_string extract_source_line(
  const ava_compile_location* location);
static ava_bool is_printable(ava_string string);

ava_compile_error* ava_compile_error_new(
  ava_string message, const ava_compile_location* location
) {
  ava_compile_error* error = AVA_NEW(ava_compile_error);
  error->message = message;
  error->location = *location;
  return error;
}

static void position_caret(char* dst, size_t limit,
                           unsigned begin, unsigned end) {
  unsigned i;

  if (begin > 0) --begin;
  if (end > 0) --end;

  if (begin >= limit-2) {
    dst[0] = 0;
    return;
  }

  if (end > limit-1)
    end = limit-1;

  i = 0;
  while (i < begin) dst[i++] = ' ';
  dst[i++] = '^';
  while (i < end) dst[i++] = '~';
  dst[i] = 0;
}

static ava_string extract_source_line(const ava_compile_location* loc) {
  const char* str = ava_string_to_cstring(loc->source);
  size_t begin = loc->line_offset, strlen = ava_string_length(loc->source), end;

  for (end = begin; end < strlen && '\n' != str[end]; ++end);

  return ava_string_of_bytes(str + begin, end - begin);
}

static ava_bool is_printable(ava_string str) {
  char tmp[AVA_STR_TMPSZ];
  const char* raw = ava_string_to_cstring_buff(tmp, str);
  char ch;
  size_t strlen = ava_string_length(str), i;

  for (i = 0; i < strlen; ++i) {
    ch = raw[i];
    if ((ch >= '\0' && ch < '\t') ||
        (ch > '\t' && ch < ' ') ||
        ch == '\x7F')
      return ava_false;
  }

  return ava_true;
}

ava_string ava_error_list_to_string(
  const ava_compile_error_list* errors,
  ava_uint max_lines,
  ava_bool ansi_colour
) {
  ava_string accum, error_header, source_line;
  ava_uint num_lines = 1; /* 1 to reserve summary */
  ava_uint errors_shown, total_errors;
  const ava_compile_error* error;
  char caret[81];

  accum = AVA_EMPTY_STRING;
  if (ansi_colour) {
    AVA_STATIC_STRING(error_with_colour, "\033[1;31m[ERROR]\033[0m ");
    error_header = error_with_colour;
  } else {
    error_header = AVA_ASCII9_STRING("[ERROR] ");
  }

#define CAT(after) (accum = ava_string_concat(accum, (after)))
#define CATA(after) CAT(AVA_ASCII9_STRING(after))
#define CATI(after) CAT(ava_to_string(ava_value_of_integer(after)))

  error = TAILQ_FIRST(errors);
  errors_shown = 0;
  for (; num_lines < max_lines && error; error = TAILQ_NEXT(error, next)) {
    /* TODO: Conditional printing */

    CAT(error_header);
    CAT(error->location.filename);
    CATA(":");
    CATI(error->location.start_line);
    CATA(":");
    CATI(error->location.start_column);
    CATA(": ");
    CAT(error->message);
    CATA("\n");
    ++num_lines;
    ++errors_shown;

    if (num_lines + 3 <= max_lines &&
        /* Only the first 1/3 of the screen has verbose errors */
        num_lines * 3 < max_lines &&
        error->location.start_column < sizeof(caret)-1) {
      source_line = extract_source_line(&error->location);
      if (is_printable(source_line)) {
        CAT(source_line);
        CATA("\n");
        position_caret(caret, sizeof(caret),
                       error->location.start_column,
                       error->location.start_line == error->location.end_line?
                       error->location.end_column :
                       ava_string_length(source_line));
        CAT(ava_string_of_cstring(caret));
        CATA("\n\n");
        num_lines += 3;
      }
    }
  }

  for (total_errors = errors_shown; error; error = TAILQ_NEXT(error, next))
    ++total_errors;

  if (total_errors) {
    AVA_STATIC_STRING(summary1, " error(s) total, ");
    AVA_STATIC_STRING(summary2, " error(s) shown\n");

    CATI(total_errors);
    CAT(summary1);
    CATI(errors_shown);
    CAT(summary2);
  }

  return accum;
}
