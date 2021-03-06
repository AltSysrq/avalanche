/*-
 * Copyright (c) 2015 Jason Lingle
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
#include <string.h>
#include <stdlib.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "-esba.h"
#include "-array-list.h"
#include "-esba-list.h"

#include "esba-list-swizzle.inc"

static const ava_attribute_tag ava_esba_list_header_tag = {
  .name = "esba-list-header"
};

/**
 * Structure used as next_attr on an ESBA.
 *
 * To optimise for the common case of one or more ava_value fields being
 * monomorphic, we store a template at the head of the list (which contains the
 * full zeroth value); if all values in the list share a field value with the
 * template, that field is not actually added to the ESBA.
 */
typedef struct {
  ava_attribute header;
  /**
   * An intex into the arrays defined in esba-list-swizzle.inc, indicating the
   * format of this list.
   */
  unsigned format;
  /**
   * The zeroth value in this list, fully expanded.
   */
  ava_value template;
} ava_esba_list_header;

/* For clarity, when dealing with "x many pointers big" */
typedef struct { void* v; } pointer;

static unsigned ava_esba_list_polymorphism(
  ava_value template, ava_value new);

static const ava_esba_list_header* ava_esba_list_header_of(ava_esba esba);
static ava_esba ava_esba_list_create_esba(
  unsigned format, ava_value template,
  size_t capacity);
static ava_esba ava_esba_list_create_esba_with_header(
  const ava_esba_list_header* header, size_t capacity);
static unsigned ava_esba_list_accum_format(
  ava_list_value list, size_t begin, size_t end,
  ava_value template);
static ava_esba ava_esba_list_append_sublist(
  ava_esba esba, unsigned format,
  ava_list_value list, size_t begin, size_t end);
static ava_esba ava_esba_list_concat_esbas(
  ava_esba dst, ava_esba src, size_t begin, size_t end);
static ava_esba ava_esba_list_make_compatible(
  ava_esba esba, unsigned new_format);


static const ava_value_trait ava_esba_list_generic_impl = {
  .header = { .tag = &ava_value_trait_tag, .next = NULL },
  .name = "esba-list",
  .to_string = ava_string_of_chunk_iterator,
  .string_chunk_iterator = ava_list_string_chunk_iterator,
  .iterate_string_chunk = ava_list_iterate_string_chunk,
};

AVA_LIST_DEFIMPL(ava_esba_list, &ava_esba_list_generic_impl)

static inline ava_esba to_esba(ava_value val) {
  return (ava_esba) {
    .handle = (void*)ava_value_attr(val),
    .length = ava_value_ulong(val)
  };
}

static inline ava_value to_value(ava_esba esba) {
  return ava_value_with_ulong(esba.handle, esba.length);
}

static inline ava_list_value to_list_value(ava_esba esba) {
  return (ava_list_value) { to_value(esba) };
}

static unsigned ava_esba_list_polymorphism(ava_value template, ava_value new) {
  unsigned ret = 0;
  if (ava_value_attr(template) != ava_value_attr(new))
    ret |= POLYMORPH_ATTR;
  if (ava_value_ulong(template) != ava_value_ulong(new))
    ret |= POLYMORPH_ULONG;

  return ret;
}

static const ava_esba_list_header* ava_esba_list_header_of(ava_esba esba) {
  return ava_esba_next_attr(esba);
}

static ava_esba ava_esba_list_create_esba(
  unsigned format, ava_value template, size_t capacity
) {
  ava_esba_list_header* header = AVA_NEW(ava_esba_list_header);
  header->header.tag = &ava_esba_list_header_tag;
  header->header.next = (const ava_attribute*)&ava_esba_list_list_impl;
  header->format = format;
  header->template = template;

  return ava_esba_list_create_esba_with_header(header, capacity);
}

static ava_esba ava_esba_list_create_esba_with_header(
  const ava_esba_list_header* header, size_t capacity
) {
  return ava_esba_new(
    sizeof(pointer) * ava_esba_list_element_size_pointers[header->format],
    capacity,
    ava_alloc_precise, (void*)header);
}

ava_list_value ava_esba_list_copy_of(
  ava_list_value list, size_t begin, size_t end
) {
  unsigned format;
  ava_value template;

  assert(end != begin);

  /* First pass through the range to determine the format */
  template = ava_list_index(list, begin);
  format = ava_esba_list_accum_format(list, begin, end, template);

  /* Second pass to populate the array */
  ava_esba esba = ava_esba_list_create_esba(format, template, end - begin);
  esba = ava_esba_list_append_sublist(esba, format, list, begin, end);

  return to_list_value(esba);
}

static unsigned ava_esba_list_accum_format(
  ava_list_value list, size_t begin, size_t end, ava_value template
) {
  const ava_esba_list_header*restrict header;
  unsigned format;
  size_t i;

  if ((header = ava_get_attribute(list.v, &ava_esba_list_header_tag))) {
    return header->format |
      ava_esba_list_polymorphism(template, header->template);
  }

  format = 0;
  for (i = begin + 1; i < end && format != POLYMORPH_ALL; ++i) {
    format |= ava_esba_list_polymorphism(
      template, ava_list_index(list, i));
  }

  return format;
}

static ava_esba ava_esba_list_append_sublist(
  ava_esba esba, unsigned format,
  ava_list_value list, size_t begin, size_t end
) {
  size_t i;
  pointer*restrict dst;

  if (ava_get_attribute(list.v, &ava_esba_list_header_tag))
    return ava_esba_list_concat_esbas(esba, to_esba(list.v), begin, end);

  dst = ava_esba_start_append(&esba, end - begin);

  for (i = begin; i < end; ++i) {
    ava_value val = ava_list_index(list, i);
    ava_esba_list_swizzle_down[format](dst, &val);
    dst += ava_esba_list_element_size_pointers[format];
  }

  ava_esba_finish_append(esba, end - begin);

  return esba;
}

static ava_esba ava_esba_list_concat_esbas(ava_esba dst_esba, ava_esba src_esba,
                                           size_t begin, size_t end) {
  const ava_esba_list_header*restrict dst_header =
    ava_esba_list_header_of(dst_esba);
  const ava_esba_list_header*restrict src_header =
    ava_esba_list_header_of(src_esba);
  size_t dst_eltsz = ava_esba_list_element_size_pointers[dst_header->format];
  size_t src_eltsz = ava_esba_list_element_size_pointers[src_header->format];
  ava_esba_list_swizzle_down_f dst_swizzle =
    ava_esba_list_swizzle_down[dst_header->format];
  ava_esba_list_swizzle_up_f src_swizzle =
    ava_esba_list_swizzle_up[src_header->format];
  ava_value value;
  const pointer*restrict src_base, *restrict src;
  pointer*restrict dst_base, *restrict dst;
  ava_esba_tx tx;
  size_t i;

  dst_base = ava_esba_start_append(&dst_esba, end - begin);

  do {
    dst = dst_base;
    src_base = ava_esba_access(src_esba, &tx);
    src = src_base + src_eltsz * begin;

    if (dst_header->format == src_header->format) {
      memcpy(dst, src, (end - begin) * dst_eltsz * sizeof(pointer));
    } else {
      for (i = begin; i < end; ++i) {
        src_swizzle(&value, &src_header->template, src);
        dst_swizzle(dst, &value);
        src += src_eltsz;
        dst += dst_eltsz;
      }
    }
  } while (!ava_esba_check_access(src_esba, src_base, tx));

  ava_esba_finish_append(dst_esba, end - begin);

  return dst_esba;
}

ava_list_value ava_esba_list_of_raw(
  const ava_value*restrict array,
  size_t length
) {
  return ava_esba_list_of_raw_strided(array, length, 1);
}

ava_list_value ava_esba_list_of_raw_strided(
  const ava_value*restrict array,
  size_t count, size_t stride
) {
  unsigned format;
  ava_value template;
  size_t i;

  assert(count > 0);

  /* First pass to determine format */
  format = 0;
  template = array[0];
  for (i = 1; i < count && format != POLYMORPH_ALL; ++i)
    format |= ava_esba_list_polymorphism(template, array[i*stride]);

  /* Second pass to copy data */
  ava_esba esba = ava_esba_list_create_esba(format, template, count);
  pointer* dst = ava_esba_start_append(&esba, count);
  for (i = 0; i < count; ++i) {
    ava_esba_list_swizzle_down[format](dst, array + i * stride);
    dst += ava_esba_list_element_size_pointers[format];
  }

  ava_esba_finish_append(esba, count);

  return to_list_value(esba);
}

static size_t ava_esba_list_list_length(ava_list_value list) {
  return ava_esba_length(to_esba(list.v));
}

static ava_value ava_esba_list_list_index(ava_list_value list, size_t ix) {
  ava_esba esba = to_esba(list.v);
  const ava_esba_list_header* header = ava_esba_list_header_of(esba);
  ava_esba_tx tx;
  const pointer* base, * src;
  ava_value ret;

  assert(ix < ava_esba_length(esba));

  do {
    base = ava_esba_access(esba, &tx);
    src = base + ix * ava_esba_list_element_size_pointers[header->format];
    ava_esba_list_swizzle_up[header->format](&ret, &header->template, src);
  } while (!ava_esba_check_access(esba, base, tx));

  return ret;
}

static ava_list_value ava_esba_list_list_slice(ava_list_value list,
                                               size_t begin, size_t end) {
  assert(begin <= end);
  assert(end <= ava_esba_length(to_esba(list.v)));

  if (begin == end)
    return ava_empty_list();

  if (0 == begin && ava_esba_list_list_length(list) == end)
    return list;

  if (end - begin < AVA_ARRAY_LIST_THRESH / 2)
    return ava_array_list_copy_of(list, begin, end);

  return ava_esba_list_copy_of(list, begin, end);
}

static ava_list_value ava_esba_list_list_append(ava_list_value list,
                                                ava_value elt) {
  ava_esba esba;
  const ava_esba_list_header*restrict header;

  esba = to_esba(list.v);
  header = ava_esba_list_header_of(esba);
  esba = ava_esba_list_make_compatible(
    esba, ava_esba_list_polymorphism(header->template, elt));
  header = ava_esba_list_header_of(esba);

  pointer swizzled[ava_esba_list_element_size_pointers[header->format]];
  ava_esba_list_swizzle_down[header->format](swizzled, &elt);

  return to_list_value(ava_esba_append(esba, swizzled, 1));
}

static ava_esba ava_esba_list_make_compatible(ava_esba src_esba,
                                              unsigned new_format) {
  const ava_esba_list_header* src_header = ava_esba_list_header_of(src_esba);

  if (new_format == (new_format & src_header->format))
    return src_esba;

  ava_esba dst_esba = ava_esba_list_create_esba(
    new_format | src_header->format, src_header->template,
    ava_esba_length(src_esba));
  return ava_esba_list_concat_esbas(dst_esba, src_esba,
                                    0, ava_esba_length(src_esba));
}

static ava_list_value ava_esba_list_list_concat(ava_list_value list,
                                                ava_list_value other) {
  ava_esba esba = to_esba(list.v);
  const ava_esba_list_header*restrict header;

  size_t other_length = ava_list_length(other);

  header = ava_esba_list_header_of(esba);
  esba = ava_esba_list_make_compatible(
    esba, ava_esba_list_accum_format(other, 0, other_length,
                                     header->template));

  header = ava_esba_list_header_of(esba);
  esba = ava_esba_list_append_sublist(esba, header->format,
                                      other, 0, other_length);

  return to_list_value(esba);
}

static ava_list_value ava_esba_list_list_remove(
  ava_list_value list, size_t begin, size_t end
) {
  ava_esba src_esba = to_esba(list.v);
  const ava_esba_list_header*restrict header =
    ava_esba_list_header_of(src_esba);
  size_t length = ava_esba_length(src_esba);

  assert(begin <= end);
  assert(end <= length);

  if (0 == begin && length == end)
    return ava_empty_list();

  if (begin == end)
    return list;

  ava_esba dst_esba = ava_esba_list_create_esba_with_header(
    header, length - (end - begin));
  dst_esba = ava_esba_list_concat_esbas(dst_esba, src_esba, 0, begin);
  dst_esba = ava_esba_list_concat_esbas(dst_esba, src_esba, end, length);

  return to_list_value(dst_esba);
}

static ava_list_value ava_esba_list_list_set(
  ava_list_value list, size_t index, ava_value value
) {
  ava_esba esba = to_esba(list.v);
  const ava_esba_list_header*restrict header;

  header = ava_esba_list_header_of(esba);
  esba = ava_esba_list_make_compatible(
    esba, ava_esba_list_polymorphism(header->template, value));

  header = ava_esba_list_header_of(esba);

  pointer swizzled[ava_esba_list_element_size_pointers[header->format]];
  ava_esba_list_swizzle_down[header->format](swizzled, &value);
  esba = ava_esba_set(esba, index, swizzled);

  return to_list_value(esba);
}

size_t ava_esba_list_element_size(ava_value list) {
  ava_esba esba = to_esba(list);
  return ava_esba_list_element_size_pointers[
    ava_esba_list_header_of(esba)->format] * sizeof(pointer);
}
