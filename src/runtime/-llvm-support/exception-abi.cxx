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

  /* Find the landing pad */
  entry_block = &exfun->getEntryBlock();
  invoke = llvm::cast<llvm::InvokeInst>(&entry_block->front());
  lp_block = invoke->getUnwindDest();
  lp = lp_block->getLandingPadInst();

  /* Dissect the landing pad */
  ex_type = llvm::cast<llvm::StructType>(lp->getType());
  ex_personality = lp->getPersonalityFn();
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

/*
  The Itanium ABI / LLVM C++ ABI makes things a bit awkward with respect to how
  rethrowing should work.

  The "natural" first approach looks like:

    landingpad:
      __cxa_begin_catch()

    yrt:
      __cxa_end_catch()

    rethrow:
      __cxa_rethrow() unwind $+1
      __cxa_end_catch()
      resume

  The problem here is that the code between landingpad and yrt/rethrow could
  *itself* throw, so we would need an extra

    inner_landingpad:
      __cxa_end_catch()
      resume

  landing pad that would replace the landing pads between those endpoints. This
  would already be extremely awkward in our model. However, there's also the
  complication that inner_landingpad would need to account for other exceptions
  getting pushed onto the exception stack.

  Therefore, we instead maintain the invariant that, as long as a
  caught-exception is on the P-Code stack, a corresponding native exception is
  ready to be resumed. This is accomplished with the observation that
  begin()/end() effectively drops the exception, while begin()/rethrow()/end()
  permits observing the exception without dropping it. Thus:

    landingpad:
      __cxa_begin_catch()
      ; evacuate exception to stable location
      __cxa_rethrow() unwind $+1
      __cxa_end_catch()

    yrt:
      __cxa_begin_catch()
      __cxa_end_catch()

    rethrow:
      resume

  A nice side-effect of this is that only the landingpad needs to care whether
  it has caught an Avalanche exception or a foreign one; yrt and rethrow can
  simply obliviously call the appropriate primitives.
 */

llvm::BasicBlock* ava::exception_abi::create_landing_pad(
  llvm::DebugLoc debug_loc,
  llvm::BasicBlock* target,
  llvm::Value* exception_dst,
  llvm::Value* exception_ctx,
  ava::driver_iface& di)
const noexcept {
  llvm::LLVMContext& context(target->getContext());
  llvm::BasicBlock* bb_lp, * bb_foreign, * bb_ava, * bb_ava2, * bb_unreachable;

  /*
    %lp:
    %cxxex = landingpad $ex_type personality $ex_personality
             catch $ex_catch_type
    %rawex = extractvalue $ex_type %cxxex, 0
    store %rawex, $exception_ctx
    %caught_type = extractvalue $ex_type %cxxex, 1
    %expected_type = tail call i32 $eh_typeid_for ($ex_catch_type)
    %ours_p = icmp eq i32 %caught_type, %expected_type
    br i1 %ours_p, label %avalanche, label %foreign

    %foreign:
    isa::foreign_exception($exception_dst)
    br $target

    %avalanche:
    %cxxex_data = extractvalue $ex_type %cxxex, 0
    %exptr = call i8* __cxa_begin_catch (%cxxex_data)
    isa::copy_exception($exception_dst, %exptr)
    invoke void __cxa_rethrow () to unreachable unwind %avalanche.2

    %avalanche.2:
    call void __cxa_end_catch ()
    br $target
   */

  bb_lp = llvm::BasicBlock::Create(
    context, "", target->getParent(), target);
  bb_foreign = llvm::BasicBlock::Create(
    context, "", target->getParent(), target);
  bb_ava = llvm::BasicBlock::Create(
    context, "", target->getParent(), target);
  bb_ava2 = llvm::BasicBlock::Create(
    context, "", target->getParent(), target);
  bb_unreachable = llvm::BasicBlock::Create(
    context, "", target->getParent(), target);

  llvm::IRBuilder<true> irb(bb_lp);
  irb.SetCurrentDebugLocation(debug_loc);

  llvm::LandingPadInst* cxxex_lp = irb.CreateLandingPad(
    ex_type, ex_personality, 1);
  cxxex_lp->addClause(ex_catch_type);
  llvm::Value* rawex = irb.CreateExtractValue(cxxex_lp, { 0 });
  irb.CreateStore(rawex, exception_ctx);
  llvm::Value* caught_type = irb.CreateExtractValue(cxxex_lp, { 1 });
  llvm::Value* expected_type = irb.CreateCall(eh_typeid_for, ex_catch_type);
  llvm::Value* ours_p = irb.CreateICmpEQ(caught_type, expected_type);
  irb.CreateCondBr(ours_p, bb_ava, bb_foreign);

  irb.SetInsertPoint(bb_foreign);
  irb.CreateCall(di.foreign_exception, exception_dst);
  irb.CreateBr(target);

  irb.SetInsertPoint(bb_ava);
  llvm::PHINode* cxxex_ava = irb.CreatePHI(cxxex_lp->getType(), 1);
  cxxex_ava->addIncoming(cxxex_lp, bb_lp);
  llvm::Value* cxx_data = irb.CreateExtractValue(cxxex_ava, { 0 });
  llvm::Value* exptr = irb.CreateCall(cxa_begin_catch, cxx_data);
  llvm::Value* exptr_cast = irb.CreateBitCast(
    exptr, exception_dst->getType());
  irb.CreateCall2(di.copy_exception, exception_dst, exptr_cast);
  irb.CreateInvoke(cxa_rethrow, bb_unreachable, bb_ava2);

  irb.SetInsertPoint(bb_ava2);
  irb.CreateCall(cxa_end_catch);
  irb.CreateBr(target);

  irb.SetInsertPoint(bb_unreachable);
  irb.CreateUnreachable();

  return bb_lp;
}

void ava::exception_abi::drop(llvm::IRBuilder<true>& irb,
                              llvm::Value* exception_ctx)
const noexcept {
  llvm::Value* rawex = irb.CreateLoad(exception_ctx);
  irb.CreateCall(cxa_begin_catch, rawex);
  irb.CreateCall(cxa_end_catch);
}

void ava::exception_abi::rethrow(
  llvm::IRBuilder<true>& irb,
  llvm::Value* exception_ctx)
const noexcept {
  llvm::Value* rawex = irb.CreateLoad(exception_ctx);
  irb.CreateResume(rawex);
}
