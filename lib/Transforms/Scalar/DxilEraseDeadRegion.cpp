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
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/ADT/SetVector.h"

#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilMetadataHelper.h"
#include "dxc/HLSL/DxilNoops.h"

#include <unordered_map>
#include <unordered_set>

using namespace llvm;
using namespace hlsl;

// TODO: Could probably move this to a common place at some point.
namespace {

struct MiniDCE {
  // Use a set vector because the same value could be added more than once, which
  // could lead to double free.
  SetVector<Instruction *> Worklist;
  void EraseAndProcessOperands(Instruction *TopI);
};

void MiniDCE::EraseAndProcessOperands(Instruction *TopI) {
  Worklist.clear();
  for (Value *Op : TopI->operands()) {
    if (Instruction *OpI = dyn_cast<Instruction>(Op))
      Worklist.insert(OpI);
  }
  TopI->eraseFromParent();
  TopI = nullptr;

  while (Worklist.size()) {
    Instruction *I = Worklist.pop_back_val();
    if (llvm::isInstructionTriviallyDead(I)) {
      for (Value *Op : I->operands()) {
        if (Instruction *OpI = dyn_cast<Instruction>(Op))
          Worklist.insert(OpI);
      }
      I->eraseFromParent();
    }
  }
}

}

struct DxilEraseDeadRegion : public FunctionPass {
  static char ID;

  DxilEraseDeadRegion() : FunctionPass(ID) {
    initializeDxilEraseDeadRegionPass(*PassRegistry::getPassRegistry());
  }

  std::unordered_map<BasicBlock *, bool> m_HasSideEffect;
  MiniDCE m_DCE;

  bool HasSideEffects(BasicBlock *BB) {
    auto FindIt = m_HasSideEffect.find(BB);
    if (FindIt != m_HasSideEffect.end()) {
      return FindIt->second;
    }

    for (Instruction &I : *BB)
      if (I.mayHaveSideEffects() && !hlsl::IsNop(&I)) {
        m_HasSideEffect[BB] = true;
        return true;
      }

    m_HasSideEffect[BB] = false;
    return false;
  }
  bool IsEmptySelfLoop(BasicBlock *BB) {
    // Make sure all inst not used outside BB.
    for (Instruction &I : *BB) {
      for (User *U : I.users()) {
        if (Instruction *UI = dyn_cast<Instruction>(U)) {
          if (UI->getParent() != BB)
            return false;
        }
      }

      if (!I.mayHaveSideEffects())
        continue;

      if (CallInst *CI = dyn_cast<CallInst>(&I)) {
        if (hlsl::OP::IsDxilOpFuncCallInst(CI)) {
          DXIL::OpCode opcode = hlsl::OP::GetDxilOpFuncCallInst(CI);
          // Wave Ops are marked has side effect to avoid move cross control flow.
          // But they're safe to remove if unused.
          if (hlsl::OP::IsDxilOpWave(opcode))
            continue;
        }
      }

      return false;
    }
    return true;
  }
  bool FindDeadRegion(BasicBlock *Begin, BasicBlock *End, std::set<BasicBlock *> &Region) {
    std::vector<BasicBlock *> WorkList;
    auto ProcessSuccessors = [this, &WorkList, Begin, End, &Region](BasicBlock *BB) {
      for (BasicBlock *Succ : successors(BB)) {
        if (Succ == End) continue;
        if (Succ == Begin) return false; // If goes back to the beginning, there's a loop, give up.
        if (Region.count(Succ)) continue;
        if (this->HasSideEffects(Succ)) return false; // Give up if the block may have side effects

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

    return Region.size() != 0;
  }

  static bool IsMetadataKind(LLVMContext &Ctx, unsigned TargetID, StringRef MDKind) {
    unsigned ID = 0;
    if (Ctx.findMDKindID(MDKind, &ID))
      return TargetID == ID;
    return false;
  }

  static bool HasUnsafeMetadata(Instruction *I) {
    SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
    I->getAllMetadata(MDs);

    LLVMContext &Context = I->getContext();
    for (auto &p : MDs) {
      if (p.first == (unsigned)LLVMContext::MD_dbg)
        continue;
      if (IsMetadataKind(Context, p.first, DxilMDHelper::kDxilControlFlowHintMDName))
        continue;
      return true;
    }

    return false;
  }

  bool TrySimplify(DominatorTree *DT, PostDominatorTree *PDT, BasicBlock *BB) {
    // Give up if BB has any Phis
    if (BB->begin() != BB->end() && isa<PHINode>(BB->begin()))
      return false;

    std::vector<BasicBlock *> Predecessors(pred_begin(BB), pred_end(BB));
    if (Predecessors.size() < 2) return false;

    // Give up if BB is a self loop
    for (BasicBlock *PredBB : Predecessors)
      if (PredBB == BB) {
        if (!IsEmptySelfLoop(BB)) {
          return false;
        } else if (Predecessors.size() != 2) {
          return false;
        } else {
          BasicBlock *PredBB0 = Predecessors[0];
          BasicBlock *PredBB1 = Predecessors[1];
          BasicBlock *LoopBB = PredBB;
          BasicBlock *LoopPrevBB = PredBB == PredBB0? PredBB1 : PredBB0;
          // Remove LoopBB, LoopPrevBB branch to succ of LoopBB.
          BranchInst *BI = cast<BranchInst>(LoopBB->getTerminator());

          if (BI->getNumSuccessors() != 2)
            return false;

          BasicBlock *Succ0 = BI->getSuccessor(0);
          BasicBlock *Succ1 = BI->getSuccessor(1);
          BasicBlock *NextBB = Succ0 == PredBB ? Succ1 : Succ0;
          // Make sure it is not a dead loop.
          if (NextBB == LoopPrevBB || NextBB == BB)
            return false;

          LoopPrevBB->getTerminator()->eraseFromParent();
          BranchInst::Create(NextBB, LoopPrevBB);
          return true;
        }
      }

    // Find the common ancestor of all the predecessors
    BasicBlock *Common = DT->findNearestCommonDominator(Predecessors[0], Predecessors[1]);
    if (!Common) return false;
    for (unsigned i = 2; i < Predecessors.size(); i++) {
      Common = DT->findNearestCommonDominator(Common, Predecessors[i]);
      if (!Common) return false;
    }

   // If there are any metadata on Common block's branch, give up.
    if (HasUnsafeMetadata(Common->getTerminator()))
      return false;

    if (!DT->properlyDominates(Common, BB))
      return false;
    if (!PDT->properlyDominates(BB, Common))
      return false;

    std::set<BasicBlock *> Region;
    if (!this->FindDeadRegion(Common, BB, Region))
      return false;

    // If BB branches INTO the region, forming a loop give up.
    for (BasicBlock *Succ : successors(BB))
      if (Region.count(Succ))
        return false;

    // Replace Common's branch with an unconditional branch to BB
    m_DCE.EraseAndProcessOperands(Common->getTerminator());
    BranchInst::Create(BB, Common);

    // Delete the region
    for (BasicBlock *BB : Region) {
      while (BB->begin() != BB->end()) {
        Instruction *I = &BB->back();
        if (!I->user_empty())
          I->replaceAllUsesWith(UndefValue::get(I->getType()));
        m_DCE.EraseAndProcessOperands(I);
      }
    }
    for (BasicBlock *BB : Region) {
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
      for (Function::iterator It = F.begin(), E = F.end(); It != E; It++) {
        BasicBlock &BB = *It;
        if (this->TrySimplify(DT, PDT, &BB)) {
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


