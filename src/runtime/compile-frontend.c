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

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/map.h"
#include "avalanche/parser.h"
#include "avalanche/macsub.h"
#include "avalanche/symtab.h"
#include "avalanche/intrinsics.h"
#include "avalanche/pcode.h"
#include "avalanche/pcode-validation.h"
#include "avalanche/code-gen.h"
#include "avalanche/errors.h"
#include "avalanche/compile-frontend.h"

ava_bool ava_compile_file(
  ava_pcode_global_list** dst_pcode,
  ava_xcode_global_list** dst_xcode,
  ava_compile_error_list* errors,
  ava_string package,
  ava_string filename, ava_string source
) {
  ava_parse_unit parse_root;
  ava_macsub_context* macsub_context;
  ava_ast_node* root_node;
  ava_pcode_global_list* pcode;
  ava_xcode_global_list* xcode;

  if (dst_pcode) *dst_pcode = NULL;
  if (dst_xcode) *dst_xcode = NULL;

  if (!ava_parse(&parse_root, errors, source, filename))
    return ava_false;

  macsub_context = ava_macsub_context_new(
    ava_symtab_new(NULL), errors, package);
  ava_register_intrinsics(macsub_context);
  root_node = ava_macsub_run(macsub_context, &parse_root.location,
                             &parse_root.v.statements,
                             ava_isrp_void);
  ava_ast_node_postprocess(root_node);
  if (!TAILQ_EMPTY(errors)) return ava_false;

  pcode = ava_codegen_run(root_node, errors);
  if (dst_pcode) *dst_pcode = pcode;
  if (!TAILQ_EMPTY(errors)) return ava_false;

  xcode = ava_xcode_from_pcode(
    pcode, errors,
    ava_map_add(ava_empty_map(),
                ava_value_of_string(filename),
                ava_value_of_string(source)));
  if (dst_xcode) *dst_xcode = xcode;
  if (!TAILQ_EMPTY(errors)) return ava_false;

  return ava_true;
}
