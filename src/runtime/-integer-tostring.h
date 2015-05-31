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
#ifndef AVA_RUNTIME__INTEGER_TOSTRING_H_
#define AVA_RUNTIME__INTEGER_TOSTRING_H_

/* This is all supposed to be in integer.c, but it seems to offend re2c */
#define TO_HIDWORD(n) ((ava_ulong)(n) << 32)
#define TO_LODWORD(n) ((ava_ulong)(n) <<  0)
#ifdef WORDS_BIGENDIAN
#define TO_DWORD0(n) TO_HIDWORD(n)
#define TO_DWORD1(n) TO_LODWORD(n)
#define PHYSICAL_SHIFT_LEFT <<
#define PHYSICAL_SHIFT_RIGHT >>
#else
#define TO_DWORD0(n) TO_LODWORD(n)
#define TO_DWORD1(n) TO_HIDWORD(n)
#define PHYSICAL_SHIFT_LEFT >>
#define PHYSICAL_SHIFT_RIGHT <<
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
static ava_ulong ava_integer_hash(ava_value value) AVA_PURE;

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

static ava_ulong ava_integer_hash(ava_value value) {
  ava_ulong str[3];
  unsigned length, lshift, rshift;

  length = ava_integer_to_ulong_string(str, ava_value_slong(value));
  /* Left-align */
  if (length & 7) {
    lshift = 8 * ((sizeof(ava_ulong) - length) & 7);
    rshift = (sizeof(ava_ulong) * 8 - lshift);
    str[0] = str[0] PHYSICAL_SHIFT_LEFT lshift;
    str[0] |= str[1] PHYSICAL_SHIFT_RIGHT rshift;
    str[1] = str[1] PHYSICAL_SHIFT_LEFT lshift;
    str[1] |= str[2] PHYSICAL_SHIFT_RIGHT rshift;
    str[2] = str[2] PHYSICAL_SHIFT_LEFT lshift;
  }

  return ava_hash_ulongs(str + 2 - (length-1)/sizeof(ava_ulong), length);
}

#endif /* AVA_RUNTIME__INTEGER_TOSTRING_H_ */
