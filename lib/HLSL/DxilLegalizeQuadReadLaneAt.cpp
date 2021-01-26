///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSignature.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// DxilLegalizeQuadReadLaneAt implementation.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/DxilValueCache.h"

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilUtil.h"

using namespace llvm;

namespace {

struct DxilLegalizeQuadReadLaneAt : public ModulePass {
  static char ID;
  DxilLegalizeQuadReadLaneAt() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DxilValueCache>();
    AU.setPreservesAll();
  }
};

}

char DxilLegalizeQuadReadLaneAt::ID;

bool DxilLegalizeQuadReadLaneAt::runOnModule(Module &M) {
  if (!M.HasDxilModule())
    return false;

  DxilValueCache *DVC = &getAnalysis<DxilValueCache>();

  bool Changed = false;
  hlsl::DxilModule &DM = M.GetDxilModule();
  for (Function &F : M) {
    if (!F.isDeclaration())
      continue;

    if (!hlsl::OP::IsDxilOpFunc(&F))
      continue;
    hlsl::OP::OpCodeClass cls;

    hlsl::OP *op = DM.GetOP();
    if (!op->GetOpCodeClass(&F, cls))
      continue;

    if (cls != hlsl::DXIL::OpCodeClass::QuadReadLaneAt)
      continue;

    for (User *U : F.users()) {
      CallInst *CI = cast<CallInst>(U);

      Value *V = CI->getOperand(hlsl::DxilInst_QuadReadLaneAt::arg_quadLane);
      ConstantInt *C = dyn_cast<ConstantInt>(V);
      if (!C) {
        C = DVC->GetConstInt(V);
        if (C) {
          CI->setOperand(hlsl::DxilInst_QuadReadLaneAt::arg_quadLane, C);
          Changed = true;
        }
      }

      const StringRef kErrorMsg =
            "quadLaneId for QuadReadLaneAt must be a constnat value from 0 and 3. ";
            "Consider unrolling the loop manually and use -O3, "
            "it may help in some cases.\n";

      if (!C || C->getZExtValue() > 3) {
        hlsl::dxilutil::EmitErrorOnInstruction(CI, kErrorMsg);
      }
    }
  }

  return Changed;
}

ModulePass *llvm::createDxilLegalizeQuadReadLaneAtPass() {
  return new DxilLegalizeQuadReadLaneAt();
}

INITIALIZE_PASS_BEGIN(DxilLegalizeQuadReadLaneAt, "dxil-legalize-quad-read-lane-at",
                "DXIL legalize sample offset", false, false)
INITIALIZE_PASS_DEPENDENCY(DxilValueCache)
INITIALIZE_PASS_END(DxilLegalizeQuadReadLaneAt, "dxil-legalize-quad-read-lane-at",
                "DXIL legalize sample offset", false, false)

