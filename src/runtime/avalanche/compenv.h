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
#error "Don't include avalanche/compenv.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_COMPENV_H_
#define AVA_RUNTIME_COMPENV_H_

#include "string.h"
#include "value.h"
#include "list.h"
#include "map.h"
#include "module-cache.h"

struct ava_macsub_context_s;
struct ava_compile_error_list_s;
struct ava_pcode_global_list_s;
struct ava_xcode_global_list_s;
struct ava_compile_location_s;

typedef struct ava_compenv_s ava_compenv;

/**
 * Function type for reading the full text of a module.
 *
 * @param dst Output. On success, an ordered map of filename to source content.
 * @param error On failure, the error message.
 * @param filename Relative name of the file to read, eg "foo/bar.ava".
 * @param compenv The contextual ava_compenv.
 * @return Whether the file was read successfully.
 */
typedef ava_bool (*ava_compenv_read_source_f)(
  ava_value* dst, ava_string* error, ava_string filename,
  ava_compenv* compenv);

/**
 * Function type for creating a fresh macro substitution context for a
 * compilation environment.
 *
 * @param compenv The contextual ava_compenv.
 * @param errors Error list to which any errors discovered in the new macro
 * substitution context are to be added.
 * @return The new macro substitution context.
 */
typedef struct ava_macsub_context_s* (*ava_compenv_new_macsub_f)(
  ava_compenv* compenv,
  struct ava_compile_error_list_s* errors);

/**
 * Used to track which modules are currently being processed within a compenv,
 * so that cyclic dependencies can be detected.
 *
 * This structure is generally stack-allocated.
 */
typedef struct ava_compenv_pending_module_s {
  /**
   * The name of the module in this entry.
   */
  ava_string module_name;
  SLIST_ENTRY(ava_compenv_pending_module_s) next;
} ava_compenv_pending_module;

/**
 * Provides all the context necessary to retrieve the contents of a source file
 * by name and compile it into P-Code.
 *
 * Note that the compilation environment may be used for more than one input,
 * unlike the contexts it controls. The environment is invoked recursively as
 * necessary to load modules required by the source which are not found in the
 * module cache.
 */
struct ava_compenv_s {
  /**
   * The basic package prefix applied to all symbols in compiled modules.
   *
   * Eg, "org.ava-lang.avast:"
   */
  ava_string package_prefix;
  /**
   * The shared package cache.
   */
  ava_module_cache_stack package_cache;
  /**
   * The shared module cache.
   */
  ava_module_cache_stack module_cache;
  /**
   * Stack tracking module names whose compiliation is currently in-progress
   * and resulted in the recursive loading of other modules.
   *
   * If an attempt is made to load a module while it is on this stack, there
   * exists a cyclic dependency and the module cannot be loaded.
   */
  SLIST_HEAD(,ava_compenv_pending_module_s) pending_modules;

  /**
   * Method to read source files by name.
   */
  ava_compenv_read_source_f read_source;
  /**
   * Arbitrary data for use by read_source.
   */
  ava_datum read_source_userdata;

  /**
   * Method for creating new macro substitution contexts.
   */
  ava_compenv_new_macsub_f new_macsub;
  /**
   * Arbitrary data for use by new_macsub.
   */
  ava_datum new_macsub_userdata;

  /**
   * A list of package names which are automatically injected as a dependency.
   * When a codegen context is created, load-pkg instructions are emitted to
   * link against these packages.
   */
  ava_list_value implicit_packages;
};

/**
 * Allocates a new, unconfigured compilation environment.
 *
 * The module cache stacks are initialised to empty, as is pending_modules. The
 * methods are uninitialised; doing so is up to the caller.
 *
 * @param package_prefix The value for the package_prefix field of the structure.
 */
ava_compenv* ava_compenv_new(ava_string package_prefix);

/**
 * Configures the given compilation environment to read source files by simply
 * prepending a given prefix to every input filename, and then use that to read
 * a file from the local file system.
 *
 * @param env The environment to configure.
 * @param prefix The path prefix to apply to input filenames.
 */
void ava_compenv_use_simple_source_reader(ava_compenv* env, ava_string prefix);
/**
 * Configures the given compilation environment to create macro substitution
 * contexts with only the intrinsics macros available.
 *
 * This is only really useful for low-level tests or compiling avast itself,
 * since most code will also want avast to be available.
 */
void ava_compenv_use_minimal_macsub(ava_compenv* env);
/**
 * Configures the given compilation environment to create macro substitution
 * contexts with both the intrinsics macros and the core library
 * (org.ava-lang.avast) available.
 *
 * The org.ava-lang.avast package is implicitly added to the list of implicit
 * packages.
 *
 * This is not available in the bootstrapping library.
 */
void ava_compenv_use_standard_macsub(ava_compenv* env);

/**
 * Performs all steps necessary to compile a source file to validated P-Code.
 *
 * @param pcode If non-NULL, a pointer in which to store the generated P-Code,
 * if any. This is always set to NULL or a structurally valid P-Code object.
 * @param xcode If non-NULL, a pointer in which to store the generated X-Code,
 * if any. This is always set to NULL or a structurally valid X-Code object.
 * @param env The compilation environment.
 * @param filename The source file to read and compile.
 * @param errors Initialised list to which any errors encountered are appended.
 * @param location Location to report if the module itself cannot be loaded. If
 * NULL, the location is treated as the beginning of the input file.
 * @return True if no errors were encountered, false otherwise. When false is
 * returned, the caller cannot assume that *pcode or *xcode is completely
 * logically consistent.
 */
ava_bool ava_compenv_compile_file(
  struct ava_pcode_global_list_s** pcode,
  struct ava_xcode_global_list_s** xcode,
  ava_compenv* env,
  ava_string filename,
  struct ava_compile_error_list_s* errors,
  const struct ava_compile_location_s* location);

/**
 * read_source implementation installed by
 * ava_compenv_use_simple_source_reader().
 *
 * This uses the read_source_userdata field in an unspecified manner.
 *
 * It is exposed here so that callers can layer other (stateless) functions on
 * top of it.
 */
ava_bool ava_compenv_simple_read_source(
  ava_value* dst, ava_string* error, ava_string filename,
  ava_compenv* compenv);
/**
 * new_macsub implementation installed by
 * ava_compenv_use_minimal_macsub().
 *
 * This does not use the new_macsub_userdata field.
 *
 * It is exposed here so that callers can layer other functions on top of it.
 */
ava_macsub_context* ava_compenv_minimal_new_macsub(
  ava_compenv* compenv,
  ava_compile_error_list* errors);
/**
 * new_macsub implementation installed by
 * ava_compenv_use_standard_macsub().
 *
 * This does not use the new_macsub_userdata field.
 *
 * It is exposed here so that callers can layer other functions on top of it.
 */
ava_macsub_context* ava_compenv_standard_new_macsub(
  ava_compenv* compenv,
  ava_compile_error_list* errors);

#endif /* AVA_RUNTIME_COMPENV_H_ */
