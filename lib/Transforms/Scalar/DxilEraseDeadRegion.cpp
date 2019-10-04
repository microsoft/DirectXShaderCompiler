
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/PostDominators.h"

using namespace llvm;

struct DxilEraseDeadRegion : public FunctionPass {
  static char ID;

  DxilEraseDeadRegion() : FunctionPass(ID) {
    initializeDxilEraseDeadRegionPass(*PassRegistry::getPassRegistry());
  }

  static bool FindRegion(PostDominatorTree *PDT, BasicBlock *Begin, BasicBlock *End, std::set<BasicBlock *> &Region) {
    std::vector<BasicBlock *> WorkList;
    auto ProcessSuccessors = [&WorkList, Begin, End, &Region, PDT](BasicBlock *BB) {
      for (BasicBlock *Succ : successors(BB)) {
        if (Succ == End) continue;
        if (Region.count(Succ)) continue;
        if (Succ == Begin) return false; // If goes back to the beginning, there's a loop, give up.
        if (!PDT->dominates(End, Succ)) return false;
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

      if (!ProcessSuccessors(BB))
        return false;
    }

    return true;
  }

  static bool TrySimplify(DominatorTree *DT, PostDominatorTree *PDT, BasicBlock *BB) {
    for (Instruction &I : *BB)
      if (isa<PHINode>(&I))
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

    std::set<BasicBlock *> Region;
    if (!FindRegion(PDT, Common, BB, Region))
      return false;

    for (BasicBlock *BB : Region) {
      for (Instruction &I : *BB)
        if (I.mayHaveSideEffects())
          return false;
    }

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

    bool Changed = false;
    while (1) {
      bool LocalChanged = false;
      for (BasicBlock &BB : F) {
        if (TrySimplify(DT, PDT, &BB)) {
          LocalChanged = true;
          break;
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


