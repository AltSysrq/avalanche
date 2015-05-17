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

#define AVA__INTERNAL_INCLUDE 1
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

const ava_attribute_tag ava_generic_trait_tag = {
  .name = "generic"
};

const void* ava_get_attribute(ava_value value,
                              const ava_attribute_tag*restrict tag) {
  const ava_attribute*restrict attr;

  for (attr = value.attr; attr; attr = attr->next)
    if (tag == attr->tag)
      return attr;

  return NULL;
}

ava_string ava_string_of_chunk_iterator(ava_value value) {
  /* Produce a near-optimal string by concatting the chunks in a perfect binary
   * tree instead of left-to-right.
   *
   * (This is only near-optimal since it assumes no coalescing occurs.)
   */
  ava_string accum[AVA_MAX_ROPE_DEPTH];
  unsigned height[AVA_MAX_ROPE_DEPTH];
  unsigned count = 0;
  ava_string chunk;
  ava_datum iterator;

  iterator = ava_string_chunk_iterator(value);

  while (ava_string_is_present(
           (chunk = ava_iterate_string_chunk(&iterator, value)))) {
    accum[count] = chunk;
    height[count] = 0;
    ++count;

    while (count > 1 && height[count-2] == height[count-1]) {
      accum[count-2] = ava_string_concat(accum[count-2], accum[count-1]);
      ++height[count-2];
      --count;
    }
  }

  if (0 == count)
    return AVA_EMPTY_STRING;

  /* Concat whatever remains right-to-left, since the right-hand-side currently
   * has smaller trees.
   */
  while (count > 1) {
    accum[count-2] = ava_string_concat(accum[count-2], accum[count-1]);
    --count;
  }

  return accum[0];
}

ava_datum ava_singleton_string_chunk_iterator(ava_value value) {
  return (ava_datum) { .str = ava_to_string(value) };
}

ava_string ava_iterate_singleton_string_chunk(ava_datum*restrict it,
                                              ava_value value) {
  ava_string ret = it->str;
  it->str = AVA_ABSENT_STRING;
  return ret;
}

static ava_string ava_string_value_to_string(ava_value value) AVA_CONSTFUN;
static ava_string ava_string_value_to_string(ava_value value) {
  return value.r1.str;
}

static size_t ava_string_value_value_weight(ava_value value) {
  return ava_string_length(value.r1.str);
}

static const ava_generic_trait ava_string_generic_impl = {
  .header = { .tag = &ava_generic_trait_tag, .next = NULL },
  .name = "string",
  .to_string = ava_string_value_to_string,
  .string_chunk_iterator = ava_singleton_string_chunk_iterator,
  .iterate_string_chunk = ava_iterate_singleton_string_chunk,
  .value_weight = ava_string_value_value_weight,
};

/* The string representation will become a bit more interesting once we have
 * caching for large values.
 */
ava_value ava_value_of_string(ava_string str) {
  return (ava_value) {
    .r1 = { .str = str },
    .r2 = { .ulong = 0 },
    .attr = (const ava_attribute*)&ava_string_generic_impl,
  };
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
  ava_string_iterator asit, bsit;
  const char*restrict ac, *restrict bc;
  char atmp[9], btmp[9];
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
  ava_string_iterator_place(&asit, AVA_EMPTY_STRING, 0);
  ava_string_iterator_place(&bsit, AVA_EMPTY_STRING, 0);

  for (;;) {
    if (!ava_string_iterator_valid(&asit)) {
      do {
        as = ava_iterate_string_chunk(&ait, a);
      } while (ava_string_is_present(as) &&
               0 == ava_string_length(as));

      if (!ava_string_is_present(as))
        a_finished = ava_true;
      else
        ava_string_iterator_place(&asit, as, 0);
    }

    if (!ava_string_iterator_valid(&bsit)) {
      do {
        bs = ava_iterate_string_chunk(&bit, b);
      } while (ava_string_is_present(bs) &&
               0 == ava_string_length(bs));

      if (!ava_string_is_present(bs))
        b_finished = ava_true;
      else
        ava_string_iterator_place(&bsit, bs, 0);
    }

    if (a_finished || b_finished) break;

    nac = ava_string_iterator_access(&ac, &asit, atmp);
    nbc = ava_string_iterator_access(&bc, &bsit, btmp);
    n = nac < nbc? nac : nbc;

    cmp = memcmp(ac, bc, n);
    if (cmp) return cmp;

    ava_string_iterator_move(&asit, n);
    ava_string_iterator_move(&bsit, n);
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

ava_ulong ava_value_hash(ava_value value) {
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
  ava_string_iterator sit;
  /* Constants and vars, from lines 69--76 */
  ava_ulong
    k0 = ava_siphash_k[0], k1 = ava_siphash_k[1],
    v0 = 0x736f6d6570736575ULL, v1 = 0x646f72616e646f6dULL,
    v2 = 0x6c7967656e657261ULL, v3 = 0x7465646279746573ULL,
    b, m;
  size_t read;

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
  ava_string_iterator_place(&sit, str, 0);

  /* Mix key with initialisation vector, and initialise b.
   *
   * Lines 80--84
   */
  v3 ^= k1;
  v2 ^= k0;
  v1 ^= k1;
  v0 ^= k0;
  b = ((ava_ulong)ava_string_length(str)) << 56;

  /* Read each full chunk and feed it through the hash algorithm.
   *
   * This is mainly lines 90--98. We zero m first so that the switch statement
   * on lines 101--111 can be simplified.
   */
  while (m = 0, sizeof(m) == (read = ava_string_iterator_read_fully(
                                (char*)&m, sizeof(m), &sit))) {
    v3 ^= m;
    SIPROUNDS(AVA_SIPHASH_C);
    v0 ^= m;
  }

  /* Lines 101--111, mix the last few bytes with b if the input size wasn't a
   * multiple sizeof(ava_ulong). The above loop is structured so that m is
   * already in the correct state for this. In fact, we can elide even the
   * branch since it will simply be 0 if the input size was a multiple of
   * sizeof(ava_ulong).
   */
  b |= m;

  /* Finishing rounds */
  v3 ^= b;
  SIPROUNDS(AVA_SIPHASH_C);
  v0 ^= b;

  SIPROUNDS(AVA_SIPHASH_D);
  return v0 ^ v1 ^ v2 ^ v3;
}
