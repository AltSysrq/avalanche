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
#include <string.h>
#include <stdio.h>

#define AVA__INTERNAL_INCLUDE 1
#include "../runtime/avalanche/defs.h"
#include "../runtime/avalanche/string.h"
#include "../runtime/avalanche/value.h"
#include "../runtime/avalanche/context.h"
#include "../runtime/avalanche/pcode.h"
#include "../runtime/avalanche/pcode-linker.h"
#include "../bsd.h"

#include "common.h"

/*

  P-Code Implementation -> Interface Converter

  Usage: make-interface infile

  The given input is read, parsed as P-Code, reduced to an interface, and the
  result output to the infile with "i" appended.

 */
static ava_value main_impl(void* arg) {
  unsigned argc = ((const main_data*)arg)->argc;
  const char*const* argv = ((const main_data*)arg)->argv;

  ava_string infile, outfile;

  if (2 != argc)
    errx(EX_USAGE, "Usage: %s <infile>", argv[0]);

  infile = ava_string_of_cstring(argv[1]);
  outfile = ava_string_concat(infile, AVA_ASCII9_STRING("i"));

  spit(outfile, ava_pcode_to_interface(slurp(infile)));

  return ava_value_of_string(AVA_EMPTY_STRING);
}
