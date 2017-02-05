//===-- DxilValidationRaceCondCheck.cpp - Dxil race condition check pass ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// \file
// Check race condition for Dxil Validation.
//
//===----------------------------------------------------------------------===//

#include "DxilTargetTransformInfo.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilOperations.h"
#include "DxilValidationRaceCondCheck.h"

#include "llvm/Analysis/PostDominators.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/Analysis/DivergenceAnalysis.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"


using namespace llvm;
using namespace hlsl;

namespace llvm {
void initializeRaceConditionCheckPassPass(PassRegistry&);

Pass *createRaceConditionCheckPass(std::vector<StoreInst *> *TGSMList,
                                   std::unordered_set<Function *> *FuncSet,
                                   RaceConditionCheckPass::ErrorEmitter *EE) {
  RaceConditionCheckPass *Checker = new RaceConditionCheckPass();
  Checker->m_pAcessList = TGSMList;
  Checker->m_pFixAddrTGSMFuncSet = FuncSet;
  Checker->m_pEmitError = EE;
  return Checker;
}
}

RaceConditionCheckPass::RaceConditionCheckPass() : FunctionPass(ID) {
  initializeRaceConditionCheckPassPass(*PassRegistry::getPassRegistry());
}

void RaceConditionCheckPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<PostDominatorTree>();
  AU.addRequired<DivergenceAnalysis>();
  AU.setPreservesAll();
}

bool RaceConditionCheckPass::runOnFunction(Function &F) {
  if (F.isDeclaration() || !m_pFixAddrTGSMFuncSet->count(&F))
    return false;

  auto &PDT = getAnalysis<PostDominatorTree>();
  auto &DA = getAnalysis<DivergenceAnalysis>();

  BasicBlock *Entry = &F.getEntryBlock();

  for (StoreInst *SI : *m_pAcessList) {
    BasicBlock *BB = SI->getParent();
    if (BB->getParent() == &F) {
      if (PDT.dominates(BB, Entry)) {
        if (DA.isDivergent(SI->getValueOperand()))
          (*m_pEmitError)(SI);
      }
    }
  }
  return false;
}


// Register this pass.
char RaceConditionCheckPass::ID = 0;
INITIALIZE_PASS_BEGIN(RaceConditionCheckPass, "dxilracecondcheck",
                      "Dxil race condition check", false, true)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(PostDominatorTree)
INITIALIZE_PASS_DEPENDENCY(DivergenceAnalysis)
INITIALIZE_PASS_END(RaceConditionCheckPass, "dxilracecondcheck",
                    "Dxil race condition check", false, true)
