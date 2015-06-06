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
#include <stdlib.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/map.h"

/*

  Implementation proper of cuckoo (hash) maps. This file is meant to be
  included by the other cuckoo-map-*.c files, with certain symbols defined to
  configure this particular implementation.

  The Cuckoo Map provides a thread-safe, persistent, ordered multimap, with
  strict O(1) access time to the latest version, amortised O(1) access to
  earlier versions, and effectively amortised O(1) insertion, update, and
  delete operations. (From a strictly theoretic standpoint, mutations are *not
  known* to be guaranteed to be amortised O(1) since the "strong" hash function
  family is only 3-independent; however, it has been suggested that having
  random inputs removes the need for c*log(n)-independence, and in any case
  even 2-independent hash function families work well in practise.)

  The cuckoo map can be thought of as three mostly independent layers:

  - The physical layer implements the hash table proper. It is oblivious to
    such concerns as concurrency, versioning, or value semantics; it simply
    provides an integer-key-to-memory-address mapping. The physical layer
    neither preserves order nor supports mapping the same key to more than one
    value, or even deleting values. All notes regarding the physical layer
    pretend the program is single-threaded.

  - The persistence layer provides concurrency and version control. It does not
    know about value semantics, but merely maps "keys" (hashes, actually) to
    memory addresses, with an API structurally similar to ava_map.

  - The value layer implements proper value semantics on top of the persistence
    layer, such as marshalling ava_values into and out of their compressed
    format and properly testing keys for equality. It implements the
    ava_map_trait proper, as well as the ava_list_trait.

  "Implementation parameters" refer to parameters provided by the dummy
  implementation file that included this file. "Dependent parameters" are
  parameters used by one layer that are defined by an upper layer.

  ----

  PHYSICAL LAYER

  Recommended reading:

  - http://en.wikipedia.org/wiki/Cuckoo_hashing
  - http://www.eecs.harvard.edu/~michaelm/postscripts/esa2009.pdf

  Implementation parameters:

  - AVA_CUCKOO_STRONG_HASH. If true, tabulation hashing is used. Otherwise, a
    weaker but faster hash function is used.

  Dependent parameters:

  - phys_hash. Type used for output for hashing in the physical layer. Must be
    either ava_uint or ava_ulong.

  - pelement_is_present(). A function that, given a pointer to a physical
    key-value pair, can determine whether anything is present in that slot.

  - PHYS_LAST_WORD_MASK. A ulong mask to apply to the final ulong in a physical
    key before hashing it.

  Physical layout:

     ____________________________________________________________
    /   _____________                                            \
    |  /             \                                           |
    |  |             v                                           v
    [ava_cuckoo_phys][low-elements........][high-elements.......][stash]
    |                                      ^
    \______________________________________/

  low-elements and high-elements are two flat arrays of (key,value) pairs (as
  "keys" and "values" are considered by the physical layer) of equal size,
  which is a power of two times the size of a key-value pair. Stash is a much
  smaller flat array of key-value pairs.

  Each element "belongs" to one bucket in each of the low and high arrays, and
  each bucket has space for two elements. The choice of two-element buckets was
  made for two reasons: (a) it greatly reduces the probability of needing to
  rehash the table, and (b) in most element formats, at least two elements fit
  within the same cache line, so the extra check is very cheap.

  The stash is used as a holding space for elements which would be part of a
  hash cycle were they in the low or high arrays, in order to reduce liklihood
  of needing a rehash. The stash has no particular ordering; elements exist in
  the order they were added. As such, it is never large. The actual size is
  determined from sizeof(phys_hash).

  In the case of AVA_CUCKOO_STRONG_HASH, each ava_cuckoo_map has its own
  pair tabulation hashing tables. Otherwise, the two hash functions are
  hard-wired into the code.

  The procedure for inserting an element is as follows:

  - Feed the key of the element through both hash functions, to produce H1 and
    H2.

  - If ((H1 ^ H2) & 1), set TA=low,TB=high,THA=H1,THB=H2, otherwise set
    TA=high,TB=low,THA=H2,THB=H1. Truncate THA and THB to the length of the low
    and high arrays.

  - Check for empty elements in the following locations:
    TA[THA], TA[THA^1], TB[THB], TB[THB^1].

  - If an empty element is found, populate it and return.

  - Otherwise, pick one of the four locations at random and replace whatever is
    there.

  - Reinsert the element that was removed, but restrict it to use the other
    array (ie, if the new element is inserted into high, the reinsertion will
    only inspect low).

  - Reinsertion fails after C*log(n) cycles, give up and append the last victim
    to the stash.

  - If the stash becomes full, instruct the calling layer that a hash function
    change and rehash is required.

  Value-set operations are performed by simply overwriting the desired value
  in-place.

  The physical layer does not support element deletion.

  Element lookup is performed as follows:

  - Feed the key of the element through both hash functions, to produce H1 and
    H2.

  - If ((H1 ^ H2) & 1), set TA=low,TB=high,THA=H1,THB=H2, otherwise set
    TA=high,TB=low,THA=H2,THB=H1. Truncate THA and THB to the length of the low
    and high arrays.

  - Check for the desired element in the following locations:
    TA[THA], TA[THA^1], TB[THB], TB[THB^1]; if found, return success.

  - Linearly search the stash for the desired element; if found, return
    success.

  - Return failure.

  Notice that positive lookups almost always require inspecting at most four
  elements in (usually) two cache lines.

  While the physical layer does not know about concurrency, it does not follow
  pointers/offsets within the data it maintains, so it is safe to perform read
  operations in the presence of unsynchronised writers, though the output of
  doing so is indeterminate.

  The two highest bits of the first ulong in a physical layer key are ignored
  for hashing.

  ---

  PERSISTENCE LAYER

  Implementation parameters:

  - AVA_CUCKOO_PERSISTENCE_FORMAT. Indicates the layout of information used by
    the persistence layer. One of the following values:

    - PERSISTENCE_FORMAT_TINY. 30-bit key, 8-bit sequence, 12-bit generation.
      Supports maps with up to 2048 slots and up to 255 identical keys.
      Overhead of 1 qword.

    - PERSISTENCE_FORMAT_COMPACT. 30-bit key, 32-bit sequence, 32-bit
      generation. Supports maps with up to 4 billion (2**32) slots and up to 4
      billion (2**32-1) identical keys, though it is not used for more than
      2**24 elements due to the hash size. Overhead of 2 qwords.

    - PERSISTENCE_FORMAT_WIDE. 62-bit key, 64-bit sequence, 64-bit generation.
      Used for any map that cannot fit in the other formats. Overhead of 4
      qwords.

  "Keys" at the persistence layer are simply the 32- or 64-bit hashes of the
  actual keys in the map proper; thus, "identical keys" at the persistence
  level also includes logical keys with different values that happen to have
  the same hash value.

  Concurrency and persistence are both managed using _generation numbers_.
  Every value referencing a cuckoo map has in its ulong the generation number
  of the map it references. This allows readers to correctly identify the data
  belonging to the version they hold, and for prospective writers to determine
  whether they are actually operating against the latest version.

  The persistence layer's structure (a simple prefix to the physical layer)
  stores two generation numbers: The effective generation, and the active
  generation. A reader begins by reading the effective generation. When the
  reader wants to act upon the data it acquired (including dereferencing
  pointers, or anything else requiring determinite values), it must check that
  the active generation still matches what it had read for the effective
  generation; if it does not, the result of the read is indeterminate and the
  reader must try again. Writers obtain exclusive write access by atomically
  CaSing the active generation from the generation number on their reference to
  its successor; if they fail, they either hold an old reference or are
  contending with another writer, and must first copy the map. The generation
  number of zero is special, and always refers to the "latest" version.

  Multimap semantics are provided by sequence numbers; a sequence number N on
  an element indicates it is the Nth element in map order with that key.

  Keys presented to the physical layer are 3-tuples of the form
  (key,sequence,generation); the payload contains (prev_generation,data), where
  data is the payload of the value layer. The upper 2 bits of key are used to
  indicate the type of the element:

  - 00: Absent element.

  - 01: Data element: The payload contains a normal data element visible to the
    value layer. prev_generation referes to the generation of the previous
    value for this slot, or 0 if there is no previous value.

  - 10: Tombstone: The payload contains no data; rather, it indicates there
    used to be a data element here. prev_generation has the same meaning as for
    a data element. The tombstone allows readers of older versions which still
    had the element to find it, and so that deletions do not need to resequence
    all subsequent elements.

  - 11: Shortcut: The payload contains no data. prev_generation is not a
    generation reference, but rather the sequence of the final element in the
    "latest" version which has the same key. This allows appenders to insert
    duplicate keys in constant time; it is not useful for readers, which must
    simply retry with the next sequence number.

  Operations on the persistence layer are described below. Write operations
  asume that exclusive write access has been obtained; rehashing when
  constraints are violated is implicit. Read operations imply the optimistic
  transaction around them.

  Append operations first check whether a value exists at (key,0,0). If there
  is none, one is created, and the append completes. If there is one, the
  element at (key,1,0) is read. If it does not exist, a shortcut is written
  there, referencing a sequence number 2, and the new value is written to
  (key,2,0). If the value at (key,1,0) does exist (which implies it is a
  Shortcut), the new element is inserted at (key,Shortcut+1,0), and the
  prev_generation value on the Shortcut element is incremented in-place. The
  prev_generation of inserted elements is the generation of the writer (which
  allows readers to determine that the element used not to be there).

  Edit operations take the value at (key,N,0) and move it to
  (key,N,G), where G is the generation of the writer. The original (key,N,0) is
  then rewritten with the new value, and a prev_generation of G.

  Deletions work the same as Edits, except they write a Tombstone instead of a
  proper element.

  Reads for some (key,N,G), where N is the desired sequence and G is the
  generation of the reader reference, begin by looking (key,N,0) up. If G >=
  prev_generation of the data, that entry is current. Otherwise, the lookup
  repeats with (key,N,prev_generation), until such an element is found, or
  prev_generation is found to be 0. If a physical lookup is negative, so is the
  persistence lookup.

  Whenever the persistence layer is reallocated for any reason, it _vacuums_
  the resulting table. This means that all tombstones and obsolete versions are
  not retained in the new table. This also results in the elimination of
  unnecessary Shortcuts as a matter of course. After the reallocation, the
  generation resets to 1.

  ---

  VALUE LAYER

  Implementation paremeters:

  - AVA_CUCKOO_POLYMORPHIC_KEY_ATTR
  - AVA_CUCKOO_POLYMORPHIC_VALUE_ATTR
  - AVA_CUCKOO_POLYMORPHIC_VALUE_DATA. Indicate whether the attribute and data
    fields of the keys and values are polymorphic, ie, need to be stored within
    the table proper. Monomorphic fields are stored in singular templates. This
    is essentially the same optimisation performed by the ESBA list. Note that
    keys are always assumed to have polymorphic data.
  - AVA_CUCKOO_POLYMORPHIC_VALUE_ATTR is always set if
  - AVA_CUCKOO_POLYMORPHIC_VALUE_DATA is set.

  - AVA_CUCKOO_ASCII9_HASH. If set (which is only permissible with
    PERSISTENCE_FORMAT_TINY), all keys are ASCII9 string values, and they are
    hashed with ava_ascii9_hash(). Otherwise, ava_value_hash() is used. This
    ony makes sense when AVA_CUCKOO_POLYMORPHIC_KEY_ATTR is unset.

  Physical layout:

     /---->[attribute-chain]
     |
    [ava_cuckoo_map]---->[order-array................]
     |   |
     |   \-->{persistence layer}
     |
     \--?->[list-index-array................]

  Map implementations at the physical layer are themselves relatively simple,
  based upon the persistence layer. The payload for the persistence layer is
  the key-value pair proper, subject to swizzling of monomorphic fields into
  templates, which are stored in the root object itself. Values returned by the
  persistence layer are compared for full key equality before being returned
  from the value layer.

  In order to preserve order and efficiently implement the list interface, the
  value layer defines two additional structures: the order array and the
  list-index array.

  The order array is guarded by the concurrency control provided by the
  persistence layer. It contains (hash,seq) pairs into the persistence layer,
  reflecting the order in which the key-value pairs were actually inserted. The
  order array is append-only. Any two ava_cuckoo_maps which reference the same
  order array also reference the same persistence layer, though the converse is
  not true; any two ava_cuckoo_maps which reference the same persistence layer
  either reference the same order array, or one's order array is a prefix of
  the other's. In order for readers to discover where the end of the array is,
  each entry also has the generation number at which it was inserted, which is
  initialised to ~0; this means readers do not need to worry about writer
  interference when reading the order array, since the timestamp can only be
  observed to be values greater than their own timestamp.

  The list-index array is a table of indices into the order array. It is built
  on-demand, and is not necessarily long enough to reflect the full length of
  the map. Any two ava_cuckoo_maps which reference the same list-index array
  also reference the same order array and retain the same number of deleted
  elements within the order array.

  Any operation which triggers a reallocation of the persistence layer also
  requires a reallocation of the value layer; however, some operations may
  reallocate the value layer while still referencing the same persistence
  layer, so the two are actually separate.

  Map read operations hash the key, then delegate to the persistence layer
  starting at (H,0), and iterating until either the lookup fails, or an entry
  is returned whose actual key matches the input key.

  Map append operations first delegate to the persistence layer to find the
  sequence number of the new element. The resulting cursor is then appended to
  the order array.

  Map edit operations simply delegate to the persistence layer.

  Map delete operations delegate to the persistence layer, then unconditionall
  reallocate the ava_cuckoo_map without a list-index array (even if it had none
  already, since it could gain one later) since sharing would violate the
  retains-same-number-of-deletes invariant.

  Whenever the persistence layer is reallocated, the order array must be
  rebuilt as well. This operation also vacuums the order array, removing
  references to dead elements.

  Cursors are usually implemented in the value layer as (hash,seq) pairs which
  can be directly used with the persistence layer. If
  AVA_CUCKOO_PERSISTENCE_FORMAT dictates a format where
  (sizeof(hash)+sizeof(seq) > sizeof(ava_map_cursor)), cursors are instead
  implemented as indices into the order array, and every element stored in the
  persistence layer additionally stores its ava_map_cursor. (Using array
  offsets, pointers, etc for cursors is not possible since offsets within the
  persistence layer are not stable.)

  ---

  UPGRADING

  Most of the myriad of variants of the cuckoo map have certain limitations
  about their contents. When such limitations are exceeeded (which only occurs
  on writes), the map _upgrades_ to a variant that can handle the data.

  For any of the AVA_CUCKOO_POLYMORPHIC_{KEY,VALUE}_{ATTR,DATA} parameters, if
  the parameter is unset but a value is encountered which makes that field
  polymorphic, the map upgrades to one with that parameter set.
  AVA_CUCKOO_ASCII9_HASH is unset in the upgrade if
  AVA_CUCKOO_POLYMORPHIC_KEY_ATTR becomes set.

  If AVA_CUCKOO_ASCII9_HASH is set if an exact hash collision is encountered
  between two distinct keys.

  If AVA_CUCKOO_PERSISTENCE_FORMAT == PERSISTENCE_FORMAT_TINY and a sequence
  number of 256 is needed or the table would expand beyond 2048 slots, it
  upgrades to PERSISTENCE_FORMAT_COMPACT.

  If AVA_CUCKOO_PERSISTENCE_FORMAT == PERSISTENCE_FORMAT_COMPACT and the table
  would expand beyond 2**24 slots, it upgrades to PERSISTENCE_FORMAT_WIDE, and
  unsets AVA_CUCKOO_ASCII9_HASHand sets AVA_CUCKOO_STRONG_HASH.

  If the physical layer fails to insert a key and exhausts its stash space, the
  map upgrades by setting AVA_CUCKOO_STRONG_HASH and clearing
  AVA_CUCKOO_ASCII9_HASH.

  ---

  VARIANT NOTATION

  The names of variants are notated in the format "$format-$hash-$polymorph".
  "$format" is one of "tiny", "compact", "wide", or "huge", corresponding to
  the four persistence formats. "$hash" is "a9" if AVA_CUCKOO_ASCII9_HASH
  (which implies not AVA_CUCKOO_STRONG_HASH), "strong" if
  AVA_CUCKOO_STRONG_HASH, and "fast", otherwise. "$polymorph" is a sequence of
  three 'p's and 'm's indicating key-attribute, value-attribute, and value-data
  polymorphism.

 */

#define PERSISTENCE_FORMAT_TINY 0
#define PERSISTENCE_FORMAT_COMPACT 1
#define PERSISTENCE_FORMAT_WIDE 2

/* Sanity check parameters */
#if AVA_CUCKOO_STRONG_HASH && AVA_CUCKOO_ASCII9_HASH
#error "Cannot use both strong and ASCII9 hashing"
#endif

#if AVA_CUCKOO_POLYMORPHIC_KEY_ATTR && AVA_CUCKOO_ASCII9_HASH
#error "Cannot use ASCII9 hashing with polymorphic attributes"
#endif

#if AVA_CUCKOO_PERSISTENCE_FORMAT >= PERSISTENCE_FORMAT_WIDE && \
  (AVA_CUCKOO_ASCII9_HASH || !AVA_CUCKOO_STRONG_HASH)
#error "Cannot use weak hashing with >32-bit hashes"
#endif

#if AVA_CUCKOO_POLYMORPHIC_VALUE_ATTR && !AVA_CUCKOO_POLYMORPHIC_VALUE_DATA
#error "Polymorphic attribute implies polymorphic data"
#endif

typedef enum {
  pt_none = 0,
  pt_present,
  pt_tombstone,
  pt_shortcut
} pers_tag;

#if PERSISTENCE_FORMAT_TINY == AVA_CUCKOO_PERSISTENCE_FORMAT
typedef ava_uint phys_hash;
typedef ava_ulong phys_key;
typedef ava_uint pers_key;
typedef ava_uchar pers_seq;
typedef ava_ushort phys_gen;
#define MAX_GENERATION 2047
#define MAX_SEQ 255
#define MAX_SLOTS 2048
#define PHYS_LAST_WORD_MASK 0xFFFFFFFFFFFFF000ULL
#define PERS_KEY_MASK 0x3FFFFFFF
static inline pers_key get_pers_key(phys_key p) {
  return p >> 32;
}
static inline phys_key with_pers_key(phys_key p, pers_key k) {
  return (p & 0xFFFFFFFFULL) | (k << 32);
}
static inline pers_seq get_pers_seq(phys_key p) {
  return (p >> 24) & 0xFF;
}
static inline phys_key with_pers_seq(phys_key p, pers_seq s) {
  return (p & 0xFFFFFFFF00FFFFFFULL) | ((ava_ulong)s << 24);
}
static inline pers_gen get_pers_gen(phys_key p) {
  return (p >> 12) & 0xFFF;
}
static inline phys_key with_pers_gen(phys_key p, pers_gen g) {
  return (p & 0xFFFFFFFFFF000FFFULL) | ((ava_ulong)g << 12);
}
static inline pers_gen get_pers_next_gen(phys_key p) {
  return p & 0xFFF;
}
static inline phys_key with_pers_next_gen(phys_key p, pers_gen g) {
  return (p & 0xFFFFFFFFFFFFF000ULL) | g;
}
static inline pers_tag get_pers_tag(pers_key k) {
  return k >> 30;
}
static inline pers_key get_pers_key_proper(pers_key k) {
  return k & PERS_KEY_MASK;
}
static inline pers_key compose_pers_key(pers_tag tag, pers_key k) {
  return k | (tag << 30);
}
#elif PERSISTENCE_FORMAT_COMPACT == AVA_CUCKOO_PERSISTENCE_FORMAT
typedef ava_uint phys_hash;
typedef struct {
  ava_ulong key_seq;
  ava_ulong gens;
} phys_key;
typedef ava_uint pers_key;
typedef ava_uint pers_seq;
typedef ava_uint phys_gen;
#define MAX_GENERATION 0xFFFFFFFF
#define MAX_SEQ 0xFFFFFFFF
#define MAX_SLOTS (1 << 24)
#define PHYS_LAST_WORD_MASK 0xFFFFFFFF00000000ULL
#define PERS_KEY_MASK 0x3FFFFFFF
static inline pers_key get_pers_key(phys_key p) {
  return p.key_seq >> 32;
}
static inline phys_key with_pers_key(phys_key p, pers_key k) {
  p.key_seq &= 0x00000000FFFFFFFFULL;
  p.key_seq |= (ava_ulong)k << 32;
  return p;
}
static inline pers_seq get_pers_seq(phys_key p) {
  return p.key_seq & 0x00000000FFFFFFFFULL;
}
static inline phys_key with_pers_seq(phys_key p, pers_seq s) {
  p.key_seq &= 0xFFFFFFFF00000000ULL;
  p.key_seq |= s;
  return p;
}
static inline pers_gen get_pers_gen(phys_key p) {
  return p.gens >> 32;
}
static inline phys_key with_pers_gen(phys_key p, pers_gen g) {
  p.gens &= 0x00000000FFFFFFFFULL;
  p.gens |= (ava_ulong)g << 32;
  return p;
}
static inline pers_gen get_pers_next_gen(phys_key p) {
  return p.gens & 0x00000000FFFFFFFFULL;
}
static inline phys_key with_pers_next_gen(phys_key p, pers_gen g) {
  p.gens &= 0xFFFFFFFF00000000ULL;
  p.gens |= g;
  return p;
}
static inline pers_tag get_pers_tag(pers_key k) {
  return k >> 30;
}
static inline pers_key get_pers_key_proper(pers_key k) {
  return k & PERS_KEY_MASK;
}
static inline pers_key compose_pers_key(pers_tag tag, pers_key k) {
  return k | (tag << 30);
}
#else /* PERSISTENCE_FORMAT_WIDE == AVA_CUCKOO_PERSISTENCE_FORMAT */
typedef ava_ulong phys_hash;
typedef struct {
  ava_ulong key, seq, gen, next_gen;
} phys_key;
typedef ava_ulong pers_key;
typedef ava_ulong pers_seq;
typedef ava_ulong phys_gen;
#define MAX_GENERATION 0xFFFFFFFFFFFFFFFFULL
#define MAX_SEQ 0xFFFFFFFFFFFFFFFFULL
#define MAX_SLOTS (1ULL << 62)
/* 0 because the next_gen is included in the key for consistency */
#define PHYS_LAST_WORD_MASK 0
#define PERS_KEY_MASK 0x3FFFFFFFFFFFFFFFULL
static inline pers_key get_pers_key(phys_key p) {
  return p.key;
}
static inline phys_key with_pers_key(phys_key p, pers_key k) {
  p.key = k;
  return p;
}
static inline pers_seq get_pers_seq(phys_key p) {
  return p.seq;
}
static inline phys_key with_pers_seq(phys_key p, pers_seq s) {
  p.seq = s;
  return p;
}
static inline pers_gen get_pers_gen(phys_key p) {
  return p.gen;
}
static inline phys_key with_pers_gen(phys_key p, pers_gen g) {
  p.gen = g;
  return p;
}
static inline pers_gen get_pers_next_gen(phys_key p) {
  return p.next_gen;
}
static inline phys_key with_pers_next_gen(phys_key p, pers_gen g) {
  p.next_gen = g;
  return p;
}
static inline pers_tag get_pers_tag(pers_key k) {
  return k >> 62;
}
static inline pers_key get_pers_key_proper(pers_key k) {
  return k & PERS_KEY_MASK;
}
static inline pers_key compose_pers_key(pers_tag tag, pers_key k) {
  return k | (tag << 62);
}
#endif

typedef struct {
  phys_key pk;
#if PERSISTENCE_FORMAT_WIDE >= AVA_CUCKOO_PERSISTENCE_FORMAT
  ava_map_cursor cursor;
#endif
#if AVA_CUCKOO_POLYMORPHIC_KEY_ATTR
  ava_value val_key;
#else
  ava_ulong val_key_data;
#endif
#if AVA_CUCKOO_POLYMORPHIC_VALUE_ATTR
  ava_value val_val;
#elif AVA_CUCKOO_POLYMORPHIC_VALUE_DATA
  ava_ulong val_val_data;
#endif
} table_entry;
