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
#include "avalanche/exception.h"
#include "avalanche/list.h"
#include "avalanche/map.h"

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
  AVA_STATIC_STRING(odd_length_message,
                    "list of odd length cannot be interpreted as map");

  if (ava_list_length(list) % 2) {
    ava_throw(&ava_format_exception,
              ava_value_of_string(odd_length_message),
              NULL);
  }

  /* TODO */
  abort();
}
