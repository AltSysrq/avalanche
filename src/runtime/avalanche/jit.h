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
#error "Don't include avalanche/jit.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_JIT_H_
#define AVA_RUNTIME_JIT_H_

struct ava_xcode_global_list_s;

/**
 * @file
 *
 * C-compatible interface to Avalanche's LLVM-backed JIT system.
 *
 * This will probably need to change a lot once Avalanche actually has its
 * threading model. For now, there's no safe way to handle the possibility of
 * JITted spawning threads other than never destroying a JIT context.
 */

/**
 * Heavy-weight context surrounding native code generation and the JIT itself.
 * This value is _not_ garbage-collectible; it _must_ be destroyed with
 * ava_jit_context_delete().
 */
typedef struct ava_jit_context_s ava_jit_context;

/**
 * Allocates a new JIT context, including an isolated LLVM context.
 */
ava_jit_context* ava_jit_context_new(void);
/**
 * Releases all resources held by the given JIT context.
 *
 * The caller must be certain that there is not currently any code actually
 * executing within JITted modules.
 */
void ava_jit_context_delete(ava_jit_context* context);

/**
 * Adds a driver to the LLVM IR code generator. The IR is not validated until
 * ava_jit_run_module() is called.
 *
 * Don't load the "main" driver into the JIT system; the JIT works by directly
 * calling the module initialisers itself.
 *
 * @see ava::xcode_to_ir_translator::add_driver()
 */
void ava_jit_add_driver(ava_jit_context* context,
                        const char* data, size_t size);
/**
 * Translates the given X-Code moudle (assumed to be valid) into native code
 * and executes it. The generated code will remain in memory until the context
 * is destroyed.
 *
 * Note that many errors currently result in LLVM fatal errors that ultimately
 * abort the process.
 *
 * @param context The JIT context into which this module will be loaded.
 * @param module The module to load.
 * @param filename The input file name to use for debugging info.
 * @param module_name The logical name of this module, used to uniquely
 * identify its initialisation function, among other things.
 * @return The absent string if the module was loaded, linked, and executed
 * successfully. A present string to indicate an error in loading or linking
 * the module.
 * @throws * Any exception that is thrown by the module passes out through this
 * call.
 */
ava_string ava_jit_run_module(
  ava_jit_context* context,
  const struct ava_xcode_global_list_s* module,
  ava_string filename,
  ava_string module_name);

#endif /* AVA_RUNTIME_JIT_H_ */
