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
#include <stdio.h>
#include "../runtime/avalanche.h"
#include "../bsd.h"

static ava_value run(void* filename) {
  FILE* infile;
  ava_string source;
  char buff[4096];
  size_t nread;
  ava_compile_error_list errors;
  ava_pcode_global_list* pcode;
  ava_value ret = ava_empty_list().v;

  TAILQ_INIT(&errors);

  infile = fopen(filename, "r");
  if (!infile) err(EX_NOINPUT, "fopen");

  source = AVA_EMPTY_STRING;
  do {
    nread = fread(buff, 1, sizeof(buff), infile);
    source = ava_string_concat(
      source, ava_string_of_bytes(buff, nread));
  } while (nread == sizeof(buff));
  fclose(infile);

  fprintf(stderr, "--- Read source from %s ---\n%s\n\n",
          (const char*)filename, ava_string_to_cstring(source));

  ava_compile_file(&pcode, NULL, &errors,
                   AVA_ASCII9_STRING("input:"),
                   ava_string_of_cstring(filename),
                   source);
  if (pcode) {
    fprintf(stderr, "--- Generated P-Code ---\n%s\n\n",
            ava_string_to_cstring(
              ava_pcode_global_list_to_string(pcode, 0)));
  }

  if (!TAILQ_EMPTY(&errors)) {
    warnx("Compilation failed.\n%s",
          ava_string_to_cstring(
            ava_error_list_to_string(&errors, 50, ava_true)));
    return ret;
  }

  fprintf(stderr, "--- Program output ---\n");
  ava_interp_exec(pcode);

  return ret;
}

int main(int argc, char** argv) {
  ava_invoke_in_context(run, argv[1]);
  return 0;
}
