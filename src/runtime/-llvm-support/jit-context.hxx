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
#ifndef AVA_RUNTIME__LLVM_SUPPORT_JIT_CONTEXT_HXX_
#define AVA_RUNTIME__LLVM_SUPPORT_JIT_CONTEXT_HXX_

#include <memory>
#include <string>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Module.h>

AVA_BEGIN_DECLS
#include "../avalanche/string.h"
#include "../avalanche/exception.h"
AVA_END_DECLS

namespace ava {
  /**
   * Wrapper around LLVM's McJit engine.
   */
  class jit_context {
    std::unique_ptr<llvm::ExecutionEngine> engine;
    std::string error_str;

  public:
    jit_context(void) noexcept;

    /**
     * Adds the given LLVM module to the JIT, and runs its initialisation
     * function.
     *
     * @param module The module to add. Ownership of the module is transferred
     * to the JIT.
     * @param module_name The Avalanche name of the module, dictating the name
     * of its initialisation function.
     * @error If an error occurs, set to the error message.
     * @return Whether loading and executing the module succeeded.
     * @throws * Any exception thrown by the module propagates out of this
     * call. If an exception is thrown, the module has already been loaded into
     * the JIT and ownership transferred.
     */
    bool run_module(std::unique_ptr<llvm::Module> module,
                    ava_string module_name,
                    std::string& error);
  };
}

#endif /* AVA_RUNTIME__LLVM_SUPPORT_JIT_CONTEXT_HXX_ */
