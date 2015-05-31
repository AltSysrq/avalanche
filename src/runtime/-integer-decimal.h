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
#ifndef AVA_RUNTIME__INTEGER_DECIMAL_H_
#define AVA_RUNTIME__INTEGER_DECIMAL_H_

/**
 * Table mapping integers from 0 to 9999 to their decimal representation.
 */
typedef struct {
  /**
   * The string representation of this value, left-padded with 0s. Readable as
   * a single dword or as four bytes.
   */
  union { ava_uint i; char c[4]; } value;
  /**
   * The number of present digits, ie, 4 minus the number of leading zeroes.
   */
  ava_uint digits;
} ava_integer_decimal_entry;
extern const ava_integer_decimal_entry ava_integer_decimal_table[10000];
/**
 * A table of ASCII9 fragments for every 4-digit integer from 0000 to 9999.
 *
 * The upper 28 bits is the actual character data; the lower 4 bits is the
 * number of digits excluding leading zeroes.
 */
extern const ava_uint ava_integer_ascii9_decimal_table[10000];

#endif /* AVA_RUNTIME__INTEGER_DECIMAL_H_ */
