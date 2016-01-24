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
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/parser.h"
#include "avalanche/macro-arg.h"

ava_bool ava_macro_arg_literal(
  ava_value* dst,
  const ava_parse_unit** error_unit,
  const ava_parse_unit* unit
) {
  const ava_parse_unit* elt;
  ava_list_value accum;
  ava_value child;

  switch (unit->type) {
  case ava_put_bareword:
  case ava_put_astring:
  case ava_put_verbatim:
    *dst = ava_value_of_string(unit->v.string);
    return ava_true;

  case ava_put_semiliteral:
    accum = ava_empty_list();
    TAILQ_FOREACH(elt, &unit->v.units, next) {
      if (!ava_macro_arg_literal(&child, error_unit, elt))
        return ava_false;
      accum = ava_list_append(accum, child);
    }
    *dst = accum.v;
    return ava_true;

  default:
    *error_unit = unit;
    return ava_false;
  }
}
