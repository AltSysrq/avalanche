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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/map.h"
#include "avalanche/parser.h"
#include "avalanche/pcode-validation.h"
#include "avalanche/errors.h"
#include "avalanche/parser.h"
#include "avalanche/macsub.h"
#include "avalanche/symtab.h"
#include "avalanche/intrinsics.h"
#include "avalanche/code-gen.h"
#include "avalanche/compenv.h"

ava_compenv* ava_compenv_new(ava_string package_prefix) {
  ava_compenv* env;

  env = AVA_NEW(ava_compenv);
  env->package_prefix = package_prefix;
  LIST_INIT(&env->package_cache);
  LIST_INIT(&env->module_cache);
  SLIST_INIT(&env->pending_modules);
  env->implicit_packages = ava_empty_list();

  return env;
}

ava_bool ava_compenv_compile_file(
  ava_pcode_global_list** dst_pcode,
  ava_xcode_global_list** dst_xcode,
  ava_compenv* env,
  ava_string filename,
  ava_compile_error_list* dst_errors,
  const ava_compile_location* base_location
) {
  ava_compenv_pending_module this_pending, * other_pending;
  ava_compile_location begining_of_file;
  ava_compile_error_list errors;
  ava_compile_error* error, * errtmp;
  ava_parse_unit parse_root;
  ava_macsub_context* macsub_context;
  ava_ast_node* root_node;
  ava_pcode_global_list* pcode;
  ava_xcode_global_list* xcode;
  ava_value sources;
  ava_string error_message;
  size_t i, len;

  if (!base_location) {
    begining_of_file.filename = filename;
    begining_of_file.source = AVA_ABSENT_STRING;
    begining_of_file.line_offset = 0;
    begining_of_file.start_line = 1;
    begining_of_file.end_line = 1;
    begining_of_file.start_column = 1;
    begining_of_file.end_column = 1;
    base_location = &begining_of_file;
  }

  TAILQ_INIT(&errors);
  if (dst_pcode) *dst_pcode = NULL;
  if (dst_xcode) *dst_xcode = NULL;

  /* Occurs check */
  SLIST_FOREACH(other_pending, &env->pending_modules, next) {
    if (ava_string_equal(filename, other_pending->module_name)) {
      ava_compile_error_add(
        dst_errors,
        ava_error_compile_cyclic_dependency(
          base_location, filename));
      return ava_false;
    }
  }

  this_pending.module_name = filename;
  SLIST_INSERT_HEAD(&env->pending_modules, &this_pending, next);

  if (!(*env->read_source)(&sources, &error_message, filename, env)) {
    ava_compile_error_add(
      dst_errors,
      ava_error_cannot_read_module_source(
        base_location, filename, error_message));
    goto error;
  }

  len = ava_list_length(sources);
  for (i = 0; i < len; i += 2) {
    if (!ava_parse(&parse_root, &errors,
                   ava_to_string(ava_list_index(sources, i + 1)),
                   ava_to_string(ava_list_index(sources, i + 0)),
                   0 == i))
      goto error;
  }

  macsub_context = (*env->new_macsub)(env, &errors);
  root_node = ava_macsub_run(macsub_context, &parse_root.location,
                             &parse_root.v.statements,
                             ava_isrp_void);
  ava_ast_node_postprocess(root_node);
  if (!TAILQ_EMPTY(&errors)) goto error;

  pcode = ava_codegen_run(root_node, env->implicit_packages, &errors);
  if (dst_pcode) *dst_pcode = pcode;
  if (!TAILQ_EMPTY(&errors)) goto error;

  xcode = ava_xcode_from_pcode(pcode, &errors, ava_map_value_of(sources));
  if (dst_xcode) *dst_xcode = xcode;
  if (!TAILQ_EMPTY(&errors)) goto error;

  SLIST_REMOVE_HEAD(&env->pending_modules, next);
  return ava_true;

  error:
  TAILQ_FOREACH_SAFE(error, &errors, next, errtmp)
    TAILQ_INSERT_TAIL(dst_errors, error, next);
  SLIST_REMOVE_HEAD(&env->pending_modules, next);
  return ava_false;
}

ava_bool ava_compenv_simple_read_source(
  ava_value* dst, ava_string* error, ava_string filename,
  ava_compenv* compenv
) {
  FILE* in;
  ava_string source;
  size_t nread;
  char buffer[4096];

  in = fopen(
    ava_string_to_cstring(
      ava_strcat(
        ava_string_of_datum(compenv->read_source_userdata),
        filename)), "r");
  if (!in) goto error;

  source = AVA_EMPTY_STRING;
  do {
    nread = fread(buffer, 1, sizeof(buffer), in);
    source = ava_strcat(source, ava_string_of_bytes(buffer, nread));
  } while (nread == sizeof(buffer));

  if (ferror(in)) goto error;

  fclose(in);

  *dst = ava_map_add(
    ava_empty_map(),
    ava_value_of_string(filename), ava_value_of_string(source)).v;
  return ava_true;

  error:
  *error = ava_string_of_cstring(strerror(errno));
  if (in) fclose(in);
  return ava_false;
}

void ava_compenv_use_simple_source_reader(
  ava_compenv* env, ava_string prefix
) {
  env->read_source = ava_compenv_simple_read_source;
  env->read_source_userdata = ava_string_to_datum(prefix);
}

ava_macsub_context* ava_compenv_minimal_new_macsub(
  ava_compenv* compenv,
  ava_compile_error_list* errors
) {
  ava_macsub_context* context;

  context = ava_macsub_context_new(
    ava_symtab_new(NULL), compenv, errors, compenv->package_prefix);
  ava_register_intrinsics(context);
  return context;
}

void ava_compenv_use_minimal_macsub(ava_compenv* env) {
  env->new_macsub = ava_compenv_minimal_new_macsub;
}
