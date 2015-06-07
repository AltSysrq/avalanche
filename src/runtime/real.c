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

#define AVA__INTERNAL_INCLUDE 1
#define AVA__IN_REAL_C
#include "avalanche/defs.h"
#include "avalanche/integer.h"
#include "avalanche/real.h"
#include "-dtoa.h"

static ava_string ava_real_value_to_string(ava_value this);

const ava_value_trait ava_real_type = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "real",
  .to_string = ava_real_value_to_string,
  .string_chunk_iterator = ava_singleton_string_chunk_iterator,
  .iterate_string_chunk = ava_iterate_singleton_string_chunk,
};

ava_real ava_real_of_nonnumeric_value(ava_value value, ava_real dfault) {
  char tmp[AVA_STR_TMPSZ];
  const char* str;
  char* end;
  ava_real ret;

  str = ava_string_to_cstring_buff(tmp, ava_to_string(value));

  /* Skip past any whitespace */
  while (*str &&
         (*str == ' ' || *str == '\n' || *str == '\r' || *str == '\t'))
    ++str;

  if (!*str)
    /* Empty string, return default */
    return dfault;

  /* First, try to let strtod() parse it */
  ret = ava_strtod(str, &end);

  /* Ensure that any characters after the end of the parsed double are actually
   * whitespace.
   */
  while (*end) {
    if (*end != ' ' && *end != '\n' && *end != '\r' && *end != '\t') {
      /* Not a valid real, fall back to integer parsing. */
      return ava_integer_of_value(value, 0);
    } else {
      ++end;
    }
  }

  /* Reached the end of the string without finding non-whitespace, so ret is a
   * valid value.
   */
  return ret;
}

static ava_string ava_real_value_to_string(ava_value this) {
  /* "it suffices to declare buf ... char buf[32];" */
  char buf[32];
  ava_dtoa_fmt(buf, ava_value_real(this));
  return ava_string_of_cstring(buf);
}
