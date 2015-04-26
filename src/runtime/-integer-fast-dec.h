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
