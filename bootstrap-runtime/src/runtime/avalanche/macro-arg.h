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
#error "Don't include avalanche/macro-arg.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_MACRO_ARG_H_
#define AVA_RUNTIME_MACRO_ARG_H_

/**
 * @file
 * @brief Preprocessor DSL for parsing macro arguments.
 *
 * The macros in this file are intended to be used within functions
 * implementing ava_macro_subst_f. They assume that all arguments are named the
 * same as in the definition of ava_macro_subst_f.
 *
 * The top-level macro is AVA_MACRO_ARG_PARSE, which encloses the others and
 * sets the basic context up. The provoker splits the statement into two
 * pieces, "left" and "right", each of which have a "begin" and "end" cursor. A
 * section is consumed if either cursor is NULL. (Anything which advances a
 * cursor NULLs it explicitly if it passes its counterpart.)
 *
 * Most macros only work with a "primary cursor", which is created with the
 * AVA_MACRO_ARG_FROM_{LEFT,RIGHT}_{BEGIN,END} macros.
 */

#define AVA_MACRO_ARG_LEFT_CONSUMED()                   \
  (!_ama_left_begin || !_ama_left_end)
#define AVA_MACRO_ARG_RIGHT_CONSUMED()                  \
  (!_ama_right_begin || !_ama_right_end)

/**
 * Usage:
 *
 * AVA_MACRO_ARG_PARSE { (* body *) }
 *
 * Provides the context for macro argument parsing. If body completes without
 * consuming all arguments, an appropriate error is emitted and the containing
 * function returns.
 */
#define AVA_MACRO_ARG_PARSE                                            \
  for (const ava_parse_unit                                            \
       * _ama_left_begin = TAILQ_FIRST(&statement->units),             \
       * _ama_left_end = TAILQ_PREV(provoker, ava_parse_unit_list_s,   \
                                    next),                             \
       * _ama_right_begin = TAILQ_NEXT(provoker, next),                \
       * _ama_right_end = TAILQ_LAST(&statement->units,                \
                                     ava_parse_unit_list_s),           \
       * _ama_apu_ctl = provoker;                                      \
       _ama_apu_ctl;                                                   \
       _ama_apu_ctl = NULL)                                            \
  for (unsigned _ama_status = 0; ;                                     \
       _ama_status = !AVA_MACRO_ARG_LEFT_CONSUMED()? 1 :               \
                     !AVA_MACRO_ARG_RIGHT_CONSUMED()? 2 : 3)           \
  if (3 == _ama_status) break;                                         \
  else if (1 == _ama_status) {                                         \
    return ava_macsub_error_result(                                    \
      context,                                                         \
      ava_error_extra_macro_args_left(                                 \
        &_ama_left_begin->location, self->full_name));                 \
  } else if (2 == _ama_status) {                                       \
    return ava_macsub_error_result(                                    \
      context,                                                         \
      ava_error_extra_macro_args_right(                                \
        &_ama_right_begin->location, self->full_name));                \
  } else /* user body */

#define AVA__MACRO_ARG_FROM_DIR(cursor, end, nullterm, closed, dir)     \
  for (const ava_parse_unit** _ama_cursor = &(cursor),                  \
         * _ama_cursor_end = (end),                                     \
         * _ama_terminal = (                                            \
           !_ama_cursor_end? nullterm :                                 \
           (dir) > 0? TAILQ_NEXT(_ama_cursor_end,next) :                \
           TAILQ_PREV(_ama_cursor_end, ava_parse_unit_list_s, next));   \
       _ama_cursor &&                                                   \
         (((closed) && (*_ama_cursor = NULL)) || 1);                    \
       _ama_cursor = NULL)                                              \
    for (signed _ama_direction = (dir); _ama_direction;                 \
         _ama_direction = 0)

/**
 * Usage:
 *   AVA_MACRO_ARG_FROM_LEFT_BEGIN { (* body *) }
 *
 * A primary cursor is established starting from the left beginning, and moving
 * forward to the left end.
 */
#define AVA_MACRO_ARG_FROM_LEFT_BEGIN                                   \
  AVA__MACRO_ARG_FROM_DIR(                                              \
    _ama_left_begin, _ama_left_end, provoker,                           \
    (!_ama_left_end || provoker == _ama_left_begin ||                   \
     _ama_left_begin == TAILQ_NEXT(_ama_left_end, next)),               \
    +1)
/**
 * Like AVA_MACRO_ARG_FROM_LEFT_BEGIN, but starts from the left end moving
 * backwards.
 */
#define AVA_MACRO_ARG_FROM_LEFT_END                                     \
  AVA__MACRO_ARG_FROM_DIR(                                              \
    _ama_left_end, _ama_left_begin, NULL,                               \
    (!_ama_left_end || provoker == _ama_left_begin ||                   \
     TAILQ_NEXT(_ama_left_end, next) == _ama_left_begin),               \
    -1)
/**
 * Like AVA_MACRO_ARG_FROM_LEFT_BEGIN, but starts from the right beginning
 * moving forwards.
 */
#define AVA_MACRO_ARG_FROM_RIGHT_BEGIN                                  \
  AVA__MACRO_ARG_FROM_DIR(                                              \
    _ama_right_begin, _ama_right_end, NULL,                             \
    (!_ama_right_end || provoker == _ama_right_end ||                   \
     TAILQ_NEXT(_ama_right_end, next) == _ama_right_begin),             \
    +1)
/**
 * Like AVA_MACRO_ARG_FROM_LEFT_BEGIN, but starts from the right end moving
 * backwards.
 */
#define AVA_MACRO_ARG_FROM_RIGHT_END                                    \
  AVA__MACRO_ARG_FROM_DIR(                                              \
    _ama_right_end, _ama_right_begin, provoker,                         \
    (!_ama_right_end || provoker == _ama_right_end ||                   \
     TAILQ_NEXT(_ama_right_end, next) == _ama_right_begin),             \
    -1)

/**
 * Indicates whether the current primary cursor points to a parse unit.
 */
#define AVA_MACRO_ARG_HAS_ARG() (*_ama_cursor && *_ama_cursor != _ama_terminal)

/**
 * Usage: AVA_MACRO_ARG_REQUIRE("name");
 *
 * Ensures that there is a parse unit at the primary cursor.
 *
 * If there is none, an error is emitted using the given name to identify the
 * argument and the function returns.
 */
#define AVA_MACRO_ARG_REQUIRE(name) do {                                \
    if (!AVA_MACRO_ARG_HAS_ARG()) {                                     \
      AVA_STATIC_STRING(_ama_msg_name, name);                           \
      return ava_macsub_error_result(                                   \
        context,                                                        \
        ava_error_macro_arg_missing(                                    \
          &(_ama_cursor_end? _ama_cursor_end : provoker)->location,     \
          self->full_name, _ama_msg_name));                             \
    }                                                                   \
  } while (0)

/**
 * Usage: AVA_MACRO_ARG_CURRENT_UNIT(var, "name");
 *
 * var (a const ava_parse_unit*) is set to the parse unit at the current
 * primary cursor. If there is no such unit, an error is emitted and the
 * function returns.
 */
#define AVA_MACRO_ARG_CURRENT_UNIT(dst, name) do {      \
    AVA_MACRO_ARG_REQUIRE(name);                        \
    (dst) = *_ama_cursor;                               \
  } while (0)

/**
 * Consumes the parse unit at the primary cursor, moving onto the next.
 *
 * It must already be known that a current unit exists.
 *
 * This is an expression so that it can be used in the update section of a for
 * loop.
 */
#define AVA_MACRO_ARG_CONSUME()                         \
  ((*_ama_cursor = (                                    \
      *_ama_cursor == _ama_cursor_end? NULL :           \
      _ama_direction < 0? TAILQ_PREV(                   \
        *_ama_cursor, ava_parse_unit_list_s, next) :    \
      TAILQ_NEXT(*_ama_cursor, next))))

/**
 * Usage: AVA_MACRO_ARG_UNIT(var, "name");
 *
 * A combination of AVA_MACRO_ARG_CURRENT_UNIT() and AVA_MACRO_ARG_CONSUME().
 */
#define AVA_MACRO_ARG_UNIT(dst, name) do {      \
    AVA_MACRO_ARG_CURRENT_UNIT(dst, name);      \
    AVA_MACRO_ARG_CONSUME();                    \
  } while (0)

/**
 * Usage: AVA_MACRO_ARG_BAREWORD(var, "name")
 *
 * var (an ava_string) is set to the string content of the current parse unit,
 * which must be a bareword. If there is no current unit, or it is not a
 * bareword, an error is emitted and the function returns.
 */
#define AVA_MACRO_ARG_BAREWORD(dst, name) do {          \
    AVA_STATIC_STRING(_arg_name, name);                 \
    AVA_MACRO_ARG_REQUIRE(name);                        \
    switch ((*_ama_cursor)->type) {                     \
    case ava_put_bareword:                              \
      dst = (*_ama_cursor)->v.string;                   \
      break;                                            \
    default:                                            \
      return ava_macsub_error_result(                   \
        context,                                        \
        ava_error_macro_arg_must_be_bareword(           \
          &(*_ama_cursor)->location, _arg_name));       \
    }                                                   \
    AVA_MACRO_ARG_CONSUME();                            \
  } while (0)

/**
 * Usage: AVA_MACRO_ARG_STRINGOID_T(tvar, svar, "name")
 *
 * svar (an ava_string) is set to the string content of the current parse unit,
 * which must be a bareword, A-String, or Verbatim, and tvar is set to its
 * type. If there is no current unit, or it is not a stringoid, an error is
 * emitted and the function returns.
 */
#define AVA_MACRO_ARG_STRINGOID_T(tdst, sdst, name) do {        \
    AVA_STATIC_STRING(_arg_name, name);                         \
    AVA_MACRO_ARG_REQUIRE(name);                                \
    switch ((*_ama_cursor)->type) {                             \
    case ava_put_bareword:                                      \
    case ava_put_astring:                                       \
    case ava_put_verbatim:                                      \
      sdst = (*_ama_cursor)->v.string;                          \
      tdst = (*_ama_cursor)->type;                              \
      break;                                                    \
    default:                                                    \
      return ava_macsub_error_result(                           \
        context,                                                \
        ava_error_macro_arg_must_be_stringoid(                  \
          &(*_ama_cursor)->location, _arg_name));               \
    }                                                           \
    AVA_MACRO_ARG_CONSUME();                                    \
  } while (0)

/**
 * Like AVA_MACRO_ARG_STRINGOID_T, but no type is returned.
 */
#define AVA_MACRO_ARG_STRINGOID(dst, name) do {         \
    ava_parse_unit_type _tdst AVA_UNUSED;               \
    AVA_MACRO_ARG_STRINGOID_T(_tdst, dst, name);        \
  } while (0)

/**
 * Usage: AVA_MACRO_ARG_BLOCK(var, "name")
 *
 * var (a const ava_parse_unit*) is set to the current parse unit, which must
 * be a block. If there is no current unit, or it is not a block, an error
 * is emitted and the function returns.
 */
#define AVA_MACRO_ARG_BLOCK(dst, name) do {             \
    AVA_STATIC_STRING(_arg_name, name);                 \
    AVA_MACRO_ARG_REQUIRE(name);                        \
    switch ((*_ama_cursor)->type) {                     \
    case ava_put_block:                                 \
      dst = (*_ama_cursor);                             \
      break;                                            \
    default:                                            \
      return ava_macsub_error_result(                   \
        context,                                        \
        ava_error_macro_arg_must_be_block(              \
          &(*_ama_cursor)->location, _arg_name));       \
    }                                                   \
    AVA_MACRO_ARG_CONSUME();                            \
  } while (0)

/**
 * Internal function.
 *
 * @see AVA_MACRO_ARG_LITERAL
 */
ava_bool ava_macro_arg_literal(
  ava_value* dst,
  const ava_parse_unit** error_unit,
  const ava_parse_unit* unit);

/**
 * Usage: AVA_MACRO_ARG_LITERAL(var, "name")
 *
 * var (an ava_value) is set to the value of the literal in the current parse
 * unit. If there is no current unit or it is not a literal, an error is
 * emitted and the function returns.
 *
 * A literal is one of:
 * - A bareword, A-string, or verbatim
 * - A semiliteral containing only literals
 */
#define AVA_MACRO_ARG_LITERAL(dst, name) do {           \
    AVA_STATIC_STRING(_arg_name, name);                 \
    const ava_parse_unit* _ama_error_unit;              \
    AVA_MACRO_ARG_REQUIRE(name);                        \
    if (!ava_macro_arg_literal(                         \
          &(dst), &_ama_error_unit, *_ama_cursor)) {    \
      return ava_macsub_error_result(                   \
        context,                                        \
        ava_error_macro_arg_must_be_literal(            \
          &_ama_error_unit->location, _arg_name));      \
    }                                                   \
    AVA_MACRO_ARG_CONSUME();                            \
  } while (0)

/**
 * Usage: AVA_MACRO_ARG_FOR_REST { (* body *) }
 *
 * body is executed in a loop until the current section is fully consumed. The
 * macro itself is a loop, so `break` and `continue` can be used with it. The
 * loop itself does not consume arguments.
 */
#define AVA_MACRO_ARG_FOR_REST while (AVA_MACRO_ARG_HAS_ARG())

#endif /* AVA_RUNTIME_MACRO_ARG_H */
