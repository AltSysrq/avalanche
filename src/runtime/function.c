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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <ffi.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/integer.h"
#include "avalanche/real.h"
#include "avalanche/pointer.h"
#include "avalanche/exception.h"
#include "avalanche/list.h"
#include "avalanche/function.h"

static ava_value_trait ava_function_generic_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "function",
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = ava_list_string_chunk_iterator,
  .iterate_string_chunk = ava_list_iterate_string_chunk,
};

AVA_LIST_DEFIMPL(ava_function, &ava_function_generic_impl)

typedef union {
  /* For integer return values */
  ffi_arg returned_uint;
  ffi_sarg returned_sint;

  unsigned char         c_ubyte;
  signed   char         c_byte;
  unsigned short        c_ushort;
  signed   short        c_short;
  unsigned int          c_uint;
  signed   int          c_int;
  unsigned long         c_ulong;
  signed   long         c_long;
  unsigned long long    c_ullong;
  signed   long long    c_llong;

  ava_ubyte             a_ubyte;
  ava_sbyte             a_sbyte;
  ava_ushort            a_ushort;
  ava_sshort            a_sshort;
  ava_uint              a_uint;
  ava_sint              a_sint;
  ava_ulong             a_ulong;
  ava_slong             a_slong;
  ava_integer           a_integer;

  size_t                c_size;

  float                 c_float;
  double                c_double;
  long double           c_ldouble;
  ava_real              a_real;

  void*                 ptr;
} ava_ffi_arg;

static ffi_type* ava_function_convert_type_to_ffi(
  ava_c_marshalling_primitive_type type);
static ava_c_marshalling_type ava_function_parse_marshal_type(ava_value val);
static ava_argument_binding ava_function_parse_binding(
  ava_list_value argspec, size_t start_ix);
static ava_value ava_function_marshal_type_to_value(
  ava_c_marshalling_type type);
static ava_value ava_function_argspec_to_value(
  ava_argument_spec arg, ava_bool marshal);
static void ava_function_explode(
  size_t* num_parms, const ava_function_parameter** parms);

void ava_function_init_ffi(ava_function* fun) {
  AVA_STATIC_STRING(bad_typedef_message,
                    "ffi_prep_cif() returned FFI_BAD_TYPEDEF.");
  AVA_STATIC_STRING(bad_abi_message,
                    "ffi_prep_cif() returned FFI_BAD_ABI.");
  AVA_STATIC_STRING(unknown_error_message,
                    "ffi_prep_cif() returned unexpected error code.");

  ffi_abi abi;
  ffi_type* rtype, ** atypes;
  size_t i, n;

  switch (fun->calling_convention) {
  case ava_cc_ava:
    /* No need for FFI for avalanche functions */
    return;

  case ava_cc_msstd:
#if AVA_HAVE_FFI_STDCALL
    abi = FFI_STDCALL;
#else
    abi = FFI_DEFAULT_ABI;
#endif
    break;

  case ava_cc_this:
#if AVA_HAVE_FFI_THISCALL
    abi = FFI_THISCALL;
#else
    abi = FFI_DEFAULT_ABI;
#endif
    break;

  case ava_cc_c:
    abi = FFI_DEFAULT_ABI;
    break;

  default: abort();
  }

  atypes = ava_alloc_atomic(sizeof(ffi_type*) * fun->num_args);

  rtype = ava_function_convert_type_to_ffi(fun->c_return_type.primitive_type);
  for (i = n = 0; i < fun->num_args; ++i) {
    if (ava_cmpt_void != fun->args[i].marshal.primitive_type) {
      atypes[n++] = ava_function_convert_type_to_ffi(
        fun->args[i].marshal.primitive_type);
    }
  }

  switch (ffi_prep_cif((ffi_cif*)fun->ffi, abi, n, rtype, atypes)) {
  case FFI_OK:
    break;

  case FFI_BAD_TYPEDEF:
    ava_throw(&ava_internal_exception, ava_value_of_string(bad_typedef_message),
              NULL);

  case FFI_BAD_ABI:
    ava_throw(&ava_internal_exception, ava_value_of_string(bad_abi_message),
              NULL);

  default:
    ava_throw(&ava_internal_exception,
              ava_value_of_string(unknown_error_message), NULL);
  }
}

static ffi_type* ava_function_convert_type_to_ffi(
  ava_c_marshalling_primitive_type type
) {
  switch (type) {
  case ava_cmpt_void: return &ffi_type_void;

  case ava_cmpt_byte:
  case ava_cmpt_ava_sbyte: return &ffi_type_sint8;

  case ava_cmpt_ubyte:
  case ava_cmpt_ava_ubyte: return &ffi_type_uint8;

  case ava_cmpt_short:
#if 2 == SIZEOF_INT
  case ava_cmpt_int:
#endif
  case ava_cmpt_ava_sshort: return &ffi_type_sint16;

  case ava_cmpt_ushort:
#if 2 == SIZEOF_INT
  case ava_cmpt_uint:
#endif
  case ava_cmpt_ava_ushort: return &ffi_type_uint16;

#if 4 == SIZEOF_INT
  case ava_cmpt_int:
#endif
#if 4 == SIZEOF_LONG
  case ava_cmpt_long:
#endif
  case ava_cmpt_ava_sint: return &ffi_type_sint32;

#if 4 == SIZEOF_INT
  case ava_cmpt_uint:
#endif
#if 4 == SIZEOF_LONG
  case ava_cmpt_ulong:
#endif
#if 4 == SIZEOF_SIZE_T
  case ava_cmpt_size:
#endif
  case ava_cmpt_ava_uint: return &ffi_type_uint32;

#if 8 == SIZEOF_INT
  case ava_cmpt_int:
#endif
#if 8 == SIZEOF_LONG
  case ava_cmpt_long:
#endif
  case ava_cmpt_llong:
  case ava_cmpt_ava_integer:
  case ava_cmpt_ava_slong: return &ffi_type_sint64;

#if 8 == SIZEOF_INT
  case ava_cmpt_uint:
#endif
#if 8 == SIZEOF_LONG
  case ava_cmpt_ulong:
#endif
#if 8 == SIZEOF_SIZE_T
  case ava_cmpt_size:
#endif
  case ava_cmpt_ullong:
  case ava_cmpt_ava_ulong: return &ffi_type_uint64;

  case ava_cmpt_float: return &ffi_type_float;

  case ava_cmpt_double:
  case ava_cmpt_ava_real: return &ffi_type_double;

  case ava_cmpt_ldouble: return &ffi_type_longdouble;

  case ava_cmpt_string:
  case ava_cmpt_pointer: return &ffi_type_pointer;
  }

  /* unreachable */
  abort();
}

ava_value ava_value_of_function(const ava_function* fun) {
  return ava_value_with_ptr(&ava_function_list_impl, fun);
}

const ava_function* ava_function_of_value(ava_value val) {
  AVA_STATIC_STRING(too_few_element_message,
                    "List has too few elements to be a function.");
  AVA_STATIC_STRING(unknown_cc_message, "Unknown calling convention: ");
  AVA_STATIC_STRING(function_must_have_arguments_message,
                    "Function specification must have at least one argument.");
  AVA_STATIC_STRING(argspec_too_short_message,
                    "Argument specification too short: [");
  AVA_STATIC_STRING(invalid_function_message_prefix,
                    "Function not valid: ");
  AVA_STATIC_STRING(null_function_pointer, "Null function pointer.");
  ava_list_value list, argspec;
  ava_function* fun;
  ava_argument_spec* args;
  ava_string calling_convention, invalid_message;
  size_t first_arg, arg;
  ava_bool use_marshal;

  if (&ava_function_list_impl == AVA_GET_ATTRIBUTE(val, ava_list_trait))
    return ava_value_ptr(val);

  list = ava_list_value_of(val);
  if (ava_list_length(list) < 2)
    ava_throw_str(&ava_format_exception, too_few_element_message);

  fun = AVA_NEW(ava_function);
  fun->address = (void(*)())ava_integer_of_value(ava_list_index(list, 0), 0);
  if (!fun->address)
    ava_throw_str(&ava_format_exception, null_function_pointer);

  calling_convention = ava_to_string(ava_list_index(list, 1));
  switch (ava_string_to_ascii9(calling_convention)) {
  case AVA_ASCII9('a','v','a'):
    fun->calling_convention = ava_cc_ava;
    use_marshal = ava_false;
    break;

  case AVA_ASCII9('c'):
    fun->calling_convention = ava_cc_c;
    use_marshal = ava_true;
    break;

  case AVA_ASCII9('m','s','s','t','d'):
    fun->calling_convention = ava_cc_msstd;
    use_marshal = ava_true;
    break;

  case AVA_ASCII9('t','h','i','s'):
    fun->calling_convention = ava_cc_this;
    use_marshal = ava_true;
    break;

  default:
    ava_throw_str(&ava_format_exception,
                  ava_string_concat(unknown_cc_message, calling_convention));
  }

  if (use_marshal) {
    first_arg = 3;
    fun->c_return_type = ava_function_parse_marshal_type(
      ava_list_index(list, 2));
  } else {
    first_arg = 2;
  }

  fun->num_args = ava_list_length(list) - first_arg;
  if (0 == fun->num_args)
    ava_throw_str(&ava_format_exception, function_must_have_arguments_message);

  fun->args = args = ava_alloc(sizeof(ava_argument_spec) * fun->num_args);

  for (arg = 0; arg < fun->num_args; ++arg) {
    argspec = ava_list_value_of(ava_list_index(list, arg + first_arg));

    if (ava_list_length(argspec) < (unsigned)(1 + use_marshal))
      ava_throw_str(&ava_format_exception,
                    ava_string_concat(
                      argspec_too_short_message,
                      ava_string_concat(
                        ava_to_string(argspec.v), AVA_ASCII9_STRING("]"))));

    if (use_marshal)
      args[arg].marshal = ava_function_parse_marshal_type(
        ava_list_index(argspec, 0));

    args[arg].binding = ava_function_parse_binding(argspec, use_marshal);
  }

  if (!ava_function_is_valid(&invalid_message, fun))
    ava_throw_str(&ava_format_exception,
                  ava_string_concat(invalid_function_message_prefix,
                                    invalid_message));

  ava_function_init_ffi(fun);
  return fun;
}

AVA_STATIC_STRING(ava_sshort_text, "ava_sshort");
AVA_STATIC_STRING(ava_ushort_text, "ava_ushort");
AVA_STATIC_STRING(ava_integer_text, "ava_integer");

static ava_c_marshalling_type ava_function_parse_marshal_type(ava_value val) {
  AVA_STATIC_STRING(unknown_marshalling_type_message,
                    "Unknown marshalling type: ");
  ava_exception_handler h;
  ava_string str = ava_to_string(val);
  ava_c_marshalling_type ret;

#define PRIM(t) ret.primitive_type = t; return ret;
#define AVA 'a','v','a','_'
  switch (ava_string_to_ascii9(str)) {
  case AVA_ASCII9('v','o','i','d'):             PRIM(ava_cmpt_void);
  case AVA_ASCII9('b','y','t','e'):             PRIM(ava_cmpt_byte);
  case AVA_ASCII9('s','h','o','r','t'):         PRIM(ava_cmpt_short);
  case AVA_ASCII9('i','n','t'):                 PRIM(ava_cmpt_int);
  case AVA_ASCII9('l','o','n','g'):             PRIM(ava_cmpt_long);
  case AVA_ASCII9('l','l','o','n','g'):         PRIM(ava_cmpt_llong);
  case AVA_ASCII9('u','b','y','t','e'):         PRIM(ava_cmpt_ubyte);
  case AVA_ASCII9('u','s','h','o','r','t'):     PRIM(ava_cmpt_ushort);
  case AVA_ASCII9('u','i','n','t'):             PRIM(ava_cmpt_uint);
  case AVA_ASCII9('u','l','o','n','g'):         PRIM(ava_cmpt_ulong);
  case AVA_ASCII9('u','l','l','o','n','g'):     PRIM(ava_cmpt_ullong);
  case AVA_ASCII9(AVA, 's','b','y','t','e'):    PRIM(ava_cmpt_ava_sbyte);
  /* ava_sshort is 10 chars long */
  case AVA_ASCII9(AVA, 's','i','n','t'):        PRIM(ava_cmpt_ava_sint);
  case AVA_ASCII9(AVA, 's','l','o','n','g'):    PRIM(ava_cmpt_ava_slong);
  case AVA_ASCII9(AVA, 'u','b','y','t','e'):    PRIM(ava_cmpt_ava_ubyte);
  /* ava_ushort is 10 chars long */
  case AVA_ASCII9(AVA, 'u','i','n','t'):        PRIM(ava_cmpt_ava_uint);
  case AVA_ASCII9(AVA, 'u','l','o','n','g'):    PRIM(ava_cmpt_ava_ulong);
  /* ava_integer is 11 chars long */
  case AVA_ASCII9('s','i','z','e'):             PRIM(ava_cmpt_size);
  case AVA_ASCII9('f','l','o','a','t'):         PRIM(ava_cmpt_float);
  case AVA_ASCII9('d','o','u','b','l','e'):     PRIM(ava_cmpt_double);
  case AVA_ASCII9('l','d','o','u','b','l','e'): PRIM(ava_cmpt_ldouble);
  case AVA_ASCII9(AVA, 'r','e','a','l'):        PRIM(ava_cmpt_ava_real);
  case AVA_ASCII9('s','t','r','i','n','g'):     PRIM(ava_cmpt_string);
  }
#undef AVA

  if (0 == ava_strcmp(str, ava_sshort_text))    PRIM(ava_cmpt_ava_sshort);
  if (0 == ava_strcmp(str, ava_ushort_text))    PRIM(ava_cmpt_ava_ushort);
  if (0 == ava_strcmp(str, ava_integer_text))   PRIM(ava_cmpt_ava_integer);
#undef PRIM

  /* Not a primitive type, try to parse as a pointer */
  ava_try (h) {
    ret.pointer_proto = ava_pointer_prototype_parse(str);
    ret.primitive_type = ava_cmpt_pointer;
    return ret;
  } ava_catch (h, ava_format_exception) {
    /* ignore and fall through */
  } ava_catch_all {
    ava_rethrow(&h);
  }

  ava_throw_str(&ava_format_exception,
                ava_string_concat(unknown_marshalling_type_message, str));
}

static ava_argument_binding ava_function_parse_binding(ava_list_value argspec,
                                                       size_t start_ix) {
  AVA_STATIC_STRING(missing_binding_message,
                    "Missing binding in argument specification.");
  AVA_STATIC_STRING(implicit_length_message,
                    "implicit binding must be followed by exactly one value.");
  AVA_STATIC_STRING(pos_length_message,
                    "pos binding must be followed by at most one value.");
  AVA_STATIC_STRING(varargs_length_message,
                    "varargs binding must not be followed by any values.");
  AVA_STATIC_STRING(named_length_message,
                    "named binding must be followed by one or two values.");
  AVA_STATIC_STRING(bool_length_message,
                    "bool binding must be followed by exactly one value.");
  AVA_STATIC_STRING(unknown_binding_message,
                    "Unknown argument binding type: ");
  size_t length = ava_list_length(argspec);
  ava_string type;
  ava_argument_binding ret;

  memset(&ret, 0, sizeof(ret));

  if (start_ix >= length)
    ava_throw_str(&ava_format_exception, missing_binding_message);

  type = ava_to_string(ava_list_index(argspec, start_ix));
  switch (ava_string_to_ascii9(type)) {
  case AVA_ASCII9('i','m','p','l','i','c','i','t'):
    if (length != start_ix + 2)
      ava_throw_str(&ava_format_exception, implicit_length_message);

    ret.type = ava_abt_implicit;
    ret.value = ava_list_index(argspec, start_ix + 1);
    break;

  case AVA_ASCII9('p','o','s'):
    if (length == start_ix + 1) {
      ret.type = ava_abt_pos;
    } else if (length == start_ix + 2) {
      ret.type = ava_abt_pos_default;
      ret.value = ava_list_index(argspec, start_ix + 1);
    } else {
      ava_throw_str(&ava_format_exception, pos_length_message);
    }
    break;

  case AVA_ASCII9('v','a','r','a','r','g','s'):
    if (length != start_ix + 1)
      ava_throw_str(&ava_format_exception, varargs_length_message);

    ret.type = ava_abt_varargs;
    break;

  case AVA_ASCII9('n','a','m','e','d'):
    if (length == start_ix + 2) {
      ret.type = ava_abt_named;
      ret.name = ava_to_string(ava_list_index(argspec, start_ix + 1));
    } else if (length == start_ix + 3) {
      ret.type = ava_abt_named_default;
      ret.name = ava_to_string(ava_list_index(argspec, start_ix + 1));
      ret.value = ava_list_index(argspec, start_ix + 2);
    } else {
      ava_throw_str(&ava_format_exception, named_length_message);
    }
    break;

  case AVA_ASCII9('b','o','o','l'):
    if (length != start_ix + 2)
      ava_throw_str(&ava_format_exception, bool_length_message);

    ret.type = ava_abt_bool;
    ret.name = ava_to_string(ava_list_index(argspec, start_ix + 1));
    break;

  default:
    ava_throw_str(&ava_format_exception,
                  ava_string_concat(unknown_binding_message, type));
  }

  return ret;
}

static size_t ava_function_list_length(ava_list_value l) {
  const ava_function* fun = ava_value_ptr(l.v);

  return 2 + fun->num_args + (ava_cc_ava != fun->calling_convention);
}

static ava_value ava_function_list_index(ava_list_value l, size_t ix) {
  const ava_function* fun = ava_value_ptr(l.v);
  ava_bool marshal = ava_cc_ava != fun->calling_convention;

  if (0 == ix)
    return ava_value_of_integer((ava_integer)fun->address);

  if (1 == ix) {
    switch (fun->calling_convention) {
    case ava_cc_ava:    return ava_value_of_string(AVA_ASCII9_STRING("ava"));
    case ava_cc_c:      return ava_value_of_string(AVA_ASCII9_STRING("c"));
    case ava_cc_msstd:  return ava_value_of_string(AVA_ASCII9_STRING("msstd"));
    case ava_cc_this:   return ava_value_of_string(AVA_ASCII9_STRING("this"));
    }
    /* unreachable */
    abort();
  }

  if (marshal && 2 == ix)
    return ava_function_marshal_type_to_value(fun->c_return_type);

  ix -= 2 + marshal;
  assert(ix < fun->num_args);
  return ava_function_argspec_to_value(fun->args[ix], marshal);
}

static ava_value ava_function_marshal_type_to_value(
  ava_c_marshalling_type type
) {
  ava_string str;

#define PRIM(t) ava_cmpt_##t: str = AVA_ASCII9_STRING(#t); break
  switch (type.primitive_type) {
  case PRIM(void);
  case PRIM(byte);
  case PRIM(short);
  case PRIM(int);
  case PRIM(long);
  case PRIM(llong);
  case PRIM(ubyte);
  case PRIM(ushort);
  case PRIM(uint);
  case PRIM(ulong);
  case PRIM(ullong);
  case PRIM(ava_sbyte);
  case PRIM(ava_sint);
  case PRIM(ava_slong);
  case PRIM(ava_ubyte);
  case PRIM(ava_uint);
  case PRIM(ava_ulong);
  case PRIM(size);
  case PRIM(float);
  case PRIM(double);
  case PRIM(ldouble);
  case PRIM(ava_real);
  case PRIM(string);

  case ava_cmpt_ava_sshort:
    str = ava_sshort_text;
    break;

  case ava_cmpt_ava_ushort:
    str = ava_ushort_text;
    break;

  case ava_cmpt_ava_integer:
    str = ava_integer_text;
    break;

  case ava_cmpt_pointer:
    str = ava_pointer_prototype_to_string(type.pointer_proto);
    break;
  }
#undef PRIM

  return ava_value_of_string(str);
}

static ava_value ava_function_argspec_to_value(
  ava_argument_spec arg, ava_bool marshal
) {
  ava_value values[4];
  size_t count;

  if (marshal)
    values[0] = ava_function_marshal_type_to_value(arg.marshal);

  switch (arg.binding.type) {
  case ava_abt_implicit:
    count = 2;
    values[1] = ava_value_of_string(AVA_ASCII9_STRING("implicit"));
    values[2] = arg.binding.value;
    /* Not break so we can convince GCC that count is used initialised without
     * needing the abort() call in a default clause, which would suppress any
     * warnings regarding forgetting an enum.
     */
    goto ok;

  case ava_abt_pos:
    count = 1;
    values[1] = ava_value_of_string(AVA_ASCII9_STRING("pos"));
    goto ok;

  case ava_abt_pos_default:
    count = 2;
    values[1] = ava_value_of_string(AVA_ASCII9_STRING("pos"));
    values[2] = arg.binding.value;
    goto ok;

  case ava_abt_varargs:
    count = 1;
    values[1] = ava_value_of_string(AVA_ASCII9_STRING("varargs"));
    goto ok;

  case ava_abt_named:
    count = 2;
    values[1] = ava_value_of_string(AVA_ASCII9_STRING("named"));
    values[2] = ava_value_of_string(arg.binding.name);
    goto ok;

  case ava_abt_named_default:
    count = 3;
    values[1] = ava_value_of_string(AVA_ASCII9_STRING("named"));
    values[2] = ava_value_of_string(arg.binding.name);
    values[3] = arg.binding.value;
    goto ok;

  case ava_abt_bool:
    count = 2;
    values[1] = ava_value_of_string(AVA_ASCII9_STRING("bool"));
    values[2] = ava_value_of_string(arg.binding.name);
    goto ok;
  }

  /* unreachable */
  abort();

  ok:
  return ava_list_of_values(values + !marshal, count + marshal).v;
}

static ava_bool is_named(ava_argument_binding_type type) {
  return type == ava_abt_named || type == ava_abt_named_default ||
    type == ava_abt_bool;
}

ava_bool ava_function_is_valid(ava_string* message, const ava_function* fun) {
  AVA_STATIC_STRING(
    var_shape_cannot_follow_varargs,
    "Variably-shaped argument found after variadic argument.");
  AVA_STATIC_STRING(
    all_var_shaped_must_be_contiguous,
    "Variably-shaped arguments are not contiguous.");
  AVA_STATIC_STRING(
    no_explicit_arguments,
    "Function has no explicit arguments.");
  AVA_STATIC_STRING(
    duplicated_name,
    "Named argument declared more than once: ");

  ava_bool has_seen_varargs = ava_false;
  ava_bool has_seen_var_shape = ava_false;
  ava_bool has_seen_fixed_shape_after_var_shape = ava_false;
  ava_bool only_implicits = ava_true;
  ava_bool is_var_shape;
  size_t i, j;

  for (i = 0; i < fun->num_args; ++i) {
    switch (fun->args[i].binding.type) {
    case ava_abt_implicit:
    case ava_abt_pos:
      is_var_shape = ava_false;
      goto ok;

    case ava_abt_pos_default:
    case ava_abt_varargs:
    case ava_abt_named:
    case ava_abt_named_default:
    case ava_abt_bool:
      is_var_shape = ava_true;
      goto ok;
    }

    /* unreachable */
    abort();

    ok:

    if (has_seen_varargs && is_var_shape) {
      *message = var_shape_cannot_follow_varargs;
      return ava_false;
    }

    if (has_seen_fixed_shape_after_var_shape && is_var_shape) {
      *message = all_var_shaped_must_be_contiguous;
      return ava_false;
    }

    if (is_named(fun->args[i].binding.type)) {
      for (j = 0; j < i; ++j) {
        if (is_named(fun->args[j].binding.type)) {
          if (0 == ava_strcmp(fun->args[i].binding.name,
                              fun->args[j].binding.name)) {
            *message = ava_string_concat(
              duplicated_name, fun->args[i].binding.name);
            return ava_false;
          }
        }
      }
    }

    has_seen_var_shape |= is_var_shape;
    has_seen_fixed_shape_after_var_shape |= !is_var_shape && has_seen_var_shape;
    has_seen_varargs |= (ava_abt_varargs == fun->args[i].binding.type);
    only_implicits &= (ava_abt_implicit == fun->args[i].binding.type);
  }

  if (only_implicits) {
    *message = no_explicit_arguments;
    return ava_false;
  }

  return ava_true;
}

ava_function_bind_status ava_function_bind(
  const ava_function* fun,
  size_t num_parms,
  const ava_function_parameter parms[],
  ava_function_bound_argument bound_args[],
  size_t variadic_collection[],
  ava_string* message
) {
  AVA_STATIC_STRING(
    missing_value_for_named_parameter,
    "Missing value for named parameter: ");
  AVA_STATIC_STRING(
    unknown_named_parameter,
    "No match for named parameter: ");
  AVA_STATIC_STRING(
    too_many_parms_message,
    "Too many parameters to function; the following could not be bound: ");
  AVA_STATIC_STRING(
    unbound_argument_message,
    "No parameter bound to mandatory argument: ");

  char consumed_args[fun->num_args];
  size_t arg, i, other, parm = 0, parm_limit = num_parms;
  ava_string target_name;

  memset(consumed_args, 0, sizeof(consumed_args));

  /* Bind all implicit arguments */
  for (arg = 0; arg < fun->num_args; ++arg) {
    if (ava_abt_implicit == fun->args[arg].binding.type) {
      bound_args[arg].type = ava_fbat_implicit;
      bound_args[arg].v.value = fun->args[arg].binding.value;
      consumed_args[arg] = ava_true;
    }
  }

  /* Bind pos arguments from left */
  for (arg = parm = 0; arg < fun->num_args && parm < parm_limit; ++arg) {
    if (consumed_args[arg]) continue;
    if (ava_abt_pos != fun->args[arg].binding.type) break;
    if (ava_fpt_spread == parms[parm].type)
      return ava_fbs_unpack;

    bound_args[arg].type = ava_fbat_parameter;
    bound_args[arg].v.parameter_index = parm;
    consumed_args[arg] = ava_true;
    ++parm;
  }
  /* Bind remaining pos arguments from right */
  for (arg = fun->num_args - 1; arg < fun->num_args &&
         parm < parm_limit; --arg) {
    if (consumed_args[arg]) continue;
    if (ava_abt_pos != fun->args[arg].binding.type) break;
    if (ava_fpt_spread == parms[parm_limit - 1].type)
      return ava_fbs_unpack;

    bound_args[arg].type = ava_fbat_parameter;
    bound_args[arg].v.parameter_index = parm_limit - 1;
    consumed_args[arg] = ava_true;
    --parm_limit;
  }

  /* Bind variable-sized arguments from left until everything consumed */
  for (arg = 0; arg < fun->num_args && parm < parm_limit; ++arg) {
    if (consumed_args[arg]) continue;

    if (is_named(fun->args[arg].binding.type)) {
      /* Named parameter. We need to know the actual value of the current parm,
       * or it's impossible to bind now.
       */
      if (ava_fpt_spread == parms[parm].type)
        return ava_fbs_unpack;
      if (ava_fpt_dynamic == parms[parm].type)
        return ava_fbs_unknown;

      target_name = ava_to_string(parms[parm].value);
      for (other = arg; other < fun->num_args &&
             is_named(fun->args[other].binding.type); ++other) {
        if (consumed_args[other]) continue;
        if (0 == ava_strcmp(target_name, fun->args[other].binding.name)) {
          /* Matched */
          if (ava_abt_bool == fun->args[other].binding.type) {
            bound_args[other].type = ava_fbat_implicit;
            bound_args[other].v.value = ava_value_of_string(
              AVA_ASCII9_STRING("true"));
            consumed_args[other] = ava_true;
            ++parm;
          } else {
            if (parm + 1 >= parm_limit) {
              *message = ava_string_concat(
                missing_value_for_named_parameter, target_name);
              return ava_fbs_impossible;
            }

            if (ava_fpt_spread == parms[parm+1].type)
              return ava_fbs_unpack;

            bound_args[other].type = ava_fbat_parameter;
            bound_args[other].v.parameter_index = parm + 1;
            parm += 2;
          }

          consumed_args[other] = ava_true;
          /* Prevent advancing the argument, since we didn't necessarily bind
           * the first.
           */
          --arg;
          goto next_arg_or_parm;
        } /* end if (matched name) */
      } /* end for (searching for name match) */

      /* Nothing matched */
      *message = ava_string_concat(
        unknown_named_parameter, target_name);
      return ava_fbs_impossible;
    } else if (ava_abt_pos_default == fun->args[arg].binding.type) {
      bound_args[arg].type = ava_fbat_parameter;
      bound_args[arg].v.parameter_index = parm;
      consumed_args[arg] = ava_true;
      ++parm;
    } else if (ava_abt_varargs == fun->args[arg].binding.type) {
      /* Gobble all the remaining parms up */
      for (i = 0; i < parm_limit - parm; ++i)
        variadic_collection[i] = parm + i;

      bound_args[arg].type = ava_fbat_collect;
      bound_args[arg].v.collection_size = parm_limit - parm;
      consumed_args[arg] = ava_true;
      parm = parm_limit;
    } else {
      /* unreachable */
      abort();
    }

    next_arg_or_parm:;
  } /* end for (while arguments and parms remain) */

  /* Make sure that everything found a home */
  if (parm < parm_limit) {
    *message = ava_string_concat(
      too_many_parms_message,
      ava_string_concat(
        ava_to_string(ava_value_of_integer(parm+1)),
        ava_string_concat(
          AVA_ASCII9_STRING(".."),
          ava_to_string(ava_value_of_integer(parm_limit)))));
    return ava_fbs_impossible;
  }

  /* Ensure all mandatory arguments are bound, and set defaults on optional
   * arguments.
   */
  for (arg = 0; arg < fun->num_args; ++arg) {
    if (consumed_args[arg]) continue;

    switch (fun->args[arg].binding.type) {
    case ava_abt_named_default:
    case ava_abt_pos_default:
      bound_args[arg].type = ava_fbat_implicit;
      bound_args[arg].v.value = fun->args[arg].binding.value;
      break;

    case ava_abt_bool:
      bound_args[arg].type = ava_fbat_implicit;
      bound_args[arg].v.value = ava_value_of_string(
        AVA_ASCII9_STRING("false"));
      break;

    case ava_abt_varargs:
      bound_args[arg].type = ava_fbat_implicit;
      bound_args[arg].v.value = ava_empty_list().v;
      break;

    case ava_abt_named:
      *message = ava_string_concat(
        unbound_argument_message, fun->args[arg].binding.name);
      return ava_fbs_impossible;

    default:
      *message = ava_string_concat(
        unbound_argument_message,
        ava_string_concat(
          AVA_ASCII9_STRING("#"),
          ava_to_string(ava_value_of_integer(arg+1))));
      return ava_fbs_impossible;
    }
  }

  return ava_fbs_bound;
}

void ava_function_apply_bind(
  size_t num_args,
  ava_value arguments[],
  const ava_function_parameter parms[],
  const ava_function_bound_argument bound_args[],
  const size_t variadic_collection[]
) {
  size_t arg, parm, i;
  ava_list_value accum;

  for (arg = 0; arg < num_args; ++arg) {
    switch (bound_args[arg].type) {
    case ava_fbat_implicit:
      arguments[arg] = bound_args[arg].v.value;
      break;

    case ava_fbat_parameter:
      parm = bound_args[arg].v.parameter_index;
      assert(ava_fpt_dynamic != parms[parm].type);
      arguments[arg] = parms[parm].value;
      break;

    case ava_fbat_collect:
      accum = ava_empty_list();
      for (i = 0; i < bound_args[arg].v.collection_size; ++i) {
        parm = variadic_collection[i];
        assert(ava_fpt_dynamic != parms[parm].type);
        if (parms[parm].type == ava_fpt_static)
          accum = ava_list_append(accum, parms[parm].value);
        else
          accum = ava_list_concat(accum, ava_list_value_of(parms[parm].value));
      }
      arguments[arg] = accum.v;
      break;
    }
  }
}

ava_value ava_function_invoke(const ava_function* fun,
                              ava_value args[]) {
  AVA_STATIC_STRING(non_empty_string_to_void_arg_message,
                    "Non-empty string passed to void argument.");

  ava_ffi_arg ffi_args[fun->num_args], *ffi_arg_ptrs[fun->num_args];
  ava_ffi_arg return_value;
  size_t logical_arg, physical_arg;

  if (ava_cc_ava == fun->calling_convention) {
    switch (fun->num_args) {
    case 1: return ((*(ava_value(*)(ava_value))
                     fun->address)
                    (args[0]));
    case 2: return ((*(ava_value(*)(ava_value,ava_value))
                      fun->address)
                    (args[0], args[1]));
    case 3: return ((*(ava_value(*)(ava_value,ava_value,ava_value))
                      fun->address)
                    (args[0], args[1], args[2]));
    case 4: return ((*(ava_value(*)(ava_value,ava_value,ava_value,ava_value))
                     fun->address)
                    (args[0], args[1], args[2], args[3]));
    case 5: return ((*(ava_value(*)(ava_value,ava_value,ava_value,ava_value,
                                    ava_value))
                     fun->address)
                    (args[0], args[1], args[2], args[3],
                     args[4]));
    case 6: return ((*(ava_value(*)(ava_value,ava_value,ava_value,ava_value,
                                    ava_value,ava_value))
                     fun->address)
                    (args[0], args[1], args[2], args[3],
                     args[4], args[5]));
    case 7: return ((*(ava_value(*)(ava_value,ava_value,ava_value,ava_value,
                                    ava_value,ava_value,ava_value))
                     fun->address)
                    (args[0], args[1], args[2], args[3],
                     args[4], args[5], args[6]));
    case 8: return ((*(ava_value(*)(ava_value,ava_value,ava_value,ava_value,
                                    ava_value,ava_value,ava_value,ava_value))
                     fun->address)
                    (args[0], args[1], args[2], args[3],
                     args[4], args[5], args[6], args[7]));
    default:
      return (*(ava_value(*)(ava_value*restrict))fun->address)(args);
    }
  }

  /* Else, FFI-based calling convention */
  physical_arg = 0;
  for (logical_arg = 0; logical_arg < fun->num_args; ++logical_arg) {
    ffi_arg_ptrs[logical_arg] = ffi_args + logical_arg;

    switch (fun->args[logical_arg].marshal.primitive_type) {
    case ava_cmpt_void:
      if (!ava_string_is_empty(ava_to_string(args[logical_arg])))
        ava_throw_str(&ava_format_exception,
                      non_empty_string_to_void_arg_message);
      break;

#define INT ava_integer_of_value(args[logical_arg], 0)
#define REAL ava_real_of_value(args[logical_arg], 0)
#define ARG ffi_args[physical_arg++]
    case ava_cmpt_byte:         ARG.c_byte      = INT; break;
    case ava_cmpt_short:        ARG.c_short     = INT; break;
    case ava_cmpt_int:          ARG.c_int       = INT; break;
    case ava_cmpt_long:         ARG.c_long      = INT; break;
    case ava_cmpt_llong:        ARG.c_llong     = INT; break;
    case ava_cmpt_ubyte:        ARG.c_ubyte     = INT; break;
    case ava_cmpt_ushort:       ARG.c_ushort    = INT; break;
    case ava_cmpt_uint:         ARG.c_uint      = INT; break;
    case ava_cmpt_ulong:        ARG.c_ulong     = INT; break;
    case ava_cmpt_ullong:       ARG.c_ullong    = INT; break;
    case ava_cmpt_ava_ubyte:    ARG.a_ubyte     = INT; break;
    case ava_cmpt_ava_ushort:   ARG.a_ushort    = INT; break;
    case ava_cmpt_ava_uint:     ARG.a_uint      = INT; break;
    case ava_cmpt_ava_ulong:    ARG.a_ulong     = INT; break;
    case ava_cmpt_ava_sbyte:    ARG.a_sbyte     = INT; break;
    case ava_cmpt_ava_sshort:   ARG.a_sshort    = INT; break;
    case ava_cmpt_ava_sint:     ARG.a_sint      = INT; break;
    case ava_cmpt_ava_slong:    ARG.a_slong     = INT; break;
    case ava_cmpt_ava_integer:  ARG.a_integer   = INT; break;
    case ava_cmpt_size:         ARG.c_size      = INT; break;
    case ava_cmpt_float:        ARG.c_float     = REAL; break;
    case ava_cmpt_double:       ARG.c_double    = REAL; break;
    case ava_cmpt_ldouble:      ARG.c_ldouble   = REAL; break;
    case ava_cmpt_ava_real:     ARG.a_real      = REAL; break;
#undef REAL
#undef INT

    case ava_cmpt_string:
      ARG.ptr = (void*)ava_string_to_cstring(ava_to_string(args[logical_arg]));
      break;

    case ava_cmpt_pointer:
      if (fun->args[logical_arg].marshal.pointer_proto->is_const)
        ARG.ptr = (void*)ava_pointer_get_const(
          args[logical_arg],
          fun->args[logical_arg].marshal.pointer_proto->tag);
      else
        ARG.ptr = ava_pointer_get_mutable(
          args[logical_arg],
          fun->args[logical_arg].marshal.pointer_proto->tag);
      break;
    }
  }

  ffi_call((ffi_cif*)fun->ffi, (void(*)(void))fun->address,
           &return_value, (void**)&ffi_arg_ptrs);

  switch (fun->c_return_type.primitive_type) {
  case ava_cmpt_void: return ava_value_of_string(AVA_EMPTY_STRING);

  case ava_cmpt_byte:
  case ava_cmpt_short:
  case ava_cmpt_int:
#if SIZEOF_FFI_ARG >= SIZEOF_LONG
  case ava_cmpt_long:
#endif
#if SIZEOF_FFI_ARG >= SIZEOF_LONG_LONG
  case ava_cmpt_llong:
#endif
  case ava_cmpt_ava_sbyte:
  case ava_cmpt_ava_sshort:
  case ava_cmpt_ava_sint:
#if SIZEOF_FFI_ARG >= 8
  case ava_cmpt_ava_slong:
  case ava_cmpt_ava_integer:
#endif
    return ava_value_of_integer(return_value.returned_sint);

  case ava_cmpt_ubyte:
  case ava_cmpt_ushort:
  case ava_cmpt_uint:
#if SIZEOF_FFI_ARG >= SIZEOF_LONG
  case ava_cmpt_ulong:
#endif
#if SIZEOF_FFI_ARG >= SIZEOF_LONG_LONG
  case ava_cmpt_ullong:
#endif
  case ava_cmpt_ava_ubyte:
  case ava_cmpt_ava_ushort:
  case ava_cmpt_ava_uint:
#if SIZEOF_FFI_ARG >= 8
  case ava_cmpt_ava_ulong:
#endif
#if SIZEOF_FFI_ARG >= SIZEOF_SIZE_T
  case ava_cmpt_size:
#endif
    return ava_value_of_integer(return_value.returned_uint);

#if SIZEOF_FFI_ARG < SIZEOF_LONG
  case ava_cmpt_long:
    return ava_value_of_integer(return_value.c_long);
  case ava_cmpt_ulong:
    return ava_value_of_integer(return_value.c_ulong);
#endif

#if SIZEOF_FFI_ARG < SIZEOF_LONG_LONG
  case ava_cmpt_llong:
    return ava_value_of_integer(return_value.c_llong);
  case ava_cmpt_ullong:
    return ava_value_of_integer(return_value.c_ullong);
#endif

#if SIZEOF_FFI_ARG < 8
  case ava_cmpt_ava_slong:
    return ava_value_of_integer(return_value.a_slong);
  case ava_cmpt_ava_ulong:
    return ava_value_of_integer(return_value.a_ulong);
  case ava_cmpt_ava_integer:
    return ava_value_of_integer(return_value.a_integer);
#endif

#if SIZEOF_FFI_ARG < SIZEOF_SIZE_T
  case ava_cmpt_size:
    return ava_value_of_integer(return_value.c_size);
#endif

  case ava_cmpt_float:
    return ava_value_of_real(return_value.c_float);
  case ava_cmpt_double:
    return ava_value_of_real(return_value.c_double);
  case ava_cmpt_ldouble:
    return ava_value_of_real(return_value.c_ldouble);
  case ava_cmpt_ava_real:
    return ava_value_of_real(return_value.a_real);

  case ava_cmpt_string:
    return ava_value_of_cstring(return_value.ptr);

  case ava_cmpt_pointer:
    return ava_pointer_of_proto(
      fun->c_return_type.pointer_proto, return_value.ptr).v;
  }

  /* unreachable */
  abort();
}

#define MAX_STACK_TEMPORARIES 32
ava_value ava_function_bind_invoke(
  const ava_function* fun,
  size_t num_parms,
  const ava_function_parameter parms[]
) {
  AVA_STATIC_STRING(error_prefix, "Can't invoke function: ");
  ava_function_bind_status bind_status;
  ava_value arguments[fun->num_args];
  ava_function_bound_argument bound_args[fun->num_args];
  ava_string message;
  /* If reasonable, use stack space for temporary data */
  size_t stack_variadic_collection[MAX_STACK_TEMPORARIES];
  size_t* variadic_collection;

  tail_call:
  if (num_parms <= MAX_STACK_TEMPORARIES)
    variadic_collection = stack_variadic_collection;
  else
    variadic_collection = ava_alloc_atomic(num_parms * sizeof(size_t));

  bind_status = ava_function_bind(fun, num_parms, parms, bound_args,
                                  variadic_collection, &message);
  switch (bind_status) {
  case ava_fbs_bound: break; /* ok */
  case ava_fbs_unknown:
    /* all arguments are supposed to be known at this point, so the caller is
     * doing something wrong.
     */
    abort();

  case ava_fbs_unpack:
    ava_function_explode(&num_parms, &parms);
    goto tail_call;

  case ava_fbs_impossible:
    ava_throw_str(&ava_error_exception,
                  ava_string_concat(error_prefix, message));
  }

  ava_function_apply_bind(fun->num_args, arguments,
                          parms, bound_args, variadic_collection);
  return ava_function_invoke(fun, arguments);
}

static void ava_function_explode(size_t* num_parms_p,
                                 const ava_function_parameter** parms_p) {
  size_t num_parms = *num_parms_p;
  size_t exploded_count, i, j;
  ava_function_parameter* exploded;
  const ava_function_parameter* parms = *parms_p;
  ava_list_value list;
  size_t list_len;

  /* Find out how big the new array will be */
  exploded_count = 0;
  for (i = 0; i < num_parms; ++i) {
    if (ava_fpt_spread == parms[i].type)
      exploded_count += ava_list_length(parms[i].value);
    else
      ++exploded_count;
  }

  exploded = ava_alloc(sizeof(ava_function_parameter) * exploded_count);

  /* Copy into the new array */
  exploded_count = 0;
  for (i = 0; i < num_parms; ++i) {
    if (ava_fpt_spread == parms[i].type) {
      list = ava_list_value_of(parms[i].value);
      list_len = ava_list_length(list);
      for (j = 0; j < list_len; ++j) {
        exploded[exploded_count].type = ava_fpt_static;
        exploded[exploded_count++].value = ava_list_index(list, j);
      }
    } else {
      exploded[exploded_count].type = ava_fpt_static;
      exploded[exploded_count++].value = parms[i].value;
    }
  }

  *num_parms_p = exploded_count;
  *parms_p = exploded;
}

static ava_list_value ava_function_list_slice(ava_list_value l,
                                              size_t b, size_t e) {
  return ava_list_copy_slice(l, b, e);
}

static ava_list_value ava_function_list_append(ava_list_value l, ava_value v) {
  return ava_list_copy_append(l, v);
}

static ava_list_value ava_function_list_concat(ava_list_value l,
                                               ava_list_value o) {
  return ava_list_copy_concat(l, o);
}

static ava_list_value ava_function_list_remove(ava_list_value l,
                                               size_t b, size_t e) {
  return ava_list_copy_remove(l, b, e);
}

static ava_list_value ava_function_list_set(ava_list_value l,
                                            size_t ix, ava_value v) {
  return ava_list_copy_set(l, ix, v);
}
