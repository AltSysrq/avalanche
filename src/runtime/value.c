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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_BSD_STDLIB_H
/* for arc4random on GNU systems */
#include <bsd/stdlib.h>
#endif

#ifdef HAVE_NMMINTRIN_H
#include <nmmintrin.h>
#endif

#define AVA__INTERNAL_INCLUDE 1
#define AVA_IN_VALUE_C
#include "avalanche/string.h"
#include "avalanche/value.h"

#ifndef AVA_SIPHASH_C
/**
 * The number of hash rounds per data element.
 */
#define AVA_SIPHASH_C 2
#endif

#ifndef AVA_SIPHASH_D
/**
 * The number of finishing hash rounds.
 */
#define AVA_SIPHASH_D 4
#endif

const ava_attribute_tag ava_value_trait_tag = {
  .name = "generic"
};

const void* ava_get_attribute(ava_value value,
                              const ava_attribute_tag*restrict tag) {
  const ava_attribute*restrict attr;

  for (attr = ava_value_attr(value); attr; attr = attr->next)
    if (tag == attr->tag)
      return attr;

  return NULL;
}

ava_string ava_string_of_chunk_iterator(ava_value value) {
  ava_string accum = AVA_EMPTY_STRING, chunk;
  ava_datum iterator;

  iterator = ava_string_chunk_iterator(value);
  while (ava_string_is_present(
           (chunk = ava_iterate_string_chunk(&iterator, value))))
    accum = ava_string_concat(accum, chunk);

  return accum;
}

ava_datum ava_singleton_string_chunk_iterator(ava_value value) {
  return ava_string_to_datum(ava_to_string(value));
}

ava_string ava_iterate_singleton_string_chunk(ava_datum*restrict it,
                                              ava_value value) {
  ava_string ret = ava_string_of_datum(*it);
  *it = ava_string_to_datum(AVA_ABSENT_STRING);
  return ret;
}

static ava_string ava_string_value_to_string(ava_value value) AVA_CONSTFUN;
static ava_string ava_string_value_to_string(ava_value value) {
  return ava_value_str(value);
}

const ava_value_trait ava_string_type = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "string",
  .to_string = ava_string_value_to_string,
  .string_chunk_iterator = ava_singleton_string_chunk_iterator,
  .iterate_string_chunk = ava_iterate_singleton_string_chunk,
};

/* The string representation will become a bit more interesting once we have
 * caching for large values.
 */
ava_value ava_value_of_string(ava_string str) {
  return ava_value_with_str(&ava_string_type, str);
}

ava_value ava_value_of_cstring(const char* str) {
  return ava_value_of_string(ava_string_of_cstring(str));
}

ava_bool ava_value_equal(ava_value a, ava_value b) {
  return 0 == ava_value_strcmp(a, b);
}

signed ava_value_strcmp(ava_value a, ava_value b) {
  ava_datum ait, bit;
  ava_string as, bs;
  const char*restrict ac, *restrict bc;
  char atmp[AVA_STR_TMPSZ], btmp[AVA_STR_TMPSZ];
  size_t nac, nbc, n;
  ava_bool a_finished = ava_false, b_finished = ava_false;
  signed cmp;

  /* If both values are byte-for-byte the same, we need not actually inspect
   * them deeply; they're definitely equal.
   */
  if (0 == memcmp(&a, &b, sizeof(a)))
    return 0;


  ait = ava_string_chunk_iterator(a);
  bit = ava_string_chunk_iterator(b);
  nac = nbc = 0;

  for (;;) {
    if (!nac) {
      do {
        as = ava_iterate_string_chunk(&ait, a);
      } while (ava_string_is_present(as) &&
               0 == ava_string_length(as));

      if (!ava_string_is_present(as)) {
        a_finished = ava_true;
      } else {
        nac = ava_string_length(as);
        ac = ava_string_to_cstring_buff(atmp, as);
      }
    }

    if (!nbc) {
      do {
        bs = ava_iterate_string_chunk(&bit, b);
      } while (ava_string_is_present(bs) &&
               0 == ava_string_length(bs));

      if (!ava_string_is_present(bs)) {
        b_finished = ava_true;
      } else {
        nbc = ava_string_length(bs);
        bc = ava_string_to_cstring_buff(btmp, bs);
      }
    }

    if (a_finished || b_finished) break;

    n = nac < nbc? nac : nbc;

    cmp = memcmp(ac, bc, n);
    if (cmp) return cmp;

    ac += n;
    bc += n;
    nac -= n;
    nbc -= n;
  }

  /* One is a prefix of the other */
  /* The perhaps odd return values below are chosen to make it more likely code
   * switch()ing on the return value is discovered to be broken on platforms
   * where memcmp() returns only +1 and -1.
   */
  if (!a_finished)
    /* b is shorter */
    return +4;
  else if (!b_finished)
    /* a is shorter */
    return -3;
  else
    /* Equal */
    return 0;
}

static ava_ulong ava_siphash_k[2];

void ava_value_hash_init(void) {
#ifdef HAVE_ARC4RANDOM_BUF
  arc4random_buf(ava_siphash_k, sizeof(ava_siphash_k));
#else
#warning "Don't know how to generate good random numbers on your platform."
#warning "Pretending rand() is good enough. (It isn't.)"
#warning "Please do not distribute binary copies of this build."
  /* The hash function is correct regardless of the key used, but we get better
   * resistence to malicious collisions if the key is hard to predict.
   *
   * Fall back on rand() seeded with the current time, and mix the current time
   * and clock into the two key parts.
   *
   * This is certainly rather dubious, but it's not like we're generating
   * encryption keys with it; it just makes DoS attacks against certain classes
   * of applications easier.
   */
  ava_ulong k0, k1;
  srand(time(0) ^ clock());
  /* Some platforms only have 15-bit rand() outputs */
  k0 = ((ava_ulong)rand() <<  0)
     ^ ((ava_ulong)rand() << 15)
     ^ ((ava_ulong)rand() << 30)
     ^ ((ava_ulong)rand() << 45)
     ^ ((ava_ulong)rand() << 60);
  k1 = ((ava_ulong)rand() <<  0)
     ^ ((ava_ulong)rand() << 15)
     ^ ((ava_ulong)rand() << 30)
     ^ ((ava_ulong)rand() << 45)
     ^ ((ava_ulong)rand() << 60);
  k0 ^= time(0);
  k1 ^= clock();
  ava_siphash_k[0] = k0;
  ava_siphash_k[1] = k1;
#endif
}

static inline ava_ulong rotl(ava_ulong x, ava_ulong b) {
  return (x << b) | (x >> (64 - b));
}

static ava_ulong ava_value_siphash(ava_value value, ava_ulong k0, ava_ulong k1);

ava_ulong ava_value_hash(ava_value value) {
  return ava_value_siphash(value, ava_siphash_k[0], ava_siphash_k[1]);
}

ava_ulong ava_value_hash_semiconsistent(ava_value value) {
  /* Chosen randomly */
  return ava_value_siphash(value, 0xE62C3CBEF7BC1A5DULL, 0xE7079F4159060EE8ULL);
}

static ava_ulong ava_value_siphash(ava_value value,
                                   ava_ulong k0, ava_ulong k1) {
  /* Adapted from https://github.com/veorq/SipHash
   *
   * More specifically,
   * https://github.com/veorq/SipHash/blob/1b85a33b71f0fdd49942037a503b6798d67ef765/siphash24.c
   *
   * This is not strictly SipHash, since on big-endian systems each group of 8
   * characters is reversed. This doesn't affect the soundness of the
   * algorithm, though, as it's equivalent to passing the string through a 1:1
   * function first.
   */

  ava_string str;
  /* Constants and vars, from lines 69--76 */
  ava_ulong
    v0 = 0x736f6d6570736575ULL, v1 = 0x646f72616e646f6dULL,
    v2 = 0x6c7967656e657261ULL, v3 = 0x7465646279746573ULL,
    b, m;
  size_t i, n, rem, strlen;
  char tmpbuf[16] AVA_STR_ALIGN = { 0 };
  const ava_ulong*restrict data;

  /* Lines 42--48 */
#define SIPROUND() do {                                         \
    v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);   \
    v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;                      \
    v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;                      \
    v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);   \
  } while (0)
#define SIPROUNDS(nrounds) do {                 \
    unsigned round;                             \
    for (round = 0; round < (nrounds); ++round) \
      SIPROUND();                               \
  } while (0)

  /* Generally, only strings and short data will be hashed. Under this
   * assumption, err on the side of simplicity and just stringify the whole
   * value; this way, we can just read from the string iterator 8 bytes at a
   * time and not worry about chunk boundaries.
   */
  str = ava_to_string(value);
  strlen = ava_string_length(str);
  data = (const ava_ulong*)ava_string_to_cstring_buff(tmpbuf, str);
  n = strlen / sizeof(ava_ulong);
  rem = strlen % sizeof(ava_ulong);

  /* Mix key with initialisation vector, and initialise b.
   *
   * Lines 80--84
   */
  v3 ^= k1;
  v2 ^= k0;
  v1 ^= k1;
  v0 ^= k0;
  b = (ava_ulong)strlen << 56;

  /* Read each full chunk and feed it through the hash algorithm.
   *
   * This is mainly lines 90--98.
   */
  for (i = 0; i < n; ++i) {
    m = data[i];
    v3 ^= m;
    SIPROUNDS(AVA_SIPHASH_C);
    v0 ^= m;
  }

  /* Lines 101--111, mix the last few bytes with b if the input size wasn't a
   * multiple sizeof(ava_ulong).
   */
  if (rem) b |= data[i];

  /* Finishing rounds */
  v3 ^= b;
  SIPROUNDS(AVA_SIPHASH_C);
  v0 ^= b;

  SIPROUNDS(AVA_SIPHASH_D);
  return v0 ^ v1 ^ v2 ^ v3;
}

ava_uint ava_ascii9_hash(ava_ascii9_string str) {
  /* Hashing ASCII9 values is a bit difficult, since the lower bits are often
   * all zeroes.
   *
   * When reasonably possible, return a CPU-provided CRC of the value.
   * Otherwise fall back on a simple algorithm that produces reasonably
   * well-distributed lower bits.
   *
   * All results include the siphash key, even if it does not improve
   * collision-resistence, so that this function will always return different
   * results in different processes (ie, to catch programming errors assuming
   * otherwise).
   *
   * This is designed for speeed for use in a hash table with no specific value
   * distribution than to be resistent to hash-collision attacks.
   */

  ava_ulong k0 = ava_siphash_k[0];
  unsigned h;
#if defined(__GNUC__) && defined(__SSE4_2__)
#ifdef HAVE_NMMINTRIN_H
  h = _mm_crc32_u64(k0, str);
#else
  h = __builtin_ia32_crc32di(k0, str);
#endif /* HAVE_NMMINTRIN_H */
  /* Mix the upper 16 bits with the lower 16 bits, since the entropy is better
   * there.
   */
  h += h >> 16;
  return h;
#else /* Not gcc/clang or don't have SSE4.2 */
  h = str;
  h ^= str >> 32;
  h ^= k0;

  /* Thomas Wang's algorithm from
   * http://burtleburtle.net/bob/hash/integer.html
   */
  h += ~(h << 15);
  h ^=  (h >> 10);
  h +=  (h << 3);
  h ^=  (h >> 6);
  h += ~(h << 11);
  h ^=  (h >> 16);
  return h;
#endif
}
