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

static ava_string ava_integer_to_string(ava_value value) AVA_PURE;
static size_t ava_integer_value_weight(ava_value value) AVA_CONSTFUN;
static const char* consume_sign_and_radix(
  const char*restrict ch, const char*restrict end,
  ava_bool* negative, char radixl, char radixu);
static void throw_overflow(const char*restrict begin,
                           const char*restrict end);
static ava_integer ava_integer_parse_bin(const char*restrict tok,
                                         const char*restrict end);
static ava_integer ava_integer_parse_oct(const char*restrict tok,
                                         const char*restrict end);
static ava_integer ava_integer_parse_dec(const char*restrict tok,
                                         const char*restrict end);
static ava_integer ava_integer_parse_hex(const char*restrict tok,
                                         const char*restrict end);

const ava_value_trait ava_integer_type = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "integer",
  .to_string = ava_integer_to_string,
  .string_chunk_iterator = ava_singleton_string_chunk_iterator,
  .iterate_string_chunk = ava_iterate_singleton_string_chunk,
  .value_weight = ava_integer_value_weight,
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

static size_t ava_integer_value_weight(ava_value value) {
  return 0;
}

static const char* consume_sign_and_radix(
  const char*restrict ch, const char*restrict end,
  ava_bool* negative, char radixl, char radixu
) {
  /* Consume sign */
  if (*ch == '+' || *ch == '-') {
    *negative = *ch == '-';
    ++ch;
  }

  /* Skip radix prefix */
  if (ch[0] == '0' && ch+1 < end &&
      (ch[1] == radixl || ch[1] == radixu))
    ch += 2;

  if (ch[0] == radixl || ch[0] == radixu)
    ++ch;

  return ch;
}

static void throw_overflow(const char*restrict begin,
                           const char*restrict end) {
  ava_throw(&ava_format_exception,
            ava_value_of_string(
              ava_string_concat(
                ava_string_of_shared_cstring("integer flows over: "),
                ava_string_of_bytes(begin, end - begin))),
            NULL);
}

/* All of the parse_*() functions get strings that are already known to be
 * valid, possibly with trailing whitespace.
 */
static ava_integer ava_integer_parse_bin(const char*restrict begin,
                                         const char*restrict end) {
  const char*restrict ch = begin;
  ava_ulong accum = 0;
  unsigned bits = 0;
  ava_bool negative = 0;

  ch = consume_sign_and_radix(ch, end, &negative, 'b', 'B');

  while (ch < end && (*ch == '0' || *ch == '1')) {
    accum <<= 1;
    accum |= *ch == '1';
    bits += bits || *ch == '1';
    ++ch;
  }

  if (bits > 64) throw_overflow(begin, end);

  return negative? -accum : accum;
}

static ava_integer ava_integer_parse_oct(const char*restrict begin,
                                         const char*restrict end) {
  const char*restrict ch = begin;
  ava_ulong accum = 0;
  unsigned bits = 0;
  ava_bool negative = 0;

  ch = consume_sign_and_radix(ch, end, &negative, 'o', 'O');

  while (ch < end && *ch >= '0' && *ch <= '7') {
    accum <<= 3;
    accum |= *ch - '0';
    /* We actually do need to care about the number of bits in the first octit,
     * since 3 does not divide evenly into 64.
     */
    if (bits)
      bits += 3;
    else
      bits += *ch >= '4'? 3 : *ch >= '2'? 2 : *ch >= 1? 1 : 0;
    ++ch;
  }

  if (bits > 64) throw_overflow(begin, end);

  return negative? -accum : accum;
}

static ava_integer ava_integer_parse_hex(const char*restrict begin,
                                         const char*restrict end) {
  const char*restrict ch = begin;
  ava_ulong accum = 0;
  unsigned bits = 0;
  ava_bool negative = 0;

  ch = consume_sign_and_radix(ch, end, &negative, 'x', 'X');

  while (ch < end && ((*ch >= '0' && *ch <= '9') ||
                      (*ch >= 'a' && *ch <= 'f') ||
                      (*ch >= 'A' && *ch <= 'F'))) {
    accum <<= 4;
    accum |= ava_hexes[*ch & 0x7F];
    bits += 4 * (bits || *ch != '0');
    ++ch;
  }

  if (bits > 64) throw_overflow(begin, end);

  return negative? -accum : accum;
}

static ava_integer ava_integer_parse_dec(const char*restrict begin,
                                         const char*restrict end) {
  const char*restrict ch = begin;
  ava_ulong accum = 0;
  unsigned val;
  ava_bool negative = 0;

  ch = consume_sign_and_radix(ch, end, &negative, 'z', 'Z');

  while (ch < end && *ch >= '0' && *ch <= '9') {
    val = *ch - '0';
    if (accum > 0xFFFFFFFFFFFFFFFFULL/10ULL)
      throw_overflow(begin, end);

    accum *= 10;
    if (0xFFFFFFFFFFFFFFFFULL - accum < val)
      throw_overflow(begin, end);

    accum += val;
    ++ch;
  }

  return negative? -accum : accum;
}

ava_integer ava_integer_of_noninteger_value(
  ava_value value, ava_integer dfault
) {
  ava_string str = ava_to_string(value);
  ava_string_iterator iterator;
  const char*restrict strdata, *restrict cursor, *restrict marker = NULL;
  const char*restrict tok;
  const char* error_message;
  char tmpbuff[9];
  size_t strdata_len AVA_UNUSED;
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
    error_message = "string too long to be interpreted as ingeger: ";
    goto error;
  }

  /* Integers are always shorter than the maximum guaranteed atomic string
   * length, so just grab the whole array at once.
   */
  ava_string_iterator_place(&iterator, str, 0);
  strdata_len = ava_string_iterator_access(
    &strdata, &iterator, tmpbuff);
  assert(strlen == strdata_len);

  cursor = strdata;

#define YYCTYPE unsigned char
#define YYPEEK() (cursor < strdata + strlen? *cursor : 0)
#define YYSKIP() (++cursor)
#define YYBACKUP() (marker = cursor)
#define YYRESTORE() (cursor = marker)
#define YYLESSTHAN(n) (strdata + strlen - cursor < (n))
#define YYFILL(n) do {} while (0)
#define END() do {                                              \
    if (cursor != strdata + strlen) {                           \
      error_message = "trailing garbage at end of integer: ";   \
      goto error;                                               \
    }                                                           \
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
      *                         { error_message = "not an integer: ";
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
              ava_string_concat(
                ava_string_of_shared_cstring(error_message),
                str)),
            NULL);
  /* unreachable */
}

ava_bool ava_string_is_integer(ava_string str) {
  ava_string_iterator iterator;
  const char*restrict strdata, *restrict cursor, * restrict marker = NULL;
  char tmpbuff[9];
  size_t strdata_len AVA_UNUSED;
  size_t strlen;

  strlen = ava_string_length(str);

  if (strlen > MAX_INTEGER_LENGTH)
    return 0;

  if (str.ascii9 & 1 &&
      PARSE_DEC_FAST_ERROR != ava_integer_parse_dec_fast(str.ascii9, strlen))
    return 1;

  ava_string_iterator_place(&iterator, str, 0);
  strdata_len = ava_string_iterator_access(
    &strdata, &iterator, tmpbuff);
  assert(strlen == strdata_len);

  cursor = strdata;

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
