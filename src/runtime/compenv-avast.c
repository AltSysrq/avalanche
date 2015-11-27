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

#include <string.h>
#include <stdlib.h>

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/string.h"
#include "avalanche/pcode.h"
#include "avalanche/macsub.h"
#include "avalanche/compenv.h"

extern const char*const ava_org_ava_lang_avast_avapi_data;
extern const size_t ava_org_ava_lang_avast_avapi_size;

AVA_STATIC_STRING(ava_compenv_avast_name, "org.ava-lang.avast");

void ava_compenv_use_standard_macsub(ava_compenv* env) {
  env->new_macsub = ava_compenv_standard_new_macsub;
  env->implicit_packages = ava_list_append(
    env->implicit_packages, ava_value_of_string(
      ava_compenv_avast_name));
}

ava_macsub_context* ava_compenv_standard_new_macsub(
  ava_compenv* compenv,
  struct ava_compile_error_list_s* errors
) {
  static AO_t avast_pcode;

  ava_macsub_context* context;
  ava_compile_location location;
  const ava_pcode_global_list* cached;

  location.filename = AVA_ASCII9_STRING("<none>");
  location.source = AVA_ABSENT_STRING;
  location.line_offset = 0;
  location.start_line = 1;
  location.end_line = 1;
  location.start_column = 1;
  location.end_column = 1;

  context = ava_compenv_minimal_new_macsub(compenv, errors);

  cached = (const ava_pcode_global_list*)AO_load_acquire_read(&avast_pcode);
  if (!cached) {
    cached = ava_pcode_global_list_of_string(
      ava_string_of_bytes(ava_org_ava_lang_avast_avapi_data,
                          ava_org_ava_lang_avast_avapi_size));
    AO_store_release_write(&avast_pcode, (AO_t)cached);
  }
  ava_macsub_insert_module(
    context, cached,
    ava_compenv_avast_name, &location, ava_true);
  return context;
}
