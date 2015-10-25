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
#error "Don't include avalanche/errors.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_ERRORS_H_
#define AVA_RUNTIME_ERRORS_H_

#include "defs.h"
#include "string.h"
#include "value.h"
#include "integer.h"

/**
 * @file
 * @brief Common definitions for working with error messages.
 *
 * All error messages emitted by the runtime are centrailised into this
 * interface (see generate-errors.tcl) to ensure consistency and to make rich
 * error messages easy to work with.
 *
 * Errors come in two flavours: string errors and compile errors. String errors
 * are used by components normally invoked at runtime, particularly for
 * exception messages. Compile errors are used for higher-level stages of
 * compilation, where a greater level of detail is desired.
 *
 * Every distinct error is assigned a unique 4-digit identifier prefixed by a
 * character indicating the high-level component or context, which is to be
 * stable across runtime versions. This is intended to simplify searching and
 * documentation.
 */

/**
 * Describes the location of an entity in source code.
 */
typedef struct {
  /**
   * The source filename.
   */
  ava_string filename;
  /**
   * The full source of the file containing this entity.
   *
   * This is absent if the source is unavailable.
   */
  ava_string source;
  /**
   * The byte offset of the line containing the start of this entity.
   */
  size_t line_offset;
  /**
   * The 1-based index of the first line of code on which the unit was
   * encountered.
   */
  unsigned start_line;
  /**
   * The 1-based index of the last line of code on which the unit was
   * encountered.
   *
   * This is equal to start_line for units that are localised to one line of
   * code.
   */
  unsigned end_line;
  /**
   * The 1-based indices of the start and end columns of the unit within the
   * first source line.
   */
  unsigned start_column, end_column;
} ava_compile_location;

/**
 * Error type used by various parts of the compilation process.
 */
typedef struct ava_compile_error_s {
  /**
   * The error message.
   */
  ava_string message;
  /**
   * The location of the error.
   */
  ava_compile_location location;

  TAILQ_ENTRY(ava_compile_error_s) next;
} ava_compile_error;

typedef TAILQ_HEAD(ava_compile_error_list_s, ava_compile_error_s)
ava_compile_error_list;

#include "gen-errors.h"

/**
 * Wraps the given string and location into an ava_compile_error.
 */
ava_compile_error* ava_compile_error_new(
  ava_string message, const ava_compile_location* location);

/**
 * Constructs an error with ava_compile_error_new() and adds it to the end of
 * the given error list.
 */
void ava_compile_error_add(
  ava_compile_error_list* dst,
  ava_string message, const ava_compile_location* location);

/**
 * Generates a string describing the given error list.
 *
 * The string is intended for human consumption, and the original error list
 * cannot be parsed back. Initially, errors include full detail, but become
 * terser as they go down. The goal is to maximise utility to the user; the
 * earliest errors are almost always the most important (eg, because they can
 * cause later errors), so it is important that they not get scrolled off the
 * user's terminal; however, we still want to fit as many errors as possible,
 * so being verbose with all of them is impractical.
 *
 * @param errors The list of errors to stringify.
 * @param max_lines The maximum number of lines to include in the output. It
 * does not make sense for this to be less than 2.
 * @param ansi_colour Whether ANSI escape sequences to colour the output should
 * be used in the string.
 * @return The generated string, suitable for printing to a terminal or file.
 */
ava_string ava_error_list_to_string(
  const ava_compile_error_list* errors,
  ava_uint max_lines,
  ava_bool ansi_colour);

#endif /* AVA_RUNTIME_ERRORS_H_ */
