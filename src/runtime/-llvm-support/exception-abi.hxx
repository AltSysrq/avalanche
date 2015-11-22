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
#ifndef AVA_RUNTIME__LLVM_SUPPORT_EXCEPTION_ABI_HXX_
#define AVA_RUNTIME__LLVM_SUPPORT_EXCEPTION_ABI_HXX_

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

namespace ava {
  struct ir_types;
  struct driver_iface;

  /**
   * Extracts information about the underlying C++ exception ABI by inspecting
   * the output of Clang++ within the ISA driver.
   *
   * The general exception handling pattern is:
   *
   *   <dosomething> unwind label %lp
   *
   *   %lp:
   *   %cxxex = landingpad $ex_type personality $ex_personality
   *            catch $ex_catch_type
   *   %caught_type = extractvalue $ex_type %cxxex, 1
   *   %expected_type = tail call _ $eh_typeid_for ($ex_catch_type)
   *   %ours_p = icmp eq _ %caught_type, %expected_type
   *   br i1 %ours_p, label %handle, label %res
   *
   *   %res:
   *   resume $ex_type $cxxex
   *
   *   %handle:
   *   %cxxex_data = extractvalue $ex_type %cxxex, 0
   *   %exptr = tail call i8* $begin_catch (%cxxex_data)
   *   ; copy *%exptr somewhere stable
   *   tail call void $end_catch ()
   *
   * See also:
   *
   *   Ground-up overview of how exceptions work on the Itanium ABI. It
   *   unfortunately oversimplifies some things and reads like The Gift Shop
   *   Sketch, but does provide valuable insight into the functions we care
   *   about here, and links to a lot of other resources:
   *   https://monoinfinito.wordpress.com/category/programming/c/exceptions/page/2/
   *
   *   Itanium C++ ABI:
   *   http://mentorembedded.github.io/cxx-abi/abi-eh.html#cxx-abi
   *
   *   LLVM C++ ABI spec:
   *   http://libcxxabi.llvm.org/spec.html
   *
   * Note that the __cxa_* functions are fetched/added to the module by name
   * and knowledge of their type rather than being extracted from the ISA; the
   * ISA only provides the RTTI and links to the appropriate personality
   * function and such.
   *
   * The cxa_* functions are documented with the detail necessary to use the
   * subset of functions exposed here.
   */
  struct exception_abi {
    exception_abi(llvm::Module& module,
                  const ava::ir_types& ir_types) noexcept;

    /**
     * Generates a landing pad which stores the details of a caught exception
     * in exception_dst, then proceeds to target. If the caught exception is
     * not an ava_exception, the exception type will be NULL and its value the
     * empty string. exception_ctx is a i8** location where the raw
     * native exception is stored.
     */
    llvm::BasicBlock* create_landing_pad(
      llvm::DebugLoc debug_loc,
      llvm::BasicBlock* target,
      llvm::Value* exception_dst,
      llvm::Value* exception_ctx,
      const ava::driver_iface& di) const noexcept;

    /**
     * Generates the necessary code to drop the given exception (ie, yrt on a
     * catch branch).
     */
    void drop(llvm::IRBuilder<true>& irb,
              llvm::Value* exception_ctx) const noexcept;

    /**
     * Generates the necessary code to rethrow the given exception.
     */
    void rethrow(llvm::IRBuilder<true>& irb,
                 llvm::Value* exception_ctx) const noexcept;

    /**
     * The physical type of an exception. Basically
     * struct { void* data; int cxx_type_id; }
     */
    llvm::StructType* ex_type;
    /**
     * The bitcast value of the personality function.
     */
    llvm::Value* ex_personality;
    /**
     * The bitcast value of the RTTI which identifies ava_exception. This is
     * both used as a "catch" clause in the landingpad instruction and is
     * passed to eh_typeid_for().
     */
    llvm::Constant* ex_catch_type;

    /**
     * The llvm.eh.typeid.for intrinsic.
     */
    llvm::Function* eh_typeid_for;

    /**
     * ava_exception* cxa_begin_catch(void* raw_exception);
     *
     * Returns the pointer to the ava_exception within the native exception.
     * This pointer is valid until the balancing call to cxa_end_catch().
     *
     * This function MUST be perfectly balanced with cxa_end_catch(). It is NOT
     * safe to use this if the exception does not have RTTI matching
     * ava_exception.
     *
     * If this function is invoked on an exception, it cannot be resumed
     * without first calling cxa_rethrow() (otherwise it resumes the next
     * exception on the stack).
     */
    llvm::Value* cxa_begin_catch;
    /**
     * void cxa_end_catch(void);
     *
     * Balances a call to cxa_begin_catch().
     */
    llvm::Value* cxa_end_catch;
    /**
     * void cxa_rethrow(void) __attribute__((__noreturn__));
     *
     * Marks the current exception (ie, the most recent one pushed by
     * cxa_begin_catch()) as rethrowable.
     *
     * The Itanium API states (and LLVM ABI copies)
     *
     * This routine marks the exception object on top of the caughtExceptions
     * stack (in an implementation-defined way) as being rethrown. If the
     * caughtExceptions stack is empty, it calls terminate() (see [C++FDIS]
     * [except.throw], 15.1.8). It then returns to the handler that called it,
     * which must call __cxa_end_catch(), perform any necessary cleanup, and
     * finally call _Unwind_Resume() to continue unwinding.
     *
     * However, libcxxrt (and presumably everything else) *actuall rethrows*
     * the exception right then and there. Calling __cxa_end_catch() is still
     * necessary, so this must be invoked with a landing pad which does just
     * that.
     *
     * Note that the way the Avalanche implementation actually uses this
     * function is a bit unusual for the sake of simplicity; see the
     * implementation of rethrow() and drop().
     */
    llvm::Value* cxa_rethrow;
  };
}

#endif /* AVA_RUNTIME__LLVM_SUPPORT_EXCEPTION_ABI_HXX_ */
