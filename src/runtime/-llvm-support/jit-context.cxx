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

#include <string>
#include <memory>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/TargetSelect.h>

#ifndef AVA_NOGC
#if defined(HAVE_GC_GC_H)
#include <gc/gc.h>
#elif defined(HAVE_GC_H)
#include <gc.h>
#else
#error "Neither <gc/gc.h> nor <gc.h> could be found."
#endif
#else /* AVA_NOGC */
static inline void GC_disable(void) { }
static inline void GC_enable(void) { }
#endif

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
AVA_BEGIN_DECLS
#include "../avalanche/string.h"
#include "../avalanche/exception.h"
#include "../avalanche/jit.h"
AVA_END_DECLS

#include "translation.hxx"
#include "jit-context.hxx"

/*
  TODO: We're going to need to completely rework JIT eventually. LLVM's JIT
  implementations aren't really useable. McJit doesn't support such exotic
  systems as Linux on AMD64. Neither provides any reasonable way to properly
  communicate the addresses of global variables to the GC. And ultimately we'll
  want something more stateless to better mesh with the nature of Avalanche.
 */

ava::jit_context::jit_context(void) noexcept
/* Awkwardly, we're not allowed to create the engine until we have an actual
 * module...
 */
: engine(nullptr)
{ }

bool ava::jit_context::run_module(
  std::unique_ptr<llvm::Module> module,
  ava_string module_name,
  std::string& error
) {
  LLVMLinkInMCJIT();
  LLVMLinkInJIT();
  llvm::InitializeNativeTarget();

  llvm::Module* borrowed_module = module.get();

  if (!engine)
    engine.reset(llvm::EngineBuilder(module.release())
                 .setEngineKind(llvm::EngineKind::JIT)
                 /*.setUseMCJIT(true)*/
                 .setErrorStr(&error_str)
                 .create());
  else
    engine->addModule(module.release());

  if (!engine) {
    error = error_str;
    return false;
  }

  /* TODO: Ensure that the function actually looks like this */
  llvm::Function* init_fun = borrowed_module->getFunction(
    ava_string_to_cstring(module_name));
  void (*init)(void) = reinterpret_cast<void(*)(void)>(
      init_fun? engine->getPointerToFunction(init_fun) : nullptr);
  if (init) {
    /* XXX The GC doesn't know about the module's global variables. For now,
     * just make sure it doesn't run while the module does, and assume the
     * module constitutes a whole program (ie, it's ok to clobber them after
     * the module finishes executing).
     */
    GC_disable();
    (*init)();
    GC_enable();
  } else {
    error = "Initialisation function not found, or error: ";
    error.append(error_str);
    return false;
  }

  return true;
}

struct ava_jit_context_s {
  llvm::LLVMContext llvm_context;
  ava::jit_context jit_context;
  ava::xcode_to_ir_translator xlate;
};

extern "C" ava_jit_context* ava_jit_context_new(void) {
  return new ava_jit_context_s;
}

extern "C" void ava_jit_context_delete(ava_jit_context* context) {
  delete context;
}


extern "C" void ava_jit_add_driver(ava_jit_context* context,
                                   const char* data, size_t size) {
  context->xlate.add_driver(data, size);
}

extern "C" ava_string ava_jit_run_module(
  ava_jit_context* context,
  const struct ava_xcode_global_list_s* xcode,
  ava_string filename,
  ava_string module_name
) {
  std::string error;
  std::unique_ptr<llvm::Module> llvm_module = context->xlate.translate(
    xcode, filename, module_name, context->llvm_context, error);

  if (!llvm_module)
    return ava_string_of_cstring(error.c_str());

  if (!context->jit_context.run_module(
        std::move(llvm_module), module_name, error))
    return ava_string_of_cstring(error.c_str());

  return AVA_ABSENT_STRING;
}