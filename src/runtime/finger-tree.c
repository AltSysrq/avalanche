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

/*
  Required reading: http://www.soi.city.ac.uk/~ross/papers/FingerTree.pdf

  The paper is rather hard to follow from the subject matter alone; the copious
  operators it invents, as well as the unique syntax that isn't even valid
  Haskell, doesn't help. Thus this C implementation is heavily annotated with
  paraphrases/interpretations of the paper. Additionally, we start with
  structure here and only delve into type classes and currying when it is
  actually needed.
 */

/*
  Page 8

  data Digit a = One   a
               | Two   a a
               | Three a a a
               | Four  a a a a

  A digit is a "buffer" of up to 4 elements used to control rebalancing of the
  finger tree; each tree has one such buffer to either side of it.

  n is 1, 2, 3, or 4 to select from the four union types. a has that many
  elements.
 */
typedef struct {
  unsigned n;
  ava_data a[];
} ava_ft_digit;

/*
  Page 10

  data Node v a = Node2 v a a | Node3 v a a a

  A Node contains two or three elements of type a (stored in n) and the
  aggregate measurement of type v, stored in field v. The data are in field a,
  which has length n.
 */
typedef {
  unsigned n;
  ava_data v;
  ava_data a[];
} ava_ft_node;

typedef union ava_ft_finger_tree_u ava_ft_finger_tree;

typedef struct ava_ft_finger_tree_thunk_s ava_ft_finger_tree_thunk;

/*
  This is discussed within the finger_tree union itself.
 */
struct ava_ft_finger_tree_thunk {
  /*
    Function implementing the thunk. It is expected to mutate the thunk
    structure so that further calls do not recompute the value. This must be
    thread-safe.
   */
  const ava_ft_finger_tree* (*thunk)(ava_ft_finger_tree_thunk*restrict);
  /*
    Arbitrary data for the thunk.
   */
  void* v[2];
};

/*
  Page 11

  data FingerTree v a = Empty
                      | Single a
                      | Deep v (Digit a) (FingerTree v (Node v a)) (Digit a)

  Empty finger trees are represented with NULL pointers.

  Single and Deep are distinguished as in the Deep case, deep.sf will be
  non-NULL, whereas it will be NULL for Single.
 */
typedef union ava_ft_finger_tree_u {
  /*
    Single: a

    For the trivial case of a FingerTree with exactly one element, this stores
    the value of that element.
  */
  ava_data single;
  /* Deep */
  struct {
    /*
      v

      This caches the summation of
      - The elements in pr
      - m->v
      - The elements in sf
     */
    ava_data v;
    /*
      Digit a

      A small array of values to the left of the main body of the tree. It is
      called "pr", for prefix, in the paper. This may also be thought of as a
      buffer for prepended elements.
     */
    const ava_ft_digit*restrict pr;
    /*
      FingerTree v (Node v a)

      It is worth emphasising that this is a finger tree _of nodes_, not of
      simple values. This means that the digits of deeply nested finger trees
      actually turn into 2-3 trees.

      As described on page 7, lazy evaluation of this value is paramount to
      performance of the finger tree. The actual value of m can be obtained
      when needed with (*m->thunk)(&m), which will only need to compute the new
      value once.
     */
    ava_ft_finger_tree_thunk*restrict m;
    /*
      Digit a

      Similar to pr, a small array of values to the right of the subtree. It is
      called "sf", for suffix, in the paper. This may also be thought of as a
      buffer for appended elements.
     */
    const ava_ft_digit*restrict sf;
  } deep;
};
