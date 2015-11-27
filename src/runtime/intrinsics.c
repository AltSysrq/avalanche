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
#include "avalanche/macsub.h"
#include "avalanche/symbol.h"
#include "avalanche/symtab.h"
#include "-intrinsics/defun.h"
#include "-intrinsics/eh.h"
#include "-intrinsics/extern.h"
#include "-intrinsics/if.h"
#include "-intrinsics/loop.h"
#include "-intrinsics/namespace.h"
#include "-intrinsics/pasta.h"
#include "-intrinsics/require.h"
#include "-intrinsics/ret.h"
#include "-intrinsics/subscript.h"
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

static void ava_register_intrinsic(
  ava_symtab* symtab,
  const char* suffix, ava_symbol_type type, ava_uint precedence,
  const void* userdata, ava_macro_subst_f fun);

void ava_register_intrinsics(ava_macsub_context* context) {
  AVA_STATIC_STRING(intrinsic_prefix, AVA_AVAST_INTRINSIC ".");
  AVA_STATIC_STRING(avast_prefix, AVA_AVAST_PACKAGE ":");
  ava_string abs, amb;

  ava_symtab* symtab = ava_macsub_get_symtab(context);

  DEFINE("alias",       CTL,    PRIVATE,        alias);
  DEFINE("Alias",       CTL,    INTERNAL,       alias);
  DEFINE("ALIAS",       CTL,    PUBLIC,         alias);
  DEFINE("break",       CTL,    NULL,           break);
  DEFINE("continue",    CTL,    NULL,           continue);
  DEFINE("extern",      CTL,    PRIVATE,        extern);
  DEFINE("Extern",      CTL,    INTERNAL,       extern);
  DEFINE("EXTERN",      CTL,    PUBLIC,         extern);
  DEFINE("defer",       CTL,    NULL,           defer);
  DEFINE("fun",         CTL,    PRIVATE,        fun);
  DEFINE("Fun",         CTL,    INTERNAL,       fun);
  DEFINE("FUN",         CTL,    PUBLIC,         fun);
  DEFINE("macro",       CTL,    PRIVATE,        user_macro);
  DEFINE("Macro",       CTL,    INTERNAL,       user_macro);
  DEFINE("MACRO",       CTL,    PUBLIC,         user_macro);
  DEFINE("goto",        CTL,    NULL,           goto);
  DEFINE("if",          CTL,    NULL,           if);
  DEFINE("each",        CTL,    "each",         loop);
  DEFINE("for",         CTL,    "for",          loop);
  DEFINE("while",       CTL,    "while",        loop);
  DEFINE("until",       CTL,    "until",        loop);
  DEFINE("do",          CTL,    "do",           loop);
  DEFINE("import",      CTL,    NULL,           import);
  DEFINE("namespace",   CTL,    NULL,           namespace);
  DEFINE("pasta",       CTL,    NULL,           pasta);
  DEFINE("reqmod",      CTL,    NULL,           reqmod);
  DEFINE("reqpkg",      CTL,    NULL,           reqpkg);
  DEFINE("ret",         CTL,    NULL,           ret);
  DEFINE("#throw#",     CTL,    NULL,           throw);
  DEFINE("try",         CTL,    NULL,           try);
  DEFINE("#set#",       CTL,    NULL,           set);
  DEFINE("#var#",       CTL,    NULL,           var);
  DEFINE("#name-subscript#", CTL, "#name-subscript#", subscript);
  DEFINE("#numeric-subscript#", CTL, "#numeric-subscript#", subscript);
  DEFINE("#string-subscript#", CTL, "#string-subscript#", subscript);
  /* Weak absolute imports of the intrinsics and standard library */
  ava_macsub_import(&abs, &amb, context,
                    intrinsic_prefix, AVA_EMPTY_STRING,
                    ava_true, ava_false);
  ava_macsub_import(&abs, &amb, context,
                    avast_prefix, AVA_EMPTY_STRING,
                    ava_true, ava_false);
  /* Strong absolute import of the user's namespace */
  ava_macsub_import(&abs, &amb, context,
                    ava_macsub_apply_prefix(context, AVA_EMPTY_STRING),
                    AVA_EMPTY_STRING,
                    ava_true, ava_true);
}

static void ava_register_intrinsic(
  ava_symtab* symtab,
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

  if (ava_symtab_put(symtab, symbol)) abort();
}
