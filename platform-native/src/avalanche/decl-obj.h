/*-
 * Copyright (c) 2016, Jason Lingle
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

/**
 * @file
 *
 * Declares a C struct suitable for use as an ava_object allocation and an
 * accompanying memory layout table describing it. Declaring the
 * ava_object_type itself is the responsibility of the client code.
 *
 * This file is used by #define'ing AVADO_OBJECT_DECL as described below, then
 * #include'ing this header. AVADO_OBJECT_DECL will be #undeffed afterwards.
 * This header can be included any number of times.
 *
 * The AVADO_OBJECT_DECL begins with a use of an AVADO_BEGIN(NAME) macro, which
 * is not defined anywhere visible. The C struct will be named NAME_s. The
 * memory layout table (an array of ava_immediate_physical_type) will be named
 * NAME_memory_layout.
 *
 * The AVADO_BEGIN() use is followed by any number of struct fields.
 *
 * Next are field declarations, which are neither terminated by semicolons nor
 * separated by commas. The following field declarations are supported:
 *
 *   AVADO_HEADER
 *     Adds `ava_object header;` to the struct definition. This is not
 *     reflected in the memory layout; it is simply used to define structs
 *     which can be used as independently-allocated objects.
 *
 *   AVADO_STDVAL(NAME)
 *     Defines a field named NAME which is an ava_stdval.
 *
 *   AVADO_INT(TYPE,NAME)
 *     Defines a field containing uninterpreted data of the given C TYPE and
 *     named NAME. TYPE must describe a 32-bit or 64-bit-wide type.
 *
 *   AVADO_BLOB(FIELD)
 *     Defines a field which contains uninterpreted data up to the end of the
 *     containing structure. FIELD is a C field declaration. For example, this
 *     could be something like
 *       AVADO_BLOB(struct my_struct field_name)
 *     to describe an arbitrary structure, or
 *       AVADO_BLOB(unsigned char field_name[])
 *     to describe a variable-sized byte array.
 *
 *   AVADO_PTR(TYPE,CTYPE,NAME)
 *     Define raw pointer fields named NAME. CTYPE is a C pointer type, eg,
 *     foo*. TYPE is one of "obj", "str", "list", or "bin".
 *
 *   AVADO_INCLUDE(TYPE,NAME)
 *     Declare a C field like `struct TYPE_s NAME;`. Obtain the memory layout
 *     by substituting `NAME_DEF`, which is a series of AVADO_* field
 *     definitions (ie, without AVADO_BEGIN or AVADO_END).
 *
 * Any field other than AVADO_INCLUDE may be modified with any number of
 * AVA_IPT_* flags by placing AVADO_FLAG(FLAG) before the field declaration.
 * FLAG is the name of an AVA_IPT_* constant with the "AVA_IPT_" prefix
 * removed.
 *
 * After the field declarations is AVADO_END followed by a semicolon.
 *
 * For example, the following input
 *
 *   #define AVADO_OBJECT_DECL AVADO_BEGIN(my_object) \
 *     AVADO_HEADER \
 *     AVADO_STDVAL(foo) \
 *     AVADO_INT(int32_t, bar) \
 *     AVADO_FLAG(RAWPTR_DEDUPLICABLE) AVADO_PTR(bin, char*, str) \
 *   AVADO_END;
 *   #include <avalanche/decl-obj.h>
 *
 * expands to something equivalent to
 *
 *   struct my_object_s {
 *     ava_object header;
 *     ava_stdval foo;
 *     int32_t bar;
 *     char* str;
 *   };
 *   static const ava_immediate_physical_type my_object_memory_layout[] = {
 *     ava_iptt_stdval,
 *     ava_iptt_dword,
 *     AVA_IPT_RAWPTR_DEDUPLICABLE | ava_iptt_ptr_bin,
 *     0
 *   };
 *   #undef AVADO_OBJECT_DECL
 */

#ifndef AVADO_OBJECT_DECL
#error "Forgot to #define AVADO_OBJECT_DECL"
#endif

#define AVADO_BEGIN(name) struct name##_s {
#define AVADO_HEADER ava_object header;
#define AVADO_STDVAL(name) ava_stdval name;
#define AVADO_INT(type,name) type name;
#define AVADO_BLOB(field) field;
#define AVADO_PTR(type,ctype,name) ctype field;
#define AVADO_INCLUDE(type,name) struct type##_s name;
#define AVADO_FLAG(flag)
#define AVADO_END }
AVADO_OBJECT_DECL
#undef AVADO_END
#undef AVADO_FLAG
#undef AVADO_INCLUDE
#undef AVADO_PTR
#undef AVADO_BLOB
#undef AVADO_INT
#undef AVADO_STDVAL
#undef AVADO_HEADER
#undef AVADO_BEGIN

extern int ava_do_error_int_type_is_not_32_or_64_bits_wide;

#define AVADO_BEGIN(name) static const ava_immediate_physical_type      \
  name##_memory_layout[] = {
#define AVADO_HEADER
#define AVADO_STDVAL(name) ava_iptt_stdval,
#define AVADO_INT(type,name) (sizeof(type) == sizeof(ava_dword)?        \
                              ava_iptt_dword :                          \
                              sizeof(type) == sizeof(ava_qword)?        \
                              ava_iptt_qword :                          \
                              ava_do_error_int_type_is_not_32_or_64_bits_wide),
#define AVADO_BLOB(field) ava_iptt_blob,
#define AVADO_PTR(type,ctype,name) ava_iptt_ptr_##type,
#define AVADO_INCLUDE(type,name) type##_DEF
#define AVADO_FLAG(flag) AVA_IPT_##flag |
#define AVADO_END 0 }
AVADO_OBJECT_DECL
#undef AVADO_END
#undef AVADO_FLAG
#undef AVADO_INCLUDE
#undef AVADO_PTR
#undef AVADO_BLOB
#undef AVADO_INT
#undef AVADO_STDVAL
#undef AVADO_HEADER
#undef AVADO_BEGIN

#undef AVADO_OBJECT_DECL
