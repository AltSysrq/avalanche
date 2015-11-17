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
#include <stdio.h>
#include <stdarg.h>

#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/value.h"
#include "runtime/avalanche/exception.h"
#include "runtime/avalanche/integer.h"
#include "runtime/avalanche/real.h"
#include "runtime/avalanche/list.h"
#include "runtime/avalanche/function.h"

defsuite(function);

static const ava_function* of_cstring(const char* str) {
  ava_value val = ava_value_of_cstring(str);
  const ava_function* fun = ava_function_of_value(val);

  /* Test the list access / stringification while we're here */
  assert_values_equal(ava_value_of_function(fun), ava_list_value_of(val).v);

  return fun;
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
  const ava_function* fun = of_cstring("42 ava [pos [hello world]]");

  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_abt_pos_default, fun->args[0].binding.type);
  assert_value_equals_str("hello world", fun->args[0].binding.value);
}

deftest(parse_binding_implicit) {
  const ava_function* fun = of_cstring(
    "42 ava [implicit [hello world]] pos");

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
  const ava_function* fun = of_cstring("42 ava [named -message]");

  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_abt_named, fun->args[0].binding.type);
  assert_value_equals_str("-message", ava_value_of_string(
                            fun->args[0].binding.name));
}

deftest(parse_binding_named_default) {
  const ava_function* fun = of_cstring(
    "42 ava [named -message [hello world]]");

  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_abt_named_default, fun->args[0].binding.type);
  assert_value_equals_str("-message", ava_value_of_string(
                            fun->args[0].binding.name));
  assert_value_equals_str("hello world", fun->args[0].binding.value);
}

deftest(parse_binding_bool) {
  const ava_function* fun = of_cstring("42 ava [bool -foo]");

  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_abt_bool, fun->args[0].binding.type);
  assert_value_equals_str("-foo", ava_value_of_string(
                            fun->args[0].binding.name));
}

deftest(parse_multiple_bindings) {
  const ava_function* fun = of_cstring(
    "42 ava [implicit foo] pos [pos bar]");

  ck_assert_int_eq(3, fun->num_args);
  ck_assert_int_eq(ava_abt_implicit, fun->args[0].binding.type);
  assert_value_equals_str("foo", fun->args[0].binding.value);

  ck_assert_int_eq(ava_abt_pos, fun->args[1].binding.type);

  ck_assert_int_eq(ava_abt_pos_default, fun->args[2].binding.type);
  assert_value_equals_str("bar", fun->args[2].binding.value);
}

deftest(parse_minimal_c_function) {
  const ava_function* fun = of_cstring("42 c void [void pos]");

  ck_assert_int_eq(ava_cc_c, fun->calling_convention);
  ck_assert_int_eq(ava_cmpt_void, fun->c_return_type.primitive_type);
  ck_assert_int_eq(1, fun->num_args);
  ck_assert_int_eq(ava_cmpt_void, fun->args[0].marshal.primitive_type);
  ck_assert_int_eq(ava_abt_pos, fun->args[0].binding.type);
}

deftest(parse_c_multi_arg_with_compound_bindings) {
  const ava_function* fun = of_cstring(
    "42 c void [int implicit 5] [float named -pi 3.14]");

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
  const ava_function* fun = of_cstring("42 msstd void [void pos]");

  ck_assert_int_eq(ava_cc_msstd, fun->calling_convention);
}

deftest(accepts_this_cc) {
  const ava_function* fun = of_cstring("42 this void [void pos]");

  ck_assert_int_eq(ava_cc_this, fun->calling_convention);
}

deftest(understands_all_primitive_types) {
  unsigned i;
#define PRIM(x) "[" #x " pos] "
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
    "42 c FILE* [* pos] [& pos] [foo& pos]");

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
  assert_rejects("56 fortran int [int pos]");
}

deftest(rejects_unknown_marshal_types) {
  assert_rejects("42 c foo [int pos]");
  assert_rejects("42 c int [foo pos]");
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

deftest(rejects_null_function) {
  assert_rejects("\"\" ava pos");
  assert_rejects("0 ava pos");
  assert_rejects("null ava pos");
}

#define BIND_BOILERPLATE                        \
  ava_function_bind_status status;              \
  size_t variadic_collection[32];               \
  ava_function_bound_argument bound_args[32];   \
  ava_string message;                           \
  ava_function_parameter parms[]

#define BIND(str) ava_function_bind(                    \
    of_cstring(str), sizeof(parms) / sizeof(parms[0]),  \
    parms, bound_args, variadic_collection, &message)

deftest(simple_bind) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_static, .value = WORD(foo) }
  };
  status = BIND("42 ava pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
}

deftest(multi_pos_bind) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_static, .value = WORD(foo) },
    { .type = ava_fpt_static, .value = WORD(bar) },
  };
  status = BIND("42 ava pos pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(1, bound_args[1].v.parameter_index);
}

deftest(simple_pos_accepts_dynamic) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic }
  };
  status = BIND("42 ava pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
}

deftest(implicit_bind) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic }
  };
  status = BIND("42 ava \\{implicit foo\\} pos \\{implicit bar\\}");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[0].type);
  assert_value_equals_str("foo", bound_args[0].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(0, bound_args[1].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[2].type);
  assert_value_equals_str("bar", bound_args[2].v.value);
}

deftest(pos_default_bind_omitted) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{pos optional\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("optional", bound_args[1].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(1, bound_args[2].v.parameter_index);
}

deftest(pos_default_bind_given) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{pos optional\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(1, bound_args[1].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(2, bound_args[2].v.parameter_index);
}

deftest(pos_default_bind_two_absent) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{pos foo\\} \\{pos bar\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("foo", bound_args[1].v.value);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[2].type);
  assert_value_equals_str("bar", bound_args[2].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[3].type);
  ck_assert_int_eq(1, bound_args[3].v.parameter_index);
}

deftest(pos_default_bind_two_mixed) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{pos foo\\} \\{pos bar\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(1, bound_args[1].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[2].type);
  assert_value_equals_str("bar", bound_args[2].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[3].type);
  ck_assert_int_eq(2, bound_args[3].v.parameter_index);
}

deftest(pos_default_bind_two_present) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{pos foo\\} \\{pos bar\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(1, bound_args[1].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(2, bound_args[2].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[3].type);
  ck_assert_int_eq(3, bound_args[3].v.parameter_index);
}

deftest(pos_default_bind_begin_absent) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava \\{pos foo\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[0].type);
  assert_value_equals_str("foo", bound_args[0].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(0, bound_args[1].v.parameter_index);
}

deftest(pos_default_bind_begin_present) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava \\{pos foo\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(1, bound_args[1].v.parameter_index);
}

deftest(pos_default_bind_end_absent) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{pos foo\\}");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("foo", bound_args[1].v.value);
}

deftest(pos_default_bind_end_present) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{pos foo\\}");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(1, bound_args[1].v.parameter_index);
}

deftest(varargs_bind_empty) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos varargs pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("", bound_args[1].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(1, bound_args[2].v.parameter_index);
}

deftest(varargs_bind_one) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos varargs pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_collect, bound_args[1].type);
  ck_assert_int_eq(1, bound_args[1].v.collection_size);
  ck_assert_int_eq(1, variadic_collection[0]);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(2, bound_args[2].v.parameter_index);
}

deftest(varargs_bind_multiple) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos varargs pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_collect, bound_args[1].type);
  ck_assert_int_eq(3, bound_args[1].v.collection_size);
  ck_assert_int_eq(1, variadic_collection[0]);
  ck_assert_int_eq(2, variadic_collection[1]);
  ck_assert_int_eq(3, variadic_collection[2]);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(4, bound_args[2].v.parameter_index);
}

deftest(varargs_bind_begin_zero) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava varargs pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[0].type);
  assert_value_equals_str("", bound_args[0].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(0, bound_args[1].v.parameter_index);
}

deftest(varargs_bind_begin_one) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava varargs pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_collect, bound_args[0].type);
  ck_assert_int_eq(1, bound_args[0].v.collection_size);
  ck_assert_int_eq(0, variadic_collection[0]);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(1, bound_args[1].v.parameter_index);
}

deftest(varargs_bind_end_zero) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos varargs");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("", bound_args[1].v.value);
}

deftest(varargs_bind_end_one) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos varargs");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_collect, bound_args[1].type);
  ck_assert_int_eq(1, bound_args[1].v.collection_size);
  ck_assert_int_eq(1, variadic_collection[0]);
}

deftest(named_mandatory_bind_one) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(2, bound_args[1].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(3, bound_args[2].v.parameter_index);
}

deftest(named_mandatory_bind_two_in_order) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-bar) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo\\} \\{named -bar\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(2, bound_args[1].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(4, bound_args[2].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[3].type);
  ck_assert_int_eq(5, bound_args[3].v.parameter_index);
}

deftest(named_mandatory_bind_two_out_of_order) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-bar) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo\\} \\{named -bar\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(4, bound_args[1].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(2, bound_args[2].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[3].type);
  ck_assert_int_eq(5, bound_args[3].v.parameter_index);
}

deftest(named_mandatory_bind_begin) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava \\{named -foo\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(1, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(2, bound_args[1].v.parameter_index);
}

deftest(named_mandatory_bind_end) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo\\}");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(2, bound_args[1].v.parameter_index);
}

deftest(named_default_bind_one_present) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo bar\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(2, bound_args[1].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(3, bound_args[2].v.parameter_index);
}

deftest(named_default_bind_one_absent) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo bar\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("bar", bound_args[1].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(1, bound_args[2].v.parameter_index);
}

deftest(named_default_bind_two_in_order) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-bar) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo xyzzy\\} \\{named -bar plugh\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(2, bound_args[1].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(4, bound_args[2].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[3].type);
  ck_assert_int_eq(5, bound_args[3].v.parameter_index);
}

deftest(named_default_bind_two_out_of_order) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-bar) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo xyzzy\\} \\{named -bar plugh\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(4, bound_args[1].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(2, bound_args[2].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[3].type);
  ck_assert_int_eq(5, bound_args[3].v.parameter_index);
}

deftest(named_default_bind_first) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo xyzzy\\} \\{named -bar plugh\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(2, bound_args[1].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[2].type);
  assert_value_equals_str("plugh", bound_args[2].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[3].type);
  ck_assert_int_eq(3, bound_args[3].v.parameter_index);
}

deftest(named_default_bind_second) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-bar) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo xyzzy\\} \\{named -bar plugh\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("xyzzy", bound_args[1].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(2, bound_args[2].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[3].type);
  ck_assert_int_eq(3, bound_args[3].v.parameter_index);
}

deftest(named_default_bind_begin_present) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava \\{named -foo bar\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(1, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(2, bound_args[1].v.parameter_index);
}

deftest(named_default_bind_end_present) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo bar\\}");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(2, bound_args[1].v.parameter_index);
}

deftest(named_default_bind_begin_absent) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava \\{named -foo bar\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[0].type);
  assert_value_equals_str("bar", bound_args[0].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(0, bound_args[1].v.parameter_index);
}

deftest(named_default_bind_end_absent) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo bar\\}");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("bar", bound_args[1].v.value);
}

deftest(bool_bind_absent) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{bool -flag\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("false", bound_args[1].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(1, bound_args[2].v.parameter_index);
}

deftest(bool_bind_present) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-flag) },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{bool -flag\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("true", bound_args[1].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(2, bound_args[2].v.parameter_index);
}

deftest(bool_bind_begin_absent) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava \\{bool -flag\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[0].type);
  assert_value_equals_str("false", bound_args[0].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(0, bound_args[1].v.parameter_index);
}

deftest(bool_bind_begin_present) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_static, .value = WORD(-flag) },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava \\{bool -flag\\} pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[0].type);
  assert_value_equals_str("true", bound_args[0].v.value);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[1].type);
  ck_assert_int_eq(1, bound_args[1].v.parameter_index);
}

deftest(bool_bind_end_absent) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{bool -flag\\}");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("false", bound_args[1].v.value);
}

deftest(bool_bind_end_present) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_static, .value = WORD(-flag) },
  };
  status = BIND("42 ava pos \\{bool -flag\\}");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_implicit, bound_args[1].type);
  assert_value_equals_str("true", bound_args[1].v.value);
}

deftest(bind_impossible_if_insufficient_parms) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos pos");

  ck_assert_int_eq(ava_fbs_impossible, status);
}

deftest(bind_impossible_if_too_many_parms) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos pos");

  ck_assert_int_eq(ava_fbs_impossible, status);
}

deftest(bind_impossible_if_incorrect_named_arg) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava \\{named -bar\\}");

  ck_assert_int_eq(ava_fbs_impossible, status);
}

deftest(bind_impossible_if_named_arg_missing_value) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_static, .value = WORD(-foo) },
  };
  status = BIND("42 ava \\{named -foo\\}");

  ck_assert_int_eq(ava_fbs_impossible, status);
}

deftest(bind_impossible_if_named_arg_bound_more_than_once) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_static, .value = WORD(-bar) },
    { .type = ava_fpt_static, .value = WORD(-bar) },
  };
  status = BIND("42 ava \\{bool -foo\\} \\{bool -bar\\}");

  ck_assert_int_eq(ava_fbs_impossible, status);
}

deftest(bind_impossible_if_mandatory_named_arg_omitted) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos \\{named -foo\\}");

  ck_assert_int_eq(ava_fbs_impossible, status);
}

deftest(bind_unknown_if_named_arg_name_dynamic) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava \\{named -foo\\}");

  ck_assert_int_eq(ava_fbs_unknown, status);
}

deftest(bind_needs_unpack_if_spread_spans_pos) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_spread },
  };
  status = BIND("42 ava pos pos");

  ck_assert_int_eq(ava_fbs_unpack, status);
}

deftest(bind_needs_unpack_if_spread_spans_named) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_spread },
  };
  status = BIND("42 ava \\{named -foo\\}");

  ck_assert_int_eq(ava_fbs_unpack, status);
}

deftest(bind_needs_unpack_if_spread_starts_on_named_value) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_static, .value = WORD(-foo) },
    { .type = ava_fpt_spread },
  };
  status = BIND("42 ava \\{named -foo\\}");

  ck_assert_int_eq(ava_fbs_unpack, status);
}

deftest(bind_needs_unpack_if_spread_terminates_parms) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_spread },
  };
  status = BIND("42 ava pos");

  ck_assert_int_eq(ava_fbs_unpack, status);
}

deftest(bind_needs_unpack_if_spread_right_of_varshape) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_spread },
  };
  status = BIND("42 ava pos \\{pos foo\\} pos");

  ck_assert_int_eq(ava_fbs_unpack, status);
}

deftest(bind_doesnt_need_unpack_for_spreads_spanning_varargs_only) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_dynamic },
    { .type = ava_fpt_spread },
    { .type = ava_fpt_spread },
    { .type = ava_fpt_dynamic },
  };
  status = BIND("42 ava pos varargs pos");

  ck_assert_int_eq(ava_fbs_bound, status);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[0].type);
  ck_assert_int_eq(0, bound_args[0].v.parameter_index);
  ck_assert_int_eq(ava_fbat_collect, bound_args[1].type);
  ck_assert_int_eq(2, bound_args[1].v.collection_size);
  ck_assert_int_eq(1, variadic_collection[0]);
  ck_assert_int_eq(2, variadic_collection[1]);
  ck_assert_int_eq(ava_fbat_parameter, bound_args[2].type);
  ck_assert_int_eq(3, bound_args[2].v.parameter_index);
}

deftest(bind_is_unpack_if_spread_onto_single_arg) {
  BIND_BOILERPLATE = {
    { .type = ava_fpt_spread },
  };
  status = BIND("42 ava \\{pos foo\\}");

  ck_assert_int_eq(ava_fbs_unpack, status);
}

#define TEST_INVOKE(test_name, expected_result_str,     \
                    function_suffix,                    \
                    return_type, args_decls, impl, ...) \
  static return_type test_name##_f args_decls {         \
    return impl;                                        \
  }                                                     \
  deftest(test_name) {                                  \
    char funspec[256];                                  \
    ava_value result;                                   \
    ava_function_parameter parms[] = { __VA_ARGS__ };   \
    snprintf(funspec, sizeof(funspec), "%lld %s",       \
             (long long)(test_name##_f),                \
             function_suffix);                          \
    result = ava_function_bind_invoke(                  \
      of_cstring(funspec),                              \
      sizeof(parms) / sizeof(parms[0]),                 \
      parms);                                           \
    assert_value_equals_str(expected_result_str,        \
                            result);                    \
  }

#define STATWORD(word) { .type = ava_fpt_static, .value = WORD(word) }

TEST_INVOKE(invoke_ava_identity, "42", "ava pos",
            ava_value, (ava_value v), v,
            { .type = ava_fpt_static, .value = INT(42) })

static ava_value cat(size_t count, ...) {
  ava_string accum = AVA_EMPTY_STRING;
  size_t i;
  va_list args;

  va_start(args, count);
  for (i = 0; i < count; ++i)
    accum = ava_strcat(accum, ava_to_string(va_arg(args, ava_value)));

  va_end(args);

  return ava_value_of_string(accum);
}

TEST_INVOKE(invoke_ava_2_arg, "ab", "ava pos pos",
            ava_value, (ava_value a, ava_value b),
            cat(2, a, b), STATWORD(a), STATWORD(b))

TEST_INVOKE(invoke_ava_3_arg, "abc",
            "ava pos pos pos",
            ava_value, (ava_value a, ava_value b, ava_value c),
            cat(3, a, b, c),
            STATWORD(a), STATWORD(b), STATWORD(c))

TEST_INVOKE(invoke_ava_4_arg, "abcd",
            "ava pos pos pos pos",
            ava_value, (ava_value a, ava_value b, ava_value c, ava_value d),
            cat(4, a, b, c, d),
            STATWORD(a), STATWORD(b), STATWORD(c), STATWORD(d))

TEST_INVOKE(invoke_ava_5_arg, "abcde",
            "ava pos pos pos pos pos",
            ava_value, (ava_value a, ava_value b, ava_value c, ava_value d,
                        ava_value e),
            cat(5, a, b, c, d, e),
            STATWORD(a), STATWORD(b), STATWORD(c), STATWORD(d),
            STATWORD(e))

TEST_INVOKE(invoke_ava_6_arg, "abcdef",
            "ava pos pos pos pos pos pos",
            ava_value, (ava_value a, ava_value b, ava_value c, ava_value d,
                        ava_value e, ava_value f),
            cat(6, a, b, c, d, e, f),
            STATWORD(a), STATWORD(b), STATWORD(c), STATWORD(d),
            STATWORD(e), STATWORD(f))

TEST_INVOKE(invoke_ava_7_arg, "abcdefg",
            "ava pos pos pos pos pos pos pos",
            ava_value, (ava_value a, ava_value b, ava_value c, ava_value d,
                        ava_value e, ava_value f, ava_value g),
            cat(7, a, b, c, d, e, f, g),
            STATWORD(a), STATWORD(b), STATWORD(c), STATWORD(d),
            STATWORD(e), STATWORD(f), STATWORD(g))

TEST_INVOKE(invoke_ava_8_arg, "abcdefgh",
            "ava pos pos pos pos pos pos pos pos",
            ava_value, (ava_value a, ava_value b, ava_value c, ava_value d,
                        ava_value e, ava_value f, ava_value g, ava_value h),
            cat(8, a, b, c, d, e, f, g, h),
            STATWORD(a), STATWORD(b), STATWORD(c), STATWORD(d),
            STATWORD(e), STATWORD(f), STATWORD(g), STATWORD(h))

TEST_INVOKE(invoke_ava_9_arg, "abcdefghi",
            "ava pos pos pos pos pos pos pos pos pos",
            ava_value, (size_t n, ava_value*restrict a),
            cat(9, a[0], a[1], a[2], a[3],
                a[4], a[5], a[6], a[7], a[8]),
            STATWORD(a), STATWORD(b), STATWORD(c), STATWORD(d),
            STATWORD(e), STATWORD(f), STATWORD(g), STATWORD(h),
            STATWORD(i))

TEST_INVOKE(invoke_c_void, "",
            "c void \"void pos\"",
            void, (void), ,
            { .type = ava_fpt_static, .value = ava_value_of_string(
                AVA_EMPTY_STRING) })

#define TEST_INVOKE_SUM(name, ctype, result, aparm, bparm)      \
  TEST_INVOKE(invoke_c_sum_##name, #result,                     \
              "c " #name " \"" #name " pos\" \"" #name " pos\"",     \
              ctype, (ctype a, ctype b), a + b,                 \
              { .type = ava_fpt_static,                         \
                  .value = ava_value_of_cstring(#aparm) },      \
              { .type = ava_fpt_static,                         \
                  .value = ava_value_of_cstring(#bparm) })

TEST_INVOKE_SUM(byte, signed char, -3, -1, -2)
TEST_INVOKE_SUM(short, signed short, -3, -1, -2)
TEST_INVOKE_SUM(int, signed int, -3, -1, -2)
TEST_INVOKE_SUM(long, signed long, -3, -1, -2)
TEST_INVOKE_SUM(llong, signed long long, -3, -1, -2)
TEST_INVOKE_SUM(ubyte, unsigned char, 254, 1, 253)
TEST_INVOKE_SUM(ushort, unsigned short, 65501, 1, 65500)
TEST_INVOKE_SUM(uint, unsigned int, 3000000001, 1, 3000000000)
TEST_INVOKE_SUM(ulong, unsigned long, 3000000001, 1, 3000000000)
/* This becomes signed anyway since ava_integer is signed */
TEST_INVOKE_SUM(ullong, unsigned long long,
                -9223372036854775807, 1, 9223372036854775808)
TEST_INVOKE_SUM(ava_sbyte, ava_sbyte, -3, -1, -2)
TEST_INVOKE_SUM(ava_ubyte, ava_ubyte, 254, 1, 253)
TEST_INVOKE_SUM(ava_sshort, ava_sshort, -3, -1, -2)
TEST_INVOKE_SUM(ava_ushort, ava_ushort, 65501, 1, 65500)
TEST_INVOKE_SUM(ava_sint, ava_sint, -3, -1, -2)
TEST_INVOKE_SUM(ava_uint, ava_uint, 3000000001, 1, 3000000000)
TEST_INVOKE_SUM(ava_slong, ava_slong, -3, -1, -2)
TEST_INVOKE_SUM(ava_ulong, ava_ulong, -9223372036854775807, 1, 9223372036854775808)
TEST_INVOKE_SUM(ava_integer, ava_integer, -3, -1, -2)
TEST_INVOKE_SUM(size, size_t, 65536, 32768, 32768)
TEST_INVOKE_SUM(float, float, 2.5, 0.5, 2.0)
TEST_INVOKE_SUM(double, double, 2.5, 0.5, 2.0)
TEST_INVOKE_SUM(ldouble, long double, 2.5, 0.5, 2.0)
TEST_INVOKE_SUM(ava_real, ava_real, 2.5, 0.5, 2.0)

TEST_INVOKE(invoke_string, "foobar",
            "c string \"string pos\" \"string pos\"",
            const char*, (const char* a, const char* b),
            ava_string_to_cstring(
              ava_strcat(ava_string_of_cstring(a),
                                ava_string_of_cstring(b))),
            STATWORD(foo), STATWORD(bar))

TEST_INVOKE(invoke_mut_pointer, "foo* null",
            "c foo* \"bar* pos\"",
            void*, (void* a), (void*)(ava_intptr)(0xBEEF != (ava_intptr)a),
            { .type = ava_fpt_static,
                .value = ava_value_of_cstring("bar* xBEEF") })

TEST_INVOKE(invoke_const_pointer, "foo& null",
            "c foo& \"bar& pos\"",
            void*, (void* a), (void*)(ava_intptr)(0xBEEF != (ava_intptr)a),
            { .type = ava_fpt_static,
                .value = ava_value_of_cstring("bar& xBEEF") })

deftest(c_void_argument_required_to_be_empty) {
  ava_exception_handler h;
  char funspec[256];
  const ava_function* fun;
  ava_function_parameter parms[1] = {
    { .type = ava_fpt_static, .value = WORD(foo) }
  };

  snprintf(funspec, sizeof(funspec), "%lld c int \"void pos\"",
           (long long)rand);
  fun = of_cstring(funspec);

  ava_try (h) {
    ava_function_bind_invoke(fun, 1, parms);
    ck_abort_msg("no exception thrown");
  } ava_catch(h, ava_format_exception) {
    /* OK */
  } ava_catch_all {
    ava_rethrow(&h);
  }
}

TEST_INVOKE(invoke_with_binding_rearrangement, "impfoobarplughfalsequux",
            "ava \"implicit imp\" pos \"named -bar\" \"named -plugh\" "
            "\"bool -flag\" pos",
            ava_value, (ava_value a, ava_value b, ava_value c, ava_value d,
                        ava_value e, ava_value f),
            cat(6, a, b, c, d, e, f),
            STATWORD(foo), STATWORD(-plugh), STATWORD(plugh),
            STATWORD(-bar), STATWORD(bar), STATWORD(quux))

TEST_INVOKE(invoke_varargs, "a b c d e f g",
            "ava varargs",
            ava_value, (ava_value v), v,
            STATWORD(a), STATWORD(b),
            { .type = ava_fpt_spread, .value = WORD(c d) },
            STATWORD(e),
            { .type = ava_fpt_spread, .value = WORD(f g) },
            { .type = ava_fpt_spread, .value = ava_empty_list().v })

TEST_INVOKE(invoke_explosively, "abcd e fg",
            "ava pos \"named -b b\" \"named -c\" varargs pos",
            ava_value, (ava_value a, ava_value b, ava_value c, ava_value va,
                        ava_value e),
            cat(5, a, b, c, va, e),
            STATWORD(a),
            { .type = ava_fpt_spread,
                .value = ava_value_of_cstring("-c c -b b d e f g") })

deftest(list_miscellanea_work) {
  ava_value base = ava_value_of_function(of_cstring("42 ava pos"));

  assert_value_equals_str("ava", ava_list_slice(base, 1, 2));
  assert_value_equals_str("42 ava pos pos", ava_list_append(base, WORD(pos)));
  assert_value_equals_str("42 ava pos 42 ava pos",
                          ava_list_concat(base, base));
  assert_value_equals_str("42 pos", ava_list_remove(base, 1, 2));
  assert_value_equals_str("42 fortran pos",
                          ava_list_set(base, 1, WORD(fortran)));
}
