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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/lex.h"
#include "avalanche/exception.h"
#include "avalanche/errors.h"
#include "avalanche/list.h"
#include "-array-list.h"
#include "-esba-list.h"

const ava_attribute_tag ava_list_trait_tag = {
  .name = "list"
};

static ava_list_value ava_list_value_of_string(
  ava_string str, ava_bool return_empty_on_fail);
static ava_bool ava_list_is_in_normal_list_form(
  ava_value val, ava_string stringified);

ava_list_value ava_list_value_of(ava_value value) {
  if (!ava_get_attribute(value, &ava_list_trait_tag))
    return ava_list_value_of_string(ava_to_string(value), ava_false);
  else
    return (ava_list_value) { value };
}

ava_fat_list_value ava_fat_list_value_of(ava_value value) {
  const ava_list_trait* trait = ava_get_attribute(
    value, &ava_list_trait_tag);

  if (!trait) {
    value = ava_list_value_of_string(ava_to_string(value), ava_false).v;
    trait = ava_get_attribute(value, &ava_list_trait_tag);
    assert(trait);
  }

  return (ava_fat_list_value) { .v = trait, .c = { value } };
}

static ava_list_value ava_list_value_of_string(
  ava_string str, ava_bool return_empty_on_fail
) {
  ava_lex_context* lex = ava_lex_new(str);
  ava_lex_result result;
  ava_list_value stack_stack[64];
  ava_list_value* accum, * heap_stack;
  size_t stack_h, stack_cap;

  ava_value buffer[64];
  unsigned buffer_ix = 0;

  accum = stack_stack;
  *accum = ava_empty_list();
  stack_h = 0;
  stack_cap = sizeof(stack_stack) / sizeof(stack_stack[0]);
  heap_stack = NULL;

#define FLUSH() do {                            \
    if (buffer_ix > 0) {                        \
      accum[stack_h] = ava_list_concat(         \
        accum[stack_h], ava_array_list_of_raw(  \
          buffer, buffer_ix));                  \
      buffer_ix = 0;                            \
    }                                           \
  } while (0)

  for (;;) {
    switch (ava_lex_lex(&result, lex)) {
    case ava_ls_ok:
      if (ava_lex_token_type_is_simple(result.type)) {
        buffer[buffer_ix++] = ava_value_of_string(result.str);
        if (buffer_ix == sizeof(buffer) / sizeof(buffer[0]))
          FLUSH();
      } else {
        switch (result.type) {
        case ava_ltt_newline: break; /* discard */
        case ava_ltt_begin_semiliteral:
          FLUSH();

          if (stack_h + 1 >= stack_cap) {
            /* Need to allocate more stack space */
            stack_cap *= 2;
            heap_stack = ava_alloc(sizeof(ava_value) * stack_cap);
            memcpy(heap_stack, accum, sizeof(ava_value) * (stack_h + 1));
            accum = heap_stack;
          }

          accum[++stack_h] = ava_empty_list();
          break;

        case ava_ltt_close_bracket:
          FLUSH();

          if (0 == stack_h) {
            if (return_empty_on_fail)
              return ava_empty_list();
            else
              ava_throw_str(&ava_format_exception,
                            ava_error_list_unbalanced_close_bracket(
                              result.index_start));
          }

          if (1 != ava_strlen(result.str)) {
            if (return_empty_on_fail)
              return ava_empty_list();
            else
              ava_throw_str(&ava_format_exception,
                            ava_error_list_tagged_close_bracket(
                              result.str, result.index_start));
          }

          --stack_h;
          accum[stack_h] = ava_list_append(accum[stack_h], accum[stack_h+1].v);
          break;

        default:
          if (return_empty_on_fail)
            return ava_empty_list();
          else
            ava_throw_str(&ava_format_exception,
                          ava_error_unexpected_token_parsing_list(
                            result.index_start, result.str));
          break;
        }
      }
      break;

    case ava_ls_end_of_input:
      goto done;

    case ava_ls_error: {
      if (return_empty_on_fail)
        return ava_empty_list();
      else
        ava_throw_str(&ava_format_exception,
                      ava_error_invalid_list_syntax(
                        result.index_start, result.str));
    } break;
    }
  }

  done:

  if (stack_h > 0) {
    if (return_empty_on_fail)
      return ava_empty_list();
    else
      ava_throw_str(&ava_format_exception,
                    ava_error_list_unbalanced_open_bracket(stack_h));
  }

  FLUSH();
  return accum[0];
}

ava_fat_list_value ava_list_copy_of(ava_fat_list_value list, size_t begin, size_t end) {
  if (end == begin)
    return ava_fat_list_value_of(ava_empty_list().v);

  if (end - begin <= AVA_ARRAY_LIST_THRESH)
    return ava_fat_list_value_of(ava_array_list_copy_of(
                                   list.c, begin, end).v);
  else
    return ava_fat_list_value_of(ava_esba_list_copy_of(
                                   list.c, begin, end).v);
}

ava_list_value ava_list_of_values(const ava_value*restrict values, size_t n) {
  if (0 == n)
    return ava_empty_list();
  else if (n <= AVA_ARRAY_LIST_THRESH)
    return ava_array_list_of_raw(values, n);
  else
    return ava_esba_list_of_raw(values, n);
}

ava_string ava_list_escape(ava_value val) {
  ava_bool contains_special = ava_false;
  ava_bool quote_with_verbatim = ava_false;
  ava_string str;

  const char*restrict strdat;
  ava_str_tmpbuff tmpbuff;
  size_t strlen, i;

  str = ava_to_string(val);
  strlen = ava_strlen(str);

  /* Special case: The empty string needs to be quoted, otherwise it will
   * disappear.
   */
  if (0 == strlen)
    return AVA_ASCII9_STRING("\"\"");

  /* Check for what needs to be escaped */
  strdat = ava_string_to_cstring_buff(tmpbuff, str);
  for (i = 0; i < strlen; ++i) {
    if ((unsigned)strdat[i] < (unsigned)' ') {
      contains_special = quote_with_verbatim = ava_true;
      goto do_escape;
    } else {
      switch (strdat[i]) {
      case '"':
      case '`':
      case '\\':
      case '\x7f':
        contains_special = quote_with_verbatim = ava_true;
        goto do_escape;

      case ';': case ' ':
      case '(': case '[': case '{':
      case '}': case ']': case ')':
        contains_special = ava_true;
        break;
      }
    }
  }

  do_escape:

  /* Return the original string if no escaping is required */
  if (!contains_special)
    return str;

  /* If the value is already in normalised list form, we can just put brackets
   * around it and expect it to work.
   */
  if (ava_list_is_in_normal_list_form(val, str))
    return ava_strcat(
      ava_strcat(AVA_ASCII9_STRING("["), str),
      AVA_ASCII9_STRING("]"));

  /* If surrounding it with double-quotes is sufficient to quote it, do that */
  if (!quote_with_verbatim)
    return ava_strcat(
      AVA_ASCII9_STRING("\""),
      ava_strcat(str, AVA_ASCII9_STRING("\"")));

  /* Escaping will be non-trivial, so use a verbatim */
  ava_string escaped = AVA_ASCII9_STRING("\\{");
  size_t clean_start = 0;
  ava_bool preceded_by_bs = ava_false;

  for (i = 0; i < strlen; ++i) {
    if (preceded_by_bs) {
      switch (strdat[i]) {
      case '{':
      case ';':
      case '}':
        /* Need to escape the preceding backslash */
        escaped = ava_strcat(
          escaped, ava_string_slice(
            str, clean_start, i-1));
        escaped = ava_strcat(
          escaped, AVA_ASCII9_STRING("\\;\\"));
        clean_start = i;
        break;
      }
    }

    if ((strdat[i] != '\n' && strdat[i] != '\t' &&
         (unsigned)strdat[i]  < (unsigned)' ') ||
        strdat[i] == '\x7f') {
      /* Illegal character; needs hex escape */
      char esc[5];
      unsigned val;

      esc[0] = '\\';
      esc[1] = ';';
      esc[2] = 'x';

      val = (strdat[i] >> 4) & 0xF;
      esc[3] = val + (val < 10? '0' : 'A' - 10);
      val = strdat[i] & 0xF;
      esc[4] = val + (val < 10? '0' : 'A' - 10);

      escaped = ava_strcat(
        escaped, ava_string_slice(str, clean_start, i));
      escaped = ava_strcat(
        escaped, ava_string_of_bytes(esc, sizeof(esc)));
      clean_start = i + 1;
    }

    preceded_by_bs = '\\' == strdat[i];
  }

  escaped = ava_strcat(
    escaped, ava_string_slice(
      str, clean_start, ava_strlen(str)));
  escaped = ava_strcat(
    escaped, AVA_ASCII9_STRING("\\}"));

  return escaped;
}

static ava_bool ava_list_is_in_normal_list_form(
  ava_value val, ava_string stringified
) {
  ava_bool ret;

  /* All lists are already in normal form */
  if (ava_get_attribute(val, &ava_list_trait_tag))
    return ava_true;

  ret = ava_string_equal(
    stringified, ava_to_string(
      /* Convert to a list, but if it can't be parsed, return the empty list so
       * we save the overhead of exceptions. (In such an error case, the
       * comparison is guaranteed false, since the stringification of the error
       * result is a valid list, whereas `stringified` is not.
       */
      ava_list_value_of_string(stringified, ava_true).v));

  return ret;
}

ava_list_value ava_list_copy_slice(ava_list_value list,
                                   size_t begin, size_t end) {
  return ava_list_copy_of(ava_fat_list_value_of(list.v), begin, end).c;
}

ava_list_value ava_list_copy_append(ava_list_value list_val,
                                    ava_value elt) {
  ava_fat_list_value list = ava_fat_list_value_of(list_val.v);
  list = ava_list_copy_of(list, 0, list.v->length(list.c));
  return list.v->append(list.c, elt);
}

ava_list_value ava_list_copy_concat(ava_list_value left_val,
                                    ava_list_value right) {
  ava_fat_list_value left = ava_fat_list_value_of(left_val.v);
  left = ava_list_copy_of(left, 0, left.v->length(left.c));
  return left.v->concat(left.c, right);
}

ava_list_value ava_list_copy_remove(ava_list_value list_val,
                                    size_t begin, size_t end) {
  ava_fat_list_value list = ava_fat_list_value_of(list_val.v);

  if (begin == end)
    return list_val;
  if (0 == begin && list.v->length(list.c) == end)
    return ava_empty_list();

  list = ava_list_copy_of(list, 0, list.v->length(list.c));
  return list.v->remove(list.c, begin, end);
}

ava_list_value ava_list_copy_set(ava_list_value list_val,
                                 size_t ix, ava_value val) {
  ava_fat_list_value list = ava_fat_list_value_of(list_val.v);
  list = ava_list_copy_of(list, 0, list.v->length(list.c));
  return list.v->set(list.c, ix, val);
}

ava_datum ava_list_string_chunk_iterator(ava_value list) {
  return (ava_datum) { .ulong = 0 };
}

ava_string ava_list_iterate_string_chunk(
  ava_datum*restrict it, ava_value list_val
) {
  ava_fat_list_value list = ava_fat_list_value_of(list_val);
  ava_value elt;
  ava_string eltstr;

  if (it->ulong >= list.v->length(list.c))
    return AVA_ABSENT_STRING;

  elt = list.v->index(list.c, it->ulong++);
  eltstr = ava_list_escape(elt);

  if (it->ulong > 1)
    return ava_strcat(AVA_ASCII9_STRING(" "), eltstr);
  else
    return eltstr;
}
