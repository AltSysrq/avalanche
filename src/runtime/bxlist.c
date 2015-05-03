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
#include <assert.h>

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "-bxlist.h"

/*
  The "B+ List" is a rather unusual data structure that most closely resembles
  the B+ Tree, but is used for holding sequences rather than general key-value
  mappings.

  A B+ List is structured as a tree of nodes. Each node is considered to have
  an some height _h_; nodes with non-zero height reference other nodes with
  height _h-1_. Nodes with _h_ zero are leaves, which contain the list's data.

  Property names used in the rest of this description:

    h_N         The height of node N
    k_N         The _current number_ of entries in N
    k^X         A k value associated with a reference
    K           The maximum value of k_N for any N
    t_N         The timestamp of node N
    t^X         A t value associated with a reference

  Each node is conceptually structured as follows:

    struct node {
      data_type type;
      atomic size k = 0;
      atomic timestamp t = 0;
      timestamp primary_activation[K] = { infinity ... };
      timestamp patch_activation[K] = { infinity ... };
      data primary[K];
      data patch[K];
    };

  Invariants cannot be described by viewing a node in isolation, since correct
  concurrent functioning also depends on metadata associated with the reference
  to the node. Given reference R, the following invariants hold:

    - 0 <= k^R <= k_N <= K
    - 0 <= t^R <= t_N
    - For all 0 <= x < k_N, primary_activation[x] <= t_N
    - For any 0 <= x < k^R, if patch_activation[x] <= t^R, then the value in
      patch[x] is initialised and supercedes the value in primary[x].
    - t_N == t^R implies k_N == k^R.

  In practise, the values k_N and k^R are not stored since they can be inferred
  from primary_activation.

  The structure permits efficient implementation of the following semantic
  operations:

  - Adding a new element. This involves a write to the end of the initialised
    section of primary followed by an edit to primary_activation.

  - Editing an element once. This is accomplished by writing the new version
    into the patch space and updating patch_activation. Note that a particular
    element can only be editted once before the containing node must be copied;
    this is still sufficient to provide amortised O(1) allocations per edit.

  In order to modify a node, the prospective writer must first atomically
  assert that t_N == t^R and increment t_N. If, and only if, this succeeds, the
  writer is guaranteed to be the *only* writer to that node. It may then make
  one or more of the following modifications:

  - If k_N < K, add an element to primary[k_N] and increment k_N.

  - For some 0 <= x < k_N, if patch_activation[x] > t_N, write a value into
    patch[x] and reduce patch_used[x] to t_N with an atomic write (which need
    not have any memory barrier). (It is guaranteed that it was "infinity"
    before, since once it is changed from infinity, it will be <= t_N for the
    rest of the node's life.)

  A writer may _redact_ its ownership of a node if it ended up making no
  changes at all, by decrementing t_N back to the original value, provided it
  has not leaked any references with t^R = t'_N.

  The structure is designed to support in-place updates without requiring _any_
  kind of synchronisation --- not even memory barriers --- on the part of the
  readers. This is safe, due to the extra data on the reference: For a
  reference to cross thread boundaries, appropriate synchronisation mechanisms
  must be used anyway, so we can disregard the possibility of another thread
  learning of a new k_N or t_N without proper synchronisation. That leaves the
  following points:

  - Reading from primary_activation[x] where primary_activation[x] <= t^R is
    safe, since such values were initialised before a reference with t^R was
    created. Otherwise, the read value from primary_activation[x] is
    nondeterministic; however, it is still _guaranteed_ to be > t^R, since any
    value read is either (a) infinity; or (b) some t'_N > t^R being written by
    some other thread. That t'_N > t^R is guaranteed because the concurrent
    writer incremented t_N to determine the new value to write to this array.
    t'_N = t_N + 1; t^R <= t_N; thus t^R < t'_N

  - A reader only observes primary[x] if primary_activation[x] <= t^R; as
    described above, the reader will thus only observe primary[x] if its
    reference is older than the write to primary[x].

  - All statements regarding primary* apply to the patch* fields as well.

  - Readers never access k_N or t_N.

  Non-leaf nodes ("branches") are a bit more complicated. Their element type
  has the format:

    struct node_element {
      node_t* node;
      timestamp foreign_t; (* discussed later *)
      size num_prior_indices;
    };

  That is, each element not only stores the child node, but also the number of
  indices (ie, data elements as exposed by the list abstraction, rather than
  the k of the direct child). t^R is inherited from the parent reference. This
  means that, while expanding most children requires using a patch slot, the
  final child can be grown in-place, acquiring new elements simply with the
  growth of t.

  In this setup, one additional invariant would need to be added added:

  - For any branch node B, every child node C of that branch has a t_C >= t_B.

  This implies that (a) node C can only be modified if t_C = t_B, and thus (b)
  either most of the tree under B is unmodifiable, or any modification to B
  needs to update t_N for many nodes in the tree.

  To get around this, we make two changes:

  - A node does not contain its t_N directly, but rather a *pointer* which is
    shared with other nodes in the tree. Thus a writer can take ownership of
    the whole tree at once. Two nodes with the same *t_N are said to be
    local; others are foreign. The set of notes with a given *t_N is a node
    family.

  - Each node element additionally tracks a t^R if it points to a foreign node.
    Readers switch to this t^R if they traverse into that node. Since this
    makes the t^R used for the foreign node independent of that of the local
    node, the t_C >= t_B invariant can be lifted for this case.

  Under B+-Tree-like operations, new nodes are part of the same family as the
  existing ones; which family is used if there is a mixture is up to the
  implementation. Nodes copied due to an inability to acquire write access are
  placed into a new family.

  Writers generally don't attempt to take ownership of a foreign node; even if
  they were to succeed, it would leave the owner of that family unable to
  modify it further, and in either case requires using a patch slot to store
  the resulting t'^R. Instead, upon encountering a foreign node, the writer
  acts as if the attempt to claim write ownership failed.

  Since taking ownership of one node takes ownership of the whole tree, the
  following invariant must be added:

  - For any two nodes A and B and timestamp t^R, there may be no more than 1
    path through the tree to reach B from A.

  The main implication is that concatting a B+ List with itself needs to copy
  one half; otherwise, in-place modifications would be reflected in more than
  one place within the tree.

  ---

  Concurrency and persistence properties aside, the B+ List behaves mostly like
  a B+ tree. Every branch node has a table of one or more children. The "key"
  for a child is the number of data elements contained by or preceding that
  child. This key is stored in the entry of the child that *follows* it, so
  that it can be interpreted to mean the number of elements *preceding* the
  child whose entry contains it.

  Data elements are added by obtaining write access on the root of the tree,
  then descending to the leaf(ves) that should contain them and adding them, be
  it by extending the primary array, updating the patch array, or resorting to
  copying the leaf.

  If a node cannot hold the data it should contain, it is split into two nodes,
  which thus requires an update to the parent node, which could also cause that
  parent node to split.

  When a node is split, the way its elements are distributed is based upon the
  node's location within the parent. If the node is the final element in the
  parent's array, the left half of the split is left at capacity, and only
  overflow elements are propagated into a new node. In most cases, this means
  that the left node is actually simply the original node that was being split.
  For any other case, half of the elements are moved into the new node. This
  differs from normal B+ trees, which divide the elements in half in all cases.
  The distinction is here as it allows growing a B+ List by adding elements to
  the end without ever needing a reallocation.

  Deleting data works similarly: Write ownership is taken, then the writer
  descends to the appropriate leaf(ves) and updates them to not contain the
  elements in question. In all cases, this requires copying the leaf, since
  there is no way to represent deleted elements.

  B+ Trees merge nodes or redistribute data whenever a node becomes less than
  half full. Since redistribution means *copying*, B+ Lists instead only merge
  adjacent nodes when their combined size is small enough to fit into one node.
  Similarly to node splits, this merge can cause the parent nodes to be merged.

 */

#define BXLIST_ORDER AVA_BXLIST_ORDER

static inline void static_assert_ulong_holds_aot(void) {
  switch (0) {
  case 0:
  case sizeof(ava_ulong) >= sizeof(AO_t):
    ;
  }
}

static inline void static_assert_ulong_holds_bxlist_order_bits(void) {
  switch (0) {
  case 0:
  case sizeof(ava_ulong)*8 >= BXLIST_ORDER:
    ;
  }
}

typedef AO_t ava_bxlist_timestamp_atomic;
#define T_INFINITY (~(ava_bxlist_timestamp)0)

typedef struct {
  ava_bxlist_timestamp available[BXLIST_ORDER];
  char*restrict data;
} ava_bxlist_table;

struct ava_bxlist_node_s {
  /**
   * The size, in bytes, of each element *in this node*.
   */
  unsigned char element_size;
  /**
   * The size of elements in the leaves.
   */
  unsigned char leaf_element_size;
  /**
   * The height of this node. Leaves have height 0.
   */
  unsigned char height;

  ava_bxlist_weight_function weight_function;
  AO_t weight;

  ava_bxlist_table primary;

  /* Since most nodes won't have patch data, it is allocated on demand.
   * patch_available is "infinity" by default; readers may only inspect *patch
   * if t^R >= patch_available.
   */
  ava_bxlist_timestamp patch_available;
  ava_bxlist_table*restrict patch;

  ava_bxlist_timestamp_atomic* family;
};

/**
 * Element type within branches.
 */
typedef struct {
  const ava_bxlist_node*restrict child;
  /**
   * If not T_INFINITY, the child is in a different family.
   */
  ava_bxlist_timestamp foreign_timestamp;
  size_t num_prior_indices;
} ava_bxlist_branch_element;

/**
 * Allocates a new B+ List family with the given initial timestamp value.
 */
static ava_bxlist_timestamp_atomic* ava_bxlist_family_new(
  ava_bxlist_timestamp init_value);
/**
 * Allocates a new node with the given parameters.
 */
static ava_bxlist_node* ava_bxlist_node_new(
  unsigned char element_size,
  unsigned char leaf_element_size,
  unsigned char height,
  ava_bxlist_weight_function weight_function,
  ava_bxlist_timestamp_atomic* family);

/**
 * Takes ownership of the given node.
 *
 * If this succeeds (ie, t^R == t_N), the returned value is the same as the
 * input, and the family's timestamp has been incremented. Otherwise, the
 * target has been copied into a new node.
 *
 * In either case, the caller is may freely modify the returned value, and *t_r
 * is updated to the new t_N.
 *
 * The returned node is not necessarily in the same family as the input.
 */
static ava_bxlist_node* ava_bxlist_node_steal(
  const ava_bxlist_node*restrict target,
  ava_bxlist_timestamp* t_r);

/**
 * Obtains ownership of a node by transfering it from a family.
 *
 * If the target node is of the given family (which the caller is assumed to
 * have ownership of), it is returned. Otherwise, it is copied into a new node
 * belonging to that family and the new node is returned.
 */
static ava_bxlist_node* ava_bxlist_node_acquire(
  const ava_bxlist_branch_element*restrict target,
  ava_bxlist_timestamp_atomic* family);

/**
 * Creates a new node with the same contents as src when viewed at timestamp
 * tr, but belonging to the chosen family.
 *
 * The resulting node is normalised. It will be using none of its patch space,
 * and all activations are either t_r or infinity.
 */
static ava_bxlist_node* ava_bxlist_node_fork(
  const ava_bxlist_node*restrict src,
  ava_bxlist_timestamp_atomic* family,
  ava_bxlist_timestamp t_r);

/**
 * Returns a pointer to the element at the given element index in the given
 * node, as visible at the given t^R.
 *
 * If no element at that index is visible, returns NULL.
 */
static const void* ava_bxlist_node_get_element(
  const ava_bxlist_node*restrict node,
  unsigned element_index,
  ava_bxlist_timestamp t_r) AVA_PURE;

/**
 * Searches the children visible at the given t^R for the one that contains the
 * given list element index. This assumes the final child is infinitely large.
 *
 * Returns the index within the node of the child that contains index.
 */
static unsigned ava_bxlist_branch_seek_index(
  const ava_bxlist_node*restrict node,
  size_t index,
  ava_bxlist_timestamp t_r) AVA_PURE;

/**
 * Returns the number of primary slots used in the given node, as observed by
 * the given timestamp.
 */
static unsigned ava_bxlist_node_used(
  const ava_bxlist_node*restrict node,
  ava_bxlist_timestamp t_r) AVA_PURE;

/**
 * Returns the number of list elements under the given node's subtree, as
 * observed by the given timestamp.
 */
static size_t ava_bxlist_node_length(
  const ava_bxlist_node*restrict node,
  ava_bxlist_timestamp t_r) AVA_PURE;

/**
 * Ensures that the caller can write new values into elements whose
 * corresponding bits in elements_to_reserve are set.
 *
 * This will allocate patch space if necessary, or an entirely new node if
 * in-place modification is not possible. In the latter case, the contents of
 * the elements identified as being reserved are clear, which is an abnormal
 * condition for a bxlist node to be in (so write_element() *must* be called to
 * fill them in).
 */
static ava_bxlist_node* ava_bxlist_node_reserve_elements(
  ava_bxlist_node*restrict node,
  ava_ulong elements_to_reserve,
  unsigned first_element_to_reserve,
  unsigned last_element_to_reserve_plus_one,
  ava_bxlist_timestamp t_n);

/**
 * Writes a data element into the given node with the given timestamp.
 *
 * The slot *must* have been reserved with a prior call to reserve_elements()
 * unless the node is brand new or it is known that element_index ==
 * ava_bxlist_node_used(node,t_n).
 *
 * Note that elements whose availability is t_n are overwritten in-place.
 */
static void ava_bxlist_node_write_element(
  ava_bxlist_node*restrict node,
  const void*restrict data,
  unsigned element_index,
  ava_bxlist_timestamp t_n);

/**
 * Weight function for branch elements.
 */
static size_t ava_bxlist_branch_weight_function(const void*restrict);

/**
 * Combines one or two result nodes into a single root.
 */
static ava_bxlist_node* ava_bxlist_node_make_root(
  ava_bxlist_node*const restrict in[2],
  ava_bxlist_timestamp t_r);

/**
 * Replaces a single list element within the given node.
 *
 * @param node The node in which can be found the element to replace.
 * @param index The index, relative to the start of node, of the element to
 * replace.
 * @param data The data of the element to use as a replacement.
 * @param t_n The t_N of the mutable tree.
 * @return The node with the modification.
 */
static ava_bxlist_node* ava_bxlist_node_replace_one(
  ava_bxlist_node*restrict node,
  size_t index,
  const void*restrict data,
  ava_bxlist_timestamp t_n);

/**
 * Inserts a single list element into the given node.
 *
 * @param out The one or two resultant output nodes (since node could split).
 * @param node The node into which to insert the element.
 * @param index The index, relative to the start at node, after which the new
 * element is to be inserted.
 * @param data The element data to insert.
 * @param t_n The t_N of the mutable tree.
 */
static void ava_bxlist_node_insert_one(
  ava_bxlist_node*restrict out[2],
  ava_bxlist_node*restrict node,
  size_t index,
  const void*restrict data,
  ava_bxlist_timestamp t_n);

/**
 * Inserts the given element directly into the given node, without recursing.
 *
 * @param out The one or two resultant output nodes.
 * @param node The node to modify.
 * @param index The index, in terms of node elements, after which the new
 * element is to be inserted.
 * @param data The element data to insert.
 * @param t_n The t_N of the mutable tree.
 */
static void ava_bxlist_node_insert_one_direct(
  ava_bxlist_node*restrict out[2],
  ava_bxlist_node*restrict node,
  unsigned index,
  const void*restrict data,
  ava_bxlist_timestamp t_n);

/**
 * Appends a single list element to the end of the subtree of the given node.
 *
 * @param out The one or two resultant output nodes (since node could split).
 * @param node The node into which to append the element.
 * @param data The element data to insert.
 * @param t_n The t_N of the mutable tree.
 */
static void ava_bxlist_node_append_one(
  ava_bxlist_node*restrict out[2],
  ava_bxlist_node*restrict node,
  const void*restrict data,
  ava_bxlist_timestamp t_n);

/**
 * Appends the given element directly onto the given node, without recursing.
 *
 * @param out The one or two resultant output nodes.
 * @param node The node to modify.
 * @param data The element data to insert.
 * @param t_n The t_N of the mutable tree.
 */
static void ava_bxlist_node_append_one_direct(
  ava_bxlist_node*restrict out[2],
  ava_bxlist_node*restrict node,
  const void*restrict data,
  ava_bxlist_timestamp t_n);

/**
 * Adjusts the indices of elements within a spilled branch.
 *
 * If the given node is non-NULL, it is assumed to be a fresh node created to
 * house spill-over from an insert or append operation. The num_prior_indices
 * of the first element will be subtracted in-place from all elements in the
 * node.
 */
static void ava_bxlist_branch_adjust_indices(ava_bxlist_node*restrict node) {
  ava_bxlist_branch_element*restrict elts;
  unsigned i;
  size_t off;

  if (!node) return;

  elts = (ava_bxlist_branch_element*restrict)node->primary.data;
  off = elts[0].num_prior_indices;

  for (i = 0; i < BXLIST_ORDER && T_INFINITY != node->primary.available[i]; ++i)
    elts[i].num_prior_indices -= off;
}

ava_bxlist ava_bxlist_new(size_t element_size,
                          ava_bxlist_weight_function weight_function) {
  ava_bxlist_node*restrict root;

  assert(element_size > 0 && element_size < 256);

  root = ava_bxlist_node_new(
    element_size, element_size, 0, weight_function, ava_bxlist_family_new(0));

  return (ava_bxlist) { .root = root, .timestamp = 0 };
}

size_t ava_bxlist_weight(ava_bxlist list) {
  return list.root->weight;
}

size_t ava_bxlist_length(ava_bxlist list) {
  const ava_bxlist_node*restrict root = list.root;
  ava_bxlist_timestamp t_r = list.timestamp;

  return ava_bxlist_node_length(root, t_r);
}

static size_t ava_bxlist_node_length(const ava_bxlist_node*restrict node,
                                     ava_bxlist_timestamp t_r) {
  size_t length = 0;

  while (node->height > 0) {
    unsigned used = ava_bxlist_node_used(node, t_r);
    const ava_bxlist_branch_element*restrict elt =
      ava_bxlist_node_get_element(node, used-1, t_r);

    length += elt->num_prior_indices;
    if (T_INFINITY != elt->foreign_timestamp)
      t_r = elt->foreign_timestamp;

    node = elt->child;
  }

  length += ava_bxlist_node_used(node, t_r);
  return length;
}

const void* ava_bxlist_access(ava_bxlist list,
                              size_t index,
                              size_t*restrict available) {
  const ava_bxlist_node*restrict node = list.root;
  ava_bxlist_timestamp t_r = list.timestamp;
  const char*restrict ret, * next;
  unsigned avail;

  while (node->height > 0) {
    unsigned ix = ava_bxlist_branch_seek_index(node, index, t_r);
    const ava_bxlist_branch_element*restrict elt =
      ava_bxlist_node_get_element(node, ix, t_r);

    index -= elt->num_prior_indices;
    if (T_INFINITY != elt->foreign_timestamp)
      t_r = elt->foreign_timestamp;

    node = elt->child;
  }

  ret = ava_bxlist_node_get_element(node, index, t_r);

  if (available) {
    for (avail = 1; index + avail < BXLIST_ORDER; ++avail) {
      next = ava_bxlist_node_get_element(node, index + avail, t_r);
      if (ret + avail * node->element_size != next)
        break;
    }

    *available = avail;
  }

  return ret;
}

static ava_bxlist_timestamp_atomic* ava_bxlist_family_new(
  ava_bxlist_timestamp init_value
) {
  ava_bxlist_timestamp_atomic* family = ava_alloc_atomic(
    sizeof(ava_bxlist_timestamp_atomic));

  *family = init_value;
  return family;
}

static ava_bxlist_node* ava_bxlist_node_new(
  unsigned char element_size,
  unsigned char leaf_element_size,
  unsigned char height,
  ava_bxlist_weight_function weight_function,
  ava_bxlist_timestamp_atomic* family
) {
  ava_bxlist_node* node;

  node = ava_alloc(sizeof(ava_bxlist_node) +
                   BXLIST_ORDER * element_size);

  node->element_size = element_size;
  node->leaf_element_size = leaf_element_size;
  node->height = height;
  node->weight_function = weight_function;
  node->family = family;

  memset(node->primary.available, -1, sizeof(node->primary.available));
  node->primary.data = (char*)(node + 1);
  node->patch_available = T_INFINITY;

  return node;
}

static ava_bxlist_node* ava_bxlist_node_steal(
  const ava_bxlist_node*restrict target,
  ava_bxlist_timestamp* t_r_inout
) {
  ava_bxlist_timestamp t_r = *t_r_inout;
  ava_bxlist_timestamp_atomic tprime_r = t_r + 1;

  if (tprime_r < T_INFINITY &&
      AO_compare_and_swap(target->family, t_r, tprime_r)) {
    /* Success, we now own target and may mutate it */
    *t_r_inout = tprime_r;
    return (ava_bxlist_node*)target;
  } else {
    /* Either someone else has already taken ownership, or we've hit the
     * integer ceiling. In either case, fork to a new node.
     */
    *t_r_inout = 0;
    return ava_bxlist_node_fork(target, ava_bxlist_family_new(0), t_r);
  }
}

static ava_bxlist_node* ava_bxlist_node_acquire(
  const ava_bxlist_branch_element*restrict target,
  ava_bxlist_timestamp_atomic* family
) {
  if (T_INFINITY == target->foreign_timestamp) {
    /* Already have ownership */
    return (ava_bxlist_node*)target->child;
  } else {
    /* No ownership. Create a new node in the current family. */
    return ava_bxlist_node_fork(
      target->child, family, target->foreign_timestamp);
  }
}

static ava_bxlist_node* ava_bxlist_node_fork(
  const ava_bxlist_node*restrict src,
  ava_bxlist_timestamp_atomic* family,
  ava_bxlist_timestamp t_r
) {
  unsigned i, nelt;
  ava_bxlist_timestamp t_n = *family;
  ava_bxlist_node*restrict dst = ava_bxlist_node_new(
    src->element_size, src->leaf_element_size, src->height,
    src->weight_function, family);
  const void*restrict elt;

  for (i = 0; i < BXLIST_ORDER &&
         (elt = ava_bxlist_node_get_element(src, i, t_r)); ++i) {
    ava_bxlist_node_write_element(dst, elt, i, t_n);
  }

  /* For branches, we may need to set foreign_timestamp if some children are
   * now cross-node.
   */
  nelt = i;
  if (src->height > 0) {
    for (i = 0; i < nelt; ++i) {
      ava_bxlist_branch_element*restrict belt =
        ((ava_bxlist_branch_element*restrict)dst->primary.data) + i;

      if (T_INFINITY == belt->foreign_timestamp &&
          family != belt->child->family) {
        belt->foreign_timestamp = t_r;
      }
    }
  }

  return dst;
}

static const void* ava_bxlist_node_get_element(
  const ava_bxlist_node*restrict node,
  unsigned ix,
  ava_bxlist_timestamp t_r
) {
  assert(ix < BXLIST_ORDER);

  if (t_r < node->primary.available[ix])
    return NULL;

  if (t_r >= node->patch_available && t_r >= node->patch->available[ix])
    return node->patch->data + ix * node->element_size;
  else
    return node->primary.data + ix * node->element_size;
}

static unsigned ava_bxlist_branch_seek_index(
  const ava_bxlist_node*restrict node,
  size_t index,
  ava_bxlist_timestamp t_r
) {
  /* lower_bound and upper_bound are both inclusive */
  unsigned lower_bound = 0, upper_bound = BXLIST_ORDER - 1, mid;
  const ava_bxlist_branch_element*restrict elt;

  while (lower_bound < upper_bound) {
    mid = (lower_bound + upper_bound + 1) / 2;
    elt = ava_bxlist_node_get_element(node, mid, t_r);

    if (NULL == elt || elt->num_prior_indices > index) {
      upper_bound = mid - 1;
    } else {
      lower_bound = mid;
    }
  }

  return lower_bound /* lower_bound == upper_bound */;
}

static unsigned ava_bxlist_node_used(
  const ava_bxlist_node*restrict node,
  ava_bxlist_timestamp t_r
) {
  unsigned lower_bound = 0, upper_bound = 1, mid;

  while (upper_bound < BXLIST_ORDER &&
         node->primary.available[upper_bound-1] <= t_r) {
    lower_bound = upper_bound;
    upper_bound *= 2;
  }

  while (lower_bound < upper_bound) {
    mid = (lower_bound + upper_bound) / 2;
    if (node->primary.available[mid] <= t_r)
      lower_bound = mid + 1;
    else
      upper_bound = mid;
  }

  /* lower_bound == upper_bound */
  return lower_bound;
}

static ava_bxlist_node* ava_bxlist_node_reserve_elements(
  ava_bxlist_node* node,
  ava_ulong elements_to_reserve,
  unsigned first_element_to_reserve,
  unsigned last_element_to_reserve,
  ava_bxlist_timestamp t_n
) {
  ava_ulong elements;
  unsigned ix;
  ava_bool needs_patch = 0;

  elements = elements_to_reserve >> first_element_to_reserve;

  for (ix = first_element_to_reserve; ix < last_element_to_reserve;
       ++ix, elements >>= 1) {
    if (elements & 1) {
      if (node->primary.available[ix] >= t_n)
        /* Can just add to or overwrite primary, nothing needed to reserve */
        continue;

      if (T_INFINITY == node->patch_available ||
          node->patch->available[ix] >= t_n) {
        /* Can fit in the patch space. Flag that we'll need it later if we
         * don't end up copying the whole node.
         */
        needs_patch = ava_true;
        continue;
      }

      /* No space in primary or patch, need a brand new node */
      goto copy_node;
    }
  }

  /* If we can continue using this node but patch space is required, make sure
   * it is there.
   */
  if (needs_patch && T_INFINITY == node->patch_available) {
    node->patch = ava_alloc(sizeof(ava_bxlist_table) +
                            node->element_size * BXLIST_ORDER);
    node->patch->data = (char*)(node->patch + 1);
    memset(node->patch->available, -1, sizeof(node->patch->available));
    node->patch_available = t_n;
  }

  return node;

  copy_node:
  /* Fork the node, but then erase the primary elements we're about to
   * overwrite
   */
  node = ava_bxlist_node_fork(node, node->family, t_n);
  elements = elements_to_reserve >> first_element_to_reserve;
  for (ix = first_element_to_reserve; ix < last_element_to_reserve;
       ++ix, elements >>= 1)
    if (elements & 1)
      node->primary.available[ix] = T_INFINITY;

  return node;
}

static void ava_bxlist_node_write_element(
  ava_bxlist_node*restrict node,
  const void*restrict data,
  unsigned ix,
  ava_bxlist_timestamp t_n
) {
  if (node->primary.available[ix] >= t_n) {
    memcpy(node->primary.data + ix * node->element_size,
           data, node->element_size);
    node->primary.available[ix] = t_n;
  } else {
    memcpy(node->patch->data + ix * node->element_size,
           data, node->element_size);
    node->patch->available[ix] = t_n;
  }

  node->weight += (*node->weight_function)(data);
}

static size_t ava_bxlist_branch_weight_function(const void*restrict vnode) {
  return ((const ava_bxlist_branch_element*restrict)vnode)->child->weight;
}

ava_bxlist ava_bxlist_replace(ava_bxlist list,
                              size_t index,
                              const void*restrict data,
                              size_t num_elements) {
  ava_bxlist_timestamp t_r = list.timestamp;
  ava_bxlist_node*restrict root = ava_bxlist_node_steal(list.root, &t_r);

  while (num_elements--) {
    root = ava_bxlist_node_replace_one(root, index++, data, t_r);
    data = root->leaf_element_size + (const char*restrict)data;
  }

  return (ava_bxlist) { .root = root, .timestamp = t_r };
}

static ava_bxlist_node* ava_bxlist_node_replace_one(
  ava_bxlist_node*restrict node,
  size_t index,
  const void*restrict data,
  ava_bxlist_timestamp t_n
) {
  if (node->height > 0) {
    unsigned elt_ix = ava_bxlist_branch_seek_index(node, index, t_n);
    const ava_bxlist_branch_element*restrict old_elt =
      ava_bxlist_node_get_element(node, elt_ix, t_n);
    ava_bxlist_node*restrict child = ava_bxlist_node_acquire(
      old_elt, node->family);

    child = ava_bxlist_node_replace_one(
      child, index - old_elt->num_prior_indices, data, t_n);

    if (child != old_elt->child) {
      /* Need to replace the element in this node */
      ava_bxlist_branch_element new_elt = {
        .child = child,
        .foreign_timestamp = T_INFINITY,
        .num_prior_indices = old_elt->num_prior_indices
      };
      node = ava_bxlist_node_reserve_elements(
        node, 1ULL << elt_ix, elt_ix, elt_ix + 1, t_n);
      ava_bxlist_node_write_element(node, &new_elt, elt_ix, t_n);
    }

  } else {
    node = ava_bxlist_node_reserve_elements(
      node, 1ULL << index, index, index + 1, t_n);
    ava_bxlist_node_write_element(node, data, index, t_n);
  }

  return node;
}

static ava_bxlist_node* ava_bxlist_node_make_root(
  ava_bxlist_node*const restrict in[2],
  ava_bxlist_timestamp t_r
) {
  if (in[1]) {
    ava_bxlist_node*restrict new_root = ava_bxlist_node_new(
      sizeof(ava_bxlist_branch_element), in[0]->leaf_element_size,
      in[0]->height + 1,
      ava_bxlist_branch_weight_function,
      in[0]->family);

    ava_bxlist_branch_element elts[2] = {
      { .child = in[0],
        .foreign_timestamp = T_INFINITY,
        .num_prior_indices = 0 },
      { .child = in[1],
        .foreign_timestamp = T_INFINITY,
        .num_prior_indices = ava_bxlist_node_length(in[0], t_r) },
    };

    new_root->primary.available[0] =
      new_root->primary.available[1] = t_r;
    memcpy(new_root->primary.data, elts, sizeof(elts));
    new_root->weight = in[0]->weight + in[1]->weight;

    return new_root;
  } else {
    return in[0];
  }
}

ava_bxlist ava_bxlist_insert(
  ava_bxlist list,
  size_t index,
  const void*restrict data,
  size_t num_elements
) {
  ava_bxlist_timestamp t_r = list.timestamp;
  ava_bxlist_node*restrict root = ava_bxlist_node_steal(list.root, &t_r);

  while (num_elements--) {
    ava_bxlist_node*restrict result[2];
    ava_bxlist_node_insert_one(result, root, index++, data, t_r);
    data = root->leaf_element_size + (const char*restrict)data;

    root = ava_bxlist_node_make_root(result, t_r);
  }

  return (ava_bxlist) { .root = root, .timestamp = t_r };
}

static void ava_bxlist_node_insert_one(
  ava_bxlist_node*restrict out[2],
  ava_bxlist_node*restrict node,
  size_t index,
  const void*restrict data,
  ava_bxlist_timestamp t_n
) {
  if (0 == node->height) {
    /* Leaf */
    ava_bxlist_node_insert_one_direct(out, node, index, data, t_n);
  } else {
    /* Branch */
    unsigned ix = ava_bxlist_branch_seek_index(node, index, t_n);
    unsigned used = ava_bxlist_node_used(node, t_n);
    unsigned i;
    const ava_bxlist_branch_element*restrict old_elt =
      ava_bxlist_node_get_element(node, ix, t_n);
    ava_bxlist_branch_element new_elt;
    ava_bxlist_node*restrict new_children[2];
    ava_bxlist_node*restrict orig_node = node;

    ava_bxlist_node_insert_one(
      new_children, ava_bxlist_node_acquire(old_elt, node->family),
      index - old_elt->num_prior_indices, data, t_n);

    /* If the main child has changed, rewrite that element */
    if (new_children[0] != old_elt->child) {
      new_elt.child = new_children[0];
      new_elt.foreign_timestamp = T_INFINITY;
      new_elt.num_prior_indices = old_elt->num_prior_indices;

      node = ava_bxlist_node_reserve_elements(
        /* Also include everything after it, since we'll be changing them all
         * anyway and don't want to allocate patch space to just end up copying
         * to a new node anyway.
         */
        node, ~0ULL << ix, ix, used, t_n);
      ava_bxlist_node_write_element(node, &new_elt, ix, t_n);
    }

    /* Adjust the num_prior_indices of all following elements */
    node = ava_bxlist_node_reserve_elements(
      node, ~0ULL << (ix + 1), ix + 1, used, t_n);
    for (i = ix + 1; i < used; ++i) {
      const ava_bxlist_branch_element*restrict cur =
        ava_bxlist_node_get_element(orig_node, i, t_n);
      new_elt = *cur;
      ++new_elt.num_prior_indices;
      ava_bxlist_node_write_element(node, &new_elt, i, t_n);
    }

    if (!new_children[1]) {
      /* The child didn't split, we're done */
      out[0] = node;
      out[1] = NULL;
    } else {
      /* The child split, need to insert the new one */
      new_elt.child = new_children[1];
      new_elt.foreign_timestamp = T_INFINITY;
      new_elt.num_prior_indices = old_elt->num_prior_indices +
        ava_bxlist_node_length(new_children[0], t_n);
      ava_bxlist_node_insert_one_direct(
        out, node, ix + 1, &new_elt, t_n);
      ava_bxlist_branch_adjust_indices(out[1]);
    }
  }
}

static void ava_bxlist_node_insert_one_direct(
  ava_bxlist_node*restrict out[2],
  ava_bxlist_node*restrict node,
  unsigned index,
  const void*restrict data,
  ava_bxlist_timestamp t_n
) {
 unsigned used = ava_bxlist_node_used(node, t_n);

 if (used < BXLIST_ORDER) {
   /* There is room for the new element */
   ava_ulong mask = (~0ULL) << index;
   unsigned i;
   ava_bxlist_node*restrict old_node = node;
   node = ava_bxlist_node_reserve_elements(node, mask, index, used+1, t_n);

   for (i = used; i > index; --i)
     ava_bxlist_node_write_element(
       node, ava_bxlist_node_get_element(old_node, i-1, t_n), i, t_n);

   ava_bxlist_node_write_element(node, data, index, t_n);

   out[0] = node;
   out[1] = NULL;
 } else {
   /* Insufficient space, split into two nodes */
   unsigned split = (used + 1) / 2, i;
   out[0] = ava_bxlist_node_new(node->element_size, node->leaf_element_size,
                                node->height, node->weight_function,
                                node->family);
   out[1] = ava_bxlist_node_new(node->element_size, node->leaf_element_size,
                                node->height, node->weight_function,
                                node->family);

   for (i = 0; i <= used; ++i) {
     ava_bxlist_node_write_element(
       out[i >= split],
       (i < index? ava_bxlist_node_get_element(node, i, t_n) :
        i == index? data :
        ava_bxlist_node_get_element(node, i-1, t_n)),
       i < split? i : i - split, t_n);
   }
 }
}

ava_bxlist ava_bxlist_append(
  ava_bxlist list,
  const void*restrict data,
  size_t num_elements
) {
  ava_bxlist_timestamp t_r = list.timestamp;
  ava_bxlist_node*restrict root = ava_bxlist_node_steal(list.root, &t_r);

  while (num_elements--) {
    ava_bxlist_node*restrict result[2];
    ava_bxlist_node_append_one(result, root, data, t_r);
    data = root->leaf_element_size + (const char*restrict)data;

    root = ava_bxlist_node_make_root(result, t_r);
  }

  return (ava_bxlist) { .root = root, .timestamp = t_r };
}

static void ava_bxlist_node_append_one(
  ava_bxlist_node*restrict out[2],
  ava_bxlist_node*restrict node,
  const void*restrict data,
  ava_bxlist_timestamp t_n
) {
  if (0 == node->height) {
    /* Leaf */
    ava_bxlist_node_append_one_direct(out, node, data, t_n);
  } else {
    /* Branch */
    unsigned ix = ava_bxlist_node_used(node, t_n) - 1;
    const ava_bxlist_branch_element*restrict old_elt =
      ava_bxlist_node_get_element(node, ix, t_n);
    ava_bxlist_node*restrict new_children[2];

    ava_bxlist_node_append_one(
      new_children, ava_bxlist_node_acquire(old_elt, node->family),
      data, t_n);

    /* Update the first child if it has changed */
    if (new_children[0] != old_elt->child) {
      ava_bxlist_branch_element new_elt = {
        .child = new_children[0],
        .foreign_timestamp = T_INFINITY,
        .num_prior_indices = old_elt->num_prior_indices
      };

      node = ava_bxlist_node_reserve_elements(
        node, 1ULL << ix, ix, ix + 1, t_n);
      ava_bxlist_node_write_element(node, &new_elt, ix, t_n);
    }

    if (new_children[1]) {
      /* Need to append the new child to self */
      ava_bxlist_branch_element new_elt = {
        .child = new_children[1],
        .foreign_timestamp = T_INFINITY,
        .num_prior_indices = old_elt->num_prior_indices +
          ava_bxlist_node_length(new_children[0], t_n)
      };

      ava_bxlist_node_append_one_direct(out, node, &new_elt, t_n);
      ava_bxlist_branch_adjust_indices(out[1]);
    } else {
      /* This node doesn't need to grow, we're done */
      out[0] = node;
      out[1] = NULL;
    }
  }
}

static void ava_bxlist_node_append_one_direct(
  ava_bxlist_node*restrict out[2],
  ava_bxlist_node*restrict node,
  const void*restrict data,
  ava_bxlist_timestamp t_n
) {
  unsigned used = ava_bxlist_node_used(node, t_n);

  if (used < BXLIST_ORDER) {
    /* Can append in-place */
    ava_bxlist_node_write_element(node, data, used, t_n);
    out[0] = node;
    out[1] = NULL;
  } else {
    /* Need to branch into a new node */
    out[0] = node;
    out[1] = ava_bxlist_node_new(
      node->element_size, node->leaf_element_size, node->height,
      node->weight_function, node->family);
    ava_bxlist_node_write_element(out[1], data, 0, t_n);
  }
}
