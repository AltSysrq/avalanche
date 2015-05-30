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

#define AVA__INTERNAL_INCLUDE 1
#define AVA__IN_INTEGER_C

#include "avalanche/defs.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/exception.h"
#include "avalanche/integer.h"
#include "-hexes.h"
#include "-integer-fast-dec.h"
#include "-integer-parse.h"

static ava_string ava_integer_to_string(ava_value value) AVA_PURE;

const ava_value_trait ava_integer_type = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "integer",
  .to_string = ava_integer_to_string,
  .string_chunk_iterator = ava_singleton_string_chunk_iterator,
  .iterate_string_chunk = ava_iterate_singleton_string_chunk,
};

static ava_string ava_integer_to_string(ava_value value) {
  char str[20];
  ava_bool negative = (ava_value_slong(value) < 0);
  ava_ulong i = negative? -ava_value_slong(value) : ava_value_slong(value);
  unsigned ix = sizeof(str);

  /* Need a special case for 0 anyway, so handle all single-digit integers the
   * same way.
   */
  if (ava_value_slong(value) >= 0 && ava_value_slong(value) < 10) {
    return (ava_string) {
      .ascii9 = 1ULL | ((ava_value_slong(value) + '0') << 57)
    };
  }

  while (i) {
    str[--ix] = '0' + (i % 10);
    i /= 10;
  }

  if (negative)
    str[--ix] = '-';

  return ava_string_of_bytes(str + ix, sizeof(str) - ix);
}

ava_integer ava_integer_of_noninteger_value(
  ava_value value, ava_integer dfault
) {
  AVA_STATIC_STRING(not_an_integer, "not an integer: ");
  AVA_STATIC_STRING(
    too_long, "string too long to be interpreted as integer: ");
  AVA_STATIC_STRING(
    trailing_garbage, "trailing garbage at end of integer: ");

  ava_string str = ava_to_string(value);
  ava_string error_message;
  const char*restrict strdata, *restrict cursor, *restrict marker = NULL;
  const char*restrict tok;
  char tmpbuff[AVA_STR_TMPSZ];
  size_t strlen;

  strlen = ava_string_length(str);

  /* Inlined case only checks for ASCII9 empty string. */
  if (0 == strlen)
    return dfault;

  if (str.ascii9 & 1) {
    ava_integer fast_result =
      ava_integer_parse_dec_fast(str.ascii9, strlen);
    if (fast_result != PARSE_DEC_FAST_ERROR)
      return fast_result;
  }

  if (strlen > MAX_INTEGER_LENGTH) {
    error_message = too_long;
    goto error;
  }

  strdata = ava_string_to_cstring_buff(tmpbuff, str);

  strdata = cursor = strdata;

#define YYCTYPE unsigned char
#define YYPEEK() (cursor < strdata + strlen? *cursor : 0)
#define YYSKIP() (++cursor)
#define YYBACKUP() (marker = cursor)
#define YYRESTORE() (cursor = marker)
#define YYLESSTHAN(n) (strdata + strlen - cursor < (n))
#define YYFILL(n) do {} while (0)
#define END() do {                                                  \
    if (cursor != strdata + strlen) {                               \
      error_message = trailing_garbage;                             \
      goto error;                                                   \
    }                                                               \
  } while (0)

  while (cursor < strdata + strlen) {
    tok = cursor;
    /*!rules:re2c

      WS = [ \t\r\n] ;
      SIGN = [+-]? ;
      BIN = '0'? 'b' ;
      OCT = '0'? 'o' ;
      HEX = '0'? 'x' ;
      TRUTHY = 'on' | 'true' | 'yes' ;
      FALSEY = 'off' | 'false' | 'no' | 'null' ;
      BIN_LITERAL = SIGN BIN [01]+  ;
      OCT_LITERAL = SIGN OCT [0-7]+ ;
      DEC_LITERAL = SIGN     [0-9]+ ;
      HEX_LITERAL = SIGN HEX [0-9a-fA-F]+ ;

      [^] \ [^]                 { continue; }
    */
    /* ^^^ [^]\[^]: XXX re2c doesn't think the rules block exists unless we
     * define a rule with executable code, so define a rule that never matches.
     */

    /*!use:re2c

      WS+                       { continue; }
      TRUTHY WS*                { END(); return 1; }
      FALSEY WS*                { END(); return 0; }
      BIN_LITERAL WS*           { END(); return ava_integer_parse_bin(tok, cursor); }
      OCT_LITERAL WS*           { END(); return ava_integer_parse_oct(tok, cursor); }
      HEX_LITERAL WS*           { END(); return ava_integer_parse_hex(tok, cursor); }
      DEC_LITERAL WS*           { END(); return ava_integer_parse_dec(tok, cursor); }
      *                         { error_message = not_an_integer;
                                  goto error; }
     */
  }
#undef YYCTYPE
#undef YYPEEK
#undef YYSKIP
#undef YYBACKUP
#undef YYRESTORE
#undef YYLESSTHAN
#undef YYFILL
#undef END

  /* String contained nothing but whitespace */
  return dfault;

  error:
  ava_throw(&ava_format_exception,
            ava_value_of_string(
              ava_string_concat(error_message, str)),
            NULL);
  /* unreachable */
}

ava_bool ava_string_is_integer(ava_string str) {
  const char*restrict strdata, *restrict cursor, * restrict marker = NULL;
  char tmpbuff[AVA_STR_TMPSZ];
  size_t strlen;

  strlen = ava_string_length(str);

  if (strlen > MAX_INTEGER_LENGTH)
    return 0;

  if (str.ascii9 & 1 &&
      PARSE_DEC_FAST_ERROR != ava_integer_parse_dec_fast(str.ascii9, strlen))
    return 1;

  strdata = cursor = ava_string_to_cstring_buff(tmpbuff, str);

#define YYCTYPE unsigned char
#define YYPEEK() (cursor < strdata + strlen? *cursor : 0)
#define YYSKIP() (++cursor)
#define YYBACKUP() (marker = cursor)
#define YYRESTORE() (cursor = marker)
#define YYLESSTHAN(n) (strdata + strlen - cursor < (n))
#define YYFILL(n) do {} while (0)
  while (cursor < strdata + strlen) {
    /*!use:re2c

      WS* TRUTHY WS*            { break; }
      WS* FALSEY WS*            { break; }
      WS* BIN_LITERAL WS*       { break; }
      WS* OCT_LITERAL WS*       { break; }
      WS* DEC_LITERAL WS*       { break; }
      WS* HEX_LITERAL WS*       { break; }
      WS*                       { break; }
      *                         { break; }

     */
  }

  return cursor == strdata + strlen;
}
