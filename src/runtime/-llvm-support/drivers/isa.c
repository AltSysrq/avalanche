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

/**
 * @file
 *
 * Provides all of the standard ISA drivers, via various preprocessor
 * definitions which toggle certain paths on or off.
 *
 * Note that the ISA driver has a fragile and rather intimate relationship with
 * the LLVM IR code generator. The generator uses the ISA to determine
 * properties about the target ABI and makes many assumptions about the types
 * of many functions within the ISA. An incorrect ISA may cause the runtime to
 * abort, segfault, or otherwise crash gracelessly.
 */

#include <stdlib.h>

#define AVA__INTERNAL_INCLUDE 1
#include "../../avalanche/defs.h"
#include "../../avalanche/alloc.h"
#include "../../avalanche/string.h"
#include "../../avalanche/value.h"
#include "../../avalanche/integer.h"
#include "../../avalanche/real.h"
#include "../../avalanche/list.h"
#include "../../avalanche/function.h"
#include "../../avalanche/exception.h"
#include "../../avalanche/list-proj.h"
#include "../../avalanche/errors.h"

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
  ava_fat_list_value fat_list;
};
void ava_c_abi_info_get$(struct ava_c_abi_info* dst) {
  /* Do nothing, this function is just here to force clang to write the
   * structure to the IR and for ir_types to find it.
   */
}

void ava_isa_g_ext_var$(const ava_value* var, ava_string name) {
}

void ava_isa_g_ext_fun_ava$(const ava_function* fun, ava_string name) {
}

void ava_isa_g_ext_fun_other$(ava_function* fun, ava_string name) {
  /* The FFI always produces the same end result, so as long as we initialise a
   * private copy, we can just copy that over the destination afterwards
   * without fear of interfering with other threads, should fun already have
   * had an initialised FFI.
   */
  ava_function copy = *fun;
  ava_function_init_ffi(&copy);
  memcpy(fun->ffi, copy.ffi, sizeof(fun->ffi));
}

void ava_isa_g_var$(ava_value* var, ava_string name, ava_bool publish) {
}

void ava_isa_g_fun_ava$(const ava_function* fun, ava_string name,
                        ava_bool publish) {
}

ava_value ava_isa_x_load_v$(const ava_value* var, ava_string name) {
  return *var;
}

ava_value ava_isa_x_load_d$(const ava_value* data, size_t ix) {
  return *data;
}

ava_integer ava_isa_x_load_i$(const ava_integer* i, size_t ix) {
  return *i;
}

const ava_function* ava_isa_x_load_f$(const ava_function*const* f, size_t ix) {
  return *f;
}

void ava_isa_x_load_l$(ava_fat_list_value*restrict dst,
                       const ava_fat_list_value*restrict src,
                       size_t ix) {
  *dst = *src;
}

ava_value ava_isa_x_load_glob_var$(
  const ava_value* var, ava_string name
) {
  return *var;
}

ava_value ava_isa_x_load_glob_fun$(
  const ava_function* fun, ava_string name
) {
  return ava_value_of_function(fun);
}

void ava_isa_x_store_v$(ava_value* dst, ava_value src, ava_string name) {
  *dst = src;
}

void ava_isa_x_store_d$(ava_value* dst, ava_value src, size_t ix) {
  *dst = src;
}

void ava_isa_x_store_i$(ava_integer* dst, ava_integer src, size_t ix) {
  *dst = src;
}

void ava_isa_x_store_f$(const ava_function** dst, const ava_function* src,
                        size_t ix) {
  *dst = src;
}

void ava_isa_x_store_l$(ava_fat_list_value* dst,
                        const ava_fat_list_value* src,
                        size_t ix) {
  *dst = *src;
}

void ava_isa_x_store_p$(
  ava_function_parameter* dst,
  ava_value val,
  ava_function_parameter_type type,
  size_t ix
) {
  dst->value = val;
  dst->type = type;
}

void ava_isa_x_store_glob_var$(ava_value* dst, ava_value src,
                               ava_string name) {
  *dst = src;
}

ava_value ava_isa_x_conv_vi$(ava_integer i) {
  return ava_value_of_integer(i);
}

ava_integer ava_isa_x_conv_iv$(ava_value val) {
  return ava_integer_of_value(val, 0);
}

ava_value ava_isa_x_conv_vf$(const ava_function* fun) {
  return ava_value_of_function(fun);
}

const ava_function* ava_isa_x_conv_fv$(ava_value val) {
  return ava_function_of_value(val);
}

ava_value ava_isa_x_conv_vl$(const ava_fat_list_value* l) {
  return l->c.v;
}

void ava_isa_x_conv_lv$(ava_fat_list_value* dst, ava_value src) {
  *dst = ava_fat_list_value_of(src);
}

void ava_isa_x_lempty$(ava_fat_list_value* dst) {
  *dst = ava_fat_list_value_of(ava_empty_list().v);
}

void ava_isa_x_lappend$(ava_fat_list_value* dst,
                        const ava_fat_list_value* src,
                        ava_value val) {
  *dst = ava_fat_list_value_of(
    (*src->v->append)(src->c, val).v);
}

void ava_isa_x_lcat$(ava_fat_list_value* dst,
                     const ava_fat_list_value* left,
                     const ava_fat_list_value* right) {
  *dst = ava_fat_list_value_of(
    (*left->v->concat)(left->c, right->c).v);
}


ava_value ava_isa_x_lhead$(const ava_fat_list_value* src) {
  AVA_STATIC_STRING(ava_isa_exception_empty_list_type, "empty-list");

  if (0 == (*src->v->length)(src->c))
    ava_throw_uex(&ava_error_exception, ava_isa_exception_empty_list_type,
                  ava_error_extract_element_from_empty_list());

  return (*src->v->index)(src->c, 0);
}

void ava_isa_x_lbehead$(ava_fat_list_value* dst,
                        const ava_fat_list_value* src) {
  AVA_STATIC_STRING(ava_isa_exception_empty_list_type, "empty-list");
  size_t length;

  length = (*src->v->length)(src->c);
  if (0 == length)
    ava_throw_uex(&ava_error_exception, ava_isa_exception_empty_list_type,
                  ava_error_extract_element_from_empty_list());

  *dst = ava_fat_list_value_of((*src->v->slice)(src->c, 1, length).v);
}

void ava_isa_x_lflatten$(ava_fat_list_value* dst,
                         const ava_fat_list_value* src) {
  *dst = ava_fat_list_value_of(ava_list_proj_flatten(src->c).v);
}

ava_value ava_isa_x_lindex$(const ava_fat_list_value* src,
                            ava_integer ix,
                            ava_string ex_type, ava_string ex_message) {
  if (ix < 0 || ix >= (*src->v->length)(src->c))
    ava_throw_uex(&ava_error_exception, ex_type, ex_message);

  return (*src->v->index)(src->c, ix);
}

ava_integer ava_isa_x_llength$(const ava_fat_list_value* src) {
  return (*src->v->length)(src->c);
}

ava_integer ava_isa_x_iadd$(ava_integer a, ava_integer b) {
  return a + b;
}

ava_integer ava_isa_x_icmp$(ava_integer a, ava_integer b) {
  return (a > b) - (a < b);
}

void ava_isa_x_aaempty$(ava_value val) {
  /* TODO */
}

void ava_isa_x_pre_invoke_s$(const ava_function* f, ava_string name) { }
void ava_isa_x_post_invoke_s$(const ava_function* f, ava_string name,
                              ava_value returned) { }

void ava_isa_x_invoke_sd_bind$(
  ava_value*restrict args,
  const ava_function*restrict fun,
  const ava_function_parameter*restrict parms,
  size_t num_parms
) {
  ava_function_force_bind(args, fun, num_parms, parms);
}

ava_value ava_isa_x_invoke_dd$(
  const ava_function*restrict fun,
  const ava_function_parameter*restrict parms,
  size_t num_parms
) {
  return ava_function_bind_invoke(fun, num_parms, parms);
}

const ava_function* ava_isa_x_partial$(
  const ava_function*restrict fun,
  const ava_value*restrict args,
  size_t count
) {
  ava_function*restrict ret;
  ava_argument_spec*restrict argspecs;

  ret = AVA_CLONE(*fun);
  ret->args = argspecs =
    ava_clone(fun->args, sizeof(ava_argument_spec) * fun->num_args);

  ava_function_partial(argspecs, fun->num_args, count, args);
  return ret;
}

ava_integer ava_isa_x_bool$(ava_integer i) {
  return !!i;
}

void ava_isa_m_to_void$(ava_value v) {
  if (!ava_string_is_empty(ava_to_string(v)))
    ava_throw_str(&ava_format_exception,
                  ava_error_non_empty_string_to_void_arg());
}

ava_value ava_isa_m_from_void$(void) {
  return ava_value_of_string(AVA_EMPTY_STRING);
}

#define MAR_INT(type,small_type,long_type)              \
  small_type ava_isa_m_to_##type##$(ava_value v) {      \
    return (small_type)ava_integer_of_value(v, 0);      \
  }                                                     \
  ava_value ava_isa_m_from_##type##$(small_type v) {    \
    return ava_value_of_integer((long_type)v);          \
  }

MAR_INT(byte,           signed char,            ava_slong)
MAR_INT(short,          signed short,           ava_slong)
MAR_INT(int,            signed int,             ava_slong)
MAR_INT(long,           signed long,            ava_slong)
MAR_INT(llong,          signed long long,       ava_slong)
MAR_INT(ubyte,          unsigned char,          ava_ulong)
MAR_INT(ushort,         unsigned short,         ava_ulong)
MAR_INT(uint,           unsigned int,           ava_ulong)
MAR_INT(ulong,          unsigned long,          ava_ulong)
MAR_INT(ullong,         unsigned long long,     ava_ulong)
MAR_INT(ava_sbyte,      ava_sbyte,              ava_slong)
MAR_INT(ava_sshort,     ava_sshort,             ava_slong)
MAR_INT(ava_sint,       ava_sint,               ava_slong)
MAR_INT(ava_slong,      ava_slong,              ava_slong)
MAR_INT(ava_ubyte,      ava_ubyte,              ava_ulong)
MAR_INT(ava_ushort,     ava_ushort,             ava_ulong)
MAR_INT(ava_uint,       ava_uint,               ava_ulong)
MAR_INT(ava_ulong,      ava_ulong,              ava_ulong)
MAR_INT(ava_integer,    ava_integer,            ava_integer)
MAR_INT(size,           size_t,                 ava_ulong)

#undef MAR_INT

#define MAR_FLT(type, ctype)                    \
  ctype ava_isa_m_to_##type##$(ava_value v) {   \
    return (ctype)ava_real_of_value(v, 0.0);    \
  }                                             \
  ava_value ava_isa_m_from_##type##$(ctype v) { \
    return ava_value_of_real((ava_real)v);      \
  }

MAR_FLT(float,          float)
MAR_FLT(double,         double)
MAR_FLT(ldouble,        long double)
MAR_FLT(ava_real,       ava_real)

#undef MAR_FLT

const char* ava_isa_m_to_string$(ava_value v) {
  return ava_string_to_cstring(ava_to_string(v));
}

ava_value ava_isa_m_from_string$(const char* v) {
  return ava_value_of_string(ava_string_of_cstring(v));
}

void* ava_isa_m_to_pointer$(ava_value v, const ava_pointer_prototype* proto) {
  if (proto->is_const) {
    return (void*)ava_pointer_get_const(v, proto->tag);
  } else {
    return ava_pointer_get_mutable(v, proto->tag);
  }
}

ava_value ava_isa_m_from_pointer$(
  const void* v, const ava_pointer_prototype* proto
) {
  return ava_pointer_of_proto(proto, v).v;
}
