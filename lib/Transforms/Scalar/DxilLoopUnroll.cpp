
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "dxc/DXIL/DxilUtil.h"

#include <set>

using namespace llvm;

namespace {

// Replace this with the stock llvm one.
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
  LoopInfo *LI = nullptr;
  DxilLoopUnroll() : LoopPass(ID) {}
  bool runOnLoop(Loop *L, LPPassManager &LPM) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    //AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    AU.addRequiredID(LoopSimplifyID);
    //AU.addRequiredID(createLoopRotatePass);
    //AU.addPreservedID(LoopSimplifyID);
    //AU.addRequiredID(LCSSAID);
    //AU.addPreservedID(LCSSAID);
    //AU.addRequired<ScalarEvolution>();
    //AU.addPreserved<ScalarEvolution>();
    //AU.addRequired<TargetTransformInfoWrapperPass>();
    // FIXME: Loop unroll requires LCSSA. And LCSSA requires dom info.
    // If loop unroll does not preserve dom info then LCSSA pass on next
    // loop will receive invalid dom info.
    // For now, recreate dom info, if loop is unrolled.
    AU.addPreserved<DominatorTreeWrapperPass>();
  }
};

char DxilLoopUnroll::ID;

static bool SimplifyPHIs(BasicBlock *BB) {
  bool Changed = false;
  for (Instruction &I : *BB) {
    PHINode *PN = dyn_cast<PHINode>(&I);
    if (!PN)
      continue;

    if (PN->getNumIncomingValues() == 1) {
      Value *V = PN->getIncomingValue(0);
      PN->replaceAllUsesWith(V);
      Changed = true;
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
  SmallVector<BasicBlock *, 16> Body;
  BasicBlock *Latch = nullptr;
  BasicBlock *Header = nullptr;
  ValueToValueMapTy VarMap;

  ClonedIteration(const ClonedIteration &o) {
    Body = o.Body;
    Latch = o.Latch;
    for (ValueToValueMapTy::const_iterator It = o.VarMap.begin(); It != o.VarMap.end(); It++)
      VarMap[It->first] = It->second;
  }
  ClonedIteration() {}
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

// Figure out what to do with this.
static bool SimplifyInstructionsInBlock_NoDelete(BasicBlock *BB,
                                       const TargetLibraryInfo *TLI) {
  bool MadeChange = false;

#ifndef NDEBUG
  // In debug builds, ensure that the terminator of the block is never replaced
  // or deleted by these simplifications. The idea of simplification is that it
  // cannot introduce new instructions, and there is no way to replace the
  // terminator of a block without introducing a new instruction.
  AssertingVH<Instruction> TerminatorVH(--BB->end());
#endif

  for (BasicBlock::iterator BI = BB->begin(), E = --BB->end(); BI != E; ) {
    assert(!BI->isTerminator());
    Instruction *Inst = BI++;

    WeakVH BIHandle(BI);
    if (recursivelySimplifyInstruction(Inst, TLI)) {
      MadeChange = true;
      if (BIHandle != BI)
        BI = BB->begin();
      continue;
    }

//    MadeChange |= RecursivelyDeleteTriviallyDeadInstructions(Inst, TLI);
    if (BIHandle != BI)
      BI = BB->begin();
  }
  return MadeChange;
}

static bool HasUnrollElements(BasicBlock *BB, Loop *L) {
  for (Instruction &I : *BB) {
    if (LoadInst *Load = dyn_cast<LoadInst>(&I)) {
      Value *PtrV = Load->getPointerOperand();
      if (hlsl::dxilutil::IsHLSLObjectType(PtrV->getType()->getPointerElementType())) {
        if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(PtrV)) {
          if (GEP->hasAllConstantIndices())
            continue;
          for (auto It = GEP->idx_begin(); It != GEP->idx_end(); It++) {
            Value *Idx = *It;
            if (!L->isLoopInvariant(Idx)) {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

bool IsMarkedFullUnroll(Loop *L) {
  if (MDNode *LoopID = L->getLoopID())
    return GetUnrollMetadata(LoopID, "llvm.loop.unroll.full");
  return false;
}

static bool HeuristicallyDetermineUnrollNecessary(Loop *L) {
  Module *M = L->getBlocks().front()->getParent()->getParent();
  SmallPtrSet<BasicBlock *, 10> BlocksInLoop;
  SmallPtrSet<Value *, 16> GlobalVariables;
  for (GlobalVariable &GV : M->globals()) {
    GlobalVariables.insert(&GV);
  }
  for (BasicBlock *BB : L->getBlocks()) {
    BlocksInLoop.insert(BB);
  }

  bool Needunroll = false;
  for (BasicBlock *BB : L->getBlocks()) {
    if (HasUnrollElements(BB, L))
      return true;
  }

  SmallVector<BasicBlock *, 16> ExitBlocks;
  L->getExitBlocks(ExitBlocks);
  for (BasicBlock *BB : ExitBlocks)
    if (HasUnrollElements(BB, L))
      return true;
  return false;
}

static bool HasSuccessorsInLoop(BasicBlock *BB, Loop *L) {
  bool PartOfOuterLoop = false;
  for (BasicBlock *Succ : successors(BB)) {
    if (L->contains(Succ)) {
      return true;
    }
  }
  return false;
}

static void ParepareBlockForRemoval(BasicBlock *BB) {
  SmallVector<BasicBlock *, 16> Successors(succ_begin(BB), succ_end(BB));
  for (BasicBlock *Succ : Successors) {
    Succ->removePredecessor(BB);
  }
  //BB->getTerminator()->eraseFromParent();
  BB->dropAllReferences();
}

bool DxilLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {
  if (!IsMarkedFullUnroll(L))
    return false;
  if (!L->isSafeToClone())
    return false;

  LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  Function *F = L->getBlocks()[0]->getParent();

  BasicBlock *Latch = L->getLoopLatch();
  BasicBlock *Header = L->getHeader();
  SmallVector<BasicBlock *, 16> ExitBlocks;
  L->getExitBlocks(ExitBlocks);

  SmallVector<BasicBlock *, 16> BlocksInLoop(L->getBlocks().begin(), L->getBlocks().end());
  BlocksInLoop.append(ExitBlocks.begin(), ExitBlocks.end());

  // Quit if we don't have a single latch block
  if (!Latch)
    return false;

  // TODO: See if possible to do this without requiring loop rotation.
  // If the loop exit condition is not in the latch, then the loop is not rotated. Give up.
  if (!cast<BranchInst>(Latch->getTerminator())->isConditional())
    return false;

  // Simplify the PHI nodes that have single incoming value.
  for (BasicBlock *BB : BlocksInLoop)
    SimplifyPHIs(BB);

  //ExitBlocks.clear();
  SmallPtrSet<BasicBlock *, 16> ExitBlocksSet;

#if 0
  // Determine if we absolutely
  if (!HeuristicallyDetermineUnrollNecessary(L))
    return false;
#endif

  // Keep track of the PHI nodes at the header.
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

  std::set<BasicBlock *> ToBeCloned;
  for (BasicBlock *BB : L->getBlocks())
    ToBeCloned.insert(BB);
  for (BasicBlock *BB : ExitBlocks) {
    // TODO: See if this needs to be cloned
    ToBeCloned.insert(BB);
    ExitBlocksSet.insert(BB);
  }

  SmallVector<ClonedIteration, 16> Clones;
  SmallVector<BasicBlock *, 16> ClonedBlocks;
  bool Succeeded = false;

  std::map<BasicBlock *, BasicBlock *> CloneMap;
  std::map<BasicBlock *, BasicBlock *> ReverseCloneMap;
  for (int i = 0; i < 16; i++) { // TODO: Num of iterations
    ClonedBlocks.clear();
    CloneMap.clear();
    ClonedIteration *PrevIteration = nullptr;
    if (Clones.size())
      PrevIteration = &Clones.back();

    Clones.resize(Clones.size() + 1);
    ClonedIteration &Cloned = Clones.back();

    // Helper function for cloning a block
    auto CloneBlock = [&ToBeCloned, &Cloned, &CloneMap, &ReverseCloneMap, &ClonedBlocks, F](BasicBlock *BB) {
      BasicBlock *ClonedBB = CloneBasicBlock(BB, Cloned.VarMap);
      ClonedBlocks.push_back(ClonedBB);
      ReverseCloneMap[ClonedBB] = BB;
      CloneMap[BB] = ClonedBB;
      ClonedBB->insertInto(F);
      Cloned.VarMap[BB] = ClonedBB;

      // If branching to outside of the loop, need to update the
      // phi nodes there to include incoming values.
      for (BasicBlock *Succ : successors(ClonedBB)) {
        if (ToBeCloned.count(Succ))
          continue;
        for (Instruction &I : *Succ) {
          PHINode *PN = dyn_cast<PHINode>(&I);
          if (!PN)
            break;
          Value *OldIncoming = PN->getIncomingValueForBlock(BB);
          Value *NewIncoming = OldIncoming;
          if (Cloned.VarMap.count(OldIncoming)) {
            NewIncoming = Cloned.VarMap[OldIncoming];
          }
          PN->addIncoming(NewIncoming, ClonedBB);
        }
      }

      return ClonedBB;
    };

    for (BasicBlock *BB : ToBeCloned) {
      BasicBlock *ClonedBB = CloneBlock(BB);
      Cloned.Body.push_back(ClonedBB);
      if (BB == Latch) {
        Cloned.Latch = ClonedBB;
      }
      if (BB == Header) {
        Cloned.Header = ClonedBB;
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
          if (CloneMap.count(OldSucc)) { // TODO: Do one query
            BI->setSuccessor(j, CloneMap[OldSucc]);
          }
        }
      }
    }

    // If this is the first block
    if (!PrevIteration) {
      // Replace the phi nodes in the clone block with the values coming
      // from outside of the loop
      for (PHINode *PN : PHIs) {
        PHINode *ClonedPN = cast<PHINode>(Cloned.VarMap[PN]);
        Value *ReplacementVal = ClonedPN->getIncomingValue(0); // TODO: Actually find the right one, also make sure there's only a single one.
        ClonedPN->replaceAllUsesWith(ReplacementVal);
        ClonedPN->eraseFromParent();
        Cloned.VarMap[PN] = ReplacementVal;
      }
    }
    else {
      for (PHINode *PN : PHIs) {
        PHINode *ClonedPN = cast<PHINode>(Cloned.VarMap[PN]);
        Value *ReplacementVal = PrevIteration->VarMap[PN->getIncomingValue(1)]; // TODO: Actually find the right one, also make sure there's only a single one.
        ClonedPN->replaceAllUsesWith(ReplacementVal);
        ClonedPN->eraseFromParent();
        Cloned.VarMap[PN] = ReplacementVal;
      }

      // Make the latch of the previous iteration branch to the header
      // of this new iteration.
      if (BranchInst *BI = dyn_cast<BranchInst>(PrevIteration->Latch->getTerminator())) {
        for (unsigned i = 0; i < BI->getNumSuccessors(); i++) {
          if (BI->getSuccessor(i) == PrevIteration->Header) {
            BI->setSuccessor(i, Cloned.Header);
            break;
          }
        }
      }
    }

    for (int i = 0; i < ClonedBlocks.size(); i++) {
      BasicBlock *ClonedBB = ClonedBlocks[i];
      SimplifyInstructionsInBlock_NoDelete(ClonedBB, NULL);
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
    // Go through the predecessors of the old header and
    // make them branch to the new header.
    SmallVector<BasicBlock *, 8> Preds(pred_begin(Header), pred_end(Header));
    for (BasicBlock *PredBB : Preds) {
      if (L->contains(PredBB))
        continue;
      BranchInst *BI = cast<BranchInst>(PredBB->getTerminator());
      for (unsigned i = 0, NumSucc = BI->getNumSuccessors(); i < NumSucc; i++) {
        if (BI->getSuccessor(i) == Header) {
          BI->setSuccessor(i, Clones.front().Header);
        }
      }
    }

    Loop *OuterL = L->getParentLoop();
    // If there's an outer loop, insert the new blocks
    // into
    if (OuterL) {
      auto &FirstIteration = Clones.front();
      for (auto &Iteration : Clones) {
        for (BasicBlock *BB :Iteration.Body) {
          if (HasSuccessorsInLoop(BB, OuterL)) // TODO: Fix this. It still has return blocks being added to the outer loop.
            OuterL->addBasicBlockToLoop(BB, *LI);
        }
      }

      for (BasicBlock *BB : ToBeCloned) {
        if (OuterL->contains(BB))
          OuterL->removeBlockFromLoop(BB);
      }
      // TODO: Simplify here, since outer loop is now weird (multiple latches etc).
      //simplifyLoop(OuterL, nullptr, LI, nullptr);
    }

    // Remove flattened loop from queue.
    // If there's an outer loop, this will also take care
    // of removing blocks.
    LPM.deleteLoopFromQueue(L); // TODO: Figure out the impact of this.
    // TODO: Update dominator tree

    for (BasicBlock *BB : ToBeCloned)
      ParepareBlockForRemoval(BB);
    for (BasicBlock *BB : ToBeCloned)
      BB->eraseFromParent();

    return true;
  }

  // If we were unsuccessful in unrolling the loop
  else {
    // Remove all the cloned blocks
    for (ClonedIteration &Iteration : Clones)
      for (BasicBlock *BB : Iteration.Body)
        ParepareBlockForRemoval(BB);
    for (ClonedIteration &Iteration : Clones)
      for (BasicBlock *BB : Iteration.Body)
        BB->eraseFromParent();
    return false;
  }
}

}

Pass *llvm::createDxilLoopUnrollPass() {
  return new DxilLoopUnroll();
}

INITIALIZE_PASS(DxilLoopUnroll, "dxil-loop-unroll", "Dxil Unroll loops", false, false)
