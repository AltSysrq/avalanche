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
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/alloc.h"
#include "avalanche/string.h"

/* TODO: Actually do twine stuff */

static void assert_aligned(const void* ptr) {
  assert(0 == (size_t)ptr % AVA_STRING_ALIGNMENT);
}

static ava_bool ava_string_is_ascii9(ava_string);
static ava_bool ava_string_can_encode_ascii9(const char*, size_t);
static ava_ascii9_string ava_ascii9_encode(const char*, size_t);
static void ava_ascii9_decode(char*restrict dst, ava_ascii9_string);
static size_t ava_ascii9_length(ava_ascii9_string);
static char ava_ascii9_index(ava_ascii9_string, size_t);
static ava_ascii9_string ava_ascii9_concat(ava_ascii9_string,
                                           ava_ascii9_string);
static ava_ascii9_string ava_ascii9_slice(ava_ascii9_string, size_t, size_t);
static ava_twine* ava_twine_alloc(size_t capacity);
static const char* ava_twine_force(const ava_twine*restrict twine);

static ava_bool ava_string_is_ascii9(ava_string str) {
  return str.ascii9 & 1;
}

static const char* ava_twine_force(const ava_twine*restrict twine) {
  /* TODO */
  return twine->body;
}

static ava_bool ava_string_can_encode_ascii9(const char* str,
                                             size_t sz) {
  unsigned i;

  if (sz > 9) return ava_false;

  for (i = 0; i < sz; ++i)
    if (0 == str[i] || (str[i] & 0x80))
      return ava_false;

  return ava_true;
}

static ava_ascii9_string ava_ascii9_encode(
  const char* str, size_t sz
) {
  ava_ascii9_string accum = 1, ch;
  unsigned shift = 64 - 7, i;

  for (i = 0; i < sz; ++i, shift -= 7) {
    ch = str[i];
    ch &= 0x7F;
    ch <<= shift;
    accum |= ch;
  }

  return accum;
}

ava_string ava_string_of_cstring(const char* str) {
  return ava_string_of_bytes(str, strlen(str));
}

ava_string ava_string_of_char(char ch) {
  return ava_string_of_bytes(&ch, 1);
}

static ava_twine* ava_twine_alloc(size_t sz) {
  ava_twine* twine;
  char* dst;
  size_t padded_sz;

  /* This isn't (+ sizeof(ava_ulong)-1) so that the NUL byte gets added */
  padded_sz = (sz + sizeof(ava_ulong)) /
    sizeof(ava_ulong) * sizeof(ava_ulong);

  twine = ava_alloc_atomic(offsetof(ava_twine, tail) + padded_sz);
  dst = (char*)&twine->tail;
  assert_aligned(dst);

  twine->body = dst;
  twine->length = sz;

  memset(dst + sz, 0, padded_sz - sz);
  return twine;
}

ava_string ava_string_of_bytes(const char* str, size_t sz) {
  ava_string ret;
  ava_twine* twine;
  char* dst;

  if (ava_string_can_encode_ascii9(str, sz)) {
    ret.ascii9 = ava_ascii9_encode(str, sz);
  } else {
    twine = ava_twine_alloc(sz);
    dst = (char*)twine->body;

    memcpy(dst, str, sz);

    ret.ascii9 = 0;
    ret.twine = twine;
  }

  return ret;
}

const char* ava_string_to_cstring(ava_string str) {
  if (ava_string_is_ascii9(str)) {
    ava_ulong* dst = ava_alloc_atomic(2 * sizeof(ava_ulong));
    dst[0] = dst[1] = 0;
    ava_ascii9_decode((char*)dst, str.ascii9);
    return (char*)dst;
  } else {
    return ava_twine_force(str.twine);
  }
}

const char* ava_string_to_cstring_buff(char buff[AVA_STR_TMPSZ],
                                       ava_string str) {
  if (ava_string_is_ascii9(str)) {
    memset(buff, 0, AVA_STR_TMPSZ);
    ava_ascii9_decode(buff, str.ascii9);
    return buff;
  } else {
    return ava_twine_force(str.twine);
  }
}

static void ava_ascii9_decode(char*restrict dst, ava_ascii9_string s) {
  unsigned shift = 64 - 7, i;
  char ch;

  for (i = 0; i < 9; ++i, shift -= 7) {
    ch = (s >> shift) & 0x7F;
    if (!ch) break;
    dst[i] = ch;
  }
}

void ava_string_to_bytes(void*restrict dst, ava_string str,
                         size_t start, size_t end) {
  char a9buf[9];
  const char*restrict src;

  if (ava_string_is_ascii9(str)) {
    ava_ascii9_decode(a9buf, str.ascii9);
    src = a9buf;
  } else {
    src = ava_twine_force(str.twine);
  }

  memcpy(dst, src + start, end - start);
}

static size_t ava_ascii9_length(ava_ascii9_string s) {
  /* Set the lowest bit of each character if _any_ of its bits are set. */
  /* 1111111 => ---1111 */
  s |= s >> 3;
  /* ---1111 => -----11 */
  s |= s >> 2;
  /* -----11 => ------1 */
  s |= s >> 1;
  /* Clear all but those lowest bits */
  s &= 0x204081020408102LL;
  /* Shift fields so that the the final accumulation is aligned with bit 0 */
  s >>= 1;
  /* Sum the resulting bits.
   * In the notation below, fields are indexed ordered by shift, rather than
   * character.
   */
  /* 0:1 1:1 2:1 3:1 4:1 5:1 6:1 7:1 8:1 =>
   * 0:2 1:0 2:2 3:0 4:2 5:0 6:2 7:0 8:1 */
  s += s >> 7;
  /* 0:2 2:2 4:2 6:2 8:1 => 0:3 2:0 4:3 6:0 8:1 */
  s += s >> 14;
  /* 0:3 4:3 8:1 => 0:4 4:0 8:1 */
  s += s >> 28;
  /* 0:4 8:1 => 0:5 8:0 */
  s += s >> 56;
  return s & 0xF;
}

size_t ava_string_length(ava_string str) {
  if (str.ascii9 & 1) {
    return ava_ascii9_length(str.ascii9);
  } else {
    return str.twine->length;
  }
}

static char ava_ascii9_index(ava_ascii9_string str, size_t ix) {
  return (str >> (1 + (8 - ix)*7)) & 0x7F;
}

char ava_string_index(ava_string str, size_t ix) {
  if (ava_string_is_ascii9(str)) {
    return ava_ascii9_index(str.ascii9, ix);
  } else {
    return ava_twine_force(str.twine)[ix];
  }
}

static ava_ascii9_string ava_ascii9_concat(
  ava_ascii9_string a, ava_ascii9_string b
) {
  return a | (b >> 7 * ava_ascii9_length(a));
}

ava_string ava_string_concat(ava_string a, ava_string b) {
  size_t alen, blen;
  ava_twine*restrict twine;
  char*restrict dst;
  const char*restrict src;
  char tmp[AVA_STR_TMPSZ];
  ava_string ret;

  /* If both are ASCII9 and are small enough to produce an ASCII9 result,
   * produce a new ASCII9 string.
   */
  if ((a.ascii9 & 1) && (b.ascii9 & 1) &&
      ava_ascii9_length(a.ascii9) + ava_ascii9_length(b.ascii9) <= 9)
    return (ava_string) { .ascii9 = ava_ascii9_concat(a.ascii9, b.ascii9) };

  alen = ava_string_length(a);
  blen = ava_string_length(b);

  /* If one is empty, return the other */
  if (0 == alen) return b;
  if (0 == blen) return a;

  /* TODO */
  twine = ava_twine_alloc(alen + blen);
  dst = (char*)twine->body;

  src = ava_string_to_cstring_buff(tmp, a);
  memcpy(dst, src, alen);
  src = ava_string_to_cstring_buff(tmp, b);
  memcpy(dst + alen, src, blen);

  ret.ascii9 = 0;
  ret.twine = twine;
  return ret;
}

static ava_ascii9_string ava_ascii9_slice(ava_ascii9_string str,
                                          size_t begin, size_t end) {
  str <<= 7 * begin;
  str &= 0xFFFFFFFFFFFFFFFELL << (9 - end + begin) * 7;
  str |= 1;
  return str;
}

ava_string ava_string_slice(ava_string str, size_t begin, size_t end) {
  ava_string ret;

  if (str.ascii9 & 1) {
    ret.ascii9 = ava_ascii9_slice(str.ascii9, begin, end);
    return ret;
  }

  if (0 == begin && ava_string_length(str) == end)
    return str;

  ret.ascii9 = 0;

  /* Convert to an ASCII9 string if possible */
  if (end - begin <= 9) {
    /* TODO */
  }

  /* TODO */

  return ava_string_of_bytes(ava_twine_force(str.twine) + begin, end - begin);
}
