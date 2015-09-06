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
#include <assert.h>

#define AVA__INTERNAL_INCLUDE 1
#define AVA_IN_POINTER_C
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/exception.h"
#include "avalanche/integer.h"
#include "avalanche/errors.h"
#include "avalanche/pointer.h"

static ava_pointer_value ava_pointer_of_list(ava_list_value list);
static void ava_pointer_check_compatible(
  const ava_pointer_prototype*restrict proto,
  ava_string expected);

/* Ensure that the GC can *never* see native pointers within an
 * ava_pointer_value, to make it more likely that programming errors assuming
 * it can see them are detected more easily.
 *
 * (While it _could_ see them in ava_pointer_values if we didn't obfuscate them
 * like this, it still would be unable to see them in, eg, the string
 * representation of a pointer, much less decomposed values.)
 */
static ava_ulong obfuscate(const void* ptr) {
  return ~(ava_ulong)(ava_intptr)ptr;
}
static void* deobfuscate(ava_ulong val) {
  return (void*)(ava_intptr)~val;
}

const ava_attribute_tag ava_pointer_prototype_tag = {
  .name = "pointer-prototype"
};
const ava_attribute_tag ava_pointer_trait_tag = {
  .name = "pointer"
};

static const ava_value_trait ava_pointer_generic_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "pointer",
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = ava_list_string_chunk_iterator,
  .iterate_string_chunk = ava_list_iterate_string_chunk,
};

AVA_LIST_DEFIMPL(ava_pointer, &ava_pointer_generic_impl)
AVA_POINTER_DEFIMPL(ava_pointer, &ava_pointer_list_impl)

const ava_pointer_prototype ava_pointer_proto_mut_void =
    AVA_INIT_POINTER_PROTOTYPE(AVA_EMPTY_STRING_INIT, ava_false);
const ava_pointer_prototype ava_pointer_proto_const_void =
  AVA_INIT_POINTER_PROTOTYPE(AVA_EMPTY_STRING_INIT, ava_true);

ava_pointer_value ava_pointer_value_of(ava_value value) {
  if (!ava_get_attribute(value, &ava_pointer_trait_tag))
    return ava_pointer_of_list(ava_list_value_of(value));
  else
    return (ava_pointer_value) { value };
}

ava_fat_pointer_value ava_fat_pointer_value_of(ava_value value) {
  const ava_pointer_trait* trait = AVA_GET_ATTRIBUTE(value, ava_pointer_trait);
  if (!trait) {
    value = ava_pointer_of_list(ava_list_value_of(value)).v;
    trait = AVA_GET_ATTRIBUTE(value, ava_pointer_trait);
    assert(trait);
  }

  return (ava_fat_pointer_value) { .v = trait, .c = { value } };
}

ava_pointer_value ava_pointer_of_proto(
  const ava_pointer_prototype*restrict prototype,
  const void*restrict ptr
) {
  return (ava_pointer_value) {
    ava_value_with_ulong(prototype, obfuscate(ptr))
  };
}

ava_pointer_value ava_pointer_of_list(ava_list_value list) {
  ava_intptr ptr;

  if (2 != ava_list_length(list))
    ava_throw_str(&ava_format_exception,
                  ava_error_list_of_non_two_length_as_pointer());
  ptr = (ava_intptr)ava_integer_of_value(ava_list_index(list, 1), 0);

  return ava_pointer_of_proto(
    ava_pointer_prototype_parse(
      ava_to_string(ava_list_index(list, 0))),
    (void*)ptr);
}

const ava_pointer_prototype* ava_pointer_prototype_parse(ava_string protostr) {
  ava_pointer_prototype*restrict prototype;
  ava_string tag;
  char constness;
  ava_bool is_const;

  if (ava_string_is_empty(protostr))
    ava_throw_str(&ava_format_exception, ava_error_bad_pointer_prototype());

  constness = ava_string_index(protostr, ava_string_length(protostr)-1);
  switch (constness) {
  case '*': is_const = ava_false; break;
  case '&': is_const = ava_true;  break;
  default:
    ava_throw_str(&ava_format_exception, ava_error_bad_pointer_prototype());
    /* unreachable */
  }

  tag = ava_string_slice(protostr, 0, ava_string_length(protostr) - 1);

  if (ava_string_is_empty(tag))
    return is_const?
      &ava_pointer_proto_const_void : &ava_pointer_proto_mut_void;

  prototype = AVA_NEW(ava_pointer_prototype);
  prototype->header.tag = &ava_pointer_prototype_tag;
  prototype->header.next = (const ava_attribute*)&ava_pointer_pointer_impl;
  prototype->tag = tag;
  prototype->is_const = is_const;
  return prototype;
}

#define PROTO(this) ((const ava_pointer_prototype*restrict)ava_value_attr(this))

static ava_bool ava_pointer_pointer_is_const(ava_pointer_value this) {
  return PROTO(this.v)->is_const;
}

static ava_string ava_pointer_pointer_get_tag(ava_pointer_value this) {
  return PROTO(this.v)->tag;
}

static ava_pointer_value ava_pointer_pointer_const_cast_to(
  ava_pointer_value this,
  ava_bool is_const
) {
  const ava_pointer_prototype*restrict old = PROTO(this.v);
  ava_pointer_prototype*restrict new;

  if (is_const == old->is_const)
    return this;

  if (ava_string_is_empty(old->tag))
    return ava_pointer_of_proto(is_const? &ava_pointer_proto_const_void :
                                &ava_pointer_proto_mut_void,
                                deobfuscate(ava_value_ulong(this.v)));

  new = ava_clone(old, sizeof(*old));
  new->is_const = is_const;
  return ava_pointer_of_proto(new, deobfuscate(ava_value_ulong(this.v)));
}

static ava_pointer_value ava_pointer_pointer_reinterpret_cast_to(
  ava_pointer_value this,
  ava_string tag
) {
  const ava_pointer_prototype*restrict old = PROTO(this.v);
  ava_pointer_prototype*restrict new;

  if (0 == ava_strcmp(tag, old->tag))
    return this;

  if (ava_string_is_empty(tag))
    return ava_pointer_of_proto(old->is_const? &ava_pointer_proto_const_void :
                                &ava_pointer_proto_mut_void,
                                deobfuscate(ava_value_ulong(this.v)));

  new = ava_clone(old, sizeof(*old));
  new->tag = tag;
  return ava_pointer_of_proto(new, deobfuscate(ava_value_ulong(this.v)));
}

static void* ava_pointer_pointer_get_mutable(ava_pointer_value this,
                                             ava_string require_tag) {
  AVA_STATIC_STRING(const_pointer, "const-pointer");
  const ava_pointer_prototype*restrict proto = PROTO(this.v);

  if (proto->is_const)
    ava_throw_uex(&ava_error_exception, const_pointer,
                  ava_error_bad_pointer_constness());

  ava_pointer_check_compatible(proto, require_tag);

  return deobfuscate(ava_value_ulong(this.v));
}

static const void* ava_pointer_pointer_get_const(ava_pointer_value this,
                                                 ava_string require_tag) {
  ava_pointer_check_compatible(PROTO(this.v), require_tag);
  return deobfuscate(ava_value_ulong(this.v));
}

static void ava_pointer_check_compatible(
  const ava_pointer_prototype*restrict proto,
  ava_string expected
) {
  AVA_STATIC_STRING(incompatible_pointer, "incompatible-pointer");

  if (ava_string_is_empty(expected) || ava_string_is_empty(proto->tag))
    return;

  if (0 == ava_strcmp(expected, proto->tag))
    return;

  ava_throw_uex(&ava_error_exception, incompatible_pointer,
                ava_error_bad_pointer_type(proto->tag, expected));
}

static size_t ava_pointer_list_length(ava_list_value this) {
  return 2;
}

static ava_value ava_pointer_list_index(ava_list_value this,
                                        size_t index) {
  const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                         '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  char buf[sizeof(void*)*2+1];
  unsigned i;
  ava_intptr v;

  assert(index < 2);

  if (0 == index) {
    return ava_value_of_string(
      ava_pointer_prototype_to_string(PROTO(this.v)));
  } else {
    v = (ava_intptr)deobfuscate(ava_value_ulong(this.v));

    if (!v)
      return ava_value_of_string(AVA_ASCII9_STRING("null"));

    buf[0] = 'x';
    for (i = sizeof(buf)-1; i > 0; --i) {
      buf[i] = hex[v & 0xF];
      v >>= 4;
    }

    return ava_value_of_string(ava_string_of_bytes(buf, sizeof(buf)));
  }
}

ava_string ava_pointer_prototype_to_string(
  const ava_pointer_prototype* prototype
) {
  return ava_string_concat(prototype->tag,
                           prototype->is_const?
                           AVA_ASCII9_STRING("&") : AVA_ASCII9_STRING("*"));
}

static ava_list_value ava_pointer_list_slice(ava_list_value this,
                                             size_t begin, size_t end) {
  return ava_list_copy_slice(this, begin, end);
}

static ava_list_value ava_pointer_list_append(ava_list_value this,
                                              ava_value val) {
  return ava_list_copy_append(this, val);
}

static ava_list_value ava_pointer_list_concat(ava_list_value this,
                                              ava_list_value that) {
  return ava_list_copy_concat(this, that);
}

static ava_list_value ava_pointer_list_remove(ava_list_value this,
                                              size_t begin, size_t end) {
  return ava_list_copy_remove(this, begin, end);
}

static ava_list_value ava_pointer_list_set(ava_list_value this,
                                           size_t ix, ava_value val) {
  return ava_list_copy_set(this, ix, val);
}
