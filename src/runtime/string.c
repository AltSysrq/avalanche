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

#ifdef HAVE_NMMINTRIN_H
#include <nmmintrin.h>
#endif

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/alloc.h"
#include "avalanche/string.h"

/*

  A Twine is a string data structure somewhat similar to a Rope, in that
  operations are effected by constructing a tree of nodes on top of the
  constituent character arrays rather than immediately producing new character
  arrays. Unlike ropes, readers of twines do not simply traverse the nodes when
  they want to access the character data; instead, they _force_ the twine into
  a flat string and memoise the result.

  The forcing of twines has several benefits over rope-style node traversal.
  First, there is no need to keep any kind of balance in the tree; a linear
  chain of a thousand concats has exactly the same performance as a perfectly
  balanced tree of same. Secondly, reads have voluminous O(1) complexity
  instead of average O(log(n)) complexity. Finally, reads can be expressed in
  terms of array access, which has a much lower constant than rope accesses,
  and even allows for such things as guaranteeing that the returned string is a
  C string (though possibly containing NULs).

  The main disadvantage is that certain unusual cases exhibit much worse
  behaviour. Eg, a sequence of alternating concats and non-voluminous reads
  results in O(n^2) runtime and memory usage, since each concat gets forced
  like a traditional immutable string.

  In order to prevent unbounded memory usage by intermediate nodes, a node is
  forced upon construction if its overhead exceeds the length of the string.
  The overhead of a node is equal to the size of a node structure times the
  number of nodes referenced plus the number of characters held by reference
  but not actually part of the twine (ie, characters discarded by slices).

  Each node is a 5-tuple of (tag,length,overhead,primary,other). Physically,
  tag and primary are packed into one field. length is always the number of
  characters logically in the twine and is never mutated. overhead tracks the
  cumulative overhead of the twine (at time of construction; forcing of
  referenced nodes reduces the actual overhead, but this is not tracked),
  except that its value is undefined on forced nodes. other is the "other"
  piece of data needed by the twine node. tag indicates the particular type of
  node.

  The possible node types are:

  - Forced. body is a const char* pointing to string data returnable from
    ava_string_to_cstring(). other is undefined. Forced is the only node type
    to which nodes can be mutated; in such cases, the other field is explicitly
    cleared to release whatever memory it may hold. overhead is also undefined
    for forced nodes, though it is never changed once set.

  - Concat. body is an ava_twine*, other is an ava_string. The forced string is
    composed of all the characters of body followed by all the characters of
    other.

  - Tacnoc. Essentially a concat in reverse order, for the case where the left
    string is an ASCII9 string (which cannot be stored in body due to alignment
    restrictions). body is an ava_twine*, other is an ASCII9 ava_string. The
    forced string is composed of all the characters of other followed by all
    the characters of body.

  - Slice. body is an ava_twine*, other is a size_t offset. The forced string
    is the first length characters of body, starting from the offsetth
    character.

 */

typedef enum {
  ava_tt_forced = 0,
  ava_tt_concat,
  ava_tt_tacnoc,
  ava_tt_slice
} ava_twine_tag;

static void assert_aligned(const void* ptr) {
  assert(0 == (size_t)ptr % AVA_STRING_ALIGNMENT);
}

static ava_bool ava_string_can_encode_ascii9(const char*, size_t);
static ava_ascii9_string ava_ascii9_encode(const char*, size_t);
static void ava_ascii9_decode(ava_ulong*restrict dst, ava_ascii9_string);
static size_t ava_ascii9_length(ava_ascii9_string);
static char ava_ascii9_index(ava_ascii9_string, size_t);
static ava_ascii9_string ava_ascii9_concat(ava_ascii9_string,
                                           ava_ascii9_string);
static ava_ascii9_string ava_ascii9_slice(ava_ascii9_string, size_t, size_t);
static ava_bool ava_ascii9_starts_with(ava_ascii9_string big,
                                       ava_ascii9_string small);

/**
 * Allocates a flat, forced twine with space for the given number of
 * characters.
 *
 * The character data begins at &twine->tail. Padding is zeroed.
 */
static ava_twine* ava_twine_alloc(size_t capacity);
/**
 * Forces the given twine, returning a pointer to its character data.
 */
static const char* ava_twine_force(const ava_twine*restrict twine);
/**
 * Forces the given twine node into the given character array.
 *
 * @param dst The destination character array.
 * @param twine The twine to force into the array. This does not memoise the
 * forcing of this node.
 * @param offset The first character in twine to write.
 * @param count The number of characters in twine to write.
 */
static void ava_twine_force_into(char*restrict dst,
                                 const ava_twine*restrict twine,
                                 size_t offset, size_t count);
/**
 * Examines the given (stack-allocated) twine, and forces it if the overhead
 * threshold is exceeded. Otherwise, simply copies it to a heap-allocated
 * twine. In either case, the result is a heap-allocated twine with the same
 * logical value.
 */
static const ava_twine* ava_twine_maybe_force(const ava_twine*restrict);
/**
 * Returns the effective overhead of the given twine.
 *
 * This may be an overestimate.
 */
static size_t ava_twine_get_overhead(const ava_twine*restrict);

static inline ava_twine_tag ava_twine_get_tag(const void* body);
static inline void* ava_twine_get_body_ptr(const void* body);
static const void* ava_twine_pack_body(ava_twine_tag tag,
                                       const void* body_ptr);

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
    ava_ascii9_decode(dst, str.ascii9);
    return (char*)dst;
  } else {
    return ava_twine_force(str.twine);
  }
}

const char* ava_string_to_cstring_buff(ava_str_tmpbuff buff,
                                       ava_string str) {
  if (ava_string_is_ascii9(str)) {
    ava_ascii9_decode(buff, str.ascii9);
    return (const char*)buff;
  } else {
    return ava_twine_force(str.twine);
  }
}

static void ava_ascii9_decode(ava_ulong*restrict dst, ava_ascii9_string s) {
  ava_ulong w0, w1, c0, c1, c2, c3, c4, c5, c6, c7, c8;

  /* This isn't the most efficient way to express things, but for now trust the
   * optimiser to figure things out.
   *
   * TODO: Clang has a really hard time with this one. GCC does an excellent
   * job and reduces it to around two dozen instructions, but clang produces a
   * sprawl of shift and or instructions that looks muck like a literal
   * translation of the C code.
   */
  s >>= 1;
  c8 = s & 0x7F; s >>= 7;
  c7 = s & 0x7F; s >>= 7;
  c6 = s & 0x7F; s >>= 7;
  c5 = s & 0x7F; s >>= 7;
  c4 = s & 0x7F; s >>= 7;
  c3 = s & 0x7F; s >>= 7;
  c2 = s & 0x7F; s >>= 7;
  c1 = s & 0x7F; s >>= 7;
  c0 = s & 0x7F;

#if WORDS_BIGENDIAN
  w1  = c8 << 56;
  w0  = c7 <<  0;
  w0 |= c6 <<  8;
  w0 |= c5 << 16;
  w0 |= c4 << 24;
  w0 |= c3 << 32;
  w0 |= c2 << 40;
  w0 |= c1 << 48;
  w0 |= c0 << 56;
#else
  w1  = c8 << 0;
  w0  = c7 << 56;
  w0 |= c6 << 48;
  w0 |= c5 << 40;
  w0 |= c4 << 32;
  w0 |= c3 << 24;
  w0 |= c2 << 16;
  w0 |= c1 <<  8;
  w0 |= c0 <<  0;
#endif

  dst[0] = w0;
  dst[1] = w1;
}

void ava_string_to_bytes(void*restrict dst, ava_string str,
                         size_t start, size_t end) {
  ava_str_tmpbuff a9buf;
  const void*restrict src;

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
  /* Sum the resulting bits.
   * In the notation below, fields are indexed ordered by shift, rather than
   * character.
   */
#if defined(__GNUC__) && defined(__SSE4_2__)
#ifdef HAVE_NMMINTRIN_H
  return _mm_popcnt_u64(s);
#else
  return __builtin_popcountll(s);
#endif /* HAVE_NMMINTRIN_H */
#else /* Don't have POPCNT */
  /* Shift fields so that the the final accumulation is aligned with bit 0 */
  s >>= 1;

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
#endif /* defined(__GNUC__) && defined(__SSE4_2__) */
}

size_t ava_strlen(ava_string str) {
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

ava_string ava_strcat(ava_string a, ava_string b) {
  size_t alen, blen;
  ava_string ret;

  /* If both are ASCII9 and are small enough to produce an ASCII9 result,
   * produce a new ASCII9 string.
   */
  if (ava_string_is_ascii9(a) && ava_string_is_ascii9(b) &&
      ava_ascii9_length(a.ascii9) + ava_ascii9_length(b.ascii9) <= 9)
    return (ava_string) { .ascii9 = ava_ascii9_concat(a.ascii9, b.ascii9) };

  alen = ava_strlen(a);
  blen = ava_strlen(b);

  /* If one is empty, return the other */
  if (0 == alen) return b;
  if (0 == blen) return a;

  ret.ascii9 = 0;

  if (ava_string_is_ascii9(a) && ava_string_is_ascii9(b)) {
    ava_twine*restrict twine = ava_twine_alloc(alen + blen);
    char*restrict dst = (char*)&twine->tail;
    ava_str_tmpbuff second;

    ava_ascii9_decode((ava_ulong*)dst, a.ascii9);
    ava_ascii9_decode(second, b.ascii9);
    memcpy(dst + alen, second, blen);
    ret.twine = twine;
  } else if (ava_string_is_ascii9(a)) {
    ava_twine twine;

    twine.body = ava_twine_pack_body(ava_tt_tacnoc, b.twine);
    twine.length = alen + blen;
    twine.tail.overhead = sizeof(ava_twine) + ava_twine_get_overhead(b.twine);
    twine.tail.other.string = a;
    ret.twine = ava_twine_maybe_force(&twine);
  } else {
    ava_twine twine;

    twine.body = ava_twine_pack_body(ava_tt_concat, a.twine);
    twine.length = alen + blen;
    twine.tail.overhead = sizeof(ava_twine) + ava_twine_get_overhead(a.twine);
    if (!ava_string_is_ascii9(b))
      twine.tail.overhead += ava_twine_get_overhead(b.twine);
    twine.tail.other.string = b;
    ret.twine = ava_twine_maybe_force(&twine);
  }

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
  ava_twine twine;

  if (ava_string_is_ascii9(str)) {
    ret.ascii9 = ava_ascii9_slice(str.ascii9, begin, end);
    return ret;
  }

  if (0 == begin && ava_strlen(str) == end)
    return str;

  /* Convert to an ASCII9 string if possible */
  if (end - begin <= 9) {
    return ava_string_of_bytes(ava_twine_force(str.twine) + begin, end - begin);
  }

  twine.body = ava_twine_pack_body(ava_tt_slice, str.twine);
  twine.length = end - begin;
  twine.tail.overhead =
    sizeof(ava_twine) + ava_twine_get_overhead(str.twine) +
    begin + (str.twine->length - end);
  twine.tail.other.offset = begin;

  ret.ascii9 = 0;
  ret.twine = ava_twine_maybe_force(&twine);
  return ret;
}

ava_string ava_string_trunc(ava_string str, size_t end) {
  if (ava_string_is_ascii9(str)) {
    str.ascii9 &= (1LL << 63) >> (7 * end) << 1;
    str.ascii9 |= 1;
    return str;
  } else {
    return ava_string_slice(str, 0, end);
  }
}

ava_string ava_string_behead(ava_string str, size_t begin) {
  if (ava_string_is_ascii9(str)) {
    str.ascii9 -= 1;
    str.ascii9 <<= 7 * begin;
    str.ascii9 |= 1;
    return str;
  } else {
    return ava_string_slice(str, begin, ava_strlen(str));
  }
}

signed ava_strcmp(ava_string a, ava_string b) {
  ava_str_tmpbuff atmp, btmp;
  const char*restrict ac, *restrict bc;
  size_t n, alen, blen;
  signed cmp;

  if (ava_string_is_ascii9(a) && ava_string_is_ascii9(b))
    return (a.ascii9 > b.ascii9) - (a.ascii9 < b.ascii9);

  ac = ava_string_to_cstring_buff(atmp, a);
  bc = ava_string_to_cstring_buff(btmp, b);
  alen = ava_strlen(a);
  blen = ava_strlen(b);
  n = alen < blen? alen : blen;
  cmp = memcmp(ac, bc, n);

  if (cmp) return cmp;

  return (alen > blen) - (alen < blen);
}

ava_bool ava_string_equal(ava_string a, ava_string b) {
  if (ava_string_is_ascii9(a) || ava_string_is_ascii9(b)) {
    return ava_string_to_ascii9(a) == ava_string_to_ascii9(b);
  } else {
    size_t alen = ava_strlen(a), blen = ava_strlen(b);
    return alen == blen &&
      0 == memcmp(ava_twine_force(a.twine), ava_twine_force(b.twine),
                  alen);
  }
}

ava_bool ava_string_starts_with(ava_string big, ava_string small) {
  size_t big_len, small_len;
  ava_str_tmpbuff bigtmp, smalltmp;
  const char* bigc, * smallc;

  if (ava_string_is_empty(small))
    return ava_true;

  if (ava_string_is_empty(big))
    return ava_false;

  if (ava_string_is_ascii9(big) && ava_string_is_ascii9(small))
    return ava_ascii9_starts_with(big.ascii9, small.ascii9);

  small_len = ava_strlen(small);
  if (small_len > 9 && ava_string_is_ascii9(big)) return ava_false;

  big_len = ava_strlen(big);
  if (small_len > big_len) return ava_false;

  smallc = ava_string_to_cstring_buff(smalltmp, small);
  bigc = ava_string_to_cstring_buff(bigtmp, big);

  return !memcmp(bigc, smallc, small_len);
}

static ava_bool ava_ascii9_starts_with(ava_ascii9_string big,
                                       ava_ascii9_string small) {
  ava_ascii9_string s;

  /* If big does start with small, it is guaranteed to be >= it */
  if (big < small) return ava_false;

  /* Make mask of present characters in small */
  s = small;
  /* 1111111 => ---1111 */
  s |= s >> 3;
  /* ---1111 => -----11 */
  s |= s >> 2;
  /* -----11 => ------1 */
  s |= s >> 1;
  /* Clear all but those lowest bits */
  s &= 0x204081020408102ULL;
  /* Select only the lowest of those bits */
  s &= -s;

  /* If big starts with small, it will be strictly less than small with the
   * final character incremented, ie, small + s. Since small+s can overflow and
   * produce zero, instead test big-s, which won't underflow since we already
   * know big>=small and small necessarily has at least 1 in that position. (It
   * could be zero if the string were empty and we got here, but then s would
   * be zero as well.)
   */
  return big - s < small;
}

ava_ascii9_string ava_string_to_ascii9(ava_string str) {
  const char* dat;
  size_t len;

  if (ava_string_is_ascii9(str))
    return str.ascii9;

  len = ava_strlen(str);
  if (len > 9)
    return 0;

  dat = ava_twine_force(str.twine);
  if (ava_string_can_encode_ascii9(dat, len))
    return ava_ascii9_encode(dat, len);
  else
    return 0;
}

ssize_t ava_ascii9_index_of_match(ava_ascii9_string a,
                                  ava_ascii9_string b) {
  ava_ascii9_string mask = 0x8102040810204081LL;
  ava_ascii9_string decr;
  ava_ascii9_string xored, decred, masked;
  unsigned bc;

  /* Rotate `a` right 1, so that the high bit is set; shift `b` right 1, so
   * that the high bit is clear.
   */
  a = (a >> 1) | (a << 63);
  b >>= 1;
  /* By XORing b into a, a character slot becomes exactly zero iff both strings
   * had the same character there. The high bit in a remains set.
   */
  xored = a ^ b;
  /* Decrement each character in xored. This has the effect that the zeroth bit
   * of each character is inverted, *except* for characters followed by a zero,
   * since that zero has to borrow for the decrement. If the zeroth character
   * was zero, this clears the high bit.
   *
   * We suppress decrementing characters which have their 1-bit set. This is
   * because that could be their *only* set bit, which could get taken away if
   * the character after them borrows it away.
   */
  decr = mask & ~ xored;
  decred = xored - decr;
  /* Produce a mask of bits which were followed by a zero character. */
  masked = (decred ^ xored ^ decr) & mask;

  if (!masked) return -1;
  bc = __builtin_clzll(masked);
  /* return bc / 7; */
  return (((8LL << 56) | (7LL << 49) | (6LL << 42) |
           (5LL << 35) | (4LL << 28) | (3LL << 21) |
           (2LL << 14) | (1LL <<  7) | (0LL <<  0))
          >> bc) & 0xF;
}

ssize_t ava_strchr(ava_string haystack, char needle) {
  const char* dat, * found;

  if (ava_string_is_ascii9(haystack)) {
    if (!_AVA_IS_ASCII9_CHAR(needle))
      /* By definition, can't be in the string */
      return -1;

    return ava_ascii9_index_of_match(
      haystack.ascii9,
      AVA_ASCII9(needle, needle, needle,
                 needle, needle, needle,
                 needle, needle, needle));
  } else {
    dat = ava_twine_force(haystack.twine);
    found = memchr(dat, needle, ava_strlen(haystack));
    if (found)
      return found - dat;
    else
      return -1;
  }
}

static inline ava_twine_tag ava_twine_get_tag(const void* body) {
  return (ava_intptr)body & 0x7;
}

static inline void* ava_twine_get_body_ptr(const void* body) {
  return (void*)((ava_intptr)body & ~(ava_intptr)0x7);
}

static const void* ava_twine_pack_body(ava_twine_tag tag,
                                       const void* body_ptr) {
  assert_aligned(body_ptr);
  return (const void*)(tag + (ava_intptr)body_ptr);
}

static const char* ava_twine_force(const ava_twine*restrict twine) {
  const void*restrict body =
    (const void*)AO_load_acquire_read((const AO_t*)&twine->body);

  if (AVA_LIKELY(ava_tt_forced == ava_twine_get_tag(body))) {
    return ava_twine_get_body_ptr(body);
  } else {
    size_t base_sz = twine->length;
    /* Include space for the terminating NUL plus padding so the size is a
     * multiple of sizeof(ava_ulong).
     */
    size_t full_sz = (base_sz + sizeof(ava_ulong)) /
      sizeof(ava_ulong) * sizeof(ava_ulong);
    char*restrict dst = ava_alloc_atomic(full_sz);

    /* Zero the padding, including the terminating NUL */
    memset(dst + base_sz, 0, full_sz - base_sz);

    ava_twine_force_into(dst, twine, 0, twine->length);

    /* Write the new body first, so that any concurrent readers either see the
     * new body first, or complete reading the node in its original state.
     * Write-release so that other readers can see the contents of dst.
     */
    AO_store_release_write((AO_t*)&twine->body,
                           (AO_t)(ava_twine_pack_body(ava_tt_forced, dst)));
    /* Destroy other so that any memory it holds is freed */
    ((ava_twine*restrict)twine)->tail.other.string.ascii9 = 0;

    return dst;
  }
}

typedef struct {
  char*restrict dst;
  const ava_twine*restrict twine;
  size_t offset, count;
} ava_twine_force_into_return;

static void ava_twine_force_into(char*restrict dst,
                                 const ava_twine*restrict twine,
                                 size_t offset, size_t count) {
  /* In order to run in constant stack space (since the twine tree may be very
   * deep), we maintain an explicit stack for the rhs of concats that is
   * allocated on the heap if too large.
   */
  ava_twine_force_into_return stack_stack[1024];
  ava_twine_force_into_return* stack, * stack_base;
  ava_twine_force_into_return* stack_ceil AVA_UNUSED;
  ava_twine_tail_other other;
  ava_twine_tag tag;
  const void* body;
  const ava_twine*restrict body_twine;
  size_t max_stack_required;
  size_t a9len;
  ava_str_tmpbuff a9tmp;

  /* Estimate how much stack space will be required.
   *
   * In the worst case (a left-handed linear chain of concats), we'll need one
   * stack slot for every node. The upper bound on the number of nodes is the
   * overhead over the node size.
   *
   * In the case of substrings, this may substantially overestimate the size
   * required. However, note that the maximum amount it will allocate is still
   * proportional to the length of the string.
   */
  max_stack_required = ava_twine_get_overhead(twine) / sizeof(ava_twine);

  if (max_stack_required <= sizeof(stack_stack) / sizeof(stack_stack[0])) {
    /* Small enough to fit on the stack */
    stack = stack_base = stack_stack;
  } else {
    /* Too big to be safely on the stack, use the heap instead */
    stack = stack_base = ava_alloc_unmanaged(
      max_stack_required * sizeof(stack[0]));
  }
  stack_ceil = stack + max_stack_required;

  tailcall:
  /* Read the twine into a stable location */
  other = twine->tail.other;
  /* Atomically read the tag and body */
  body = (const void*)AO_load_acquire_read((const AO_t*)&twine->body);
  tag = ava_twine_get_tag(body);
  body = ava_twine_get_body_ptr(body);

  switch (tag) {
  case ava_tt_forced:
    /* other is indeterminate, but we don't care what it is anyway */
    memcpy(dst, (const char*restrict)body + offset, count);
    break;

  case ava_tt_concat:
    body_twine = body;
    if (offset + count <= body_twine->length) {
      /* Only need the left child */
      twine = body_twine;
      goto tailcall;
    } else if (offset >= body_twine->length) {
      /* Only need the right child */
      if (ava_string_is_ascii9(other.string)) {
        ava_ascii9_decode(a9tmp, other.string.ascii9);
        memcpy(dst, (const char*)a9tmp + offset - body_twine->length, count);
      } else {
        twine = other.string.twine;
        offset -= body_twine->length;
        goto tailcall;
      }
    } else {
      /* Need to inspect both sides.
       *
       * If the right side is ASCII9, decode it now so we don't need to suport
       * returning to an ASCII9 string.
       */
      if (ava_string_is_ascii9(other.string)) {
        ava_ascii9_decode(a9tmp, other.string.ascii9);
        memcpy(dst + body_twine->length - offset,
               a9tmp, count - body_twine->length);
      } else {
        /* Save the right-hand-side for later */
        assert(stack < stack_ceil);
        stack->dst = dst + body_twine->length - offset;
        stack->twine = other.string.twine;
        stack->offset = 0;
        stack->count = count + offset - body_twine->length;
        ++stack;
      }

      /* Recurse to the left */
      twine = body_twine;
      count = twine->length - offset;
      goto tailcall;
    }
    break;

  case ava_tt_tacnoc:
    assert(ava_string_is_ascii9(other.string));
    a9len = ava_ascii9_length(other.string.ascii9);

    if (offset < a9len) {
      ava_ascii9_decode(a9tmp, other.string.ascii9);
      memcpy(dst, (const char*)a9tmp + offset,
             count > a9len - offset? a9len - offset : count);
    }

    if (offset + count > a9len) {
      twine = body;
      dst += offset > a9len? 0 : a9len - offset;
      count = offset > a9len? count : count - (a9len - offset);
      offset = offset > a9len? offset - a9len : 0;
      goto tailcall;
    }
    break;

  case ava_tt_slice:
    offset += other.offset;
    twine = body;
    goto tailcall;

  default: abort();
  }

  /* If there are any remaining return points on the stack, loop back to them
   * now.
   */
  if (stack != stack_base) {
    assert(stack > stack_base);
    --stack;
    dst = stack->dst;
    twine = stack->twine;
    offset = stack->offset;
    count = stack->count;
    goto tailcall;
  }

  /* If the stack was heap-allocated, release it */
  if (stack_stack != stack_base)
    ava_free_unmanaged(stack_base);
}

static size_t ava_twine_get_overhead(const ava_twine*restrict twine) {
  /* It is safe to read the body even non-atomically. Either the twine has
   * always been forced (so we get a forced tag regardless and always ignore
   * the undefined overhead field), or it was non-forced at some time and still
   * retains the same overhead field from that time (so worst-case we still
   * return a valid overhead, even if the platform can give non-trivial read
   * results).
   */
  if (ava_tt_forced == ava_twine_get_tag(twine->body))
    return 0;
  else
    return twine->tail.overhead;
}

static const ava_twine* ava_twine_maybe_force(const ava_twine*restrict twine) {
  ava_twine*restrict heap_twine;

  if (twine->tail.overhead > twine->length) {
    heap_twine = ava_twine_alloc(twine->length);
    ava_twine_force_into((char*)&heap_twine->tail, twine, 0, twine->length);
    return heap_twine;
  } else {
    return ava_clone(twine, sizeof(ava_twine));
  }
}
