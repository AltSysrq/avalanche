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

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#elif defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#endif

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "-atree.h"

/*
  An A[rray] Tree is a persistent data structure providing O(log(n))
  algorithmic complexity for non-modifying, in-place, or append operations, but
  with only amortised O(1) allocations in cases where old versions are not
  modified. No attempt is made to make insertion or deletion any more efficient
  than with mundane arrays.

  Any reference to an ATree also carries a timestamp, t^R, and a length, k^R,
  which together allow any reader to consume that particular version of the
  ATree without any kind of synchronisation. Writers manuplate live structures
  in such a way that readers behave the same even in the presence of concurrent
  modifications, even out-of-order modifications that may be seen without
  memory barriers.

  Every ATree node contains ATREE_ORDER children. Nodes with height 0 hold data
  elements; those with height n>0 hold children of height n-1. The nature of
  the latter type of child reference is discussed later. Every child of a node
  is full, except for the final used one, which may be only partially in use.
  This permits descending the tree by bitshifting the index, rather than binary
  searching the children (as in the B+ List). Additionally, every node belongs
  to a "family", which is a pointer to an atomically-incrementable timestamp.

  The general concurrency strategy is as follows. Since readers know the length
  of their view of the ATree, they can entirely avoid looking at sections that
  writers may have initialised later; this permits very efficient appending of
  elements. In initialised areas, they compare their t^R against another
  timestamp that is initialised to infinity and then updated at least once, to
  a value that is also guaranteed to be greater than t^R of any concurrent
  readers. Because of this, no memory barrier is needed, since a reader will
  make the same decision regardless of the value it sees. In any case where a
  reader sees such a timestamp that is less than or equal to t^R, it is
  guaranteed that any memory protected by that timestamp is frozen. Here as
  well, no synchronisation is needed, since the very fact that the reader holds
  a reference with t^R already implies that the necessary memory barriers to
  _give_ it such a reference have already occurred.

  Writers are required to first take _ownership_ of the whole family by
  atomically compare-and-swapping the family t_N from t^R to t^R+1. This
  pattern guarantees that t_N only ever increases, and thus that any such
  compare-and-swap (a) only succeeds if the prospective writer already has the
  latest version, (b) the new t_N is greater than any t^R in existence, and (c)
  there can be no more than one writer at any given time. Updates made by the
  writer always use the new t_N, which becomes the t^R of the resulting
  reference to the new version of the tree. Since a writer already has the
  latest version of the structure, and there are no other writers, writers may
  safely access any part of the structure after acquiring ownership without
  synchronisation.

  Writers which fail to acquire ownership are forced to copy nodes as they
  encounter them. So that the entire tree need not be copied, branches are
  permitted to point to nodes in other families; writers copy nodes into the
  family of the parent as needed.

  In order to prevent holding on to the memory of every version of an ATree
  that ever existed, each node tracks the number of dead elements it
  references; the full tree is rebuilt on write whenever this exceeds the
  length of the array. This guarantees that memory usage does not exceed double
  what is required. Since such rebuilds only occur after n mutations, this does
  not increase the amortised runtime of operations.

 */

#define ATREE_ORDER_BITS 6
#define ATREE_ORDER (1 << ATREE_ORDER_BITS)

typedef AO_t ava_atree_timestamp_atomic;
/* Need to cast twice because ~ promotes unsigned short to signed int */
#define T_INFINITY ((ava_atree_timestamp)~(ava_atree_timestamp)0)

typedef struct ava_atree_node_damage_s ava_atree_node_damage;

/**
 * A single node in an ATree.
 */
struct ava_atree_node_s {
  /**
   * The family in which this node lives.
   *
   * This pointer is written once when the node is allocated and is then never
   * modified; it may be read at any time. The pointee is subject to concurrent
   * modification; the only valid operation to perform upon it is atomic
   * compare-and-swap to take ownership.
   */
  ava_atree_timestamp_atomic* family;
  /**
   * The advisory value weight of this atree.
   *
   * Writers: Safe to update arbitrarily.
   *
   * Readers: Safe for unsynchronised access. The value will be indeterminate
   * in concurrent situations, but this is acceptable since it is only used for
   * performance advisement.
   */
  size_t weight;
  /**
   * The approximate number of dead elements held by this node.
   *
   * Writers: Safe to update arbitrarily.
   *
   * Readers: Unreadable
   */
  size_t dead_elements;

  /**
   * The height of this node.
   *
   * This value is written when the node is constructed, and then never
   * updated.
   */
  unsigned char height;
  /**
   * The value of *family when this node was created.
   *
   * This value is set at creation time and then never written again.
   *
   * Writers: If created == *family, there are no readers that have *any* kind
   * of reference to this node, so arbitrary in-place modification is
   * permissable.
   *
   * Readers: This value is stable.
   */
  ava_atree_timestamp created;
  /**
   * The t_N at which this node was damaged.
   *
   * Initialised to T_INFINITY.
   *
   * Writers: May update from T_INFINITY to t_N. If doing so, damage must be
   * initialised prior to releasing any reference with t^R = t_N.
   *
   * Readers: May be read to test damaged <= t^R. Any other operation is
   * meaningless, since this value itself will be indeterminate.
   */
  ava_atree_timestamp damaged;
  /**
   * Damaged data for this node.
   *
   * Initialised to NULL.
   *
   * Writers: Initialised on demand, along with an update to damaged.
   *
   * Readers: Indeterminate unless damaged <= t^R, in which case it is
   * guaranteed to be non-NULL.
   */
  ava_atree_node_damage*restrict damage;

  /**
   * The elements contained in this node.
   *
   * Contains ATREE_ORDER elements, whatever the element size is for this node.
   * This is declared as a void* to ensure proper alignment.
   *
   * Writers: Elements implied to exist by k^R are frozen, unless *family ==
   * created. Those beyond that limit may be mutated freely. This permits
   * efficient in-place append.
   *
   * Readers: Elements implied to exist by k^R are readable. Those beyond that
   * limit have indeterminate values.
   */
  void* data[];
};

/**
 * Buffers in-place updates to the array.
 *
 * When a writer wishes to overwrite elements, it attempts to use the damage
 * array to store the new values. The damage array is a secondary array of
 * elements of length ATREE_ORDER, each paired with a timestamp. If the
 * timestamp of a damage element is less than or equal to a t^R, the damage
 * element supercedes the primary element in the node.
 *
 * This system only permits modifying a particular element before the node must
 * be copied. However, this is sufficient to achieve O(1) allocations per edit;
 * 50% of edits to an element will involve 1 copy, 25% 2 copies, 12.5% 3
 * copies, and so on.
 */
struct ava_atree_node_damage_s {
  /**
   * Timestamps at which each element within the containing node was damaged.
   *
   * Initialised to T_INFINITY.
   *
   * Writers: May update from T_INFINITY to t_N when an element is damaged.
   * When doing so, an element must be written into the other three arrays to
   * store the new value. If not T_INFINITY, the element is already used, and
   * the writer must copy the node.
   *
   * Readers: May be read to test elts_damaged[ix] <= t^R. Any other operation
   * is meaningless, since this value itself will be indeterminate.
   */
  ava_atree_timestamp t[ATREE_ORDER];

  /**
   * Replacement node elements.
   *
   * This is an array of size ATREE_ORDER; each element is the size of whatever
   * element type the node contains. This is declared as a void* to ensure
   * correct alignment.
   *
   * Writers: Populated with a corresponding reduction of the parallel t value
   * to t_N.
   *
   * Readers: Indeterminate unless t[ix] <= t^R, in which case safe.
   */
  void* data[];
};

/**
 * A reference from one atree to another which is not in the same family.
 *
 * Writers cannot cross foreign boundaries; they must instead copy the foreign
 * node into a new local node first.
 *
 * Readers change their t^R when crossing a foreign boundary.
 */
typedef struct {
  /**
   * The foreign node itself.
   */
  const ava_atree_node*restrict node;
  /**
   * The t^R of this reference. Readers switch to this t^R when they traverse
   * into node.
   */
  ava_atree_timestamp timestamp;
} ava_atree_foreign_node_reference;

/**
 * Element type for branch (height > 0) nodes.
 *
 * Each element references exactly one other node, which may either be of the
 * same family ("local") or another ("foreign"). These two cases are
 * discriminated by the zero bit; if zero, the node is local, otherwise it is
 * foreign. Foreign nodes must go through an extra level of indirection via
 * ava_atree_foreign_node_reference.
 */
typedef union {
  /**
   * Integer representation, for twiddling the zero bit.
   */
  intptr_t i;
  const ava_atree_node*restrict local_node;
  const ava_atree_foreign_node_reference*restrict foreign_node;
} ava_atree_branch_elt;

/**
 * Allocates a new ATree family, initialised to zero.
 */
static ava_atree_timestamp_atomic* ava_atree_family_new(void);
/**
 * Allocates a new ava_atree_node as well as any foreign references it requires.
 *
 * @param height The height of the node.
 * @param family The family to which the node will belong. The caller must own
 * this family.
 * @param element_size The size of elements within the node.
 * @return The new node.
 */
static ava_atree_node* ava_atree_node_new(
  unsigned char height, ava_atree_timestamp_atomic* family,
  size_t element_size);
/**
 * Creates a foreign reference to the given node.
 *
 * @param node The node to reference.
 * @param t_r The t^R of the reference.
 */
static const ava_atree_foreign_node_reference*
ava_atree_foreign_node_reference_new(
  const ava_atree_node*restrict node,
  ava_atree_timestamp t_r);

/**
 * Allocates a new damage array.
 *
 * @param height The height of the node that will be given this damage array.
 * @param spec The spec for the list elements in this atree.
 * @return A new ava_atree_node_damage.
 */
static ava_atree_node_damage* ava_atree_node_damage_new(
  unsigned height, const ava_atree_spec*restrict spec);

/**
 * Copies the given node into a fresh node of the given family.
 *
 * The new node is fully normalised; it is undamaged, and has a creation
 * timestamp equal to *family.
 *
 * @param node The node to copy.
 * @param family The family into which to copy the node. The caller must own
 * this family.
 * @param spec The spec for elements within the list (not the node itself).
 * @param t_r The t^R of the reference to node.
 */
static ava_atree_node* ava_atree_node_fork(
  const ava_atree_node*restrict node,
  ava_atree_timestamp_atomic* family,
  const ava_atree_spec*restrict spec,
  ava_atree_timestamp t_r);
/**
 * Returns whether the given branch element is a foreign node reference.
 */
static ava_bool ava_atree_is_foreign_ref(ava_atree_branch_elt elt);

/**
 * Assuming the given branch element contains a foreign reference, returns the
 * foreign reference to which it points.
 */
static const ava_atree_foreign_node_reference*
ava_atree_get_foreign_ref(ava_atree_branch_elt elt);

/**
 * Produces a branch element pointing to the given foreign reference.
 */
static ava_atree_branch_elt ava_atree_make_foreign_ref(
  const ava_atree_foreign_node_reference*restrict ptr);

/**
 * Returns a pointer to the given element within the given node.
 *
 * @param node The node to read.
 * @param ix The index of the element in node to access.
 * @param spec The spec for list elements.
 * @param t_r The t^R associated with the calling reference.
 * @return A pointer to the selected element.
 */
static const void* ava_atree_node_get_element(
  const ava_atree_node*restrict node, unsigned ix,
  const ava_atree_spec*restrict spec, ava_atree_timestamp t_r);
/**
 * Like ava_atree_node_get_element(), but also indicates how many contiguous
 * elements are available.
 *
 * @param node The node to read.
 * @param ix The index of the element in node to access.
 * @param spec The spec for list elements.
 * @param t_r The t^R associated with the calling reference.
 * @param count Set to the number of elements available starting at the return
 * value. Always at least one.
 * @return A pointer to the selected element.
 */
static const void* ava_atree_node_get_elements(
  const ava_atree_node*restrict node, unsigned ix,
  const ava_atree_spec*restrict spec, ava_atree_timestamp t_r,
  size_t*restrict count);
/**
 * Returns the byte offset to apply for the given element in the given node.
 *
 * @param node The node for which the offset is to be determined.
 * @param ix The index of the desired element within node.
 * @param spec The spec for list elements, in case node is a leaf.
 * @return The byte offset, eg, to be added to node->data after casting it to a
 * char*.
 */
static size_t ava_atree_node_offset(const ava_atree_node*restrict node,
                                    unsigned ix,
                                    const ava_atree_spec*restrict spec);

/**
 * Ensures that a call to write_element() for the given index will succeed.
 *
 * This allocates the damage array if neeeded, or forks the whole node if the
 * required space in the damage array is already in use.
 *
 * @param node The node in which to reserve the element.
 * @param ix The element to reserve.
 * @param spec The element spec.
 * @param t_n The t_N of the owned family.
 * @return A node where the given index can be written.
 */
static ava_atree_node* ava_atree_node_reserve_element(
  ava_atree_node*restrict node, unsigned ix,
  const ava_atree_spec*restrict spec, ava_atree_timestamp t_n);

/**
 * Directly writes the given element of the given node.
 *
 * Unless t_n == node->created, the damage array must already be allocated.
 *
 * @param node The node to which to write the element.
 * @param ix The index of the element to write.
 * @param src The source data to copy.
 * @param spec The spec of the list elements.
 * @param t_n The value of *node->family.
 */
static void ava_atree_node_write_element(
  ava_atree_node*restrict node, unsigned ix, const void*restrict src,
  const ava_atree_spec*restrict spec, ava_atree_timestamp t_n);

/**
 * Acquires ownership of a node, including the rest of its family.
 *
 * If the attempt fails, the node is forked into a new family.
 *
 * In either case, the caller will be permitted to perform modifications on the
 * returned value.
 *
 * @param node The root node whose family is to be taken.
 * @param spec The list element spec.
 * @param t_r The t^R of the reference to target. After the call, the t_n of
 * the whole family.
 * @return A mutable node with the same contents as target.
 */
static ava_atree_node* ava_atree_node_steal(
  const ava_atree_node*restrict target,
  const ava_atree_spec*restrict spec,
  ava_atree_timestamp*restrict t_r);
/**
 * "Transfers" ownership of a family through a branch element boundary.
 *
 * If the element refers to a local node, it is simply returned, since the
 * caller already owns the family. Otherwise, the foreign node is forked into
 * the family and that result is returned.
 *
 * @param element The element whose pointee is to be acquired.
 * @param spec The list element spec.
 * @param family The family that the caller owns and that the return value will
 * be a part of.
 * @param t_n The value of *family.
 * @return A node writable by virtue of being a member of family. The caller
 * must be prepared to this changing the reference within element, ie, it may
 * need to rewrite that element within its node.
 */
static ava_atree_node* ava_atree_node_transfer(
  ava_atree_branch_elt element,
  const ava_atree_spec*restrict spec,
  ava_atree_timestamp_atomic* family,
  ava_atree_timestamp t_n);

/**
 * Returns the bitshift used to convert list element indices to and from node
 * element indices.
 */
static unsigned ava_atree_node_shift(const ava_atree_node*restrict node);

/**
 * Walks down the tree to locate the elements at the given index.
 *
 * @param node The initial node from which to start the search.
 * @param ix The element index, relative to the start of node, to locate.
 * @param spec The spec for elements in the list.
 * @param t_r The t^R of the reference.
 * @param avail Set to the number of elements available after the given index.
 * @return A pointer to the first element after ix.
 */
static const void* ava_atree_node_get(
  const ava_atree_node*restrict node,
  size_t ix, const ava_atree_spec*restrict spec,
  ava_atree_timestamp t_r, size_t*restrict avail);

/**
 * Returns the maximum number of elements the given node can contain.
 */
static size_t ava_atree_node_capacity(const ava_atree_node*restrict node);

/**
 * Appends elements to the given node.
 *
 * @param node The node to which to append elements.
 * @param current_length The current length, in list elements, of this node.
 * @param data The elements to append.
 * @param num_elts The number of elements to append. It is the caller's
 * responsibility to ensure that current_length+num_elts <= capacity(node).
 * @param spec The spec for the elements to append.
 * @param t_n The t_N of the node family.
 * @return The node with the elements appended.
 */
static ava_atree_node* ava_atree_node_append(
  ava_atree_node*restrict node, size_t current_length,
  const void*restrict data, size_t num_elts,
  const ava_atree_spec*restrict spec, ava_atree_timestamp t_n);

ava_atree ava_atree_new(const ava_atree_spec*restrict spec) {
  ava_atree_node*restrict node =
    ava_atree_node_new(0, ava_atree_family_new(), spec->elt_size);

  return (ava_atree) {
    .root = node,
    .attr = {
      .v = {
        .length = 0,
        .timestamp = 0
      }
    }
  };
}

size_t ava_atree_weight(ava_atree tree) {
  return tree.root->weight;
}

static ava_atree_timestamp_atomic* ava_atree_family_new(void) {
  ava_atree_timestamp_atomic* ret =
    ava_alloc_atomic(sizeof(ava_atree_timestamp_atomic));

  *ret = 0;
  return ret;
}

static ava_atree_node* ava_atree_node_new(
  unsigned char height, ava_atree_timestamp_atomic* family,
  size_t element_size
) {
  ava_atree_node*restrict node =
    ava_alloc(sizeof(ava_atree_node) + ATREE_ORDER * element_size);

  node->height = height;
  node->family = family;
  node->created = *family;
  node->damaged = T_INFINITY;

  return node;
}

static const ava_atree_foreign_node_reference*
ava_atree_foreign_node_reference_new(
  const ava_atree_node*restrict node,
  ava_atree_timestamp t_r
) {
  ava_atree_foreign_node_reference* ref = ava_alloc(
    sizeof(ava_atree_foreign_node_reference));

  ref->node = node;
  ref->timestamp = t_r;
  return ref;
}

static ava_atree_node_damage* ava_atree_node_damage_new(
  unsigned height, const ava_atree_spec*restrict spec
) {
  ava_atree_node_damage* d = ava_alloc(
    sizeof(ava_atree_node_damage) +
    ATREE_ORDER * (height? sizeof(ava_atree_branch_elt) : spec->elt_size));

  memset(d->t, ~0, sizeof(d->t));
  return d;
}

static ava_atree_node* ava_atree_node_fork(const ava_atree_node*restrict src,
                                           ava_atree_timestamp_atomic* family,
                                           const ava_atree_spec* spec,
                                           ava_atree_timestamp t_r) {
  ava_atree_timestamp t_n = *family;
  ava_atree_node*restrict dst;
  unsigned i;

  dst = ava_atree_node_new(
    src->height, family,
    src->height? sizeof(ava_atree_branch_elt) : spec->elt_size);

  for (i = 0; i < ATREE_ORDER; ++i) {
    const void* elt = ava_atree_node_get_element(src, i, spec, t_r);
    ava_atree_branch_elt branch;

    if (src->height > 0) {
      /* Might need to make the node reference foreign */
      branch = *(const ava_atree_branch_elt*restrict)elt;
      elt = &branch;

      if (!branch.i)
        /* No more elements to copy */
        break;

      if (!ava_atree_is_foreign_ref(branch) &&
          branch.local_node->family != family) {
        /* This means src->family != dst->family, ie, we're forking into a new
         * family.
         */
        branch = ava_atree_make_foreign_ref(
          ava_atree_foreign_node_reference_new(branch.local_node, t_r));
      }
    }

    ava_atree_node_write_element(dst, i, elt, spec, t_n);
  }

  return dst;
}

static ava_bool ava_atree_is_foreign_ref(ava_atree_branch_elt elt) {
  return elt.i & 1;
}

static const ava_atree_foreign_node_reference*
ava_atree_get_foreign_ref(ava_atree_branch_elt elt) {
  elt.i &= ~(intptr_t)1;
  return elt.foreign_node;
}

static ava_atree_branch_elt ava_atree_make_foreign_ref(
  const ava_atree_foreign_node_reference*restrict ptr
) {
  ava_atree_branch_elt elt;

  elt.foreign_node = ptr;
  elt.i |= 1;
  return elt;
}

static size_t ava_atree_node_offset(const ava_atree_node*restrict node,
                                    unsigned ix,
                                    const ava_atree_spec*restrict spec) {
  return (node->height? ix * sizeof(ava_atree_branch_elt) :
          ix * spec->elt_size);
}

static const void* ava_atree_node_get_element(
  const ava_atree_node*restrict node, unsigned ix,
  const ava_atree_spec*restrict spec, ava_atree_timestamp t_r
) {
  const char* data;

  if (node->damaged <= t_r && node->damage->t[ix] <= t_r) {
    data = (const char*)node->damage->data;
  } else {
    data = (const char*)node->data;
  }

  return data + ava_atree_node_offset(node, ix, spec);
}

static const void* ava_atree_node_get_elements(
  const ava_atree_node*restrict node, unsigned ix,
  const ava_atree_spec*restrict spec, ava_atree_timestamp t_r,
  size_t*restrict count_out
) {
  const char* data;
  size_t count;

  if (node->damaged <= t_r && node->damage->t[ix] <= t_r) {
    data = (const char*)node->damage->data;

    for (count = 1; count + ix < ATREE_ORDER &&
           node->damage->t[ix] <= t_r; ++count);
  } else {
    data = (const char*)node->data;

    if (node->damaged <= t_r) {
      for (count = 1; count + ix < ATREE_ORDER &&
             node->damage->t[ix] > t_r; ++count);
    } else {
      count = ATREE_ORDER - ix;
    }
  }

  *count_out = count;

  return data + ava_atree_node_offset(node, ix, spec);
}

static ava_atree_node* ava_atree_node_reserve_element(
  ava_atree_node*restrict node, unsigned ix,
  const ava_atree_spec*restrict spec, ava_atree_timestamp t_n
) {
  if (T_INFINITY == node->damaged) {
    node->damaged = t_n;
    node->damage = ava_atree_node_damage_new(node->height, spec);
    return node;
  }

  if (T_INFINITY == node->damage->t[ix])
    return node;

  /* No space to write this element, need to fork */
  return ava_atree_node_fork(node, node->family, spec, t_n);
}

static void ava_atree_node_write_element(
  ava_atree_node*restrict node, unsigned ix, const void*restrict src,
  const ava_atree_spec*restrict spec, ava_atree_timestamp t_n
) {
  char*restrict dst =
    (char*)(t_n != node->created? node->damage->data : node->data);

  memcpy(dst + ava_atree_node_offset(node, ix, spec),
         src, node->height? sizeof(ava_atree_branch_elt) : spec->elt_size);

  if (dst != (char*restrict)node->data)
    node->damage->t[ix] = t_n;

  if (node->height) {
    const ava_atree_branch_elt*restrict elt = src;
    const ava_atree_node*restrict elt_node =
      ava_atree_is_foreign_ref(*elt)?
      ava_atree_get_foreign_ref(*elt)->node : elt->local_node;

    node->weight += elt_node->weight;
  } else {
    node->weight += (*spec->weight_function)(src, 1);
  }
}

static ava_atree_node* ava_atree_node_steal(
  const ava_atree_node*restrict target,
  const ava_atree_spec*restrict spec,
  ava_atree_timestamp*restrict t_r
) {
  ava_atree_timestamp_atomic cur = *t_r, new = cur + 1;

  if (new < T_INFINITY && AO_compare_and_swap(target->family, cur, new)) {
    /* Success, the caller now owns target */
    *t_r = new;
    return (ava_atree_node*restrict)target;
  } else {
    /* Either someone else has modified this version previously, or the
     * timestamp is about to overflow.
     *
     * We've no choice but to fork to a new family.
     */
    *t_r = 0;
    return ava_atree_node_fork(
      target, ava_atree_family_new(), spec, cur);
  }
}

static ava_atree_node* ava_atree_node_transfer(
  ava_atree_branch_elt element,
  const ava_atree_spec*restrict spec,
  ava_atree_timestamp_atomic* family,
  ava_atree_timestamp t_n
) {
  if (ava_atree_is_foreign_ref(element)) {
    const ava_atree_foreign_node_reference*restrict ref =
      ava_atree_get_foreign_ref(element);

    return ava_atree_node_fork(ref->node, family, spec,
                               ref->timestamp);
  } else {
    return (ava_atree_node*restrict)element.local_node;
  }
}

static unsigned ava_atree_node_shift(const ava_atree_node*restrict node) {
  return ATREE_ORDER_BITS * node->height;
}

const void* ava_atree_get(ava_atree tree, size_t ix,
                          const ava_atree_spec*restrict spec,
                          size_t*restrict avail) {
  size_t reported_avail;
  const void* ret;

  ret = ava_atree_node_get(tree.root, ix, spec, tree.attr.v.timestamp,
                           &reported_avail);

  if (avail)
    *avail = reported_avail < tree.attr.v.length - ix?
      reported_avail : tree.attr.v.length - ix;

  return ret;
}

static const void* ava_atree_node_get(
  const ava_atree_node*restrict node,
  size_t ix, const ava_atree_spec*restrict spec,
  ava_atree_timestamp t_r, size_t*restrict avail
) {
  while (node->height > 0) {
    unsigned shift = ava_atree_node_shift(node);
    unsigned sub_ix = ix >> shift;
    ix -= ((size_t)sub_ix) << shift;

    const ava_atree_branch_elt*restrict elt =
      ava_atree_node_get_element(node, sub_ix, spec, t_r);
    if (ava_atree_is_foreign_ref(*elt)) {
      const ava_atree_foreign_node_reference*restrict ref =
        ava_atree_get_foreign_ref(*elt);

      node = ref->node;
      t_r = ref->timestamp;
    } else {
      node = elt->local_node;
    }
  }

  return ava_atree_node_get_elements(node, ix, spec, t_r, avail);
}

ava_atree ava_atree_append(ava_atree tree,
                           const void*restrict data,
                           size_t num_elts,
                           const ava_atree_spec*restrict spec) {
  ava_atree_timestamp t_n = tree.attr.v.timestamp;
  size_t orig_len = tree.attr.v.length, new_len = orig_len + num_elts;
  ava_atree_node*restrict root = ava_atree_node_steal(
    tree.root, spec, &t_n);

  /* Preemptively create parents to hold overflowing nodes */
  while (ava_atree_node_capacity(root) < new_len) {
    ava_atree_node*restrict new_root = ava_atree_node_new(
      root->height + 1, root->family, sizeof(ava_atree_branch_elt));
    ava_atree_branch_elt elt = { .local_node = root };
    ava_atree_node_write_element(new_root, 0, &elt, spec, t_n);

    root = new_root;
  }

  root = ava_atree_node_append(root, orig_len, data, num_elts, spec, t_n);

  return (ava_atree) {
    .root = root,
    .attr = {
      .v = {
        .timestamp = t_n,
        .length = new_len
      }
    }
  };
}

static size_t ava_atree_node_capacity(const ava_atree_node*restrict node) {
  size_t base = ATREE_ORDER;

  return base << ava_atree_node_shift(node);
}

static ava_atree_node* ava_atree_node_append(
  ava_atree_node*restrict node, size_t current_length,
  const void*restrict data, size_t num_elts,
  const ava_atree_spec*restrict spec, ava_atree_timestamp t_n
) {
  if (0 == node->height) {
    char*restrict dst = (char*restrict)node->data;

    dst += current_length * spec->elt_size;

    node->weight += (*spec->weight_function)(data, num_elts);
    memcpy(dst, data, num_elts * spec->elt_size);
  } else {
    unsigned shift = ava_atree_node_shift(node);
    size_t child_cap = ((size_t)1) << shift;
    unsigned child_ix = current_length >> shift;
    size_t child_cur_len = current_length - (((size_t)child_ix) << shift);
    const char*restrict src = (const char*restrict)data;

    if (child_cur_len != 0) {
      /* Append in-place to the last child */
      const ava_atree_branch_elt*restrict elt = ava_atree_node_get_element(
        node, child_ix, spec, t_n);
      ava_atree_node*restrict child = ava_atree_node_transfer(
        *elt, spec, node->family, t_n);
      size_t child_append = num_elts > child_cap - child_cur_len?
        child_cap - child_cur_len : num_elts;

      child = ava_atree_node_append(
        child, child_cur_len, src, child_append, spec, t_n);
      node->weight += (*spec->weight_function)(src, child_append);

      if (child != elt->local_node) {
        ava_atree_branch_elt new_elt = { .local_node = child };
        node = ava_atree_node_reserve_element(
          node, child_ix, spec, t_n);
        ava_atree_node_write_element(node, child_ix, &new_elt, spec, t_n);
      }

      src += spec->elt_size * child_append;
      num_elts -= child_append;
      ++child_ix;
    }

    /* Append new children to the end */
    while (num_elts > 0) {
      ava_atree_node*restrict child = ava_atree_node_new(
        node->height - 1, node->family,
        node->height > 1? sizeof(ava_atree_branch_elt) : spec->elt_size);
      size_t child_append = num_elts < child_cap? num_elts : child_cap;

      child = ava_atree_node_append(child, 0, src, child_append, spec, t_n);
      ava_atree_branch_elt new_elt = { .local_node = child };
      ((ava_atree_branch_elt*restrict)node->data)[child_ix] = new_elt;
      node->weight += child->weight;

      src += child_append * spec->elt_size;
      num_elts -= child_append;
      ++child_ix;
    }
  }

  return node;
}
