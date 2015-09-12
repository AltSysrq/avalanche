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

#include "avalanche/defs.h"
#include "avalanche/string.h"
#include "avalanche/name-mangle.h"
#include "-hexes.h"

ava_demangled_name ava_name_demangle(ava_string instr) {
  size_t srclen = ava_string_length(instr);
  char dst[srclen];
  ava_str_tmpbuff srctmp;
  const unsigned char* src;
  size_t in, out;

  src = (const unsigned char*)ava_string_to_cstring_buff(srctmp, instr);

  if (srclen >= 3 && 'a' == src[0] && '$' == src[1]) {
    /* Looks like avalanche name mangling */
    in = 2;
    out = 0;

    while (in < srclen) {
      switch (src[in]) {
      case '_':
        if (in+1 >= srclen || '_' != src[in+1]) {
          dst[out++] = '-';
          in += 1;
        } else if (in+2 >= srclen || '_' != src[in+2]) {
          dst[out++] = '.';
          in += 2;
        } else {
          dst[out++] = ':';
          in += 3;
        }
        break;

      case '$':
        if (in + 2 >= srclen ||
            255 == ava_hexes[src[in+1]] ||
            255 == ava_hexes[src[in+2]] ||
            /* For the sake of normalisation, expressly forbid lowercase
             * hexits
             */
            (src[in+1] >= 'a' && src[in+1] <= 'f') ||
            (src[in+2] >= 'a' && src[in+2] <= 'f'))
          /* Invalid ava format, assume no name mangling */
          goto no_name_mangling;

        dst[out++] = (ava_hexes[src[in+1]] << 4) | ava_hexes[src[in+2]];
        in += 3;
        break;

      default:
        dst[out++] = src[in++];
      }
    }

    return (ava_demangled_name) {
      .scheme = ava_nms_ava,
      .name = ava_string_of_bytes(dst, out)
    };
  }

  no_name_mangling:
  return (ava_demangled_name) {
    .scheme = ava_nms_none,
    .name = instr
  };
}

ava_string ava_name_mangle(ava_demangled_name name) {
  static const char hexits[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                   '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  size_t srclen = ava_string_length(name.name);
  char dst[2 + srclen*3];
  ava_str_tmpbuff srctmp;
  const unsigned char* src;
  size_t in, out;

  src = (const unsigned char*)ava_string_to_cstring_buff(srctmp, name.name);

  switch (name.scheme) {
  case ava_nms_none:
    return name.name;

  case ava_nms_ava:
    out = 0;
    dst[out++] = 'a';
    dst[out++] = '$';

    for (in = 0; in < srclen; ++in) {
      if (src[in] == '-' && '_' != dst[out-1]) {
        dst[out++] = '_';
      } else if (src[in] == '.' && '_' != dst[out-1]) {
        dst[out++] = '_';
        dst[out++] = '_';
      } else if (src[in] == ':' && '_' != dst[out-1]) {
        dst[out++] = '_';
        dst[out++] = '_';
        dst[out++] = '_';
      } else if ((src[in] >= 'a' && src[in] <= 'z') ||
                 (src[in] >= 'A' && src[in] <= 'Z') ||
                 (src[in] >= '0' && src[in] <= '9')) {
        dst[out++] = src[in];
      } else {
        dst[out++] = '$';
        dst[out++] = hexits[(src[in] >> 4) & 0xF];
        dst[out++] = hexits[src[in] & 0xF];
      }
    }

    return ava_string_of_bytes(dst, out);
  }

  /* unreachable */
  abort();
}
