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

/* To be included by gen-pcode.c */

static void format_error(ava_string message) AVA_NORETURN;
static void format_error(ava_string message) {
  ava_throw_str(&ava_format_exception, message);
}

#define FORMAT_ERROR(message) do {                      \
    AVA_STATIC_STRING(_message, message);               \
    format_error(ava_error_bad_pcode(_message));        \
  } while (0)

static ava_pcode_register_type ava_pcode_parse_register_type(
  ava_value value
) {
  ava_string str = ava_to_string(value);
  size_t length = ava_strlen(str);

  if (1 != length)
    FORMAT_ERROR("Register type of non-1 length");

  switch (ava_string_index(str, 0)) {
  case 'v': return ava_prt_var; break;
  case 'd': return ava_prt_data; break;
  case 'i': return ava_prt_int; break;
  case 'f': return ava_prt_function; break;
  case 'l': return ava_prt_list; break;
  case 'p': return ava_prt_parm; break;
  default: FORMAT_ERROR("Illegal register type");
  }
}

static ava_string ava_pcode_register_type_to_string(
  ava_pcode_register_type type
) {
  switch (type) {
  case ava_prt_var:     return AVA_ASCII9_STRING("v");
  case ava_prt_data:    return AVA_ASCII9_STRING("d");
  case ava_prt_int:     return AVA_ASCII9_STRING("i");
  case ava_prt_function:return AVA_ASCII9_STRING("f");
  case ava_prt_list:    return AVA_ASCII9_STRING("l");
  case ava_prt_parm:    return AVA_ASCII9_STRING("p");
  default:
    /* unreachable */
    abort();
  }
}

static ava_pcode_register ava_pcode_parse_register(ava_value value) {
  ava_string str = ava_to_string(value);
  size_t length = ava_strlen(str);
  ava_pcode_register reg;
  ava_integer index;

  if (length < 2)
    FORMAT_ERROR("Illegal register name (empty string or one char)");

  reg.type = ava_pcode_parse_register_type(
    ava_value_of_string(ava_string_slice(str, 0, 1)));

  index = ava_integer_of_value(
    ava_value_of_string(ava_string_slice(str, 1, length)), -1);
  if (index < 0 || index > 65535)
    FORMAT_ERROR("Illegal register index");
  reg.index = index;

  return reg;
}

static ava_string ava_pcode_register_to_string(ava_pcode_register reg) {
  return ava_strcat(
    ava_pcode_register_type_to_string(reg.type),
    ava_to_string(ava_value_of_integer(reg.index)));
}

static ava_integer ava_pcode_parse_int(ava_value value) {
  return ava_integer_of_value(value, 0);
}

static ava_string ava_pcode_int_to_string(ava_integer i) {
  return ava_to_string(ava_value_of_integer(i));
}

static ava_bool ava_pcode_parse_bool(ava_value value) {
  return !!ava_integer_of_value(value, 0);
}

static ava_string ava_pcode_bool_to_string(ava_bool b) {
  return b? AVA_ASCII9_STRING("true") : AVA_ASCII9_STRING("false");
}

static ava_string ava_pcode_parse_str(ava_value value) {
  return ava_to_string(value);
}

static ava_string ava_pcode_str_to_string(ava_string str) {
  return str;
}

static ava_demangled_name ava_pcode_parse_demangled_name(ava_value value) {
  size_t length = ava_list_length(value);
  ava_demangled_name ret;

  if (length != 2)
    FORMAT_ERROR("demangled-name must be list of length 2");

  ret.name = ava_to_string(ava_list_index(value, 1));

  switch (ava_string_to_ascii9(ava_to_string(ava_list_index(value, 0)))) {
  case AVA_ASCII9('n','o','n','e'):
    ret.scheme = ava_nms_none;
    break;

  case AVA_ASCII9('a','v','a'):
    ret.scheme = ava_nms_ava;
    break;

  default:
    FORMAT_ERROR("Illegal name mangling scheme");
  }

  return ret;
}

static ava_string ava_pcode_demangled_name_to_string(ava_demangled_name name) {
  ava_value values[2];

  switch (name.scheme) {
  case ava_nms_none:
    values[0] = ava_value_of_string(AVA_ASCII9_STRING("none"));
    break;

  case ava_nms_ava:
    values[0] = ava_value_of_string(AVA_ASCII9_STRING("ava"));
    break;

  default:
    /* unreachable */
    abort();
  }

  values[1] = ava_value_of_string(name.name);

  return ava_to_string(ava_list_of_values(values, 2).v);
}

static const ava_function* ava_pcode_parse_function(ava_value value) {
  /* Prepend a dummy address */
  ava_value dummy_address = ava_value_of_integer(-1);
  value = ava_list_concat(
    ava_list_of_values(&dummy_address, 1).v, value);

  /* TODO: This currently fails if the function uses a calling convention that
   * doesn't exist on the host platform.
   *
   * We should add a parameter to elide generating the FFI information.
   */
  return ava_function_of_value(value);
}

static ava_string ava_pcode_function_to_string(const ava_function* fun) {
  ava_value value = ava_value_of_function(fun);
  /* Strip dummy address away */
  value = ava_list_slice(value, 1, ava_list_length(value));
  return ava_to_string(value);
}

static ava_list_value ava_pcode_parse_list(ava_value value) {
  return ava_list_value_of(value);
}

static ava_string ava_pcode_list_to_string(ava_list_value list) {
  return ava_to_string(list.v);
}

static const ava_struct* ava_pcode_parse_sxt(ava_value value) {
  return ava_struct_of_value(value);
}

static ava_string ava_pcode_sxt_to_string(const ava_struct* sxt) {
  return ava_to_string(ava_value_of_struct(sxt));
}

static ava_string apply_indent(ava_string str, ava_uint indent) {
  ava_string base = AVA_EMPTY_STRING;
  ava_uint i;

  for (i = 0; i < indent; ++i)
    base = ava_strcat(base, AVA_ASCII9_STRING("\t"));

  return ava_strcat(base, str);
}

static ava_string ava_pcode_elt_escape(ava_string elt_string) {
  return ava_strcat(
    AVA_ASCII9_STRING("["),
    ava_strcat(
      elt_string,
      AVA_ASCII9_STRING("]")));
}

static ava_bool ava_pcode_is_valid_ex_type(ava_integer type) {
  return type >= 0 && type < ava_pet_other_exception;
}
