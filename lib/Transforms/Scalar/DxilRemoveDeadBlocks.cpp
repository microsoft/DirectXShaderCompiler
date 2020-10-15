///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilRemoveDeadBlocks.cpp                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Pass to use value tracker to remove dead blocks.                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/CFG.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Analysis/DxilValueCache.h"

#include "dxc/DXIL/DxilMetadataHelper.h"

#include <unordered_set>

using namespace llvm;

static void RemoveIncomingValueFrom(BasicBlock *SuccBB, BasicBlock *BB) {
  for (auto inst_it = SuccBB->begin(); inst_it != SuccBB->end();) {
    Instruction *I = &*(inst_it++);
    if (PHINode *PN = dyn_cast<PHINode>(I))
      PN->removeIncomingValue(BB, true);
    else
      break;
  }
}


static bool EraseDeadBlocks(Function &F, DxilValueCache *DVC) {
  std::unordered_set<BasicBlock *> Seen;
  std::vector<BasicBlock *> WorkList;

  bool Changed = false;

  auto Add = [&WorkList, &Seen](BasicBlock *BB) {
    if (!Seen.count(BB)) {
      WorkList.push_back(BB);
      Seen.insert(BB);
    }
  };

  Add(&F.getEntryBlock());

  // Go through blocks
  while (WorkList.size()) {
    BasicBlock *BB = WorkList.back();
    WorkList.pop_back();

    if (BranchInst *Br = dyn_cast<BranchInst>(BB->getTerminator())) {
      if (Br->isUnconditional()) {
        BasicBlock *Succ = Br->getSuccessor(0);
        Add(Succ);
      }
      else {
        if (ConstantInt *C = DVC->GetConstInt(Br->getCondition())) {
          bool IsTrue = C->getLimitedValue() != 0;
          BasicBlock *Succ = Br->getSuccessor(IsTrue ? 0 : 1);
          BasicBlock *NotSucc = Br->getSuccessor(!IsTrue ? 0 : 1);

          Add(Succ);

          // Rewrite conditional branch as unconditional branch if
          // we don't have structural information that needs it to
          // be alive.
          if (!Br->getMetadata(hlsl::DXIL::kDxBreakMDName)) {
            BranchInst *NewBr = BranchInst::Create(Succ, BB);
            hlsl::DxilMDHelper::CopyMetadata(*NewBr, *Br);
            RemoveIncomingValueFrom(NotSucc, BB);

            Br->eraseFromParent();
            Br = nullptr;
            Changed = true;
          }
        }
        else {
          Add(Br->getSuccessor(0));
          Add(Br->getSuccessor(1));
        }
      }
    }
    else if (SwitchInst *Switch = dyn_cast<SwitchInst>(BB->getTerminator())) {
      for (unsigned i = 0; i < Switch->getNumSuccessors(); i++) {
        Add(Switch->getSuccessor(i));
      }
    }
  }

  if (Seen.size() == F.size())
    return Changed;

  std::vector<BasicBlock *> DeadBlocks;

  // Reconnect edges and everything
  for (auto it = F.begin(); it != F.end();) {
    BasicBlock *BB = &*(it++);
    if (Seen.count(BB))
      continue;

    DeadBlocks.push_back(BB);

    // Make predecessors branch somewhere else and fix the phi nodes
    for (auto pred_it = pred_begin(BB); pred_it != pred_end(BB);) {
      BasicBlock *PredBB = *(pred_it++);
      if (!Seen.count(PredBB)) // Don't bother fixing it if it's gonna get deleted anyway
        continue;
      TerminatorInst *TI = PredBB->getTerminator();
      if (!TI) continue;
      BranchInst *Br = dyn_cast<BranchInst>(TI);
      if (!Br || Br->isUnconditional()) continue;

      BasicBlock *Other = Br->getSuccessor(0) == BB ?
        Br->getSuccessor(1) : Br->getSuccessor(0);

      BranchInst *NewBr = BranchInst::Create(Other, Br);
      hlsl::DxilMDHelper::CopyMetadata(*NewBr, *Br);
      Br->eraseFromParent();
    }

    // Fix phi nodes in successors
    for (auto succ_it = succ_begin(BB); succ_it != succ_end(BB); succ_it++) {
      BasicBlock *SuccBB = *succ_it;
      if (!Seen.count(SuccBB)) continue; // Don't bother fixing it if it's gonna get deleted anyway
      RemoveIncomingValueFrom(SuccBB, BB);
    }

    // Erase all instructions in block
    while (BB->size()) {
      Instruction *I = &BB->back();
      if (!I->getType()->isVoidTy())
        I->replaceAllUsesWith(UndefValue::get(I->getType()));
      I->eraseFromParent();
    }
  }

  for (BasicBlock *BB : DeadBlocks) {
    BB->eraseFromParent();
  }

  return true;
}

namespace {

struct DxilRemoveDeadBlocks : public FunctionPass {
  static char ID;
  DxilRemoveDeadBlocks() : FunctionPass(ID) {
    initializeDxilRemoveDeadBlocksPass(*PassRegistry::getPassRegistry());
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DxilValueCache>();
  }
  bool runOnFunction(Function &F) override {
    DxilValueCache *DVC = &getAnalysis<DxilValueCache>();
    return EraseDeadBlocks(F, DVC);
  }
};

}

char DxilRemoveDeadBlocks::ID;

Pass *llvm::createDxilRemoveDeadBlocksPass() {
  return new DxilRemoveDeadBlocks();
}

INITIALIZE_PASS_BEGIN(DxilRemoveDeadBlocks, "dxil-remove-dead-blocks", "DXIL Remove Dead Blocks", false, false)
INITIALIZE_PASS_DEPENDENCY(DxilValueCache)
INITIALIZE_PASS_END(DxilRemoveDeadBlocks, "dxil-remove-dead-blocks", "DXIL Remove Dead Blocks", false, false)
