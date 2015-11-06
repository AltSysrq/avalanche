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
#include "../runtime/avalanche/errors.h"
#include "../runtime/avalanche/pcode.h"
#include "../runtime/avalanche/compenv.h"
#include "../bsd.h"

/*

  Single Avalanche Module Compiler (to P-Code)

  Usage: compile-module filename

  filename must be a relative name with at least one leading directory and
  which ends with ".ava". The leading directory is used as the filename prefix
  for `reqmod` loading. A colon is added to the leading directory (minus
  trailing slash) to produce the package prefix.

  If the module compiles successfully, the P-Code is dumped to a file with the
  same name as the input, except with the extension ".avam".

 */

typedef struct {
  unsigned argc;
  const char*const* argv;
} main_data;

static ava_value main_impl(void* arg);
static const ava_pcode_global_list* compile(
  ava_string package_prefix, ava_string file_prefix, ava_string infile);
static void spit(ava_string outfile, const ava_pcode_global_list* pcode);

int main(int argc, const char*const* argv) {
  main_data md = { argc, argv };

  ava_init();
  ava_invoke_in_context(main_impl, &md);

  return 0;
}

static ava_value main_impl(void* arg) {
  unsigned argc = ((const main_data*)arg)->argc;
  const char*const* argv = ((const main_data*)arg)->argv;

  ava_string package_name, package_prefix, file_prefix, infile, outfile;
  const char* slash;
  size_t len;
  const ava_pcode_global_list* pcode;

  if (2 != argc)
    errx(EX_USAGE, "Usage: %s <filename>", argv[0]);

  slash = strchr(argv[1], '/');
  if (!slash || slash == argv[1])
    errx(EX_USAGE, "Bad input filename: %s", argv[1]);

  package_name = ava_string_of_bytes(argv[1], slash - argv[1]);
  package_prefix = ava_string_concat(package_name, AVA_ASCII9_STRING(":"));
  file_prefix = ava_string_concat(package_name, AVA_ASCII9_STRING("/"));
  infile = ava_string_of_cstring(slash + 1);

  len = ava_string_length(infile);
  if (len < 5 || !ava_string_equal(AVA_ASCII9_STRING(".ava"),
                                   ava_string_slice(infile, len - 4, len)))
    errx(EX_USAGE, "Bad input filename: %s", argv[1]);

  outfile = ava_string_concat(
    ava_string_slice(infile, 0, len - 4), AVA_ASCII9_STRING(".avam"));

  pcode = compile(package_prefix, file_prefix, infile);
  spit(ava_string_concat(file_prefix, outfile), pcode);

  return ava_value_of_string(AVA_EMPTY_STRING);
}

static const ava_pcode_global_list* compile(
  ava_string package_prefix, ava_string file_prefix, ava_string infile
) {
  ava_compenv* compenv;
  ava_compile_error_list errors;
  ava_pcode_global_list* pcode;

  TAILQ_INIT(&errors);

  compenv = ava_compenv_new(package_prefix);
  ava_compenv_use_simple_source_reader(compenv, file_prefix);
  ava_compenv_use_minimal_macsub(compenv);
  ava_compenv_compile_file(&pcode, NULL, compenv,
                           infile, &errors, NULL);

  if (!TAILQ_EMPTY(&errors))
    errx(EX_DATAERR, "Compilation failed.\n%s",
         ava_string_to_cstring(
           ava_error_list_to_string(&errors, 50, ava_false)));

  return pcode;
}

static void spit(ava_string outfile, const ava_pcode_global_list* pcode) {
  FILE* out;
  ava_string pcode_string;
  size_t len;

  pcode_string = ava_pcode_global_list_to_string(pcode, 0);
  len = ava_string_length(pcode_string);

  out = fopen(ava_string_to_cstring(outfile), "w");
  if (!out)
    err(EX_CANTCREAT, "Failed to open %s", ava_string_to_cstring(outfile));

  if (len != fwrite(ava_string_to_cstring(pcode_string), 1, len, out))
    err(EX_IOERR, "Error writing %s", ava_string_to_cstring(outfile));

  fclose(out);
}
