///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilNoOptLegalize.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "llvm/IR/Operator.h"

using namespace llvm;

class DxilNoOptLegalize : public ModulePass {
  SmallVector<Value *, 16> Worklist;

public:
  static char ID;
  DxilNoOptLegalize() : ModulePass(ID) {
    initializeDxilNoOptLegalizePass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override;
  bool RemoveStoreUndefsForPointer(Value *V);
  bool RemoveStoreUndefs(Module &M);
};
char DxilNoOptLegalize::ID;

bool DxilNoOptLegalize::RemoveStoreUndefsForPointer(Value *Ptr) {
  bool Changed = false;
  Worklist.clear();
  Worklist.push_back(Ptr);

  while (Worklist.size()) {
    Value *V = Worklist.back();
    Worklist.pop_back();
    if (isa<AllocaInst>(V) || isa<GlobalVariable>(V) || isa<GEPOperator>(V)) {
      for (User *U : V->users())
        Worklist.push_back(U);
    }
    else if (StoreInst *Store = dyn_cast<StoreInst>(V)) {
      if (isa<UndefValue>(Store->getValueOperand())) {
        Store->eraseFromParent();
        Changed = true;
      }
    }
  }

  return Changed;
}

bool DxilNoOptLegalize::RemoveStoreUndefs(Module &M) {
  bool Changed = false;
  for (GlobalVariable &GV : M.globals()) {
    Changed |= RemoveStoreUndefsForPointer(&GV);
  }
  for (Function &F : M) {
    if (F.empty())
      continue;

    BasicBlock &Entry = F.getEntryBlock();
    for (Instruction &I : Entry) {
      if (isa<AllocaInst>(&I))
        Changed |= RemoveStoreUndefsForPointer(&I);
    }
  }
  return Changed;
}

bool DxilNoOptLegalize::runOnModule(Module &M) {
  bool Changed = false;
  Changed |= RemoveStoreUndefs(M);
  return Changed;
}

ModulePass *llvm::createDxilNoOptLegalizePass() {
  return new DxilNoOptLegalize();
}

INITIALIZE_PASS(DxilNoOptLegalize, "dxil-o0-legalize", "DXIL No-Opt Legalize", false, false)

