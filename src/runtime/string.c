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

#include <assert.h>
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

static ava_rope* ava_rope_new_concat(const ava_rope*restrict,
                                     const ava_rope*restrict);
static ava_rope* ava_rope_new_leafv(const char*restrict,
                                    size_t length, size_t external_size);
static ava_rope* ava_rope_new_leaf9(ava_ascii9_string);

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
static const ava_rope* ava_rope_rebalance(const ava_rope*restrict);
static const ava_rope* ava_rope_force_rebalance(const ava_rope*restrict);
static void ava_rope_rebalance_iterate(
  const ava_rope*restrict[AVA_MAX_ROPE_DEPTH],
  const ava_rope*restrict);
static const ava_rope* ava_rope_rebalance_flush(
  const ava_rope*restrict[AVA_MAX_ROPE_DEPTH], unsigned, unsigned);
static int ava_rope_is_balanced(const ava_rope*restrict);
static int ava_rope_concat_would_be_node_balanced(
  const ava_rope*restrict, const ava_rope*restrict);
static unsigned ava_bif(size_t);
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

static ava_rope* ava_rope_new_concat(const ava_rope*restrict a,
                                     const ava_rope*restrict b) {
  ava_rope* concat = AVA_NEW(ava_rope);

  *concat = (ava_rope) {
    .length = a->length + b->length,
    .external_size = a->external_size + b->external_size,
    .depth = 1 + (a->depth > b->depth? a->depth : b->depth),
    .descendants = 2 + a->descendants + b->descendants,
    .v = { .concat_left = a },
    .concat_right = b
  };

  return concat;
}

static ava_rope* ava_rope_new_leafv(const char*restrict v,
                                    size_t length, size_t external_size) {
  ava_rope* r = AVA_NEW(ava_rope);

  *r = (ava_rope) {
    .length = length,
    .external_size = external_size,
    .depth = 0,
    .descendants = 0,
    .v = { .leafv = v },
    .concat_right = ROPE_FORMAT_LEAFV
  };

  return r;
}

static ava_rope* ava_rope_new_leaf9(ava_ascii9_string s) {
  ava_rope* r = AVA_NEW(ava_rope);

  *r = (ava_rope) {
    .length = ava_ascii9_length(s),
    .external_size = 0,
    .depth = 0,
    .descendants = 0,
    .v = { .leaf9 = s },
    .concat_right = ROPE_FORMAT_LEAF9
  };

  return r;
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

  if (ava_string_can_encode_ascii9(str, sz)) {
    s.ascii9 = ava_ascii9_encode(str, sz);
  } else {
    s.ascii9 = 0;
    s.rope = ava_rope_new_leafv(str, sz, sz);
  }

  return s;
}

ava_string ava_string_of_bytes(const char* str, size_t sz) {
  ava_string s;

  if (ava_string_can_encode_ascii9(str, sz)) {
    s.ascii9 = ava_ascii9_encode(str, sz);
  } else {
    s.ascii9 = 0;
    s.rope = ava_rope_new_leafv(ava_clone_atomic(str, sz),
                                sz, sz);
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
  /* Coalesce into a single node if proferable */
  if (a->length + b->length <= 9 &&
      ava_rope_is_leaf9(a) && ava_rope_is_leaf9(b))
    return ava_rope_new_leaf9(ava_ascii9_concat(
                                a->v.leaf9, b->v.leaf9));

  if (a->length + b->length <= ROPE_NONFLAT_THRESH) {
    char* strdat = ava_alloc_atomic(a->length + b->length);
    ava_rope_to_bytes(strdat, a, 0, a->length);
    ava_rope_to_bytes(strdat + a->length, b, 0, b->length);
    return ava_rope_new_leafv(strdat, a->length + b->length,
                              a->length + b->length);
  }

  return ava_rope_new_concat(a, b);
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
  if (alen + blen <= ROPE_NONFLAT_THRESH) {
    ava_string ret;
    char* strdat;

    strdat = ava_alloc_atomic(alen + blen);
    ava_string_to_bytes(strdat, a, 0, alen);
    ava_string_to_bytes(strdat + alen, b, 0, blen);

    ret.ascii9 = 0;
    ret.rope = ava_rope_new_leafv(strdat, alen + blen, alen + blen);
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
    return ava_rope_new_leaf9(str.ascii9);
  } else {
    return str.rope;
  }
}

static const ava_rope* ava_rope_concat(const ava_rope*restrict left,
                                       const ava_rope*restrict right) {
  const ava_rope*restrict concat;

  /* If a trivial concat is balanced, just do that */
  if (ava_rope_concat_would_be_node_balanced(left, right))
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

  return ava_rope_rebalance(concat);
}

/* Inverse offset fibonacci function, where fib(0) == 0 and fib(1) == 1 */
static unsigned ava_bif(size_t sz) {
  unsigned iterations = 0;
  unsigned long long a = 0, b = 1, c;

  while (sz >= a) {
    ++iterations;
    c = a + b;
    a = b;
    b = c;
  }

  return iterations;
}

static int ava_rope_is_balanced(const ava_rope*restrict rope) {
  /* The rope is considered balanced if fib(depth+2) <= length. */
  return rope->depth+2 <= ava_bif(rope->length) + 1;
}

static int ava_rope_concat_would_be_node_balanced(
  const ava_rope*restrict left, const ava_rope*restrict right
) {
  unsigned depth = left->depth > right->depth? left->depth : right->depth;

  return (1u << depth) <= (left->descendants + right->descendants)*2;
}

ava_string ava_string_optimise(ava_string str) {
  if (str.ascii9 & 1) return str;

  str.rope = ava_rope_force_rebalance(str.rope);
  return str;
}

static const ava_rope* ava_rope_rebalance(const ava_rope*restrict root) {
  if (ava_rope_is_balanced(root)) return root;
  else return ava_rope_force_rebalance(root);
}

static const ava_rope* ava_rope_force_rebalance(const ava_rope*restrict root) {
  const ava_rope*restrict accum[AVA_MAX_ROPE_DEPTH];

  /* Rebalance the whole rope */
  memset((void*)accum, 0, sizeof(accum));
  ava_rope_rebalance_iterate(accum, root);
  return ava_rope_rebalance_flush(accum, 0, AVA_MAX_ROPE_DEPTH);
}

static void ava_rope_rebalance_iterate(
  const ava_rope*restrict accum[AVA_MAX_ROPE_DEPTH],
  const ava_rope*restrict rope
) {
  unsigned clear, bif;
  int is_slot_free;

  if (ava_rope_is_concat(rope)) {
    ava_rope_rebalance_iterate(accum, rope->v.concat_left);
    ava_rope_rebalance_iterate(accum, rope->concat_right);
    return;
  }

  /* This is a leaf; insert it into the array */
  clear = 0;
  for (;;) {
    bif = ava_bif(rope->length) - 1; /* -1 since size 0 never occurs */

    /* Ensure all slots below the target are free */
    is_slot_free = 1;
    for (; clear <= bif; ++clear) {
      if (accum[clear]) {
        is_slot_free = 0;
        break;
      }
    }

    if (is_slot_free) {
      /* Free, done with this element */
      accum[bif] = rope;
      break;
    } else {
      /* Not free. Produce a new rope by flushing the accumulator, create a
       * Concat between that and this one, then repeat with the new, larger
       * rope.
       */
      rope = ava_rope_concat_of(
        ava_rope_rebalance_flush(accum, clear, bif+1), rope);
      clear = bif;
    }
  }
}

static const ava_rope* ava_rope_rebalance_flush(
  const ava_rope*restrict accum[AVA_MAX_ROPE_DEPTH],
  unsigned start, unsigned end
) {
  const ava_rope*restrict right = NULL;
  unsigned i;

  for (i = start; i < end; ++i) {
    if (accum[i]) {
      if (right) {
        right = ava_rope_concat_of(accum[i], right);
      } else {
        right = accum[i];
      }

      accum[i] = NULL;
    }
  }

  return right;
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

      return ava_rope_new_leafv(strdat, end - begin, end - begin);
    }

    return ava_rope_concat_of(
      ava_rope_slice(
        rope->v.concat_left, begin, rope->v.concat_left->length),
      ava_rope_slice(
        rope->concat_right, 0, end - rope->v.concat_left->length));
  } else if (ava_rope_is_leaf9(rope)) {
    return ava_rope_new_leaf9(ava_ascii9_slice(rope->v.leaf9, begin, end));
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

  ret.rope = ava_rope_rebalance(ava_rope_slice(str.rope, begin, end));

  return ret;
}

void ava_string_iterator_place(ava_string_iterator* it,
                               ava_string str, size_t index) {
  assert(index < ava_string_length(str));

  memset(it, 0, sizeof(ava_string_iterator));

  if (str.ascii9 & 1) {
    it->ascii9 = str.ascii9;
  } else {
    size_t off = index;
    const ava_rope*restrict rope;

    assert(str.rope->depth <= AVA_MAX_ROPE_DEPTH);

    rope = it->stack[0].rope = str.rope;
    while (ava_rope_is_concat(rope)) {
      if (off < rope->v.concat_left->length) {
        it->stack[it->top].offset = 0;
        ++it->top;
        rope = it->stack[it->top].rope = rope->v.concat_left;
      } else {
        it->stack[it->top].offset =
          it->stack[it->top].rope->v.concat_left->length;
        ++it->top;
        off -= rope->v.concat_left->length;
        rope = it->stack[it->top].rope = rope->concat_right;
      }
    }

    it->stack[it->top].offset = off;
  }

  it->real_index = it->logical_index = index;
  it->length = ava_string_length(str);
}

int ava_string_iterator_move(ava_string_iterator* it,
                             ssize_t logical_distance) {
  size_t new_real_index;
  ssize_t real_distance;

  it->logical_index += logical_distance;

  /* <= so that empty strings will place the real index at 0 rather than ~0u */
  if (it->logical_index <= 0)
    new_real_index = 0;
  else if (it->logical_index >= (ssize_t)it->length)
    new_real_index = it->length - 1;
  else
    new_real_index = it->logical_index;

  real_distance = new_real_index - it->real_index;
  it->real_index = new_real_index;

  if (!it->ascii9) {
    /* Walk up the tree until we find a rope that contains the index we are
     * looking for.
     */
    if (real_distance > 0) {
      while (real_distance >= ((ssize_t)it->stack[it->top].rope->length -
                               (ssize_t)it->stack[it->top].offset)) {
        real_distance += it->stack[it->top].offset;
        --it->top;
      }
    } else {
      while (real_distance + (ssize_t)it->stack[it->top].offset < 0) {
        real_distance += it->stack[it->top].offset;
        --it->top;
      }

      real_distance += it->stack[it->top].offset;
      it->stack[it->top].offset = 0;
    }

    assert(real_distance >= 0);
    size_t dist = real_distance;

    /* Walk down the tree to find the leaf containing the desired index */
    while (ava_rope_is_concat(it->stack[it->top].rope)) {
      if (dist + it->stack[it->top].offset <
          it->stack[it->top].rope->v.concat_left->length) {
        dist += it->stack[it->top].offset;
        it->stack[it->top].offset = 0;
        it->stack[it->top+1].rope = it->stack[it->top].rope->v.concat_left;
        it->stack[it->top+1].offset = 0;
        ++it->top;
      } else {
        dist -= it->stack[it->top].rope->v.concat_left->length -
          it->stack[it->top].offset;
        it->stack[it->top].offset =
          it->stack[it->top].rope->v.concat_left->length;
        it->stack[it->top+1].rope = it->stack[it->top].rope->concat_right;
        it->stack[it->top+1].offset = 0;
        ++it->top;
      }
    }

    /* Adjust offset in final leaf */
    it->stack[it->top].offset += dist;
  }

  return ava_string_iterator_valid(it);
}

int ava_string_iterator_valid(const ava_string_iterator* it) {
  return it->logical_index >= 0 && it->logical_index < (ssize_t)it->length;
}

char ava_string_iterator_get(const ava_string_iterator* it) {
  if (it->ascii9)
    return ava_ascii9_index(it->ascii9, it->real_index);
  else if (ava_rope_is_leaf9(it->stack[it->top].rope))
    return ava_ascii9_index(it->stack[it->top].rope->v.leaf9,
                            it->stack[it->top].offset);
  else
    return it->stack[it->top].rope->v.leafv[
      it->stack[it->top].offset];
}

size_t ava_string_iterator_read_hold(char*restrict dst, size_t n,
                                     const ava_string_iterator* it) {
  size_t nread;

  if (!ava_string_iterator_valid(it))
    return 0;

  if (it->ascii9) {
    char c9[9];

    nread = it->length - it->real_index;
    if (n < nread) nread = n;

    ava_ascii9_decode(c9, it->ascii9);
    memcpy(dst, c9 + it->real_index, nread);
  } else {
    const ava_rope*restrict rope = it->stack[it->top].rope;
    size_t offset = it->stack[it->top].offset;
    nread = rope->length - it->stack[it->top].offset;
    if (n < nread) nread = n;

    if (ava_rope_is_leaf9(rope)) {
      char c9[9];
      ava_ascii9_decode(c9, rope->v.leaf9);
      memcpy(dst, c9 + offset, nread);
    } else {
      memcpy(dst, rope->v.leafv + offset, nread);
    }
  }

  return nread;
}

size_t ava_string_iterator_read(char*restrict dst, size_t n,
                                ava_string_iterator* it) {
  size_t nread = ava_string_iterator_read_hold(dst, n, it);
  ava_string_iterator_move(it, nread);
  return nread;
}

size_t ava_string_iterator_index(const ava_string_iterator* it) {
  if (it->logical_index >= 0 && it->logical_index < (ssize_t)it->length)
    return it->logical_index;
  else if (it->logical_index < 0)
    return 0;
  else
    return it->length;
}
