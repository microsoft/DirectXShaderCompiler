

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

using namespace llvm;
//using namespace hlsl;

namespace {

class DxilLoopUnroll : public LoopPass {
public:
  static char ID;
  DxilLoopUnroll() : LoopPass(ID) {}
  bool runOnLoop(Loop *L, LPPassManager &LPM) override;
};

char DxilLoopUnroll::ID;

static bool SimplifyPHIs(Function &F) {
  bool Changed = false;
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      PHINode *PN = dyn_cast<PHINode>(&I);
      if (!PN)
        continue;

      if (PN->getNumIncomingValues() == 1) {
        Value *V = PN->getIncomingValue(0);
        PN->replaceAllUsesWith(V);
        Changed = true;
      }
    }
  }

  return Changed;
}

static void FindAllDataDependency(Instruction *I, std::set<Instruction *> &Set, std::set<BasicBlock *> &Blocks) {
  for (User *U : I->users()) {
    if (PHINode *PN = dyn_cast<PHINode>(U)) {
      continue;
    }
    else if (Instruction *UserI = dyn_cast<Instruction>(U)) {
      if (!Set.count(UserI)) {
        Set.insert(UserI);
        Blocks.insert(UserI->getParent());
        FindAllDataDependency(UserI, Set, Blocks);
      }
    }
  }
}

struct ClonedIteration {
  SmallVector<BasicBlock *, 16> Exits;
  SmallVector<BasicBlock *, 16> Body;
  BasicBlock *Latch = nullptr;
  BasicBlock *Header = nullptr;
};

static void ReplaceUsersIn(BasicBlock *BB, Value *Old, Value *New) {
  SmallVector<Use *, 16> Uses;
  for (Use &U : Old->uses()) {
    if (Instruction *I = dyn_cast<Instruction>(U.getUser())) {
      if (I->getParent() == BB) {
        Uses.push_back(&U);
      }
    }
  }
  
  for (Use *U : Uses) {
    U->set(New);
  }
}

bool DxilLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {
  Function *F = L->getBlocks()[0]->getParent();

  bool safe = L->isSafeToClone();
  BasicBlock *Latch = L->getLoopLatch();
  if (!Latch)
    return false;

  SimplifyPHIs(*F);

  PHINode *Counter = L->getCanonicalInductionVariable();
  std::set<Instruction *> DependentInst;
  std::set<BasicBlock *> DependentBlocks;
  FindAllDataDependency(Counter, DependentInst, DependentBlocks);

  SmallVector<BasicBlock *, 16> ToBeCloned;
  for (BasicBlock *BB : L->getBlocks()) {
    ToBeCloned.push_back(BB);
  }

  SmallVector<BasicBlock *, 16> ExitBlocks;
  L->getExitBlocks(ExitBlocks);
  for (BasicBlock *BB : ExitBlocks) {
    if (DependentBlocks.count(BB)) {
      ToBeCloned.push_back(BB);
    }
  }

  SmallVector<ClonedIteration, 16> Clones;
  IRBuilder<> Builder(F->getContext());
  for (int i = 0; i < 16; i++) {
    ClonedIteration Cloned;
    Value *NewCounter = Builder.getInt32(i);
    ValueToValueMapTy ValueMap;
    //ValueMap[Counter] = NewCounter;
    for (BasicBlock *BB : L->getBlocks()) {
      BasicBlock *ClonedBB = CloneBasicBlock(BB, ValueMap);
      ReplaceUsersIn(ClonedBB, Counter, NewCounter);
      Cloned.Body.push_back(ClonedBB);
      if (BB == Latch) {
        Cloned.Latch = ClonedBB;
      }
    }
    for (BasicBlock *BB : ExitBlocks) {
      if (!DependentBlocks.count(BB))
        continue;
      BasicBlock *ClonedBB = CloneBasicBlock(BB, ValueMap);
      ReplaceUsersIn(ClonedBB, Counter, NewCounter);
      Cloned.Exits.push_back(ClonedBB);
    }
  }

  return false;
}

}

Pass *llvm::createDxilLoopUnrollPass() {
  return new DxilLoopUnroll();
}

INITIALIZE_PASS(DxilLoopUnroll, "dxil-loop-unroll", "Dxil Unroll loops", false, false)
