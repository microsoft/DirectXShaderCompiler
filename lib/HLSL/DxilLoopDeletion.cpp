//===- DxilLoopDeletion.cpp - Dead Loop Deletion Pass -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file run .
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Dominators.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "llvm/IR/LegacyPassManager.h"

using namespace llvm;

#define DEBUG_TYPE "dxil-loop-delete"

STATISTIC(NumDeleted, "Number of loops deleted");

namespace {
  class DxilLoopDeletion : public FunctionPass {
  public:
    static char ID; // Pass ID, replacement for typeid
    DxilLoopDeletion() : FunctionPass(ID) {
    }

    // Possibly eliminate loop L if it is dead.
    bool runOnFunction(Function &F) override;

  private:
    bool isLoopDead(Loop *L, SmallVectorImpl<BasicBlock *> &exitingBlocks,
                    SmallVectorImpl<BasicBlock *> &exitBlocks,
                    bool &Changed, BasicBlock *Preheader);

  };
}

char DxilLoopDeletion::ID = 0;
INITIALIZE_PASS(DxilLoopDeletion, "dxil-loop-deletion",
                "Delete dead loops", false, false)

FunctionPass *llvm::createDxilLoopDeletionPass() { return new DxilLoopDeletion(); }

static bool usedOutSideLoop(Instruction *I, Loop *L) {
  for (User *U : I->users()) {
    if (Instruction *UI = dyn_cast<Instruction>(U)) {
      if (L->contains(UI->getParent())) {
        return true;
      }
    }
  }
  return false;
}

/// isLoopDead - Determined if a loop is dead.  This assumes that we've already
/// checked for unique exit and exiting blocks, and that the code is in LCSSA
/// form.
bool DxilLoopDeletion::isLoopDead(Loop *L,
                              SmallVectorImpl<BasicBlock *> &exitingBlocks,
                              SmallVectorImpl<BasicBlock *> &exitBlocks,
                              bool &Changed, BasicBlock *Preheader) {
  BasicBlock *exitBlock = exitBlocks[0];

  // Make sure that all PHI entries coming from the loop are loop invariant.
  // Because the code is in LCSSA form, any values used outside of the loop
  // must pass through a PHI in the exit block, meaning that this check is
  // sufficient to guarantee that no loop-variant values are used outside
  // of the loop.
  BasicBlock::iterator BI = exitBlock->begin();
  while (PHINode *P = dyn_cast<PHINode>(BI)) {
    Value *incoming = P->getIncomingValueForBlock(exitingBlocks[0]);

    // Make sure all exiting blocks produce the same incoming value for the exit
    // block.  If there are different incoming values for different exiting
    // blocks, then it is impossible to statically determine which value should
    // be used.
    for (unsigned i = 1, e = exitingBlocks.size(); i < e; ++i) {
      if (incoming != P->getIncomingValueForBlock(exitingBlocks[i]))
        return false;
    }

    if (Instruction *I = dyn_cast<Instruction>(incoming))
      if (!L->makeLoopInvariant(I, Changed, Preheader->getTerminator()))
        if (usedOutSideLoop(I, L))
          return false;

    ++BI;
  }

  // Make sure that no instructions in the block have potential side-effects.
  // This includes instructions that could write to memory, and loads that are
  // marked volatile.  This could be made more aggressive by using aliasing
  // information to identify readonly and readnone calls.
  for (Loop::block_iterator LI = L->block_begin(), LE = L->block_end();
       LI != LE; ++LI) {
    for (BasicBlock::iterator BI = (*LI)->begin(), BE = (*LI)->end();
         BI != BE; ++BI) {
      if (BI->mayHaveSideEffects())
        return false;
    }
  }

  return true;
}

bool DxilLoopDeletion::runOnFunction(Function &F) {
  // Run loop simplify first to make sure loop invariant is moved so loop
  // deletion will not update the function if not delete.
  legacy::FunctionPassManager DeleteLoopPM(F.getParent());

  DeleteLoopPM.add(createLoopDeletionPass());
  bool bUpdated = false;

  legacy::FunctionPassManager SimpilfyPM(F.getParent());
  SimpilfyPM.add(createCFGSimplificationPass());
  SimpilfyPM.add(createDeadCodeEliminationPass());

  const unsigned kMaxIteration = 3;
  unsigned i=0;
  while (i<kMaxIteration) {
    if (!DeleteLoopPM.run(F))
      break;

    SimpilfyPM.run(F);
    i++;
    bUpdated = true;
  }

  return bUpdated;
}
