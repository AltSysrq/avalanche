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
#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/strangelet.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_STRANGELET_H_
#define AVA_RUNTIME_STRANGELET_H_

#include "value.h"

/**
 * Used as the head of an ava_value attribute chain to indicate that the
 * ava_value is actually a strangelet.
 *
 * Strangelets are a general-purpose escape hatch used to support interop with
 * the underlying platform and to represent certain non-value concepts.
 *
 * Strangelets are not considered true values. While they implement the full
 * value trait sufficiently to not be unsafe if used in a context unaware of
 * them, they are not useful as such. For example, the stringification of a
 * strangelet causes its strangeness to be lost irreversibly.
 *
 * Strangelets provides the following guarantees when stringified:
 * - Stringification never throws in ordinary circumstances.
 * - A given strangelet always stringifies to the same string.
 * - Two different strangelets never stringify to the same string.
 *
 * The data field of a strangelet is always a pointer. Code operating on a
 * strangelet may assume that if it observes a strangelet with a pointer only
 * it governs, the strangelet has exactly the meaning that code wishes to
 * ascribe to it. (Eg, it is reasonable to use strangelets as sentinels.)
 *
 * Note that unlike pointer values, strangelets retain the thing they point to,
 * and can therefore safely be used to reference garbage-collectable memory.
 */
extern const ava_value_trait ava_strangelet_type;

/**
 * Convenience function for creating a strangelet holding the given pointer.
 */
static inline ava_value ava_strange_ptr(const void* ptr) {
  return ava_value_with_ptr(&ava_strangelet_type, ptr);
}

#endif /* AVA_RUNTIME_STRANGELET_H_ */
