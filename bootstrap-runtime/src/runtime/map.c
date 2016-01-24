/*-
 * Copyright (c) 2015 Jason Lingle
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
#include <assert.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/errors.h"
#include "avalanche/exception.h"
#include "avalanche/list.h"
#include "avalanche/map.h"
#include "-list-map.h"
#include "-hash-map.h"

const ava_attribute_tag ava_map_trait_tag = {
  .name = "map"
};

static ava_map_value ava_map_value_of_list(ava_list_value);

ava_map_value ava_map_value_of(ava_value value) {
  if (!ava_get_attribute(value, &ava_map_trait_tag))
    return ava_map_value_of_list(ava_list_value_of(value));
  else
    return (ava_map_value) { value };
}

ava_fat_map_value ava_fat_map_value_of(ava_value value) {
  const ava_map_trait* trait = ava_get_attribute(
    value, &ava_map_trait_tag);

  if (!trait) {
    value = ava_map_value_of_list(ava_list_value_of(value)).v;
    trait = ava_get_attribute(value, &ava_map_trait_tag);
    assert(trait);
  }

  return (ava_fat_map_value) { .v = trait, .c = { value } };
}

static ava_map_value ava_map_value_of_list(ava_list_value list) {
  size_t length;

  length = ava_list_length(list);
  if (length % 2) {
    ava_throw_str(&ava_format_exception, ava_error_odd_length_list_to_map());
  }

  if (0 == length)
    return ava_empty_map();

  if (length <= AVA_LIST_MAP_THRESH)
    return ava_list_map_of_list(list);
  else
    return ava_hash_map_of_list(list);
}

ava_map_value ava_map_of_values(const ava_value*restrict keys,
                                size_t key_stride,
                                const ava_value*restrict values,
                                size_t value_stride,
                                size_t count) {
  if (0 == count) {
    return ava_empty_map();
  } else if (count <= AVA_LIST_MAP_THRESH) {
    ava_value interleaved[2*count];
    size_t i;

    for (i = 0; i < count; ++i) {
      interleaved[i*2 + 0] = keys[i * key_stride];
      interleaved[i*2 + 1] = values[i * value_stride];
    }

    return ava_list_map_of_list(ava_list_of_values(interleaved, count*2));
  } else {
    return ava_hash_map_of_raw(keys, key_stride, values, value_stride, count);
  }
}
