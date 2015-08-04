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
#ifndef PARSER_UTILS_H_
#define PARSER_UTILS_H_

static void dump_errors(const ava_compile_error_list* errors);
static void position_caret(char* dst, size_t limit,
                           unsigned start, unsigned end);
static const char* extract_source_line(const ava_compile_location* location);

static void dump_errors(const ava_compile_error_list* errors) {
  ava_compile_error* error;
  char caret_string[256];

  TAILQ_FOREACH(error, errors, next) {
    position_caret(caret_string, sizeof(caret_string),
                   error->location.start_column, error->location.end_column);
    warnx("%s: %d:%d -- %d:%d: error: %s\n%s\n%s",
          ava_string_to_cstring(error->location.filename),
          error->location.start_line, error->location.start_column,
          error->location.end_line, error->location.end_column,
          ava_string_to_cstring(error->message),
          extract_source_line(&error->location),
          caret_string);
  }
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

  ck_assert_int_lt(i, limit);
}

static const char* extract_source_line(const ava_compile_location* loc) {
  const char* str = ava_string_to_cstring(loc->source);
  char* result;
  size_t begin = loc->line_offset, strlen = ava_string_length(loc->source), end;

  for (end = begin; end < strlen && '\n' != str[end]; ++end);

  result = ava_alloc_atomic(1 + end - begin);
  memcpy(result, str + begin, end - begin);
  result[end - begin] = 0;
  return result;
}

#endif /* PARSER_UTILS_H_ */
