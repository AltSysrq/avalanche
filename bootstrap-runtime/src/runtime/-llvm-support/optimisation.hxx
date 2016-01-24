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
#ifndef AVA_RUNTIME__LLVM_SUPPORT_OPTIMISATION_HXX_
#define AVA_RUNTIME__LLVM_SUPPORT_OPTIMISATION_HXX_

#include <llvm/IR/Module.h>

namespace ava {
  /**
   * Applies optimisation passes to the given LLVM module in-place.
   *
   * The particular optimisation pass sequence is optimised for Avalanche (in
   * particular, the way in which its IR codegen works) and probably not widely
   * applicable to other languages.
   *
   * @param module The module to optimise.
   * @param level The optimisation level. 0 makes the function a noop. 3 is
   * currently the maximum useful optimisation level.
   */
  void optimise_module(llvm::Module& module, unsigned level);
}

#endif /* AVA_RUNTIME__LLVM_SUPPORT_OPTIMISATION_HXX_ */
