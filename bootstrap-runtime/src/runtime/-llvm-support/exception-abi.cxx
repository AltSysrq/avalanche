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

#include <cassert>

#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
#include "../avalanche/pcode.h"

#include "driver-iface.hxx"
#include "ir-types.hxx"
#include "exception-abi.hxx"

ava::exception_abi::exception_abi(llvm::Module& module,
                                  const ava::ir_types& ir_types)
noexcept {
  const llvm::Function* exfun;
  const llvm::BasicBlock* entry_block, * lp_block;
  const llvm::InvokeInst* invoke;
  const llvm::LandingPadInst* lp;
  llvm::AttributeSet attr_none;

  exfun = module.getFunction("ava_c_abi_info_catch_pattern$");
  assert(exfun);

  personality_fn = exfun->getPersonalityFn();
  assert(personality_fn);

  /* Find the landing pad */
  entry_block = &exfun->getEntryBlock();
  invoke = llvm::cast<llvm::InvokeInst>(&entry_block->front());
  lp_block = invoke->getUnwindDest();
  lp = lp_block->getLandingPadInst();

  /* Dissect the landing pad */
  ex_type = llvm::cast<llvm::StructType>(lp->getType());
  ex_catch_type = lp->getClause(0);

  eh_typeid_for = llvm::Intrinsic::getDeclaration(
    &module, llvm::Intrinsic::eh_typeid_for);
  cxa_begin_catch = module.getOrInsertFunction(
    "__cxa_begin_catch", attr_none,
    llvm::PointerType::getUnqual(ir_types.ava_exception),
    ir_types.general_pointer, nullptr);
  cxa_end_catch = module.getOrInsertFunction(
    "__cxa_end_catch", attr_none,
    ir_types.c_void, nullptr);
  cxa_rethrow = module.getOrInsertFunction(
    "__cxa_rethrow", attr_none,
    ir_types.c_void, nullptr);
}

llvm::BasicBlock* ava::exception_abi::create_landing_pad(
  llvm::DebugLoc debug_loc,
  llvm::BasicBlock* target,
  llvm::Value* exception_dst,
  size_t num_cleanup_exes,
  bool is_cleanup,
  bool has_cleanup,
  bool has_catch,
  const ava::driver_iface& di)
const noexcept {
  llvm::LLVMContext& context(target->getContext());
  llvm::BasicBlock* bb_lp;

  /*
    Roughly:

    %lp:
    %cxxex = landingpad $ex_type catch i8* null
    ; drop cleanup exceptions
    %cxxex_data = extractvalue $ex_type %cxxex, 0
    %exptr = call i8* __cxa_begin_catch (%cxxex_data)

    ; if !is_cleanup
    %caught_type = extractvalue $ex_type %cxxex, 1
    %expected_type = tail call i32 $eh_typeid_for ($ex_catch_type)
    %ours_p = icmp eq i32 %caught_type, %expected_type
    %cpyfun = select i1 %ours_p, isa::copy_exception, isa::foreign_exception
    call void %cpyfun ($exception_dst, %exptr)
    br $target

    ; if is_cleanup
    call void isa::foreign_exception ($exception_dst, %exptr)
    br $target
   */

  bb_lp = llvm::BasicBlock::Create(
    context, "", target->getParent(), target);

  llvm::IRBuilder<true> irb(bb_lp);
  irb.SetCurrentDebugLocation(debug_loc);

  llvm::LandingPadInst* cxxex_lp = irb.CreateLandingPad(ex_type, 1);
  if (!is_cleanup || has_catch)
    cxxex_lp->addClause(ex_catch_type);
  /* Need to be a cleanup if requested as such or if we have implicit cleanup
   * to do.
   */
  cxxex_lp->setCleanup(is_cleanup || has_cleanup || num_cleanup_exes > 0);

  for (size_t i = 0; i < num_cleanup_exes; ++i)
    drop(irb, di);

  llvm::Value* cpyfun;

  llvm::Value* cxxex_data = irb.CreateExtractValue(cxxex_lp, { 0 });
  if (!is_cleanup) {
    llvm::Value* caught_type = irb.CreateExtractValue(cxxex_lp, { 1 });
    llvm::Value* expected_type = irb.CreateCall(eh_typeid_for, ex_catch_type);
    llvm::Value* ours_p = irb.CreateICmpEQ(caught_type, expected_type);
    cpyfun = irb.CreateSelect(
      ours_p, di.copy_exception, di.foreign_exception);
  } else {
    cpyfun = di.foreign_exception;
  }

  llvm::Value* exptr = irb.CreateCall(cxa_begin_catch, cxxex_data);
  irb.CreateCall(cpyfun, { exception_dst, exptr });
  irb.CreateBr(target);

  return bb_lp;
}

llvm::BasicBlock* ava::exception_abi::create_cleanup(
  llvm::BasicBlock* after,
  llvm::DebugLoc debug_loc,
  size_t num_cleanup_exes,
  const ava::driver_iface& di)
const noexcept {
  llvm::BasicBlock* bb_lp;

  bb_lp = llvm::BasicBlock::Create(
    after->getContext(), "", after->getParent(), after->getNextNode());
  llvm::IRBuilder<true> irb(bb_lp);
  irb.SetCurrentDebugLocation(debug_loc);

  llvm::LandingPadInst* cxxex_lp = irb.CreateLandingPad(ex_type, 1);
  cxxex_lp->setCleanup(true);
  for (size_t i = 0; i < num_cleanup_exes; ++i)
    drop(irb, di);

  irb.CreateResume(cxxex_lp);

  return bb_lp;
}

void ava::exception_abi::drop(llvm::IRBuilder<true>& irb,
                              const ava::driver_iface& di)
const noexcept {
  irb.CreateCall(cxa_end_catch);
}
