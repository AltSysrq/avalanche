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

#define AVA__INTERNAL_INCLUDE
#include "avalanche/defs.h"
#include "avalanche/value.h"
#include "avalanche/integer.h"
#include "-integer-fast-dec.h"

#define A9(str) _AVA_ASCII9_ENCODE_STR(str)
#define A9_1(str) (A9(str) >> 1)

/* XXX This file is logically a part of integer.c, but re2c with the -r option
 * can't operate on files longer than 16384 bytes.
 */

ava_integer ava_integer_parse_dec_fast(
  ava_ascii9_string s, unsigned strlen
) {
  ava_ascii9_string mask = ~0ULL << ((9 - strlen) * 7 + 1);
  int negative;
  s &= mask;

  negative = (A9("-") & ~1ULL) == (s & A9("\x7f"));

  /* Are all the characters covered by mask decimal, or in the case of the
   * first character, hyphen?
   *
   * First check for anything that has bits set that definitely shouldn't be.
   */
  if (s != (s & A9("\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F")))
    return PARSE_DEC_FAST_ERROR;

  /* If negative, remove the negative sign */
  if (negative) {
    s <<= 7;
    mask <<= 7;
    --strlen;

    if (!strlen)
      return PARSE_DEC_FAST_ERROR;
  }

  /* Stop if anthing is in the 3a..3f range
   *       Common prefix  Zero for 8, 9         Ignore
   * 8 38  0011 1         00                    0
   * 9 39  0011 1         00                    1
   * : 3A  0011 1         01                    0
   * ; 3B  0011 1         01                    1
   * < 3C  0011 1         10                    0
   * = 3D  0011 1         10                    1
   * > 3E  0011 1         11                    0
   * ? 3F  0011 1         11                    1
   *
   * Anything with leading non-00 was discarded by the previous test, so we can
   * ignore those. It's also ok to discard things that start with 0000, 0001,
   * 0010, or 0011, so we really only care about bit 3 (the rightmost on the
   * common prefix).
   */
  ava_ascii9_string bit3 = s & A9("\x08\x08\x08\x08\x08\x08\x08\x08\x08");
  bit3 >>= 1;
  bit3 |= bit3 >> 1;
  if (s & bit3)
    return PARSE_DEC_FAST_ERROR;

  /* We now know that every character is between 01 and 3F, and furthermore is
   * not between XA and XF for any row X.
   *
   * Now discard anything that isn't in the 0x3X range.
   */
  if ((~s & mask) & A9("\x30\x30\x30\x30\x30\x30\x30\x30\x30"))
    return PARSE_DEC_FAST_ERROR;

  /* It's a base-10 string, possibly with a leading question mark indicating it
   * is negative.
   *
   * Subtract the '0' bias.
   */
  s -= A9("000000000") & mask;
  /* Align to bit zero */
  s >>= 64 - (7 * strlen);

  /* Convert to binary.
   *
   * On each step, pairs of fields, starting from the right, are multiplied
   * together, producing a new field taking up both prior fields.
   *
   * It is important to remember here that s is no longer truly an ASCII9
   * string. It contains NULs, and is right-aligned instead of left-aligned,
   * without even the special treatment of bit 0. The format is basically the
   * same, so we still use the A9() definition to produce masks and just shift
   * them over to account for bit 0 (hence A9_1()).
   */
  /* Bits: 7 7 7 7 7 7 7 7    7 14 14 14 14
   * Max:  9 9 9 9 9 9 9 9 => 9 99 99 99 99
   * Need: 4 4 4 4 4 4 4 4    4  7  7  7  7
   */
  s = (s & A9_1("\x7f\x00\x7f\x00\x7f\x00\x7f\x00\x7f")) +
    10 * ((s >> 7) & A9_1("\x7f\x00\x7f\x00\x7f\x00\x7f\x00\x7f"));
  /* Bits: 7 14 14 14 14     7   28   28
   * Max:  9 99 99 99 99 =>  9 9999 9999
   * Need: 4  7  7  7  7     4   14   14
   */
  s = (s & A9_1("\x7f\x00\x00\x7f\x7f\x00\x00\x7f\x7f")) +
    100 * ((s >> 14) & A9_1("\x7f\x00\x00\x7f\x7f\x00\x00\x7f\x7f"));
  /* Bits: 7   28   28    7       56
   * Max:  9 9999 9999 => 9 99999999
   * Need: 4   14   14    4       27
   */
  s = (s & A9_1("\x7f\x00\x00\x00\x00\x7f\x7f\x7f\x7f")) +
    10000 * ((s >> 28) & A9_1("\x7f\x00\x00\x00\x00\x7f\x7f\x7f\x7f"));
  /* Bits: 7       56           64
   * Max:  9 99999999 => 999999999
   * Need: 4       27           30
   */
  s = (s & A9_1("\x00\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f")) +
    100000000 * ((s >> 56) & A9_1("\x00\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"));

  return negative? -s : s;
}
