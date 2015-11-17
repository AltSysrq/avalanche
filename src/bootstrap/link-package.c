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
#include "../runtime/avalanche/errors.h"
#include "../bsd.h"

#include "common.h"

/*

  Links one or more modules (.avam) into a fat package (.avap).

  Usage: link-package module [...]

  The module must have a directory prefix. This directory is used as the
  package name; ie, given an input of foo/bar.avam, the output is foo.avap. All
  modules after the first must begin with the same directory prefix.

 */

static ava_string validate_infile(ava_string dir_prefix, ava_string infile);

static ava_value main_impl(void* arg) {
  unsigned argc = ((const main_data*)arg)->argc;
  const char*const* argv = ((const main_data*)arg)->argv;

  ava_string package_name, dir_prefix, outfile, infile, module_name;
  const char* slash;
  unsigned i;
  ava_pcode_linker* linker;
  const ava_pcode_global_list * package;
  ava_compile_error_list errors;

  if (1 == argc)
    errx(EX_USAGE, "Usage: %s infile [...]", argv[0]);

  slash = strchr(argv[1], '/');
  if (!slash || slash == argv[1])
    errx(EX_USAGE, "Bad infile: %s", argv[1]);

  package_name = ava_string_of_bytes(argv[1], slash - argv[1]);
  dir_prefix = ava_strcat(package_name, AVA_ASCII9_STRING("/"));
  outfile = ava_strcat(package_name, AVA_ASCII9_STRING(".avap"));

  linker = ava_pcode_linker_new();
  TAILQ_INIT(&errors);

  for (i = 1; i < argc; ++i) {
    infile = ava_string_of_cstring(argv[i]);
    module_name = validate_infile(dir_prefix, infile);
    ava_pcode_linker_add_module(linker, module_name, slurp(infile));
  }

  package = ava_pcode_linker_link(linker, &errors);
  if (!TAILQ_EMPTY(&errors))
    errx(EX_DATAERR, "Link failed.\n%s",
         ava_string_to_cstring(
           ava_error_list_to_string(&errors, 50, ava_false)));

  spit(outfile, package);

  return ava_value_of_string(AVA_EMPTY_STRING);
}

static ava_string validate_infile(ava_string dir_prefix, ava_string infile) {
  size_t len;

  if (!ava_string_starts_with(infile, dir_prefix))
    errx(EX_USAGE, "%s does not start with %s",
         ava_string_to_cstring(infile), ava_string_to_cstring(dir_prefix));

  len = ava_strlen(infile);
  if (len < 5 || !ava_string_equal(AVA_ASCII9_STRING(".avam"),
                                   ava_string_slice(infile, len - 5, len)))
    errx(EX_USAGE, "%s does not end with .avam",
         ava_string_to_cstring(infile));

  return ava_string_slice(infile, ava_strlen(dir_prefix), len - 5);
}
