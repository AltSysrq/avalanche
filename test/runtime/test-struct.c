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

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/pointer.h"
#include "runtime/avalanche/struct.h"
#include "runtime/avalanche/exception.h"
#include "runtime/-internal-defs.h"

defsuite(struct);

#define STR(x) _STR(x)
#define _STR(x) #x

#define STRUCT(args) "[struct " args "]"
#define UNION(args) "[union " args "]"
#define EXTENDS(parent) " [" parent "]"
#undef INT
#define INT(size,sext,atomic,align,bo,name)     \
  " [int " #size " " #sext " " #atomic " " STR(align) " " #bo " " #name "]"
#define REAL(size,align,bo,name)                \
  " [real " #size " " STR(align) " " #bo " " #name "]"
#define PTR(prot,atomic,name)                   \
  " [ptr " #prot " " #atomic " " #name "]"
#define HYBRID(prot,name)                       \
  " [hybrid " #prot " " #name "]"
#define VALUE(name)                             \
  " [value " #name "]"
#define COMPOSE(member,name)                    \
  " [compose [" member "] " #name "]"
#define ARRAY(member,length,name)               \
  " [array [" member "] " #length " " #name "]"
#define TAIL(member,name)                       \
  " [tail [" member "] " #name "]"

#define NATURAL AVA_STRUCT_NATURAL_ALIGNMENT
#define NATIVE AVA_STRUCT_NATIVE_ALIGNMENT

static const ava_struct* fromstr(const char* cstr) {
  ava_value input = ava_value_of_cstring(cstr);
  const ava_struct* ret = ava_struct_of_value(input);
  /* Ensure it stringifies to the same thing */
  assert_values_equal(input, ava_value_of_struct(ret));

  return ret;
}

static void try_to_struct(void* str) {
  ava_struct_of_value(ava_value_of_cstring(str));
}

static void assert_rejects(const char* expected, const char* cstr) {
  ava_exception caught;
  const char* message;

  ck_assert_msg(ava_catch(&caught, try_to_struct, (char*)cstr),
                "Struct accepted unexpectedly");
  message = ava_string_to_cstring(
    ava_to_string(ava_exception_get_value(&caught)));
  ck_assert_msg(strstr(message, expected),
                "Struct rejected with unexpected message: %s", message);
}

deftest(empty_struct) {
  const ava_struct* sxt = fromstr(STRUCT("empty"));

  ck_assert_int_eq(0, sxt->num_fields);
  ck_assert_int_eq(0, sxt->size);
  ck_assert_int_eq(1, sxt->alignment);
  ck_assert(sxt->is_composable);
  ck_assert(!sxt->is_union);
  ck_assert_str_eq("empty", ava_string_to_cstring(sxt->name));
}

deftest(empty_union) {
  const ava_struct* sxt = fromstr(UNION("empty"));

  ck_assert_int_eq(0, sxt->num_fields);
  ck_assert_int_eq(0, sxt->size);
  ck_assert_int_eq(1, sxt->alignment);
  ck_assert(sxt->is_composable);
  ck_assert(sxt->is_union);
  ck_assert_str_eq("empty", ava_string_to_cstring(sxt->name));
}

deftest(understands_all_int_sizes) {
  const ava_struct* sxt = fromstr(
    STRUCT("all-ints")
    INT(ava-integer,    0, 0, NATURAL, native, ava-integer)
    INT(word,           0, 0, NATURAL, native, word)
    INT(byte,           0, 0, NATURAL, native, byte)
    INT(short,          0, 0, NATURAL, native, short)
    INT(int,            0, 0, NATURAL, native, int)
    INT(long,           0, 0, NATURAL, native, long)
    INT(c-short,        0, 0, NATURAL, native, c-short)
    INT(c-int,          0, 0, NATURAL, native, c-int)
    INT(c-long,         0, 0, NATURAL, native, c-long)
    INT(c-llong,        0, 0, NATURAL, native, c-long-long)
    INT(c-size,         0, 0, NATURAL, native, c-size)
    INT(c-intptr,       0, 0, NATURAL, native, c-intptr));

#define CHECK(ix,dsize)                                                 \
  do {                                                                  \
    ck_assert_int_eq(ava_sft_int, sxt->fields[ix].type);                \
    ck_assert_int_eq(ava_sis_##dsize, sxt->fields[ix].v.vint.size);     \
  } while (0)

  ck_assert_int_eq(12, sxt->num_fields);
  CHECK(0, ava_integer);
  CHECK(1, word);
  CHECK(2, byte);
  CHECK(3, short);
  CHECK(4, int);
  CHECK(5, long);
  CHECK(6, c_short);
  CHECK(7, c_int);
  CHECK(8, c_long);
  CHECK(9, c_llong);
  CHECK(10, c_size);
  CHECK(11, c_intptr);
#undef CHECK
}

deftest(understands_all_real_sizes) {
  const ava_struct* sxt = fromstr(
    STRUCT("all-reals")
    REAL(ava-real,      NATURAL, native, ava-real)
    REAL(single,        NATURAL, native, single)
    REAL(double,        NATURAL, native, double)
    REAL(extended,      NATURAL, native, extended));

#define CHECK(ix,dsize)                                                 \
  do {                                                                  \
    ck_assert_int_eq(ava_sft_real, sxt->fields[ix].type);               \
    ck_assert_int_eq(ava_srs_##dsize, sxt->fields[ix].v.vreal.size);    \
  } while (0)

  ck_assert_int_eq(4, sxt->num_fields);
  CHECK(0, ava_real);
  CHECK(1, single);
  CHECK(2, double);
  CHECK(3, extended);
#undef CHECK
}

deftest(understands_pointer_types) {
  const ava_struct* sxt = fromstr(
    STRUCT("pointers")
    PTR(FILE*, 1, file)
    HYBRID(foo&, hybrid));

  ck_assert_int_eq(2, sxt->num_fields);

  ck_assert_int_eq(ava_sft_ptr, sxt->fields[0].type);
  ck_assert_int_eq(ava_true, sxt->fields[0].v.vptr.is_atomic);
  ck_assert(!sxt->fields[0].v.vptr.prot->is_const);
  ck_assert_str_eq("FILE", ava_string_to_cstring(
                     sxt->fields[0].v.vptr.prot->tag));

  ck_assert_int_eq(ava_sft_hybrid, sxt->fields[1].type);
  ck_assert_int_eq(ava_false, sxt->fields[1].v.vptr.is_atomic);
  ck_assert(sxt->fields[1].v.vptr.prot->is_const);
  ck_assert_str_eq("foo", ava_string_to_cstring(
                     sxt->fields[1].v.vptr.prot->tag));
}

deftest(understands_value_type) {
  const ava_struct* sxt = fromstr(
    STRUCT("value")
    VALUE(foo));

  ck_assert_int_eq(1, sxt->num_fields);
  ck_assert_int_eq(ava_sft_value, sxt->fields[0].type);
}

deftest(understands_composed_types) {
  const ava_struct* sxt = fromstr(
    STRUCT("composition")
    COMPOSE(STRUCT("foo") VALUE(foo), composed-foo)
    ARRAY(STRUCT("int") INT(int,0,0,NATURAL,preferred,val), 42, ints)
    TAIL(STRUCT("foo") VALUE(foo), tail-foo));

  ck_assert_int_eq(3, sxt->num_fields);

  ck_assert_int_eq(ava_sft_compose, sxt->fields[0].type);
  ck_assert_int_eq(1, sxt->fields[0].v.vcompose.array_length);
  ck_assert_int_eq(1, sxt->fields[0].v.vcompose.member->num_fields);

  ck_assert_int_eq(ava_sft_array, sxt->fields[1].type);
  ck_assert_int_eq(42, sxt->fields[1].v.vcompose.array_length);
  ck_assert_int_eq(1, sxt->fields[1].v.vcompose.member->num_fields);

  ck_assert_int_eq(ava_sft_tail, sxt->fields[2].type);
  ck_assert_int_eq(0, sxt->fields[2].v.vcompose.array_length);
  ck_assert_int_eq(1, sxt->fields[2].v.vcompose.member->num_fields);
}

deftest(int_christmas_tree) {
  const ava_struct* sxt = fromstr(
    STRUCT("int-christmas-tree")
    INT(word, 1, 1, NATIVE, native, atomic)
    INT(long, 1, 0, 10, preferred, large-align)
    INT(c-llong, 0, 0, NATIVE, little, little-endian)
    INT(c-llong, 0, 0, NATIVE, big, big-endian));

  ck_assert_int_eq(4, sxt->num_fields);
  ck_assert(sxt->fields[0].v.vint.sign_extend);
  ck_assert(sxt->fields[0].v.vint.is_atomic);
  ck_assert_int_eq(ava_sbo_native, sxt->fields[0].v.vint.byte_order);
  ck_assert(sxt->fields[1].v.vint.sign_extend);
  ck_assert(!sxt->fields[1].v.vint.is_atomic);
  ck_assert_int_eq(1024, sxt->alignment);
  ck_assert_int_eq(2048, sxt->size);
  ck_assert_int_eq(ava_sbo_preferred, sxt->fields[1].v.vint.byte_order);
  ck_assert_int_eq(ava_sbo_little, sxt->fields[2].v.vint.byte_order);
  ck_assert_int_eq(ava_sbo_big, sxt->fields[3].v.vint.byte_order);
}

deftest(real_christmas_tree) {
  const ava_struct* sxt = fromstr(
    STRUCT("real-christmas-tree")
    REAL(single, NATIVE, native, native)
    REAL(double, NATURAL, preferred, natural)
    REAL(extended, 10, little, large-aligned)
    REAL(ava-real, 0, big, big-endian));

  ck_assert_int_eq(4, sxt->num_fields);
  ck_assert_int_eq(ava_sbo_native, sxt->fields[0].v.vreal.byte_order);
  ck_assert_int_eq(ava_sbo_preferred, sxt->fields[1].v.vreal.byte_order);
  ck_assert_int_eq(ava_sbo_little, sxt->fields[2].v.vreal.byte_order);
  ck_assert_int_eq(1024, sxt->alignment);
  ck_assert_int_eq(2048, sxt->size);
  ck_assert_int_eq(ava_sbo_big, sxt->fields[3].v.vreal.byte_order);
}

deftest(primitive_struct_layout) {
  const ava_struct* sxt = fromstr(
    STRUCT("primitive-layout")
    INT(byte, 0, 0, NATURAL, preferred, byte)
    INT(int, 0, 0, NATURAL, preferred, dword)
    INT(byte, 0, 0, NATURAL, preferred, misalign)
    INT(int, 0, 0, 1, preferred, misaligned));

  ck_assert_int_eq(4, sxt->num_fields);
  ck_assert_int_eq(4, sxt->alignment);
  /* B---IIIIB-IIII-- */
  ck_assert_int_eq(16, sxt->size);
  ck_assert_int_eq(0, sxt->fields[0].offset);
  ck_assert_int_eq(4, sxt->fields[1].offset);
  ck_assert_int_eq(8, sxt->fields[2].offset);
  ck_assert_int_eq(10, sxt->fields[3].offset);
}

deftest(composed_struct_layout) {
  const ava_struct* sxt = fromstr(
    STRUCT("composed-struct")
    COMPOSE(STRUCT("byte-container")
            INT(byte, 0, 0, NATURAL, preferred, byte),
            byte-container)
    ARRAY(STRUCT("int-container")
          INT(int, 0, 0, NATURAL, preferred, int),
          42, int-container)
    TAIL(STRUCT("long-container")
         INT(long, 0, 0, NATURAL, preferred, long),
         long-container));

  ck_assert_int_eq(3, sxt->num_fields);
  ck_assert_int_eq(8, sxt->alignment);
  /* 1 byte, 3 padding, 42x4 int, 4 padding, tail */
  ck_assert_int_eq(176, sxt->size);

  ck_assert_int_eq(0, sxt->fields[0].offset);
  ck_assert_int_eq(4, sxt->fields[1].offset);
  ck_assert_int_eq(176, sxt->fields[2].offset);
}

deftest(layout_of_struct_extending_parent_of_lesser_alignment) {
  const ava_struct* sxt = fromstr(
    STRUCT("child" EXTENDS(
             STRUCT("parent")
             INT(short, 0, 0, NATURAL, preferred, short)))
    INT(int, 0, 0, NATURAL, preferred, int));

  ck_assert_int_eq(1, sxt->num_fields);
  /* Alignment of child's field dominates */
  ck_assert_int_eq(4, sxt->alignment);
  /* 2 byte int, 2 byte padding, 4 byte int */
  ck_assert_int_eq(8, sxt->size);
  ck_assert_int_eq(4, sxt->fields[0].offset);
}

deftest(layout_of_struct_extending_parent_of_greater_alignment) {
  const ava_struct* sxt = fromstr(
    STRUCT("child" EXTENDS(
             STRUCT("parent")
             INT(long, 0, 0, NATURAL, preferred, long)
             INT(short, 0, 0, NATURAL, preferred, short)))
    INT(int, 0, 0, NATURAL, preferred, int));

  ck_assert_int_eq(1, sxt->num_fields);
  /* Alignment of parent dominates */
  ck_assert_int_eq(8, sxt->alignment);
  /* 8 byte int, 2 byte int, 6 byte padding, 4 byte int, 4 byte padding */
  ck_assert_int_eq(24, sxt->size);
  ck_assert_int_eq(16, sxt->fields[0].offset);
}

deftest(union_layout) {
  const ava_struct* sxt = fromstr(
    UNION("union")
    INT(byte, 0, 0, NATURAL, preferred, byte)
    INT(long, 0, 0, NATURAL, preferred, largest-member)
    INT(byte, 0, 0, NATURAL, preferred, last));

  ck_assert_int_eq(3, sxt->num_fields);
  ck_assert_int_eq(8, sxt->size);
  ck_assert_int_eq(8, sxt->alignment);
  ck_assert_int_eq(0, sxt->fields[0].offset);
  ck_assert_int_eq(0, sxt->fields[1].offset);
  ck_assert_int_eq(0, sxt->fields[2].offset);
}

deftest(parse_empty_list) {
  assert_rejects("R0047", "");
}

deftest(header_too_short) {
  assert_rejects("R0048", "struct foo bar");
}

deftest(header_too_long) {
  assert_rejects("R0049", "[struct foo bar baz]");
}

deftest(bad_header_type) {
  assert_rejects("R0050", "[class foo]");
}

deftest(singleton_field_spec) {
  assert_rejects("R0051", STRUCT("foo") " [int]");
}

deftest(unknown_field_type) {
  assert_rejects("R0052", STRUCT("foo") " [foo bar]");
}

deftest(int_wrong_spec_length) {
  assert_rejects("R0053", STRUCT("foo") " [int name]");
}

deftest(real_wrong_spec_length) {
  assert_rejects("R0053", STRUCT("foo") " [real name]");
}

deftest(ptr_wrong_spec_length) {
  assert_rejects("R0053", STRUCT("foo") " [ptr name]");
}

deftest(hybrid_wrong_spec_length) {
  assert_rejects("R0053", STRUCT("foo") " [hybrid name]");
}

deftest(value_wrong_spec_length) {
  assert_rejects("R0053", STRUCT("foo") " [value xyzzy name]");
}

deftest(compose_wrong_spec_length) {
  assert_rejects("R0053", STRUCT("foo") " [compose name]");
}

deftest(array_wrong_spec_length) {
  assert_rejects("R0053", STRUCT("foo") " [array name]");
}

deftest(tail_wrong_spec_length) {
  assert_rejects("R0053", STRUCT("foo") " [tail name]");
}

deftest(int_bad_size) {
  assert_rejects("R0054", STRUCT("foo") INT(blah, 0, 0, NATIVE, natural, foo));
}

deftest(int_negative_align) {
  assert_rejects("R0054", STRUCT("foo") INT(int, 0, 0, -1, natural, foo));
}

deftest(int_too_big_align) {
  assert_rejects("R0054", STRUCT("foo") INT(int, 0, 0, 64, natural, foo));
}

deftest(bad_byte_order) {
  assert_rejects("R0054", STRUCT("foo") INT(int, 0, 0, NATURAL, pdp, foo));
}

deftest(real_bad_size) {
  assert_rejects("R0054", STRUCT("foo") REAL(blah, NATURAL, natural, foo));
}

deftest(real_negative_align) {
  assert_rejects("R0054", STRUCT("foo") REAL(single, -1, natural, foo));
}

deftest(real_too_big_align) {
  assert_rejects("R0054", STRUCT("foo") REAL(single, 64, natural, foo));
}

deftest(array_negative_length) {
  assert_rejects("R0054", STRUCT("foo")
                 ARRAY(STRUCT("bar"), -2, foo));
}

deftest(atomic_integer_with_non_word_size) {
  assert_rejects("R0055", STRUCT("foo") INT(byte, 0, 1, NATIVE, native, foo));
}

deftest(atomic_integer_with_unnatural_align) {
  assert_rejects("R0055", STRUCT("foo") INT(word, 0, 1, 1, native, foo));
}

deftest(atomic_integer_with_unnatural_byte_order) {
  assert_rejects("R0055", STRUCT("foo") INT(word, 0, 1, NATIVE, big, foo));
}

deftest(extend_tailed_struct) {
  assert_rejects("R0056", STRUCT("foo" EXTENDS(STRUCT("bar")
                                               TAIL(STRUCT("baz"), baz))));
}

deftest(compose_tailed_struct) {
  assert_rejects("R0057", STRUCT("foo")
                 COMPOSE(STRUCT("bar")
                         TAIL(STRUCT("baz"), bazzes), bars));
}

deftest(tail_not_at_end) {
  assert_rejects("R0058", STRUCT("foo")
                 TAIL(STRUCT("bar"), bars)
                 INT(word, 0, 0, NATIVE, native, quux));
}

deftest(extend_union_with_struct) {
  assert_rejects("R0059",
                 STRUCT("foo" EXTENDS(UNION("bar"))));
}

deftest(extend_struct_with_union) {
  assert_rejects("R0059",
                 UNION("foo" EXTENDS(STRUCT("bar"))));
}
