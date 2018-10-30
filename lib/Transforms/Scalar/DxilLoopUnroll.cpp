
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

using namespace llvm;

namespace {

static void Unroll_RemapInstruction(Instruction *I,
                                    ValueToValueMapTy &VMap) {
  for (unsigned op = 0, E = I->getNumOperands(); op != E; ++op) {
    Value *Op = I->getOperand(op);
    ValueToValueMapTy::iterator It = VMap.find(Op);
    if (It != VMap.end())
      I->setOperand(op, It->second);
  }

  if (PHINode *PN = dyn_cast<PHINode>(I)) {
    for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
      ValueToValueMapTy::iterator It = VMap.find(PN->getIncomingBlock(i));
      if (It != VMap.end())
        PN->setIncomingBlock(i, cast<BasicBlock>(It->second));
    }
  }
}


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
  ValueToValueMapTy VarMap;

  ClonedIteration() {}
  ClonedIteration(ClonedIteration &&o) {
    for (auto Entry : o.VarMap) {
      VarMap[Entry.first] = Entry.second;
    }
    Exits = std::move(o.Exits);
    Body = std::move(o.Body);
    Latch = o.Latch;
    Header = o.Header;
    o.VarMap.clear();
  }

  void Erase() {
    for (BasicBlock *BB : Body) {
      BB->eraseFromParent();
    }
    Body.clear();
    for (BasicBlock *BB : Exits) {
      BB->eraseFromParent();
    }
    Exits.clear();
  }
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

static bool IsConstantI1(Value *V, bool *Val=nullptr) {
  if (ConstantInt *C = dyn_cast<ConstantInt>(V)) {
    if (V->getType() == Type::getInt1Ty(V->getContext())) {
      if (Val)
        *Val = (bool)C->getLimitedValue();
      return true;
    }
  }
  return false;
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

  BasicBlock *Header = L->getHeader();

  SmallVector<PHINode *, 16> PHIs;
  for (auto it = Header->begin(); it != Header->end(); it++) {
    if (PHINode *PN = dyn_cast<PHINode>(it)) {
      if (PN->getNumIncomingValues() != 2)
        return false;
      PHIs.push_back(PN);
    }
    else {
      break;
    }
  }

  SmallVector<ClonedIteration, 16> Clones;
  IRBuilder<> Builder(F->getContext());
  bool Succeeded = false;

  std::map<BasicBlock *, BasicBlock *> CloneMap;
  std::map<BasicBlock *, BasicBlock *> ReverseCloneMap;
  for (int i = 0; i < 16; i++) {
    CloneMap.clear();
    SmallVector<BasicBlock *, 16> ClonedBlocks;
    ClonedIteration Cloned;
    Value *NewCounter = Builder.getInt32(i);

    auto CloneBlock = [&Cloned, &CloneMap, &ReverseCloneMap, &ClonedBlocks, F](BasicBlock *BB) {
      BasicBlock *ClonedBB = CloneBasicBlock(BB, Cloned.VarMap);
      ClonedBB->insertInto(F);
      ClonedBlocks.push_back(ClonedBB);
      ReverseCloneMap[ClonedBB] = BB;
      CloneMap[BB] = ClonedBB;
      return ClonedBB;
    };

    for (BasicBlock *BB : L->getBlocks()) {
      BasicBlock *ClonedBB = CloneBlock(BB);
      Cloned.Body.push_back(ClonedBB);
      if (BB == Latch) {
        Cloned.Latch = ClonedBB;
      }
      if (BB == Header) {
        Cloned.Header = ClonedBB;
      }
    }
    for (BasicBlock *BB : ExitBlocks) {
      BasicBlock *ClonedBB = CloneBlock(BB);
      Cloned.Exits.push_back(ClonedBB);
      for (BasicBlock *Succ : successors(ClonedBB)) {
        for (Instruction &I : *Succ) {
          PHINode *PN = dyn_cast<PHINode>(&I);
          if (!PN)
            break;
          Value *OldIncoming = PN->getIncomingValueForBlock(BB);
          Value *NewIncoming = Cloned.VarMap[OldIncoming] ? Cloned.VarMap[OldIncoming] : OldIncoming;
          PN->addIncoming(NewIncoming, ClonedBB);
        }
      }
    }

    for (int i = 0; i < ClonedBlocks.size(); i++) {
      for (Instruction &I : *ClonedBlocks[i]) {
        Unroll_RemapInstruction(&I, Cloned.VarMap);
      }
    }

    for (int i = 0; i < ClonedBlocks.size(); i++) {
      BasicBlock *ClonedBB = ClonedBlocks[i];
      TerminatorInst *TI = ClonedBB->getTerminator();
      if (BranchInst *BI = dyn_cast<BranchInst>(TI)) {
        for (unsigned j = 0, NumSucc = BI->getNumSuccessors(); j < NumSucc; j++) {
          BasicBlock *OldSucc = BI->getSuccessor(j);
          if (CloneMap.count(OldSucc)) {
            BI->setSuccessor(j, CloneMap[OldSucc]);
          }
        }
      }
    }

    if (Clones.size()) {
      ClonedIteration &LastIteration = Clones.back();
      for (PHINode *PN : PHIs) {
        PHINode *ClonedPN = cast<PHINode>(Cloned.VarMap[PN]);
        Value *ReplacementVal = LastIteration.VarMap[PN->getIncomingValue(1)];
        ClonedPN->replaceAllUsesWith(ReplacementVal);
        ClonedPN->eraseFromParent();
        Cloned.VarMap[PN] = ReplacementVal;
      }

      if (BranchInst *BI = dyn_cast<BranchInst>(LastIteration.Latch->getTerminator())) {
        for (unsigned i = 0; i < BI->getNumSuccessors(); i++) {
          if (BI->getSuccessor(i) == LastIteration.Header) {
            BI->setSuccessor(i, Cloned.Header);
            break;
          }
        }
      }
    }
    else {
      for (PHINode *PN : PHIs) {
        PHINode *ClonedPN = cast<PHINode>(Cloned.VarMap[PN]);
        Value *ReplacementVal = ClonedPN->getIncomingValue(0);
        ClonedPN->replaceAllUsesWith(ReplacementVal);
        ClonedPN->eraseFromParent();
        Cloned.VarMap[PN] = ReplacementVal;
      }
    }

    Clones.push_back(std::move(Cloned));

    for (int i = 0; i < ClonedBlocks.size(); i++) {
      BasicBlock *ClonedBB = ClonedBlocks[i];
      SimplifyInstructionsInBlock(ClonedBB);
    }

    if (BranchInst *BI = dyn_cast<BranchInst>(Cloned.Latch->getTerminator())) {
      bool Cond = false;
      if (!IsConstantI1(BI->getCondition(), &Cond)) {
        break;
      }

      if (!Cond && BI->getSuccessor(0) == Cloned.Header) {
        Succeeded = true;
        break;
      }
      else if (Cond && BI->getSuccessor(1) == Cloned.Header) {
        Succeeded = true;
        break;
      }
    }
  }

  if (Succeeded) {
    SmallVector<BasicBlock *, 8> Preds(pred_begin(Header), pred_end(Header));
    for (BasicBlock *PredBB : Preds) {
      BranchInst *BI = cast<BranchInst>(PredBB->getTerminator());
      for (unsigned i = 0, NumSucc = BI->getNumSuccessors(); i < NumSucc; i++) {
        if (BI->getSuccessor(i) == Header) {
          BI->setSuccessor(i, Clones.front().Header);
        }
      }
    }
    llvm::removeUnreachableBlocks(*F);
  }

  else {
    llvm::removeUnreachableBlocks(*F);
    return false;
  }
  return true;
}

}

Pass *llvm::createDxilLoopUnrollPass() {
  return new DxilLoopUnroll();
}

INITIALIZE_PASS(DxilLoopUnroll, "dxil-loop-unroll", "Dxil Unroll loops", false, false)
