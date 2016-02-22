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
 * Declares the stack map and memory layout for the safepoint-preserved pointer
 * locals in a function, and sets the local heap for the function up.
 *
 * This file is used by #define'ing AVAFP_LOCALS to a sequence of
 * local-variable declarations as described below (without separation by
 * semicolons or commas), then #include'ing this header. This is typically done
 * immediately after all non-pointer or non-safepoint-preserved locals have
 * been declared in the normal C fashion.
 *
 * Each local-variable declaration is one of the AVADO_* macros as described in
 * decl-obj.h, though it doesn't make sense to use AVADO_HEADER or AVADO_BLOB,
 * and an error will occur if they are used. AVADO_INT is not useful at
 * top-level and will result in an error, but is supported when introduced
 * through AVADO_INCLUDE.
 *
 * The local variables defined via this mechanism will be accessible under a
 * true local variable named `M`.
 *
 * AVAFP_LOCALS is automatically #undeffed at the end of this include.
 *
 * For example, the following
 *
 *   void my_function(AVA_DEF_ARGS1(ava_stdval foo_)) {
 *   #define AVAFP_LOCALS \
 *     AVADO_STDVAL(foo) \
 *     AVADO_FLAG(RAWPTR_DEDUPLICABLE) AVADO_PTR(bin, char*, bar)
 *   #include <avalanche/fun-prologue.h>
 *     M.foo = foo_;
 *     (* code *)
 *   }
 *
 * expands to something equivalent to
 *
 *   void my_function(AVA_DEF_ARGS1(ava_stdval foo_)) {
 *     static const ava_immediate_physical_type _ava_stack_map_layout[] = {
 *       ava_iptt_stdval,
 *       AVA_IPT_RAWPTR_DEDUPLICABLE | ava_iptt_ptr_bin,
 *       0
 *     };
 *     struct {
 *       ava_stack_map _ava_header;
 *       ava_stdval foo;
 *       char* bar;
 *     } M;
 *     ava_gc_push_local_heap(&M, &_ava_stack_map_layout,
 *                            _ava_tagged_caller_stack_map);
 *     M.foo = foo_;
 *     (* code *)
 *   }
 */

#ifndef AVAFP_LOCALS
#error "Forgot to #define AVAFP_LOCALS"
#endif

static const ava_immediate_physical_type _ava_stack_map_layout[] = {
#define AVADO_STDVAL(name) ava_iptt_stdval,
#define AVADO_PTR(type,ctype,name) ava_iptt_ptr_##type,
#define AVADO_INT(type,name) (sizeof(type) == sizeof(ava_dword)?        \
                              ava_iptt_dword :                          \
                              sizeof(type) == sizeof(ava_qword)?        \
                              ava_ipt_qword :                           \
                              ava_do_error_int_type_is_not_32_or_64_bits_wide),
#define AVADO_INCLUDE(type,name) type_##DEF
#define AVADO_FLAG(flag) AVA_IPT_##flag |
  AVAFP_LOCALS
#undef AVADO_FLAG
#undef AVADO_INCLUDE
#undef AVADO_INT
#undef AVADO_PTR
#undef AVADO_STDVAL
};

struct {
  ava_stack_map _ava_header;
#define AVADO_STDVAL(name) ava_stdval name;
#define AVADO_PTR(type,ctype,name) ctype name;
#define AVADO_INCLUDE(type,name) struct type##_s name;
#define AVADO_FLAG(flag)
  AVAFP_LOCALS
#undef AVADO_FLAG
#undef AVADO_INCLUDE
#undef AVADO_PTR
#undef AVADO_STDVAL
} M;

M._ava_header.layout = &_ava_stack_map_layout;
M._ava_header.parent = _ava_tagged_caller_stack_map;
M._ava_header.local_heap = NULL;
M._ava_header.parent_heap = NULL;

#undef AVAFP_LOCALS
