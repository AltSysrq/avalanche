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

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/map.h"
#include "-hash-map.h"

ava_map_value ava_hash_map_of_raw(const ava_value*restrict keys,
                                  size_t key_stride,
                                  const ava_value*restrict values,
                                  size_t value_stride,
                                  size_t count) {
  if (count < (1 << 12))
    return ava_hash_map_of_raw_ava_ushort(
      keys, key_stride, values, value_stride, count);
  else if (count < (1 << 24))
    return ava_hash_map_of_raw_ava_uint(
      keys, key_stride, values, value_stride, count);
  else
    return ava_hash_map_of_raw_ava_ulong(
      keys, key_stride, values, value_stride, count);
}

ava_map_value ava_hash_map_of_list(ava_list_value list) {
  size_t count = ava_list_length(list) / 2;

  /* Note that these thresholds are 1 lower than what's used in the
   * specialisations. This is necessary since the specialisations delegate to
   * this function to promote themselves to larger types when they hit that
   * threshold.
   */
  if (count < (1 << 12))
    return ava_hash_map_of_list_ava_ushort(list);
  else if (count < (1 << 24))
    return ava_hash_map_of_list_ava_uint(list);
  else
    return ava_hash_map_of_list_ava_ulong(list);
}

const char* ava_hash_map_get_hash_function(ava_map_value map) {
  /* All the specialisations have a similar enough layout to use any of them.
   * Kind of hacky, but this function is only used for tests anyway.
   */
  return ava_hash_map_get_hash_function_ava_ushort(map);
}
