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

#include <llvm/IR/Module.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Vectorize.h>

#include "optimisation.hxx"

void ava::optimise_module(llvm::Module& module, unsigned level) {
  if (0 == level) return;

  /* Yes, we're using the *legacy* pass manager.
   *
   * The new one takes objects rather than pointers to them, which is all well
   * and good, except that it isn't actually possible to _construct_ some of
   * them that way; for example, TypeBasedAliasAnalysis isn't accessible here,
   * and can only be constructed in such a way that it can be passed to the
   * legacy pass manager.
   */
  llvm::legacy::PassManager pass_manager;

  pass_manager.add(llvm::createTypeBasedAliasAnalysisPass());
  pass_manager.add(llvm::createBasicAliasAnalysisPass());
  pass_manager.add(llvm::createGlobalsModRefPass());
  if (level >= 2) {
    pass_manager.add(llvm::createLazyValueInfoPass());
    pass_manager.add(llvm::createDependenceAnalysisPass());
  }
  if (level >= 3) {
    pass_manager.add(llvm::createRegionInfoPass());
    pass_manager.add(llvm::createCostModelAnalysisPass());
    pass_manager.add(llvm::createDelinearizationPass());
  }

  /* Slim the module down by trimming off low-hanging fruit. */
  pass_manager.add(llvm::createGlobalDCEPass());
  pass_manager.add(llvm::createConstantMergePass());
  pass_manager.add(llvm::createCFGSimplificationPass());
  pass_manager.add(llvm::createFlattenCFGPass());
  /* Infer attributes */
  pass_manager.add(llvm::createFunctionAttrsPass());
  /* Inline all the ISA functions and so forth.
   *
   * This is necessary very early on since the ISA functions take pointers to
   * lots of local vars.
   */
  pass_manager.add(llvm::createAlwaysInlinerPass());
  /* Split fat_list_values and such up when possible, to produce better
   * register allocation.
   */
  pass_manager.add(llvm::createScalarReplAggregatesPass());
  /* Move all the locals into first-class SSA registers. Since we take the
   * address of relatively few locals, this should move pretty much everything
   * to registers.
   */
  pass_manager.add(llvm::createPromoteMemoryToRegisterPass());
  if (level >= 2) {
    /* More of the above */
    pass_manager.add(llvm::createSROAPass());
    /* Improve instruction simplification */
    pass_manager.add(llvm::createRegionInfoPass());
  }
  /* Trim fat within basic blocks */
  pass_manager.add(llvm::createDeadInstEliminationPass());
  pass_manager.add(llvm::createMergedLoadStoreMotionPass());
  pass_manager.add(llvm::createDeadStoreEliminationPass());
  /* Local constant propagation */
  pass_manager.add(llvm::createSCCPPass());
  /* Peephole optimisation */
  pass_manager.add(llvm::createInstructionCombiningPass());
  if (level >= 2) {
    /* Common Subexpression Elimination is pretty important for calls to
     * ava_integer_of_value and so forth.
     */
    pass_manager.add(llvm::createEarlyCSEPass());
  }

  if (level >= 2) {
    /* Another pass of peephole optimisation, DCE, and CFG simplification in
     * preparation for looking at loops.
     */
    pass_manager.add(llvm::createDeadCodeEliminationPass());
    pass_manager.add(llvm::createDeadStoreEliminationPass());
    pass_manager.add(llvm::createInstructionCombiningPass());
    pass_manager.add(llvm::createDeadCodeEliminationPass());
    pass_manager.add(llvm::createCFGSimplificationPass());

    /* Prep for loop optimisations */
    pass_manager.add(llvm::createLCSSAPass());
    pass_manager.add(llvm::createIndVarSimplifyPass());

    pass_manager.add(llvm::createLoopSimplifyPass());
    pass_manager.add(llvm::createLICMPass());
    pass_manager.add(llvm::createLoopIdiomPass());
    pass_manager.add(llvm::createLoopStrengthReducePass());
    pass_manager.add(llvm::createLoopUnswitchPass());

    if (level >= 3) {
      pass_manager.add(llvm::createLoopUnrollPass());
      pass_manager.add(llvm::createLoopRotatePass());
    }

    pass_manager.add(llvm::createLoopDeletionPass());
    pass_manager.add(llvm::createDeadCodeEliminationPass());

    /* More local simplifications (mostly to clean up the above) */
    pass_manager.add(llvm::createCFGSimplificationPass());
    pass_manager.add(llvm::createInstructionCombiningPass());
  }

  if (level >= 3) {
    /* Other local stuff */
    pass_manager.add(llvm::createConstantHoistingPass());
    pass_manager.add(llvm::createJumpThreadingPass());

    /* Vectorisation */
    pass_manager.add(llvm::createSLPVectorizerPass());
    pass_manager.add(llvm::createLoopVectorizePass());

    /* Inter-procedural optimisation */
    pass_manager.add(llvm::createIPSCCPPass());
    pass_manager.add(llvm::createInstructionCombiningPass());
    pass_manager.add(llvm::createDeadInstEliminationPass());
    pass_manager.add(llvm::createDeadArgEliminationPass());
    pass_manager.add(llvm::createFunctionInliningPass());
    /* XXX llc-3.5 segfaults if confronted with code passed through the partial
     * inliner. It appears to infinitely recurse through debugging info. (So it
     * may be a bug in this pass rather than llc.)
    pass_manager.add(llvm::createPartialInliningPass());
    */
    pass_manager.add(llvm::createMergedLoadStoreMotionPass());

    /* Local optimisations on the result of inlining */
    pass_manager.add(llvm::createSCCPPass());
    pass_manager.add(llvm::createInstructionCombiningPass());
    pass_manager.add(llvm::createDeadInstEliminationPass());
    pass_manager.add(llvm::createCFGSimplificationPass());
  }

  /* Strip away everything dead */
  pass_manager.add(llvm::createGlobalDCEPass());
  pass_manager.add(llvm::createAggressiveDCEPass());

  pass_manager.run(module);
}
