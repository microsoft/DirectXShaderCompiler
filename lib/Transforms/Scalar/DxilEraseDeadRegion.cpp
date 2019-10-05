//===- DxilEraseDeadRegion.cpp - Heuristically Remove Dead Region ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Overview:
//   1. Identify potentially dead regions by finding blocks with multiple
//      predecessors but no PHIs
//   2. Find common dominant ancestor of all the predecessors
//   3. Ensure original block post-dominates the ancestor
//   4. Ensure no instructions in the region have side effects (not including
//      original block and ancestor)
//   5. Remove all blocks in the region (excluding original block and ancestor)
//

#include "llvm/Pass.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"

using namespace llvm;

struct DxilEraseDeadRegion : public FunctionPass {
  static char ID;

  DxilEraseDeadRegion() : FunctionPass(ID) {
    initializeDxilEraseDeadRegionPass(*PassRegistry::getPassRegistry());
  }

  static bool FindDeadRegion(PostDominatorTree *PDT, BasicBlock *Begin, BasicBlock *End, std::set<BasicBlock *> &Region) {
    std::vector<BasicBlock *> WorkList;
    auto ProcessSuccessors = [&WorkList, Begin, End, &Region, PDT](BasicBlock *BB) {
      for (BasicBlock *Succ : successors(BB)) {
        if (Succ == End) continue;
        if (Region.count(Succ)) continue;
        if (Succ == Begin) return false; // If goes back to the beginning, there's a loop, give up.
        WorkList.push_back(Succ);
        Region.insert(Succ);
      }
      return true;
    };

    if (!ProcessSuccessors(Begin))
      return false;

    while (WorkList.size()) {
      BasicBlock *BB = WorkList.back();
      WorkList.pop_back();

      for (Instruction &I : *BB)
        if (I.mayHaveSideEffects())
          return false;

      if (!ProcessSuccessors(BB))
        return false;
    }

    return true;
  }

  static bool TrySimplify(DominatorTree *DT, PostDominatorTree *PDT, BasicBlock *BB) {
    if (BB->begin() != BB->end() && isa<PHINode>(BB->begin()))
      return false;

    std::vector<BasicBlock *> Predecessors(pred_begin(BB), pred_end(BB));
    if (Predecessors.size() < 2) return false;

    BasicBlock *Common = DT->findNearestCommonDominator(Predecessors[0], Predecessors[1]);
    if (!Common) return false;

    if (Common->getTerminator()->hasMetadataOtherThanDebugLoc())
      return false;

    for (unsigned i = 2; i < Predecessors.size(); i++) {
      Common = DT->findNearestCommonDominator(Common, Predecessors[i]);
      if (!Common) return false;
    }

    if (!DT->dominates(Common, BB))
      return false;
    if (!PDT->dominates(BB, Common))
      return false;

    std::set<BasicBlock *> Region;
    if (!FindDeadRegion(PDT, Common, BB, Region))
      return false;

    Common->getTerminator()->eraseFromParent();
    BranchInst::Create(BB, Common);

    for (BasicBlock *BB : Region) {
      for (Instruction &I : *BB)
        I.dropAllReferences();
      BB->dropAllReferences();
    }
    for (BasicBlock *BB : Region) {
      while (BB->begin() != BB->end())
        BB->begin()->eraseFromParent();
      BB->eraseFromParent();
    }

    return true;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<PostDominatorTree>();
  }

  bool runOnFunction(Function &F) override {
    auto *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    auto *PDT = &getAnalysis<PostDominatorTree>();

    std::set<BasicBlock *> FailedSet;
    bool Changed = false;
    while (1) {
      bool LocalChanged = false;
      for (Function::iterator It = F.begin(), E = F.end(); It != E; It++) {
        BasicBlock &BB = *It;
        if (FailedSet.count(&BB))
          continue;

        if (TrySimplify(DT, PDT, &BB)) {
          LocalChanged = true;
          break;
        }
        else {
          FailedSet.insert(&BB);
        }
      }

      Changed |= LocalChanged;
      if (!LocalChanged)
        break;
    }

    return Changed;
  }
};

char DxilEraseDeadRegion::ID;

Pass *llvm::createDxilEraseDeadRegionPass() {
  return new DxilEraseDeadRegion();
}

INITIALIZE_PASS_BEGIN(DxilEraseDeadRegion, "dxil-erase-dead-region", "Dxil Erase Dead Region", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(PostDominatorTree)
INITIALIZE_PASS_END(DxilEraseDeadRegion, "dxil-erase-dead-region", "Dxil Erase Dead Region", false, false)


