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
#include <stddef.h>

#include <atomic_ops.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/exception.h"
#include "avalanche/errors.h"
#include "avalanche/pointer.h"
#include "avalanche/struct.h"
#include "-internal-defs.h"

static ava_string ava_struct_to_string(ava_value value);
static ava_struct* ava_struct_parse(ava_list_value list);
static void ava_struct_parse_header(ava_struct* this, ava_list_value header);
static ava_value ava_struct_stringify_header(const ava_struct* this);

static void ava_struct_parse_field(ava_struct_field* dst, ava_list_value spec);
static void ava_struct_parse_int_field(
  ava_struct_field* dst, ava_list_value spec);
static void ava_struct_parse_real_field(
  ava_struct_field* dst, ava_list_value spec);
static void ava_struct_parse_ptr_field(
  ava_struct_field* dst, ava_list_value spec);
static void ava_struct_parse_hybrid_field(
  ava_struct_field* dst, ava_list_value spec);
static void ava_struct_parse_value_field(
  ava_struct_field* dst, ava_list_value spec);
static void ava_struct_parse_compose_field(
  ava_struct_field* dst, ava_list_value spec);
static void ava_struct_parse_array_field(
  ava_struct_field* dst, ava_list_value spec);
static void ava_struct_parse_tail_field(
  ava_struct_field* dst, ava_list_value spec);
static ava_struct_byte_order ava_struct_parse_byte_order(
  ava_string field_name, ava_string value);

static ava_value ava_struct_stringify_field(const ava_struct_field* dst);
static ava_value ava_struct_stringify_int_field(
  const ava_struct_field* dst);
static ava_value ava_struct_stringify_real_field(
  const ava_struct_field* dst);
static ava_value ava_struct_stringify_ptr_field(
  const ava_struct_field* dst);
static ava_value ava_struct_stringify_hybrid_field(
  const ava_struct_field* dst);
static ava_value ava_struct_stringify_value_field(
  const ava_struct_field* dst);
static ava_value ava_struct_stringify_compose_field(
  const ava_struct_field* dst);
static ava_value ava_struct_stringify_array_field(
  const ava_struct_field* dst);
static ava_value ava_struct_stringify_tail_field(
  const ava_struct_field* dst);
static ava_value ava_struct_stringify_byte_order(
  ava_struct_byte_order byte_order);

static void ava_struct_check_spec_length(
  ava_string field_name, ava_string type_name,
  ava_list_value spec, unsigned expected);
static void ava_struct_check_range(
  ava_string field_name, const char* elt_name,
  ava_integer value, ava_integer min_inc, ava_integer max_ex);

static void ava_struct_check_composition(ava_struct* this);
static void ava_struct_lay_out(ava_struct* this);

static const ava_value_trait ava_struct_type = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "struct",
  .to_string = ava_struct_to_string,
  .string_chunk_iterator = ava_singleton_string_chunk_iterator,
  .iterate_string_chunk = ava_iterate_singleton_string_chunk,
};

const ava_struct* ava_struct_of_value(ava_value value) {
  if (&ava_struct_type == AVA_GET_ATTRIBUTE(value, ava_value_trait))
    return ava_value_ptr(value);

  return ava_struct_parse(ava_list_value_of(value));
}

ava_value ava_value_of_struct(const ava_struct* sxt) {
  return ava_value_with_ptr(&ava_struct_type, sxt);
}

#define DIE(suffix,...)                                 \
  ava_throw_str(&ava_format_exception,                  \
                ava_error_struct_##suffix(__VA_ARGS__))

static ava_struct* ava_struct_parse(ava_list_value list) {
  ava_struct* this;
  size_t i;

  if (0 == ava_list_length(list))
    DIE(empty_list);

  this = AVA_NEWA(ava_struct, fields, ava_list_length(list) - 1);
  this->num_fields = ava_list_length(list) - 1;

  ava_struct_parse_header(this, ava_list_value_of(ava_list_index(list, 0)));
  for (i = 0; i < this->num_fields; ++i)
    ava_struct_parse_field(
      this->fields + i, ava_list_value_of(ava_list_index(list, i+1)));

  ava_struct_check_composition(this);
  ava_struct_lay_out(this);

  return this;
}

static ava_string ava_struct_to_string(ava_value val) {
  const ava_struct* this = ava_value_ptr(val);
  ava_list_value accum;
  size_t i;

  accum = ava_empty_list();
  accum = ava_list_append(accum, ava_struct_stringify_header(this));
  for (i = 0; i < this->num_fields; ++i)
    accum = ava_list_append(
      accum, ava_struct_stringify_field(this->fields + i));

  return ava_to_string(accum.v);
}

static void ava_struct_parse_header(ava_struct* this, ava_list_value header) {
  ava_string type;

  if (ava_list_length(header) < 2)
    DIE(header_too_short, ava_list_length(header));

  if (ava_list_length(header) > 3)
    DIE(header_too_long, ava_list_length(header));

  type = ava_to_string(ava_list_index(header, 0));
  if (ava_string_equal(AVA_ASCII9_STRING("struct"), type))
    this->is_union = ava_false;
  else if (ava_string_equal(AVA_ASCII9_STRING("union"), type))
    this->is_union = ava_true;
  else
    DIE(bad_header_type, type);

  this->name = ava_to_string(ava_list_index(header, 1));

  if (ava_list_length(header) > 2) {
    this->parent = ava_struct_of_value(ava_list_index(header, 2));
  } else {
    this->parent = NULL;
  }

  if (this->parent &&
      (this->is_union || this->parent->is_union))
    DIE(union_in_extension, this->name, this->parent->name);
}

static ava_value ava_struct_stringify_header(const ava_struct* this) {
  ava_list_value accum = ava_empty_list();

  accum = ava_list_append(
    accum, ava_value_of_string(
      this->is_union? AVA_ASCII9_STRING("union") :
      AVA_ASCII9_STRING("struct")));
  accum = ava_list_append(accum, ava_value_of_string(this->name));

  if (this->parent)
    accum = ava_list_append(accum, ava_value_of_struct(this->parent));

  return accum.v;
}

static void ava_struct_parse_field(ava_struct_field* dst, ava_list_value spec) {
  ava_string type;

  if (ava_list_length(spec) < 2)
    DIE(field_spec_too_short, ava_list_length(spec));

  type = ava_to_string(ava_list_index(spec, 0));
  dst->name = ava_to_string(
    ava_list_index(spec, ava_list_length(spec) - 1));

  switch (ava_string_to_ascii9(type)) {
  case AVA_ASCII9('i','n','t'):
    ava_struct_parse_int_field(dst, spec);
    break;

  case AVA_ASCII9('r','e','a','l'):
    ava_struct_parse_real_field(dst, spec);
    break;

  case AVA_ASCII9('p','t','r'):
    ava_struct_parse_ptr_field(dst, spec);
    break;

  case AVA_ASCII9('h','y','b','r','i','d'):
    ava_struct_parse_hybrid_field(dst, spec);
    break;

  case AVA_ASCII9('v','a','l','u','e'):
    ava_struct_parse_value_field(dst, spec);
    break;

  case AVA_ASCII9('c','o','m','p','o','s','e'):
    ava_struct_parse_compose_field(dst, spec);
    break;

  case AVA_ASCII9('a','r','r','a','y'):
    ava_struct_parse_array_field(dst, spec);
    break;

  case AVA_ASCII9('t','a','i','l'):
    ava_struct_parse_tail_field(dst, spec);
    break;

  default:
    DIE(bad_type, type);
  }
}

static ava_value ava_struct_stringify_field(const ava_struct_field* field) {
  switch (field->type) {
  case ava_sft_int:
    return ava_struct_stringify_int_field(field);
  case ava_sft_real:
    return ava_struct_stringify_real_field(field);
  case ava_sft_ptr:
    return ava_struct_stringify_ptr_field(field);
  case ava_sft_hybrid:
    return ava_struct_stringify_hybrid_field(field);
  case ava_sft_value:
    return ava_struct_stringify_value_field(field);
  case ava_sft_compose:
    return ava_struct_stringify_compose_field(field);
  case ava_sft_array:
    return ava_struct_stringify_array_field(field);
  case ava_sft_tail:
    return ava_struct_stringify_tail_field(field);
  }

  /* unreachable */
  abort();
}

static void ava_struct_check_spec_length(
  ava_string field_name, ava_string type_name,
  ava_list_value spec, unsigned expected
) {
  if (expected != ava_list_length(spec))
    DIE(bad_field_spec_length, field_name, type_name,
        expected, ava_list_length(spec));
}

static void ava_struct_parse_int_field(
  ava_struct_field* dst, ava_list_value spec
) {
  AVA_STATIC_STRING(ava_integer_name, "ava-integer");

  ava_string size_str, byte_order_str;
  ava_integer sign_extend, is_atomic, alignment;

  dst->type = ava_sft_int;

  ava_struct_check_spec_length(
    dst->name, AVA_ASCII9_STRING("int"), spec, 7);

  size_str = ava_to_string(ava_list_index(spec, 1));
  sign_extend = ava_integer_of_value(ava_list_index(spec, 2), 0);
  is_atomic = ava_integer_of_value(ava_list_index(spec, 3), 0);
  alignment = ava_integer_of_value(ava_list_index(spec, 4), 0);
  byte_order_str = ava_to_string(ava_list_index(spec, 5));

  switch (ava_string_to_ascii9(size_str)) {
  default:
    if (ava_string_equal(ava_integer_name, size_str))
      dst->v.vint.size = ava_sis_ava_integer;
    else
      DIE(bad_field_spec_element, dst->name, AVA_ASCII9_STRING("size"),
          ava_list_index(spec, 1));
    break;

#define CASE(sym,...)                           \
  case AVA_ASCII9(__VA_ARGS__):                 \
    dst->v.vint.size = ava_sis_##sym;           \
    break

    /* 123456789 */
  CASE(word     , 'w','o','r','d');
  CASE(byte     , 'b','y','t','e');
  CASE(short    , 's','h','o','r','t');
  CASE(int      , 'i','n','t');
  CASE(long     , 'l','o','n','g');
  CASE(c_short  , 'c','-','s','h','o','r','t');
  CASE(c_int    , 'c','-','i','n','t');
  CASE(c_long   , 'c','-','l','o','n','g');
  CASE(c_llong  , 'c','-','l','l','o','n','g');
  CASE(c_size   , 'c','-','s','i','z','e');
  CASE(c_intptr , 'c','-','i','n','t','p','t','r');

#undef CASE
  }

  dst->v.vint.sign_extend = !!sign_extend;
  dst->v.vint.is_atomic = !!is_atomic;
  ava_struct_check_range(dst->name, "alignment", alignment, 0, 16);
  dst->v.vint.alignment = alignment;
  dst->v.vint.byte_order = ava_struct_parse_byte_order(
    dst->name, byte_order_str);

  if (dst->v.vint.is_atomic &&
      ((AVA_STRUCT_NATURAL_ALIGNMENT != dst->v.vint.alignment &&
        AVA_STRUCT_NATIVE_ALIGNMENT != dst->v.vint.alignment) ||
       (ava_sbo_preferred != dst->v.vint.byte_order &&
        ava_sbo_native != dst->v.vint.byte_order) ||
       ava_sis_word != dst->v.vint.size))
    DIE(nonnative_atomic, dst->name);
}

static ava_value ava_struct_stringify_int_field(const ava_struct_field* field) {
  AVA_STATIC_STRING(ava_integer_name, "ava-integer");

  ava_value vals[7];

  vals[0] = ava_value_of_string(AVA_ASCII9_STRING("int"));
  switch (field->v.vint.size) {
#define CASE(sym,name)                                          \
  case ava_sis_##sym:                                           \
    vals[1] = ava_value_of_string(AVA_ASCII9_STRING(#name));    \
    break

  case ava_sis_ava_integer:
    vals[1] = ava_value_of_string(ava_integer_name);
    break;

  CASE(word,word);
  CASE(byte,byte);
  CASE(short,short);
  CASE(int,int);
  CASE(long,long);
  CASE(c_short,c-short);
  CASE(c_int,c-int);
  CASE(c_long,c-long);
  CASE(c_llong,c-llong);
  CASE(c_size,c-size);
  CASE(c_intptr,c-intptr);
#undef CASE

  default: abort();
  }

  vals[2] = ava_value_of_integer(field->v.vint.sign_extend);
  vals[3] = ava_value_of_integer(field->v.vint.is_atomic);
  vals[4] = ava_value_of_integer(field->v.vint.alignment);
  vals[5] = ava_struct_stringify_byte_order(field->v.vint.byte_order);
  vals[6] = ava_value_of_string(field->name);

  return ava_list_of_values(vals, ava_lenof(vals)).v;
}

static void ava_struct_parse_real_field(
  ava_struct_field* dst, ava_list_value spec
) {
  ava_string size_str, byte_order_str;
  ava_integer alignment;

  dst->type = ava_sft_real;

  ava_struct_check_spec_length(
    dst->name, AVA_ASCII9_STRING("real"), spec, 5);

  size_str = ava_to_string(ava_list_index(spec, 1));
  alignment = ava_integer_of_value(ava_list_index(spec, 2), 0);
  byte_order_str = ava_to_string(ava_list_index(spec, 3));

  switch (ava_string_to_ascii9(size_str)) {
#define CASE(sym,...)                           \
  case AVA_ASCII9(__VA_ARGS__):                 \
    dst->v.vreal.size = ava_srs_##sym;          \
    break

    /* 123456789 */
  CASE(ava_real , 'a','v','a','-','r','e','a','l');
  CASE(single   , 's','i','n','g','l','e');
  CASE(double   , 'd','o','u','b','l','e');
  CASE(extended , 'e','x','t','e','n','d','e','d');
#undef CASE

  default:
    DIE(bad_field_spec_element, dst->name, AVA_ASCII9_STRING("size"),
        ava_list_index(spec, 1));
  }

  ava_struct_check_range(dst->name, "alignment", alignment, 0, 16);
  dst->v.vreal.alignment = alignment;
  dst->v.vreal.byte_order = ava_struct_parse_byte_order(
    dst->name, byte_order_str);
}

static ava_value ava_struct_stringify_real_field(
  const ava_struct_field* field
) {
  ava_value vals[5];

  vals[0] = ava_value_of_string(AVA_ASCII9_STRING("real"));
  switch (field->v.vreal.size) {
#define CASE(sym,name)                                          \
  case ava_srs_##sym:                                           \
    vals[1] = ava_value_of_string(AVA_ASCII9_STRING(#name));    \
    break

  CASE(ava_real, ava-real);
  CASE(single, single);
  CASE(double, double);
  CASE(extended, extended);

#undef CASE

  default: abort();
  }

  vals[2] = ava_value_of_integer(field->v.vreal.alignment);
  vals[3] = ava_struct_stringify_byte_order(field->v.vreal.byte_order);
  vals[4] = ava_value_of_string(field->name);

  return ava_list_of_values(vals, ava_lenof(vals)).v;
}

static void ava_struct_parse_ptr_field(
  ava_struct_field* dst, ava_list_value spec
) {
  ava_struct_check_spec_length(
    dst->name, AVA_ASCII9_STRING("ptr"), spec, 4);

  dst->type = ava_sft_ptr;
  dst->v.vptr.prot = ava_pointer_prototype_parse(
    ava_to_string(ava_list_index(spec, 1)));
  dst->v.vptr.is_atomic = !!ava_integer_of_value(
    ava_list_index(spec, 2), 0);
}

static ava_value ava_struct_stringify_ptr_field(
  const ava_struct_field* field
) {
  ava_value vals[] = {
    ava_value_of_string(AVA_ASCII9_STRING("ptr")),
    ava_value_of_string(ava_pointer_prototype_to_string(
                          field->v.vptr.prot)),
    ava_value_of_integer(field->v.vptr.is_atomic),
    ava_value_of_string(field->name),
  };

  return ava_list_of_values(vals, ava_lenof(vals)).v;
}

static void ava_struct_parse_hybrid_field(
  ava_struct_field* dst, ava_list_value spec
) {
  ava_struct_check_spec_length(
    dst->name, AVA_ASCII9_STRING("hybrid"), spec, 3);

  dst->type = ava_sft_hybrid;
  dst->v.vptr.prot = ava_pointer_prototype_parse(
    ava_to_string(ava_list_index(spec, 1)));
  dst->v.vptr.is_atomic = ava_false;
}

static ava_value ava_struct_stringify_hybrid_field(
  const ava_struct_field* field
) {
  ava_value vals[] = {
    ava_value_of_string(AVA_ASCII9_STRING("hybrid")),
    ava_value_of_string(ava_pointer_prototype_to_string(
                          field->v.vptr.prot)),
    ava_value_of_string(field->name),
  };

  return ava_list_of_values(vals, ava_lenof(vals)).v;
}

static void ava_struct_parse_value_field(
  ava_struct_field* dst, ava_list_value spec
) {
  ava_struct_check_spec_length(
    dst->name, AVA_ASCII9_STRING("value"), spec, 2);

  dst->type = ava_sft_value;
}

static ava_value ava_struct_stringify_value_field(
  const ava_struct_field* field
) {
  ava_value vals[] = {
    ava_value_of_string(AVA_ASCII9_STRING("value")),
    ava_value_of_string(field->name),
  };

  return ava_list_of_values(vals, ava_lenof(vals)).v;
}

static void ava_struct_parse_compose_field(
  ava_struct_field* dst, ava_list_value spec
) {
  ava_struct_check_spec_length(
    dst->name, AVA_ASCII9_STRING("compose"), spec, 3);

  dst->type = ava_sft_compose;
  dst->v.vcompose.member = ava_struct_of_value(
    ava_list_index(spec, 1));
  dst->v.vcompose.array_length = 1;
}

static ava_value ava_struct_stringify_compose_field(
  const ava_struct_field* field
) {
  ava_value vals[] = {
    ava_value_of_string(AVA_ASCII9_STRING("compose")),
    ava_value_of_struct(field->v.vcompose.member),
    ava_value_of_string(field->name),
  };

  return ava_list_of_values(vals, ava_lenof(vals)).v;
}

static void ava_struct_parse_array_field(
  ava_struct_field* dst, ava_list_value spec
) {
  ava_integer length;

  ava_struct_check_spec_length(
    dst->name, AVA_ASCII9_STRING("array"), spec, 4);

  dst->type = ava_sft_array;
  dst->v.vcompose.member = ava_struct_of_value(
    ava_list_index(spec, 1));
  length = ava_integer_of_value(ava_list_index(spec, 2), 0);
  if (length < 0 || length != (ava_integer)(size_t)length)
    DIE(bad_field_spec_element, dst->name, AVA_ASCII9_STRING("length"),
        ava_list_index(spec, 2));
  dst->v.vcompose.array_length = length;
}

static ava_value ava_struct_stringify_array_field(
  const ava_struct_field* field
) {
  ava_value vals[] = {
    ava_value_of_string(AVA_ASCII9_STRING("array")),
    ava_value_of_struct(field->v.vcompose.member),
    ava_value_of_integer(field->v.vcompose.array_length),
    ava_value_of_string(field->name),
  };

  return ava_list_of_values(vals, ava_lenof(vals)).v;
}

static void ava_struct_parse_tail_field(
  ava_struct_field* dst, ava_list_value spec
) {
  ava_struct_check_spec_length(
    dst->name, AVA_ASCII9_STRING("tail"), spec, 3);

  dst->type = ava_sft_tail;
  dst->v.vcompose.member = ava_struct_of_value(
    ava_list_index(spec, 1));
  dst->v.vcompose.array_length = 0;
}

static ava_value ava_struct_stringify_tail_field(
  const ava_struct_field* field
) {
  ava_value vals[] = {
    ava_value_of_string(AVA_ASCII9_STRING("tail")),
    ava_value_of_struct(field->v.vcompose.member),
    ava_value_of_string(field->name),
  };

  return ava_list_of_values(vals, ava_lenof(vals)).v;
}

static ava_struct_byte_order ava_struct_parse_byte_order(
  ava_string field_name, ava_string byte_order
) {
  AVA_STATIC_STRING(byte_order_name, "byte-order");

  switch (ava_string_to_ascii9(byte_order)) {
  case AVA_ASCII9('p','r','e','f','e','r','r','e','d'):
    return ava_sbo_preferred;

  case AVA_ASCII9('l','i','t','t','l','e'):
    return ava_sbo_little;

  case AVA_ASCII9('b','i','g'):
    return ava_sbo_big;

  case AVA_ASCII9('n','a','t','i','v','e'):
    return ava_sbo_native;

  default:
    DIE(bad_field_spec_element, field_name, byte_order_name,
        ava_value_of_string(byte_order));
  }
}

static ava_value ava_struct_stringify_byte_order(
  ava_struct_byte_order order
) {
  switch (order) {
#define CASE(c)                                                         \
    case ava_sbo_##c: return ava_value_of_string(AVA_ASCII9_STRING(#c));
  CASE(preferred);
  CASE(little);
  CASE(big);
  CASE(native);
#undef CASE
  }

  /* unreachable */
  abort();
}

static void ava_struct_check_range(
  ava_string field_name, const char* elt_name, ava_integer val,
  ava_integer min_inc, ava_integer max_ex
) {
  if (val < min_inc || val >= max_ex)
    DIE(bad_field_spec_element, field_name, ava_string_of_cstring(elt_name),
        ava_value_of_integer(val));
}

static void ava_struct_check_composition(ava_struct* this) {
  size_t i;

  if (this->parent && !this->parent->is_composable)
    DIE(extends_non_composable, this->name, this->parent->name);

  for (i = 0; i < this->num_fields; ++i) {
    if ((ava_sft_compose == this->fields[i].type ||
         ava_sft_array == this->fields[i].type ||
         ava_sft_tail == this->fields[i].type) &&
        !this->fields[i].v.vcompose.member->is_composable)
      DIE(composes_non_composable, this->name, this->fields[i].name);

    if (i + 1 < this->num_fields && ava_sft_tail == this->fields[i].type)
      DIE(var_length_field_not_at_end, this->name, this->fields[i].name);
  }

  this->is_composable =
    this->num_fields == 0 ||
    ava_sft_tail != this->fields[this->num_fields-1].type;
}

static void ava_struct_lay_out(ava_struct* this) {
  size_t offset, field_alignment, field_offset, field_size, i;
  size_t native_align, natural_align;

  offset = this->parent? this->parent->size : 0;
  this->size = offset;
  this->alignment = this->parent? this->parent->alignment : 1;

  for (i = 0; i < this->num_fields; ++i) {
    switch (this->fields[i].type) {
    case ava_sft_int: {
      switch (this->fields[i].v.vint.size) {
#define CASE(sym,type)                          \
      case ava_sis_##sym:                       \
        field_size = sizeof(type);              \
        native_align = ava_alignof(type);       \
        natural_align = field_size;             \
        break
      CASE(ava_integer, ava_integer);
      CASE(word, AO_t);
      CASE(byte, ava_ubyte);
      CASE(short, ava_ushort);
      CASE(int, ava_uint);
      CASE(long, ava_ulong);
      CASE(c_short, short);
      CASE(c_int, int);
      CASE(c_long, long);
      CASE(c_llong, long long);
      CASE(c_size, size_t);
      CASE(c_intptr, ava_intptr);
#undef CASE
      }

      if (AVA_STRUCT_NATURAL_ALIGNMENT == this->fields[i].v.vint.alignment)
        field_alignment = natural_align;
      else if (AVA_STRUCT_NATIVE_ALIGNMENT == this->fields[i].v.vint.alignment)
        field_alignment = native_align;
      else
        field_alignment = (size_t)1 << this->fields[i].v.vint.alignment;
    } break;

    case ava_sft_real: {
      switch (this->fields[i].v.vreal.size) {
#define CASE(sym,type)                          \
      case ava_srs_##sym:                       \
        field_size = sizeof(type);              \
        native_align = ava_alignof(type);       \
        natural_align = 4;                      \
        break

      CASE(ava_real, ava_real);
      CASE(single, float);
      CASE(double, double);
      CASE(extended, long double);
#undef CASE
      }

      if (AVA_STRUCT_NATURAL_ALIGNMENT == this->fields[i].v.vreal.alignment)
        field_alignment = natural_align;
      else if (AVA_STRUCT_NATIVE_ALIGNMENT == this->fields[i].v.vreal.alignment)
        field_alignment = native_align;
      else
        field_alignment = (size_t)1 << this->fields[i].v.vreal.alignment;
    } break;

    case ava_sft_ptr: {
      field_size = sizeof(void*);
      field_alignment = ava_alignof(void*);
    } break;

    case ava_sft_hybrid: {
      field_size = sizeof(void*) > sizeof(ava_intptr)?
        sizeof(void*) : sizeof(ava_intptr);
      field_alignment = ava_alignof(void*) > ava_alignof(ava_intptr)?
        ava_alignof(void*) : ava_alignof(ava_intptr);
    } break;

    case ava_sft_value: {
      field_size = sizeof(ava_value);
      field_alignment = ava_alignof(ava_value);
    } break;

    case ava_sft_compose:
    case ava_sft_array:
    case ava_sft_tail: {
      field_size = this->fields[i].v.vcompose.member->size *
        this->fields[i].v.vcompose.array_length;
      field_alignment = this->fields[i].v.vcompose.member->alignment;
    } break;
    }

    if (field_alignment > this->alignment)
      this->alignment = field_alignment;

    field_offset = (offset + field_alignment - 1) /
      field_alignment * field_alignment;
    this->fields[i].offset = field_offset;

    if (field_offset + field_size > this->size)
      this->size = field_offset + field_size;

    if (!this->is_union)
      offset = field_offset + field_size;
  }

  this->size = (this->size + this->alignment - 1)
    / this->alignment * this->alignment;
}
