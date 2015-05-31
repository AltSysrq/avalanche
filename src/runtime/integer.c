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
#include "-integer-decimal.h"
#include "-integer-parse.h"

#define TO_HIDWORD(n) ((ava_ulong)(n) << 32)
#define TO_LODWORD(n) ((ava_ulong)(n) <<  0)
#ifdef WORDS_BIGENDIAN
#define TO_DWORD0(n) TO_HIDWORD(n)
#define TO_DWORD1(n) TO_LODWORD(n)
#else
#define TO_DWORD0(n) TO_LODWORD(n)
#define TO_DWORD1(n) TO_HIDWORD(n)
#endif

/**
 * Converts the given integer to its string representation, using the given
 * long array as its destination.
 *
 * The result is right-aligned within the array, but can otherwise be
 * reinterpreted as a char* correctly.
 *
 * Characters outside the byte range (24-return..24) have undefined contents.
 *
 * @return The number of characters in the string.
 */
static unsigned ava_integer_to_ulong_string(ava_ulong dst[3], ava_integer i);
static ava_string ava_integer_to_string(ava_value value) AVA_PURE;

const ava_value_trait ava_integer_type = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "integer",
  .to_string = ava_integer_to_string,
  .string_chunk_iterator = ava_singleton_string_chunk_iterator,
  .iterate_string_chunk = ava_iterate_singleton_string_chunk,
};

static unsigned ava_integer_to_ulong_string(ava_ulong dst[3], ava_integer i) {
  ava_bool negative = i < 0;
  ava_ulong u = negative? -i : i, rem;
  unsigned digits = 0, count, n;

  count = 1 + (u >= 100000000LL) + (u >= 10000000000000000LL);
  for (n = 0; n < count; ++n) {
    dst[2 - n] = 0;

    rem = u % 10000LL;
    dst[2 - n] |= TO_DWORD1(ava_integer_decimal_table[rem].value.i);
    digits = ava_integer_decimal_table[rem].digits;
    u /= 10000LL;

    rem = u % 10000LL;
    dst[2 - n] |= TO_DWORD0(ava_integer_decimal_table[rem].value.i);
    digits = u? ava_integer_decimal_table[rem].digits + 4 : digits;
    u /= 10000LL;
  }

  digits += sizeof(ava_ulong) * (n-1);

  if (!digits) digits = 1;
  if (negative) ((char*)dst)[24 - digits - 1] = '-';
  return digits + negative;
}

static ava_string ava_integer_to_string(ava_value value) {
  ava_ulong str[3];
  unsigned length;

  /* Special case for small, positive integers */
  if (ava_value_slong(value) >= 0 && ava_value_slong(value) < 10) {
    return (ava_string) {
      .ascii9 = 1ULL | ((ava_value_slong(value) + '0') << 57)
    };
  }

  length = ava_integer_to_ulong_string(str, ava_value_slong(value));
  return ava_string_of_bytes((char*)str + sizeof(str) - length, length);
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
