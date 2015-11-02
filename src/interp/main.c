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
#include "../runtime/-llvm-support/drivers.h"
#include "../bsd.h"

static ava_value run(void* filename) {
  ava_compenv* compenv;
  ava_compile_error_list errors;
  ava_string jit_error;
  ava_pcode_global_list* pcode;
  ava_xcode_global_list* xcode;
  ava_jit_context* jit;
  ava_value ret = ava_empty_list().v;

  TAILQ_INIT(&errors);

  compenv = ava_compenv_new(AVA_ASCII9_STRING("input:"));
  ava_compenv_use_simple_source_reader(compenv, AVA_EMPTY_STRING);
  ava_compenv_use_minimal_macsub(compenv);
  ava_compenv_compile_file(&pcode, &xcode, compenv,
                           ava_string_of_cstring(filename),
                           &errors, NULL);
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

  jit = ava_jit_context_new();
  ava_jit_add_driver(
    jit, ava_driver_isa_unchecked_data,
    ava_driver_isa_unchecked_size);
  jit_error = ava_jit_run_module(
    jit, xcode, ava_string_of_cstring(filename),
    ava_string_of_cstring(filename),
    ava_string_of_cstring("input:"));

  if (ava_string_is_present(jit_error)) {
    warnx("JIT failed: %s", ava_string_to_cstring(jit_error));
    return ret;
  }

  ava_jit_context_delete(jit);

  return ret;
}

int main(int argc, char** argv) {
  ava_invoke_in_context(run, argv[1]);
  return 0;
}
