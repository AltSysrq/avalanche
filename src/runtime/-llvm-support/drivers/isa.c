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

#include "../../avalanche.h"

/* Tell the code generator about the platform's C ABI */
struct ava_c_abi_info {
  char ch;
  short sh;
  int i;
  long l;
  long long ll;
  size_t size;
  long double ldouble;
  ava_bool ab;
  ava_function_parameter_type fpt;
  ava_function fun;
  ava_argument_spec argspec;
  ava_twine twine;
  ava_string str;
  ava_function_parameter parm;
};
void ava_c_abi_info_get$(struct ava_c_abi_info* dst) {
  /* Do nothing, this function is just here to force clang to write the
   * structure to the IR and for ir_types to find it.
   */
}

void ava_isa_g_var_init$(
  ava_value* address,
  ava_bool publish, const char* name
) {
  *address = ava_value_of_string(AVA_EMPTY_STRING);
}


void ava_isa_g_fun_init$(
  void (*address)(), ava_value prototype,
  ava_bool publish, const char* name
) {
}

ava_value ava_isa_x_ld_imm_vd$(ava_value src) {
  return src;
}

ava_integer ava_isa_x_ld_imm_i$(ava_integer src) {
  return src;
}

ava_value ava_isa_x_ld_glob$(const ava_value* src) {
  /* TODO: Check whether legal */
  return *src;
}

ava_value ava_isa_x_ld_reg_dvdv$(ava_value src) {
  return src;
}

ava_integer ava_isa_x_ld_reg_ii$(ava_integer src) {
  return src;
}

const ava_function* ava_isa_x_ld_reg_ff$(const ava_function* src) {
  return src;
}

ava_list_value ava_isa_x_ld_reg_ll$(ava_list_value src) {
  return src;
}

ava_integer ava_isa_x_ld_reg_ivd$(ava_value src) {
  return ava_integer_of_value(src, 0);
}

ava_value ava_isa_x_ld_reg_vdi$(ava_integer src) {
  return ava_value_of_integer(src);
}

const ava_function* ava_isa_x_ld_reg_fvd$(ava_value src) {
  return ava_function_of_value(src);
}

ava_value ava_isa_x_ld_reg_vdf$(const ava_function* src) {
  return ava_value_of_function(src);
}

ava_list_value ava_isa_x_ld_reg_lvd$(ava_value src) {
  return ava_list_value_of(src);
}

ava_value ava_isa_x_ld_reg_vdl$(ava_list_value src) {
  return src.v;
}

void ava_isa_x_ld_reg_pdv$(ava_function_parameter* dst,
                           ava_value src,
                           ava_bool spread) {
  dst->type = spread? ava_fpt_spread : ava_fpt_static;
  dst->value = src;
}

void ava_isa_x_set_glob$(ava_value* dst, ava_value src) {
  /* TODO: Check whether legal */
  *dst = src;
}

ava_list_value ava_isa_x_lempty$(void) {
  return ava_empty_list();
}

ava_list_value ava_isa_x_lappend$(ava_list_value src, ava_value val) {
  return ava_list_append(src, val);
}

ava_list_value ava_isa_x_lcat$(ava_list_value left, ava_list_value right) {
  return ava_list_concat(left, right);
}

ava_value ava_isa_x_lhead$(ava_list_value src) {
  AVA_STATIC_STRING(ava_isa_exception_empty_list_type, "empty-list");

  if (0 == ava_list_length(src))
    ava_throw_uex(&ava_error_exception, ava_isa_exception_empty_list_type,
                  ava_error_extract_element_from_empty_list());

  return ava_list_index(src, 0);
}

ava_list_value ava_isa_x_lbehead$(ava_list_value src) {
  AVA_STATIC_STRING(ava_isa_exception_empty_list_type, "empty-list");
  size_t length;

  length = ava_list_length(src);
  if (0 == length)
    ava_throw_uex(&ava_error_exception, ava_isa_exception_empty_list_type,
                  ava_error_extract_element_from_empty_list());

  return ava_list_slice(src, 1, length);
}

ava_list_value ava_isa_x_lflatten$(ava_list_value src) {
  return ava_list_proj_flatten(src);
}

ava_value ava_isa_x_lindex$(ava_list_value src, ava_integer ix,
                            ava_string ex_type, ava_string ex_message) {
  if (ix < 0 || ix >= ava_list_length(src))
    ava_throw_uex(&ava_error_exception, ex_type, ex_message);

  return ava_list_index(src, ix);
}

ava_integer ava_isa_x_llength$(ava_list_value src) {
  return ava_list_length(src);
}

ava_integer ava_isa_x_iadd_imm$(ava_integer a, ava_integer b) {
  return a + b;
}

ava_integer ava_isa_x_icmp$(ava_integer left, ava_integer right) {
  return (left > right) - (left < right);
}

void ava_isa_x_aaempty$(ava_value val) {
  /* TODO */
}

/* TODO, everything else */
