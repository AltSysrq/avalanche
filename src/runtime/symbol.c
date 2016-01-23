/*-
 * Copyright (c) 2015, 2016, Jason Lingle
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
#include "avalanche/string.h"
#include "avalanche/symbol.h"

ava_string ava_symbol_type_name(const ava_symbol* sym) {
  AVA_STATIC_STRING(global_variable_str, "global variable");
  AVA_STATIC_STRING(global_function_str, "global function");
  AVA_STATIC_STRING(local_variable_str, "local variable");
  AVA_STATIC_STRING(local_function_str, "local function");
  const ava_string struct_str = AVA_ASCII9_STRING("struct");
  AVA_STATIC_STRING(expander_macro_str, "expander macro");
  AVA_STATIC_STRING(control_macro_str, "control macro");
  AVA_STATIC_STRING(operator_macro_str, "operator macro");
  AVA_STATIC_STRING(function_macro_str, "function macro");

  switch (sym->type) {
  case ava_st_global_variable: return global_variable_str;
  case ava_st_global_function: return global_function_str;
  case ava_st_local_variable: return local_variable_str;
  case ava_st_local_function: return local_function_str;
  case ava_st_struct: return struct_str;
  case ava_st_expander_macro: return expander_macro_str;
  case ava_st_control_macro: return control_macro_str;
  case ava_st_operator_macro: return operator_macro_str;
  case ava_st_function_macro: return function_macro_str;
  case ava_st_other:
    return ava_string_of_cstring(sym->v.other.type->name);
  }

  /* unreachable */
  abort();
}
