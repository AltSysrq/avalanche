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
     * empty string.
     *
     * Note that the generated landing pad catches *all* exceptions (as with
     * `catch (...)` in C++).
     *
     * The has_* arguments are needed to correctly handle nested
     * catch/finally/etc. For example, if a catch is nested within a finally,
     * the catch needs to be declared a cleanup so it gets all exceptions and
     * can correctly unwind to the finally. Similarly, a finally landing pad
     * whose body is wrapped in another external try (not a catch) must declare
     * that it catches ava_exception so that the C++ runtime is aware that it
     * is productive to unwind to the finally, just so that it can continue
     * unwinding to the eventual catch.
     *
     * @param debug_loc Location to use when emitting debug information for the
     * generated landing pad.
     * @param target The target to proceed to after the exception has been
     * extracted.
     * @param exception_dst Pointer into which the caught exception is copied.
     * @param num_cleanup_exes Number of caught-exceptions to clean up before
     * starting handling of the new exception.
     * @param is_cleanup Whether this landing pad is a cleanup or a catch. A
     * cleanup will catch all exceptions but will treat all exceptions as
     * foreign (as per `cleanup` on the `try` P-Code instruction.)
     * @param has_cleanup Whether the landing-pad should be tagged as a cleanup
     * even if it isn't one. That is, whether there are any cleanup
     * exception-handler entries on the exception stack of the basic block this
     * landing-pad heads.
     * @param has_catch Whether the landing-pad should declare that it catches
     * ava_exception even if it doesn't. That is, whether there is at least one
     * non-cleanup exception-handler entry on the exception stack of the basic
     * block this landing-pad heads.
     * @param di The driver_iface used in the current context.
     * @return A basic block which is a landing pad and performs the above
     * setup before proceeding to target.
     */
    llvm::BasicBlock* create_landing_pad(
      llvm::DebugLoc debug_loc,
      llvm::BasicBlock* target,
      llvm::Value* exception_dst,
      size_t num_cleanup_exes,
      bool is_cleanup,
      bool has_cleanup,
      bool has_catch,
      const ava::driver_iface& di) const noexcept;

    /**
     * Creates a landingpad block which cleans up the given number of
     * caught-exceptions before resuming propagation.
     */
    llvm::BasicBlock* create_cleanup(
      llvm::BasicBlock* after,
      llvm::DebugLoc debug_loc,
      size_t num_cleanup_exes,
      const ava::driver_iface& di) const noexcept;

    /**
     * Generates the necessary code to drop a caught-exception (ie, yrt on a
     * catch branch).
     */
    void drop(llvm::IRBuilder<true>& irb,
              const ava::driver_iface& di) const noexcept;

    /**
     * The "personality function" to apply to all functions that may need
     * exception handling.
     */
    llvm::Constant* personality_fn;

    /**
     * The physical type of an exception. Basically
     * struct { void* data; int cxx_type_id; }
     */
    llvm::StructType* ex_type;
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
     * This function MUST be perfectly balanced with cxa_end_catch(). It is
     * safe to use this if the exception does not have RTTI matching
     * ava_exception, but nothing useful can be done with the resulting
     * pointer.
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
     */
    llvm::Value* cxa_rethrow;
  };
}

#endif /* AVA_RUNTIME__LLVM_SUPPORT_EXCEPTION_ABI_HXX_ */
