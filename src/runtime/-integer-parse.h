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
#ifndef AVA_RUNTIME__INTEGER_PARSE_H_
#define AVA_RUNTIME__INTEGER_PARSE_H_

/* This is all supposed to be in integer.c, but it seems to offend re2c */

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
  ava_throw_str(&ava_format_exception,
                ava_error_integer_overflow(
                  ava_string_of_bytes(begin, end - begin)));
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

typedef struct {
  ava_integer* dst;
  ava_string str;
  ava_integer dfault;
} ava_integer_try_parse_data;

static void ava_integer_try_parse_impl(void* vdata) {
  ava_integer_try_parse_data* data = vdata;

  *data->dst = ava_integer_of_value(
    ava_value_of_string(data->str), data->dfault);
}

ava_bool ava_integer_try_parse(
  ava_integer* dst, ava_string str, ava_integer dfault
) {
  ava_exception caught;
  ava_integer_try_parse_data data = {
    .dst = dst, .str = str, .dfault = dfault
  };

  if (ava_catch(&caught, ava_integer_try_parse_impl, &data)) {
    if (&ava_format_exception == caught.type)
      return ava_false;
    else
      ava_rethrow(caught);
  } else {
    return ava_true;
  }
}

#endif /* AVA_RUNTIME__INTEGER_PARSE_H_ */
