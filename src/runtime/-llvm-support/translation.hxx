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
#ifndef AVA_RUNTIME__LLVM_SUPPORT_TRANSLATION_HXX_
#define AVA_RUNTIME__LLVM_SUPPORT_TRANSLATION_HXX_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <vector>
#include <list>
#include <memory>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

struct ava_xcode_global_list_s;

namespace ava {
  /**
   * Supports translating valid X-Code into LLVM IR.
   *
   * In order for this class to do anything useful, at the very least an ISA
   * driver must be loaded with add_driver(). The code generator makes many
   * blind assumptions about the contents of the ISA driver, and may produce
   * invalid IR if they are not met.
   */
  class xcode_to_ir_translator {
  public:
    xcode_to_ir_translator(void);

    /**
     * Whether full debug information should be written. If false, only line
     * numbers are written.
     *
     * Defaults to true.
     */
    bool full_debug;

    /**
     * Adds the given driver to this translator's list of drivers.
     *
     * @param data The pointer to the LLVM IR bitcode for this driver. The
     * memory contents are copied into the translator's own storage, so the
     * pointer need not remain valid after this call.
     * @param size The size of the driver bitcode, in bytes.
     */
    void add_driver(const char* data, size_t size) noexcept;

    /**
     * Translates the given X-Code into LLVM IR.
     *
     * @param xcode The input code to translate.
     * @param file_name The name of the input file. May be absent, in which
     * case the first src-pos filename is used, or "<unknown>" as fallback.
     * @param module_name The module name / identifier to pass into LLVM. This
     * is also used as the unmangled name of the module initialisation function
     * if there is no driver providing a main function. This should be the
     * empty string for a whole package.
     * @param package_prefix The prefix to apply to any references to a module
     * name, both via module_name above and for the load-mod instruction.
     * @param llvm_context The LLVM context to use for generation.
     * @param error If an error occurs, set to a description of the error.
     * @return The generated module, or a NULL pointer if an error occurs.
     */
    std::unique_ptr<llvm::Module> translate(
      const struct ava_xcode_global_list_s* xcode,
      ava_string file_name,
      ava_string module_name,
      ava_string package_prefix,
      llvm::LLVMContext& llvm_context,
      std::string& error) const noexcept;

  private:
    std::list<std::vector<char> > drivers;
  };

  /**
   * Returns the linkage name of the package or module initialiser identified
   * by the given package/module name pair.
   */
  std::string get_init_fun_name(
    const std::string& package, const std::string& module);

  /**
   * Returns the unmangled version of get_init_fun_name().
   */
  std::string get_unmangled_init_fun_name(
    const std::string& package, const std::string& module);
}

#endif /* AVA_RUNTIME__LLVM_SUPPORT_TRANSLATION_HXX_ */
