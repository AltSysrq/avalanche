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

#include "runtime/avalanche/string.h"
#include "runtime/avalanche/module-cache.h"

#define FOO AVA_ASCII9_STRING("foo")

defsuite(module_cache);

/* Dummy pcode objects, since the memory cache doesn't care.
 */
typedef struct ava_pcode_global_list_s {
  int dummy;
} ava_pcode_global_list;

static ava_pcode_global_list modules[4];

static ava_module_cache* caches[2];
static ava_module_cache_stack cache_stack;

defsetup {
  caches[0] = ava_memory_module_cache_new();
  caches[1] = ava_memory_module_cache_new();

  LIST_INIT(&cache_stack);
  LIST_INSERT_HEAD(&cache_stack, caches[1], next);
  LIST_INSERT_HEAD(&cache_stack, caches[0], next);
}

defteardown { }

deftest(empty_finds_nothing) {
  ava_string error;

  ck_assert_ptr_eq(NULL, ava_module_cache_get(&cache_stack, FOO, &error));
  ck_assert(!ava_string_is_present(error));
}

deftest(put_inserts_to_all_levels) {
  ava_string error;

  ava_module_cache_put(&cache_stack, FOO, modules + 0);
  ck_assert_ptr_eq(modules + 0,
                   ava_module_cache_get(&cache_stack, FOO, &error));
  ck_assert(!ava_string_is_present(error));

  ck_assert_ptr_eq(
    modules + 0, (*caches[0]->get)(caches[0], FOO, &error));
  ck_assert_ptr_eq(
    modules + 0, (*caches[1]->get)(caches[1], FOO, &error));
}

deftest(hit_on_level_0_doesnt_insert_into_1) {
  ava_string error;

  (*caches[0]->put)(caches[0], FOO, modules + 0);
  ck_assert_ptr_eq(modules + 0,
                   ava_module_cache_get(&cache_stack, FOO, &error));
  ck_assert(!ava_string_is_present(error));

  ck_assert_ptr_eq(NULL, (*caches[1]->get)(caches[1], FOO, &error));
}

deftest(hit_on_level_1_inserts_into_0) {
  ava_string error;

  (*caches[1]->put)(caches[1], FOO, modules + 0);
  ck_assert_ptr_eq(modules + 0,
                   ava_module_cache_get(&cache_stack, FOO, &error));
  ck_assert(!ava_string_is_present(error));

  ck_assert_ptr_eq(modules + 0,
                   (*caches[0]->get)(caches[0], FOO, &error));
}
