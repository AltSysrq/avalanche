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
#include <stdio.h>

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/lex.h"
#include "avalanche/exception.h"
#include "avalanche/list.h"
#include "-array-list.h"
#include "-esba-list.h"

const ava_attribute_tag ava_list_trait_tag = {
  .name = "list"
};

static ava_list_value ava_list_value_of_string(ava_string);

ava_list_value ava_list_value_of(ava_value value) {
  const ava_list_trait* trait = ava_get_attribute(
    value, &ava_list_trait_tag);

  if (trait) {
    return (ava_list_value) {
      .v = trait,
      .r1 = value.r1,
      .r2 = value.r2
    };
  }

  return ava_list_value_of_string(ava_to_string(value));
}

static ava_list_value ava_list_value_of_string(ava_string str) {
  ava_lex_context* lex = ava_lex_new(str);
  ava_lex_result result;
  ava_list_value accum = ava_empty_list;

  ava_value buffer[64];
  unsigned buffer_ix = 0;

  for (;;) {
    switch (ava_lex_lex(&result, lex)) {
    case ava_ls_ok:
      if (ava_lex_token_type_is_simple(result.type)) {
        buffer[buffer_ix++] = ava_value_of_string(result.str);
        if (buffer_ix == sizeof(buffer) / sizeof(buffer[0])) {
          accum = accum.v->concat(
            accum, ava_array_list_of_raw(buffer, buffer_ix));
          buffer_ix = 0;
        }
      } else {
        if (ava_ltt_newline != result.type) {
          char message[256];
          snprintf(message, sizeof(message),
                   "unexpected token parsing list at index %d: ",
                   (int)result.index_start);
          ava_throw(&ava_format_exception,
                    ava_value_of_string(
                      ava_string_concat(
                        ava_string_of_cstring(message),
                        result.str)),
                    NULL);
        }
      }
      break;

    case ava_ls_end_of_input:
      goto done;

    case ava_ls_error: {
      char message[256];
      snprintf(message, sizeof(message),
               "invalid syntax parsing list at index %d: ",
               (int)result.index_start);
      ava_throw(&ava_format_exception,
                ava_value_of_string(
                  ava_string_concat(
                    ava_string_of_cstring(message),
                    result.str)),
                NULL);
    } break;
    }
  }

  done:
  if (buffer_ix)
    accum = accum.v->concat(
      accum, ava_array_list_of_raw(buffer, buffer_ix));
  return accum;
}

ava_list_value ava_list_copy_of(ava_list_value list, size_t begin, size_t end) {
  if (end == begin)
    return ava_empty_list;

  if (end - begin <= AVA_ARRAY_LIST_THRESH)
    return ava_array_list_copy_of(list, begin, end);
  else
    return ava_esba_list_copy_of(list, begin, end);
}

ava_list_value ava_list_of_values(const ava_value*restrict values, size_t n) {
  if (0 == n)
    return ava_empty_list;
  else if (n <= AVA_ARRAY_LIST_THRESH)
    return ava_array_list_of_raw(values, n);
  else
    return ava_esba_list_of_raw(values, n);
}

ava_string ava_list_escape(ava_string str) {
  ava_bool contains_special = ava_false;
  ava_bool quote_with_verbatim = ava_false;

  ava_string_iterator it;
  const char*restrict strdat;
  char tmpbuff[9];
  size_t strdat_len, i;

  /* Special case: The empty string needs to be quoted, otherwise it will
   * disappear.
   */
  if (0 == ava_string_length(str))
    return AVA_ASCII9_STRING("\"\"");

  /* Check for what needs to be escaped */
  ava_string_iterator_place(&it, str, 0);
  while (ava_string_iterator_valid(&it)) {
    strdat_len = ava_string_iterator_access(
      &strdat, &it, tmpbuff);

    for (i = 0; i < strdat_len; ++i) {
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

    ava_string_iterator_move(&it, strdat_len);
  }

  do_escape:

  /* Return the original string if no escaping is required */
  if (!contains_special)
    return str;

  /* If surrounding it with double-quotes is sufficient to quote it, do that */
  if (!quote_with_verbatim)
    return ava_string_concat(
      AVA_ASCII9_STRING("\""),
      ava_string_concat(str, AVA_ASCII9_STRING("\"")));

  /* Escaping will be non-trivial, so use a verbatim */
  ava_string escaped = AVA_ASCII9_STRING("\\{");
  size_t clean_start = 0;
  ava_bool preceded_by_bs = ava_false;

  ava_string_iterator_place(&it, str, 0);
  while (ava_string_iterator_valid(&it)) {
    strdat_len = ava_string_iterator_access(
      &strdat, &it, tmpbuff);

    for (i = 0; i < strdat_len; ++i) {
      if (preceded_by_bs) {
        switch (strdat[i]) {
          /* TODO: It'd be best to only escape \{ and \} if they would be
           * unbalanced otherwise.
           */
        case '{':
        case ';':
        case '}':
          /* Need to escape the preceding backslash */
          escaped = ava_string_concat(
            escaped, ava_string_slice(
              str, clean_start,
              ava_string_iterator_index(&it) + i - 1));
          escaped = ava_string_concat(
            escaped, AVA_ASCII9_STRING("\\;\\"));
          clean_start = ava_string_iterator_index(&it) + i;
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

        escaped = ava_string_concat(
          escaped, ava_string_slice(
            str, clean_start,
            ava_string_iterator_index(&it) + i));
        escaped = ava_string_concat(
          escaped, ava_string_of_bytes(esc, sizeof(esc)));
        clean_start = ava_string_iterator_index(&it) + i + 1;
      }

      preceded_by_bs = '\\' == strdat[i];
    }

    ava_string_iterator_move(&it, strdat_len);
  }

  escaped = ava_string_concat(
    escaped, ava_string_slice(
      str, clean_start, ava_string_length(str)));
  escaped = ava_string_concat(
    escaped, AVA_ASCII9_STRING("\\}"));

  return escaped;
}

ava_list_value ava_list_copy_slice(
  ava_list_value list, size_t begin, size_t end
) {
  return ava_list_copy_of(list, begin, end);
}

ava_list_value ava_list_copy_append(ava_list_value list, ava_value elt) {
  list = ava_list_copy_of(list, 0, list.v->length(list));
  return list.v->append(list, elt);
}

ava_list_value ava_list_copy_concat(ava_list_value left, ava_list_value right) {
  left = ava_list_copy_of(left, 0, left.v->length(left));
  return left.v->concat(left, right);
}

ava_list_value ava_list_copy_delete(ava_list_value list,
                                    size_t begin, size_t end) {
  if (begin == end)
    return list;
  if (0 == begin && list.v->length(list) == end)
    return ava_empty_list;

  list = ava_list_copy_of(list, 0, list.v->length(list));
  return list.v->delete(list, begin, end);
}

ava_list_value ava_list_copy_set(ava_list_value list,
                                 size_t ix, ava_value val) {
  list = ava_list_copy_of(list, 0, list.v->length(list));
  return list.v->set(list, ix, val);
}

size_t ava_list_ix_iterator_size(ava_list_value list) {
  return sizeof(size_t);
}

void ava_list_ix_iterator_place(
  ava_list_value list, void*restrict iterator, size_t ix
) {
  *(size_t*restrict)iterator = ix;
}

ava_value ava_list_ix_iterator_get(
  ava_list_value list, const void*restrict iterator
) {
  return list.v->index(list, *(const size_t*restrict)iterator);
}

void ava_list_ix_iterator_move(
  ava_list_value list, void*restrict iterator, ssize_t off
) {
  *(size_t*restrict)iterator += off;
}

size_t ava_list_ix_iterator_index(
  ava_list_value el, const void*restrict iterator
) {
  return *(const size_t*restrict)iterator;
}

ava_datum ava_list_string_chunk_iterator(ava_value list) {
  return (ava_datum) { .ulong = 0 };
}

ava_string ava_list_iterate_string_chunk(
  ava_datum*restrict it, ava_value list_val
) {
  ava_list_value list = ava_list_value_of(list_val);
  ava_string elt;

  if (it->ulong >= list.v->length(list))
    return AVA_ABSENT_STRING;

  elt = ava_to_string(list.v->index(list, it->ulong++));
  elt = ava_list_escape(elt);

  if (it->ulong > 1)
    return ava_string_concat(AVA_ASCII9_STRING(" "), elt);
  else
    return elt;
}
