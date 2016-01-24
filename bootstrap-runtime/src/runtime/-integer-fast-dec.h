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
/* This is an internal header file */
#ifndef AVA_RUNTIME__INTEGER_FAST_DEC_H_
#define AVA_RUNTIME__INTEGER_FAST_DEC_H_

#define PARSE_DEC_FAST_ERROR ((ava_integer)0x8000000000000000LL)

/**
 * Converts an ASCII9 string of a given length to an integer.
 *
 * This is much faster than matching the string with re2c and then using
 * ava_integer_parse_dec(), but it can only accept strings matching the
 * expression `-?[0-9]+`.
 *
 * This extern both so that tests can test it directly (since otherwise
 * simply returning PARSE_DEC_FAST_ERROR for all cases would pass) and because
 * of re2c file size limitations with the -r flag.
 *
 * @param str The string to parse.
 * @param strlen The length of str.
 * @return The parsed integer, or PARSE_DEC_FAST_ERROR if this function cannot
 * parse str.
 */
ava_integer ava_integer_parse_dec_fast(
  ava_ascii9_string str, unsigned strlen) AVA_CONSTFUN;

#endif /* AVA_RUNTIME__INTEGER_FAST_DEC_H_ */
