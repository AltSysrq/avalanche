/*-
 * Copyright (c) 2015 Jason Lingle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

static ava_list_value ava_list_value_of_string(ava_string);

AVA_DEFINE_ACCELERATOR(ava_list_accelerator);

ava_value ava_list_imbue(ava_value value) {
  ava_list_value as_list;

  if (ava_query_accelerator(value, &ava_list_accelerator, NULL))
    return value;

  as_list = ava_list_value_of(value);
  return as_list.v->to_value(as_list);
}

ava_list_value ava_list_value_of(ava_value value) {
  const ava_list_iface* iface = ava_query_accelerator(
    value, &ava_list_accelerator, NULL);

  if (iface) {
    return (ava_list_value) {
      .v = iface,
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
      return accum;

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
}

ava_list_value ava_list_copy_of(ava_list_value list, size_t begin, size_t end) {
  if (end == begin)
    return ava_empty_list;

  if (end - begin <= AVA_ARRAY_LIST_THRESH)
    return ava_array_list_copy_of(list, begin, end);

  /* TODO: Use a btree list */
  return ava_array_list_copy_of(list, begin, end);
}

ava_string ava_list_escape(ava_string str) {
  ava_bool contains_special = ava_false;
  ava_bool quote_with_verbatim = ava_false;

  ava_string_iterator it;
  const char*restrict strdat;
  char tmpbuff[9];
  size_t strdat_len, i;

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

      if ((strdat[i] != '\n' &&
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
