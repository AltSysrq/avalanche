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

#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/module-cache.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_MODULE_CACHE_H_
#define AVA_RUNTIME_MODULE_CACHE_H_

#include "string.h"

struct ava_pcode_global_list_s;

typedef struct ava_module_cache_s ava_module_cache;

/**
 * Retrieves an item from a module cache.
 *
 * @param cache The cache from which to retrieve the item.
 * @param name The name of the item.
 * @param error If the cache may have an item by the given name, but was not
 * able to load it, set to an error message describing why the cache failed.
 * Under normal operation, this field is untouched.
 * @return The P-Code object corresponding to the given name, or NULL if the
 * cache does not have an object by that name or if it may have such an object
 * but was unable to load it.
 */
typedef const struct ava_pcode_global_list_s* (*ava_module_cache_get_f)(
  const ava_module_cache* cache, ava_string name, ava_string* error);
/**
 * Inserts an item into a module cache.
 *
 * It is acceptable for this function to silently fail in any circumstances,
 * but if it fails, it must do so atomically.
 *
 * If the cache already contains an object of the given name, it may or may not
 * replace it with the new object.
 *
 * @param cache The cache into which to add the item.
 * @param name The name of the item.
 * @param pcode The P-Code object to add to the cache.
 */
typedef void (*ava_module_cache_put_f)(
  ava_module_cache* cache, ava_string name,
  const struct ava_pcode_global_list_s* pcode);

/**
 * Provides facilities for storing and retrieving already-compiled (i.e., to
 * P-Code) modules and packages. Typically the P-Code objects are interfaces
 * rather than implementations.
 *
 * Besides simply caching objects to speed compilation, module caches also
 * implement access to things like the package search path and packages
 * compiled into the runtime or host application.
 *
 * Any function pointers in this struct may be NULL to indicate that the
 * module cache does not support that operation.
 *
 * Note that while the interface and API for caching modules and packages is
 * the same, the two are distinct, in that one must have a cache stack for
 * modules and another cache stack for packages.
 */
struct ava_module_cache_s {
  ava_module_cache_get_f get;
  ava_module_cache_put_f put;

  LIST_ENTRY(ava_module_cache_s) next;
};

/**
 * A stack of module caches.
 *
 * Virtually all compilation scenarios involve a stack of more than one module
 * cache. The ava_module_cache_*() functions are designed to propagate cached
 * entities upward through the stack, so typical organisation (from top down)
 * may look like
 *
 * - Read-only intrinsics, such as things compiled into the runtime.
 *
 * - The memory-only cache.
 *
 * - Read-only disk caches (e.g., the package search path)
 *
 * - The writable disk cache, if any
 */
typedef LIST_HEAD(ava_module_cache_stack_s,ava_module_cache_s)
ava_module_cache_stack;

/**
 * Returns the first P-Code module in the given cache stack which matches the
 * given name.
 *
 * If such a module is found, it is added to every cache level above the one in
 * which it was located.
 *
 * @param cache The cache stack to search.
 * @param name The name of the module to find.
 * @param error If any cache encounters an error reading a P-Code module, set
 * to an error message describing the problem. Otherwise set to the absent
 * string.
 * @return The P-Code module with the given name, or NULL if no such module
 * could be found or if an error occurs loading a module.
 */
const struct ava_pcode_global_list_s*
ava_module_cache_get(const ava_module_cache_stack* cache,
                     ava_string name,
                     ava_string* error);

/**
 * Adds the given module to every level of the given cache stack.
 *
 * If a module of the given name already exists in the cache, whether later
 * calls to ava_module_cache_get() return this P-Code object or another is
 * unspecified.
 *
 * If the underlying cache(s) fail to store the object, later calls to
 * ava_module_cache_get() on the same name may return NULL. Such failures are
 * silent.
 *
 * @param cache The caches into which to add the module.
 * @param name The name of the module.
 * @param pcode The module contents.
 */
void ava_module_cache_put(ava_module_cache_stack* cache,
                          ava_string name,
                          const struct ava_pcode_global_list_s* pcode);

/**
 * Returns a module cache which stores the modules in an in-memory map.
 */
ava_module_cache* ava_memory_module_cache_new(void);

#endif /* AVA_RUNTIME_MODULE_CACHE_H_ */
