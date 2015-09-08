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
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/symbol-table.h"
#include "avalanche/macsub.h"
#include "avalanche/symbol.h"
#include "-intrinsics/extern.h"
#include "-intrinsics/namespace.h"
#include "-intrinsics/user-macro.h"
#include "-intrinsics/variable.h"

static const ava_visibility
  ava_visibility_private = ava_v_private,
  ava_visibility_internal = ava_v_internal,
  ava_visibility_public = ava_v_public;

#define PRIVATE &ava_visibility_private
#define INTERNAL &ava_visibility_internal
#define PUBLIC &ava_visibility_public
#define CTL ava_st_control_macro, 0
#define FUN ava_st_function_macro, 0
#define OP(p) ava_st_operator_macro, p
#define DEFINE(name, type_and_prec, ud, base)                           \
  ava_register_intrinsic(symtab, AVA_AVAST_INTRINSIC "." name,          \
                         type_and_prec, ud,                             \
                         ava_intr_##base##_subst)
#define SUCCEED(expected, actual) do {          \
    if ((expected) != (actual)) abort();        \
  } while (0)

static void ava_register_intrinsic(
  ava_symbol_table* symtab,
  const char* suffix, ava_symbol_type type, ava_uint precedence,
  const void* userdata, ava_macro_subst_f fun);

void ava_register_intrinsics(ava_macsub_context* context) {
  AVA_STATIC_STRING(intrinsic_prefix, AVA_AVAST_INTRINSIC ".");
  AVA_STATIC_STRING(avast_prefix, AVA_AVAST_PACKAGE ":");

  ava_symbol_table* symtab = ava_macsub_get_current_symbol_table(context);
  ava_symbol_table_import_status status;

  DEFINE("extern",      CTL,    PRIVATE,        extern);
  DEFINE("Extern",      CTL,    INTERNAL,       extern);
  DEFINE("EXTERN",      CTL,    PUBLIC,         extern);
  DEFINE("macro",       CTL,    PRIVATE,        user_macro);
  DEFINE("Macro",       CTL,    INTERNAL,       user_macro);
  DEFINE("MACRO",       CTL,    PUBLIC,         user_macro);
  DEFINE("import",      CTL,    NULL,           import);
  DEFINE("namespace",   CTL,    NULL,           namespace);
  DEFINE("alias",       CTL,    PRIVATE,        alias);
  DEFINE("Alias",       CTL,    INTERNAL,       alias);
  DEFINE("ALIAS",       CTL,    PUBLIC,         alias);
  DEFINE("#var#",       CTL,    NULL,           var);
  DEFINE("#set#",       CTL,    NULL,           set);
  SUCCEED(ava_stis_ok, ava_symbol_table_import(
            symtab, intrinsic_prefix, AVA_EMPTY_STRING, ava_false, ava_false));
  SUCCEED(ava_stis_ok, ava_symbol_table_import(
            symtab, avast_prefix, AVA_EMPTY_STRING, ava_false, ava_true));
  status = ava_symbol_table_import(
    symtab, ava_macsub_apply_prefix(context, AVA_EMPTY_STRING),
    AVA_EMPTY_STRING, ava_true, ava_true);
  if (status != ava_stis_ok && status != ava_stis_no_symbols_imported)
    abort();
}

static void ava_register_intrinsic(
  ava_symbol_table* symtab,
  const char* name, ava_symbol_type type, ava_uint precedence,
  const void* userdata, ava_macro_subst_f fun
) {
  ava_symbol* symbol;

  symbol = AVA_NEW(ava_symbol);
  symbol->type = type;
  symbol->level = 0;
  symbol->visibility = ava_v_public;
  symbol->full_name = ava_string_of_cstring(name);
  symbol->v.macro.precedence = precedence;
  symbol->v.macro.macro_subst = fun;
  symbol->v.macro.userdata = userdata;

  SUCCEED(ava_stps_ok, ava_symbol_table_put(
            symtab, symbol->full_name, symbol));
}
