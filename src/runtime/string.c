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

#include <string.h>
#include <stdlib.h>

#include "avalanche.h"

/* Size under which non-flat ropes are not produced */
#define ROPE_NONFLAT_THRESH (2 * sizeof(ava_rope))

#define ROPE_FORMAT_LEAFV ((const ava_rope*restrict)0)
#define ROPE_FORMAT_LEAF9 ((const ava_rope*restrict)1)

static inline int ava_rope_is_leafv(const ava_rope*restrict rope) {
  return ROPE_FORMAT_LEAFV == rope->concat_right;
}

static inline int ava_rope_is_leaf9(const ava_rope*restrict rope) {
  return ROPE_FORMAT_LEAF9 == rope->concat_right;
}

static inline int ava_rope_is_concat(const ava_rope*restrict rope) {
  return rope->concat_right > ROPE_FORMAT_LEAF9;
}

static int ava_string_can_encode_ascii9(const char*, size_t);
static ava_ascii9_string ava_ascii9_encode(const char*, size_t);
static void ava_rope_flatten_into(char*restrict dst, const ava_rope*restrict);
static void ava_ascii9_decode(char*restrict dst, ava_ascii9_string);
static void ava_ascii9_to_bytes(void*restrict dst, ava_ascii9_string,
                                size_t, size_t);
static void ava_rope_to_bytes(void*restrict dst, const ava_rope*restrict,
                              size_t, size_t);
static size_t ava_ascii9_length(ava_ascii9_string);
static char ava_ascii9_index(ava_ascii9_string, size_t);
static char ava_rope_index(const ava_rope*restrict, size_t);
static const ava_rope* ava_rope_concat(const ava_rope*restrict,
                                       const ava_rope*restrict);
static const ava_rope* ava_rope_concat_of(const ava_rope*restrict,
                                          const ava_rope*restrict);
static ava_ascii9_string ava_ascii9_concat(ava_ascii9_string,
                                           ava_ascii9_string);
static const ava_rope* ava_rope_of(ava_string);
static int ava_rope_concat_would_be_strongly_balanced(
  const ava_rope*restrict, const ava_rope*restrict);
static ava_ascii9_string ava_ascii9_slice(ava_ascii9_string, size_t, size_t);
static const ava_rope* ava_rope_slice(const ava_rope*restrict, size_t, size_t);

static int ava_string_can_encode_ascii9(const char* str,
                                        size_t sz) {
  unsigned i;

  if (sz > 9) return 0;

  for (i = 0; i < sz; ++i)
    if (0 == str[i] || (str[i] & 0x80))
      return 0;

  return 1;
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

ava_string ava_string_of_shared_cstring(const char* str) {
  return ava_string_of_shared_bytes(str, strlen(str));
}

ava_string ava_string_of_cstring(const char* str) {
  return ava_string_of_bytes(str, strlen(str));
}

ava_string ava_string_of_char(char ch) {
  return ava_string_of_bytes(&ch, 1);
}

ava_string ava_string_of_shared_bytes(const char* str, size_t sz) {
  ava_string s;
  ava_rope* rope;

  if (ava_string_can_encode_ascii9(str, sz)) {
    s.ascii9 = ava_ascii9_encode(str, sz);
  } else {
    s.ascii9 = 0;
    rope = AVA_NEW(ava_rope);
    rope->length = sz;
    rope->external_size = sz;
    rope->depth = 0;
    rope->leaves = 1;
    rope->v.leafv = str;
    rope->concat_right = ROPE_FORMAT_LEAFV;
    s.rope = rope;
  }

  return s;
}

ava_string ava_string_of_bytes(const char* str, size_t sz) {
  ava_string s;
  ava_rope* rope;

  if (ava_string_can_encode_ascii9(str, sz)) {
    s.ascii9 = ava_ascii9_encode(str, sz);
  } else {
    s.ascii9 = 0;
    rope = AVA_NEW(ava_rope);
    rope->length = sz;
    rope->external_size = sz;
    rope->depth = 0;
    rope->leaves = 1;
    rope->v.leafv = ava_clone_atomic(str, sz);
    rope->concat_right = ROPE_FORMAT_LEAFV;
    s.rope = rope;
  }

  return s;
}

char* ava_string_to_cstring(ava_string str) {
  return ava_string_to_cstring_buff(NULL, 0, str);
}

char* ava_string_to_cstring_buff(char* buff, size_t buffsz,
                                 ava_string str) {
  size_t required = 1 + ava_string_length(str);
  char* dst = (required <= buffsz? buff : ava_alloc_atomic(required));

  if (str.ascii9 & 1)
    ava_ascii9_decode(dst, str.ascii9);
  else
    ava_rope_flatten_into(dst, str.rope);

  dst[required-1] = 0;

  return dst;
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

static void ava_rope_flatten_into(char*restrict dst, const ava_rope* rope) {
  tail_call:

  if (ava_rope_is_concat(rope)) {
    ava_rope_flatten_into(dst, rope->v.concat_left);

    dst += rope->v.concat_left->length;
    rope = rope->concat_right;
    goto tail_call;
  } else if (ava_rope_is_leaf9(rope)) {
    ava_ascii9_decode(dst, rope->v.leaf9);
  } else {
    memcpy(dst, rope->v.leafv, rope->length);
  }
}

char* ava_string_to_cstring_tmpbuff(char** buffptr, size_t* szptr,
                                    ava_string str) {
  size_t required = ava_string_length(str) + 1;

  if (!*buffptr || required > *szptr) {
    /* Round up to the next multiple of 16 bytes */
    *szptr = (required + 15) / 16 * 16;
    *buffptr = ava_alloc_atomic(*szptr);
  }

  return ava_string_to_cstring_buff(*buffptr, *szptr, str);
}

void ava_string_to_bytes(void*restrict dst, ava_string str,
                         size_t start, size_t end) {
  if (str.ascii9 & 1) {
    ava_ascii9_to_bytes(dst, str.ascii9, start, end);
  } else {
    ava_rope_to_bytes(dst, str.rope, start, end);
  }
}

static void ava_ascii9_to_bytes(void*restrict dst, ava_ascii9_string str,
                                size_t start, size_t end) {
  char dat[9];

  ava_ascii9_decode(dat, str);
  memcpy(dst, dat + start, end - start);
}

static void ava_rope_to_bytes(void*restrict dst, const ava_rope*restrict rope,
                              size_t start, size_t end) {
  if (ava_rope_is_concat(rope)) {
    if (end <= rope->v.concat_left->length) {
      ava_rope_to_bytes(dst, rope->v.concat_left, start, end);
    } else if (start >= rope->v.concat_left->length) {
      ava_rope_to_bytes(dst, rope->concat_right,
                        start - rope->v.concat_left->length,
                        end - rope->v.concat_left->length);
    } else {
      ava_rope_to_bytes(dst, rope->v.concat_left,
                        start, rope->v.concat_left->length);
      ava_rope_to_bytes(dst + rope->v.concat_left->length - start,
                        rope->concat_right,
                        0, end - rope->v.concat_left->length);
    }
  } else if (ava_rope_is_leaf9(rope)) {
    ava_ascii9_to_bytes(dst, rope->v.leaf9, start, end);
  } else {
    memcpy(dst, rope->v.leafv + start, end - start);
  }
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
    return str.rope->length;
  }
}

static char ava_ascii9_index(ava_ascii9_string str, size_t ix) {
  return (str >> (1 + (8 - ix)*7)) & 0x7F;
}

static char ava_rope_index(const ava_rope*restrict rope, size_t ix) {
  while (ava_rope_is_concat(rope)) {
    if (ix >= rope->v.concat_left->length) {
      ix -= rope->v.concat_left->length;
      rope = rope->concat_right;
    } else {
      rope = rope->v.concat_left;
    }
  }

  if (ava_rope_is_leaf9(rope))
    return ava_ascii9_index(rope->v.leaf9, ix);
  else
    return rope->v.leafv[ix];
}

char ava_string_index(ava_string str, size_t ix) {
  if (str.ascii9 & 1)
    return ava_ascii9_index(str.ascii9, ix);
  else
    return ava_rope_index(str.rope, ix);
}

static ava_ascii9_string ava_ascii9_concat(
  ava_ascii9_string a, ava_ascii9_string b
) {
  return a | (b >> 7 * ava_ascii9_length(a));
}

static const ava_rope* ava_rope_concat_of(const ava_rope*restrict a,
                                          const ava_rope*restrict b) {
  ava_rope* concat = AVA_NEW(ava_rope);
  *concat = (ava_rope) {
    .length = a->length + b->length,
    .external_size = a->external_size + b->external_size,
    .depth = 1 + (a->depth > b->depth? a->depth : b->depth),
    .leaves = a->leaves + b->leaves,
    .v = { .concat_left = a },
    .concat_right = b
  };

  return concat;
}

ava_string ava_string_concat(ava_string a, ava_string b) {
  size_t alen, blen;

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

  /* If the result will be too small to justify rope overhead, produce a flat
   * string.
   */
  if (alen + blen < ROPE_NONFLAT_THRESH) {
    ava_string ret;
    ava_rope* rope;
    char* strdat;

    strdat = ava_alloc_atomic(alen + blen);
    ava_string_to_bytes(strdat, a, 0, alen);
    ava_string_to_bytes(strdat + alen, b, 0, blen);

    rope = AVA_NEW(ava_rope);
    rope->length = alen + blen;
    rope->external_size = alen + blen;
    rope->depth = 0;
    rope->leaves = 1;
    rope->v.leafv = strdat;

    ret.ascii9 = 0;
    ret.rope = rope;
    return ret;
  }

  /* No special cases occurred, so just do a rope concat */
  ava_string ret;
  ret.ascii9 = 0;
  ret.rope = ava_rope_concat(ava_rope_of(a), ava_rope_of(b));
  return ret;
}

static const ava_rope* ava_rope_of(ava_string str) {
  if (str.ascii9 & 1) {
    ava_rope* rope = AVA_NEW(ava_rope);
    rope->length = ava_ascii9_length(str.ascii9);
    rope->external_size = 0;
    rope->depth = 0;
    rope->leaves = 1;
    rope->v.leaf9 = str.ascii9;
    rope->concat_right = ROPE_FORMAT_LEAF9;
    return rope;
  } else {
    return str.rope;
  }
}

static const ava_rope* ava_rope_concat(const ava_rope*restrict left,
                                       const ava_rope*restrict right) {
  const ava_rope*restrict concat;

  /* If a trivial concat is balanced, just do that */
  if (ava_rope_concat_would_be_strongly_balanced(left, right))
    return ava_rope_concat_of(left, right);

  /* Would be unbalanced if done trivially. See if one can be passed down a
   * branch of the other. (This optimises for linear concatenations of shallow
   * strings.)
   */
  /* a->depth > b->depth guarantees that a is a concat */
  if (left->depth > right->depth) {
    concat = ava_rope_concat_of(
      left->v.concat_left,
      ava_rope_concat(left->concat_right, right));
  } else if (right->depth > left->depth) {
    concat = ava_rope_concat_of(
      ava_rope_concat(left, right->v.concat_left),
      right->concat_right);
  } else {
    /* Fall back to trivial and let a rebalance do its thing */
    concat = ava_rope_concat_of(left, right);
  }

  return concat;
}

static int ava_rope_concat_would_be_strongly_balanced(
  const ava_rope*restrict left, const ava_rope*restrict right
) {
  unsigned depth = left->depth > right->depth? left->depth : right->depth;

  return (1uLL << depth) <= (left->leaves + right->leaves)*2;
}

static ava_ascii9_string ava_ascii9_slice(ava_ascii9_string str,
                                          size_t begin, size_t end) {
  str <<= 7 * begin;
  str &= 0xFFFFFFFFFFFFFFFELL << (9 - end + begin) * 7;
  str |= 1;
  return str;
}

static const ava_rope* ava_rope_slice(const ava_rope*restrict rope,
                                      size_t begin, size_t end) {
  ava_rope* new;

  if (0 == begin && rope->length == end)
    return rope;

  if (ava_rope_is_concat(rope)) {
    /* If one side alone can handle the split, simply delegate */
    if (begin >= rope->v.concat_left->length)
      return ava_rope_slice(
        rope->concat_right,
        begin - rope->v.concat_left->length,
        end - rope->v.concat_left->length);

    if (end <= rope->v.concat_left->length)
      return ava_rope_slice(
        rope->v.concat_left, begin, end);

    /* The string will straddle the two sides. Coalesce eagerly if too small. */
    if (end - begin < ROPE_NONFLAT_THRESH) {
      char* strdat = ava_alloc_atomic(end - begin);
      ava_rope_to_bytes(strdat, rope, begin, end);

      ava_rope* ret = AVA_NEW(ava_rope);
      *ret = (ava_rope) {
        .length = end - begin,
        .external_size = end - begin,
        .depth = 0,
        .leaves = 1,
        .v = { .leafv = strdat },
        .concat_right = ROPE_FORMAT_LEAFV
      };
      return ret;
    }

    return ava_rope_concat_of(
      ava_rope_slice(
        rope->v.concat_left, begin, rope->v.concat_left->length),
      ava_rope_slice(
        rope->concat_right, 0, end - rope->v.concat_left->length));
  } else if (ava_rope_is_leaf9(rope)) {
    new = AVA_NEW(ava_rope);
    *new = *rope;
    new->v.leaf9 = ava_ascii9_slice(new->v.leaf9, begin, end);
    new->length = end - begin;
    return new;
  } else {
    new = AVA_NEW(ava_rope);
    *new = *rope;
    new->length = end - begin;
    new->v.leafv += begin;
    /* If we're now using less than half of the original buffer, reallocate. */
    if (new->length*2 < new->external_size) {
      new->v.leafv = ava_clone_atomic(new->v.leafv, new->length);
      new->external_size = new->length;
    }

    return new;
  }
}

ava_string ava_string_slice(ava_string str, size_t begin, size_t end) {
  ava_string ret;

  if (str.ascii9 & 1) {
    ret.ascii9 = ava_ascii9_slice(str.ascii9, begin, end);
    return ret;
  }

  ret.ascii9 = 0;

  /* Convert to an ASCII9 string if possible */
  if (end - begin <= 9) {
    char c9[9];
    ava_rope_to_bytes(c9, str.rope, begin, end);
    if (ava_string_can_encode_ascii9(c9, end - begin)) {
      ret.ascii9 = ava_ascii9_encode(c9, end - begin);
      return ret;
    }
  }

  ret.rope = ava_rope_slice(str.rope, begin, end);

  return ret;
}
