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
#include "test.c"

#include <stdlib.h>
#include <string.h>

#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/exception.h"
#include "runtime/avalanche/integer.h"
#include "runtime/avalanche/real.h"
#include "runtime/avalanche/function.h"

defsuite(function);

static const ava_function* of_cstring(const char* str) {
  return ava_function_of_value(ava_value_of_cstring(str));
}

static void assert_rejects(const char* str) {
  ava_exception_handler h;
  const ava_function* fun;

  ava_try (h) {
    fun = of_cstring(str);
    ck_abort_msg("Parsed %p from %s", fun, str);
  } ava_catch (h, ava_format_exception) {
    /* OK */
  } ava_catch_all {
    ava_rethrow(&h);
  }
}

deftest(simple_parse) {
  const ava_function* fun = of_cstring("42 ava pos");

  ck_assert_int_eq(42, (ava_intptr)fun->address);
  ck_assert_int_eq(ava_cc_ava, fun->calling_convention);
  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_abt_pos, fun->args[0].binding.type);
}

deftest(parse_binding_pos_default) {
  const ava_function* fun = of_cstring("42 ava \\{pos \"hello world\"\\}");

  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_abt_pos_default, fun->args[0].binding.type);
  assert_value_equals_str("hello world", fun->args[0].binding.value);
}

deftest(parse_binding_implicit) {
  const ava_function* fun = of_cstring(
    "42 ava \\{implicit \"hello world\"\\} pos");

  ck_assert_int_eq(2, fun->num_args);
  ck_assert_int_eq(ava_abt_implicit, fun->args[0].binding.type);
  assert_value_equals_str("hello world", fun->args[0].binding.value);
}

deftest(parse_binding_varargs) {
  const ava_function* fun = of_cstring("42 ava varargs");

  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_abt_varargs, fun->args[0].binding.type);
}

deftest(accepts_constshape_after_varargs) {
  const ava_function* fun = of_cstring("42 ava varargs pos");

  ck_assert_int_eq(2, fun->num_args);
  ck_assert_int_eq(ava_abt_varargs, fun->args[0].binding.type);
  ck_assert_int_eq(ava_abt_pos, fun->args[1].binding.type);
}

deftest(parse_binding_named) {
  const ava_function* fun = of_cstring("42 ava \\{named -message\\}");

  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_abt_named, fun->args[0].binding.type);
  assert_value_equals_str("-message", ava_value_of_string(
                            fun->args[0].binding.name));
}

deftest(parse_binding_named_default) {
  const ava_function* fun = of_cstring(
    "42 ava \\{named -message \"hello world\"\\}");

  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_abt_named_default, fun->args[0].binding.type);
  assert_value_equals_str("-message", ava_value_of_string(
                            fun->args[0].binding.name));
  assert_value_equals_str("hello world", fun->args[0].binding.value);
}

deftest(parse_binding_bool) {
  const ava_function* fun = of_cstring("42 ava \\{bool -foo\\}");

  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_abt_bool, fun->args[0].binding.type);
  assert_value_equals_str("-foo", ava_value_of_string(
                            fun->args[0].binding.name));
}

deftest(parse_multiple_bindings) {
  const ava_function* fun = of_cstring(
    "42 ava \\{implicit foo\\} pos \\{pos bar\\}");

  ck_assert_int_eq(3, fun->num_args);
  ck_assert_int_eq(ava_abt_implicit, fun->args[0].binding.type);
  assert_value_equals_str("foo", fun->args[0].binding.value);

  ck_assert_int_eq(ava_abt_pos, fun->args[1].binding.type);

  ck_assert_int_eq(ava_abt_pos_default, fun->args[2].binding.type);
  assert_value_equals_str("bar", fun->args[2].binding.value);
}

deftest(parse_minimal_c_function) {
  const ava_function* fun = of_cstring("42 c void \\{void pos\\}");

  ck_assert_int_eq(ava_cc_c, fun->calling_convention);
  ck_assert_int_eq(ava_cmpt_void, fun->c_return_type.primitive_type);
  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_cmpt_void, fun->args[0].marshal.primitive_type);
  ck_assert_int_eq(ava_abt_pos, fun->args[0].binding.type);
}

deftest(parse_c_multi_arg_with_compound_bindings) {
  const ava_function* fun = of_cstring(
    "42 c void \\{int implicit 5\\} \\{float named -pi 3.14\\}");

  ck_assert_int_eq(ava_cc_c, fun->calling_convention);
  ck_assert_int_eq(ava_cmpt_void, fun->c_return_type.primitive_type);
  ck_assert_int_eq(2, fun->num_args);
  ck_assert_int_eq(ava_cmpt_int, fun->args[0].marshal.primitive_type);
  ck_assert_int_eq(ava_abt_implicit, fun->args[0].binding.type);
  assert_value_equals_str("5", fun->args[0].binding.value);
  ck_assert_int_eq(ava_cmpt_float, fun->args[1].marshal.primitive_type);
  ck_assert_int_eq(ava_abt_named_default, fun->args[1].binding.type);
  assert_value_equals_str("-pi", ava_value_of_string(
                            fun->args[1].binding.name));
  assert_value_equals_str("3.14", fun->args[1].binding.value);
}

deftest(accpts_msstd_cc) {
  const ava_function* fun = of_cstring("42 msstd void \\{void pos\\}");

  ck_assert_int_eq(ava_cc_msstd, fun->calling_convention);
}

deftest(accepts_this_cc) {
  const ava_function* fun = of_cstring("42 this void \\{void pos\\}");

  ck_assert_int_eq(ava_cc_this, fun->calling_convention);
}

deftest(understands_all_primitive_types) {
  unsigned i;
#define PRIM(x) "\\{" #x " pos\\} "
  const ava_function* fun = of_cstring(
    "42 this void "
    PRIM(void)
    PRIM(byte)
    PRIM(short)
    PRIM(int)
    PRIM(long)
    PRIM(llong)
    PRIM(ubyte)
    PRIM(ushort)
    PRIM(uint)
    PRIM(ulong)
    PRIM(ullong)
    PRIM(ava_sbyte)
    PRIM(ava_sshort)
    PRIM(ava_sint)
    PRIM(ava_slong)
    PRIM(ava_ubyte)
    PRIM(ava_ushort)
    PRIM(ava_uint)
    PRIM(ava_ulong)
    PRIM(ava_integer)
    PRIM(size)
    PRIM(float)
    PRIM(double)
    PRIM(ldouble)
    PRIM(ava_real)
    PRIM(string));
#undef PRIM

  ck_assert_int_eq(26, fun->num_args);

  i = 0;
#define PRIM(t) ck_assert_int_eq(ava_cmpt_##t, fun->args[i++].marshal.primitive_type)
  PRIM(void);
  PRIM(byte);
  PRIM(short);
  PRIM(int);
  PRIM(long);
  PRIM(llong);
  PRIM(ubyte);
  PRIM(ushort);
  PRIM(uint);
  PRIM(ulong);
  PRIM(ullong);
  PRIM(ava_sbyte);
  PRIM(ava_sshort);
  PRIM(ava_sint);
  PRIM(ava_slong);
  PRIM(ava_ubyte);
  PRIM(ava_ushort);
  PRIM(ava_uint);
  PRIM(ava_ulong);
  PRIM(ava_integer);
  PRIM(size);
  PRIM(float);
  PRIM(double);
  PRIM(ldouble);
  PRIM(ava_real);
  PRIM(string);
#undef PRIM
}

deftest(understands_pointer_types) {
  const ava_function* fun = of_cstring(
    "42 c FILE* \\{* pos\\} \\{& pos\\} \\{foo& pos\\}");

  ck_assert_int_eq(ava_cmpt_pointer, fun->c_return_type.primitive_type);
  ck_assert(!fun->c_return_type.pointer_proto->is_const);
  ck_assert_str_eq("FILE", ava_string_to_cstring(
                     fun->c_return_type.pointer_proto->tag));

  ck_assert_int_eq(ava_cmpt_pointer, fun->args[0].marshal.primitive_type);
  ck_assert(!fun->args[0].marshal.pointer_proto->is_const);
  ck_assert(ava_string_is_empty(fun->args[0].marshal.pointer_proto->tag));

  ck_assert_int_eq(ava_cmpt_pointer, fun->args[1].marshal.primitive_type);
  ck_assert(fun->args[1].marshal.pointer_proto->is_const);
  ck_assert(ava_string_is_empty(fun->args[1].marshal.pointer_proto->tag));

  ck_assert_int_eq(ava_cmpt_pointer, fun->args[2].marshal.primitive_type);
  ck_assert(fun->args[2].marshal.pointer_proto->is_const);
  ck_assert_str_eq("foo", ava_string_to_cstring(
                     fun->args[2].marshal.pointer_proto->tag));
}

deftest(rejects_truncated_lists) {
  assert_rejects("");
  assert_rejects("42");
  assert_rejects("42 ava");
  assert_rejects("42 c void");
  assert_rejects("42 c pos");
}

deftest(rejects_unknown_cc) {
  assert_rejects("56 fortran int \\{int pos\\}");
}

deftest(rejects_unknown_marshal_types) {
  assert_rejects("42 c foo \\{int pos\\}");
  assert_rejects("42 c int \\{foo pos\\}");
}

deftest(rejects_invalid_argspecs) {
  assert_rejects("42 ava \"\"");
  assert_rejects("42 c void pos");
  assert_rejects("42 c void void");
  assert_rejects("42 ava blah");
}

deftest(rejects_argspecs_missing_parms) {
  assert_rejects("42 ava implicit pos");
  assert_rejects("42 ava named");
  assert_rejects("42 ava bool");
}

deftest(rejects_argspecs_with_extraneous_parms) {
  assert_rejects("42 ava \\{implicit a b\\}");
  assert_rejects("42 ava \\{pos a b\\}");
  assert_rejects("42 ava \\{named a b c\\}");
  assert_rejects("42 ava \\{bool a b\\}");
  assert_rejects("42 ava \\{varargs a\\}");
}

deftest(rejects_no_explicit_arguments) {
  assert_rejects("42 ava \\{implicit foo\\}");
}

deftest(rejects_duplicate_named_arguments) {
  assert_rejects("42 ava \\{named -a\\} \\{named -a\\}");
  assert_rejects("42 ava \\{named -a\\} \\{bool -a\\}");
  assert_rejects("42 ava \\{bool -a\\} \\{bool -a\\}");
  assert_rejects("42 ava \\{named -a foo\\} \\{named -b\\} \\{named -a\\}");
}

deftest(rejects_noncontiguous_varshape) {
  assert_rejects("42 ava \\{pos foo\\} pos \\{pos foo\\}");
  assert_rejects("42 ava \\{named -a\\} pos \\{named -b\\}");
}

deftest(rejects_varshape_after_varargs) {
  assert_rejects("42 ava varargs \\{named foo\\}");
  assert_rejects("42 ava varargs pos varargs");
}
