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
#error "Don't include avalanche/pcode-linker.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_PCODE_LINKER_H_
#define AVA_RUNTIME_PCODE_LINKER_H_

#include "string.h"

/**
 * @file
 *
 * Provides facilities for linking multiple P-Code modules into one, and for
 * reducing P-Code modules from implementations to interfaces.
 *
 * All functions in this file assume they are operating on already-validated
 * P-Code.
 */

struct ava_pcode_global_list_s;
struct ava_compile_error_list_s;

/**
 * Stores state used for linking multiple P-Code modules into a package, or
 * multiple packages into an application. Note that mixing these two usages
 * generally doesn't make sense; module names are not scoped to packages, so
 * all input modules must necessarily be from the same package.
 *
 * Each linker instance is used to accumulate a single output object.
 */
typedef struct ava_pcode_linker_s ava_pcode_linker;

/**
 * Converts the given P-Code into an interface module.
 *
 * All unexported global entities are deleted. Exported `fun` and `var`
 * entities are transformed into `ext-fun` and `ext-var`, respectively.
 *
 * @param pcode The P-Code to reduce to an interface. This is not modified; the
 * input is copied.
 * @return A list of global P-Code elements representing the externally-visible
 * interface of the P-Code.
 */
struct ava_pcode_global_list_s* ava_pcode_to_interface(
  const struct ava_pcode_global_list_s* pcode);

/**
 * Initialises a new, empty P-Code linker.
 */
ava_pcode_linker* ava_pcode_linker_new(void);

/**
 * Adds the given module to the linker input.
 *
 * Modules may be added in any order.
 *
 * @param linker The linker to which to add the module.
 * @param module_name The name of the module, eg "dir/foo".
 * @param module The module to add to the linker.
 */
void ava_pcode_linker_add_module(
  ava_pcode_linker* linker,
  ava_string module_name,
  const struct ava_pcode_global_list_s* module);

/**
 * Adds the given package to the linker input.
 *
 * Packages may be added in any order.
 *
 * @param linker The linker to which to add the package.
 * @param package_name The name of the package, eg "org.ava-lang.avast".
 * @param package The package to add to the linker.
 */
void ava_pcode_linker_add_package(
  ava_pcode_linker* linker,
  ava_string package_name,
  const struct ava_pcode_global_list_s* package);

/**
 * Links all the inputs to the linker into one P-Code object.
 *
 * Published external globals of the same name are merged into one entry.
 * Non-external published globals replace published external globals of the
 * same name. load-mod globals which resolve to modules added to the linker are
 * eliminated. load-pkg globals which resolve to packages added to the linker
 * are eliminated. All init globals are preserved, in the correct orde. Exports
 * not marked for re-export are eliminated.
 *
 * On success, returns a non-NULL result object. On error, returns NULL, and
 * adds any error(s) encountered to the given error list.
 *
 * @param linker The linker whose accumulated inputs are to be linked into one
 * object.
 * @param errors List to which any errors encountered are added.
 * @return A non-NULL link result on success, NULL on error.
 */
struct ava_pcode_global_list_s* ava_pcode_linker_link(
  ava_pcode_linker* linker, struct ava_compile_error_list_s* errors);

#endif /* AVA_RUNTIME_PCODE_LINKER_H_ */
