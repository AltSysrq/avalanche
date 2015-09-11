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
  ava_parse_unit parse_root;
  ava_macsub_context* macsub_context;
  ava_ast_node* root_node;
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

  if (!ava_parse(&parse_root, &errors, source,
                 ava_string_of_cstring(filename))) {
    warnx("Parse failed.\n%s", ava_string_to_cstring(
            ava_error_list_to_string(&errors, 50, ava_true)));
    return ret;
  }

  macsub_context = ava_macsub_context_new(
    ava_symtab_new(NULL), &errors,
    AVA_ASCII9_STRING("input:"));
  ava_register_intrinsics(macsub_context);
  root_node = ava_macsub_run(macsub_context, &parse_root.location,
                             &parse_root.v.statements,
                             ava_isrp_void);
  ava_ast_node_postprocess(root_node);
  if (!TAILQ_EMPTY(&errors)) {
    warnx("Macro substitution failed.\n%s", ava_string_to_cstring(
            ava_error_list_to_string(&errors, 50, ava_true)));
    return ret;
  }

  fprintf(stderr, "--- Macro substitution result ---\n%s\n\n",
          ava_string_to_cstring(ava_ast_node_to_string(root_node)));

  pcode = ava_codegen_run(root_node, &errors);
  if (!TAILQ_EMPTY(&errors)) {
    warnx("P-Code generation failed.\n%s", ava_string_to_cstring(
            ava_error_list_to_string(&errors, 50, ava_true)));
    return ret;
  }

  fprintf(stderr, "--- Generated P-Code ---\n%s\n\n",
          ava_string_to_cstring(
            ava_pcode_global_list_to_string(pcode, 0)));

  fprintf(stderr, "--- Program output ---\n");
  ava_interp_exec(pcode);

  return ret;
}

int main(int argc, char** argv) {
  ava_invoke_in_context(run, argv[1]);
  return 0;
}
