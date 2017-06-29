///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilRemoveDiscards.cpp                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides a pass to stomp a pixel shader's output color to a given         //
// constant value                                                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilSignatureElement.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "dxc/HLSL/DxilInstructions.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Local.h"
#include <memory>
#include <unordered_set>

using namespace llvm;
using namespace hlsl;

class DxilRemoveDiscards : public ModulePass {

  enum VisualizerInstrumentationMode
  {
    PRESERVE_ORIGINAL_INSTRUCTIONS,
    REMOVE_DISCARDS_AND_OPTIONALLY_OTHER_INSTRUCTIONS
  };


public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilRemoveDiscards() : ModulePass(ID) {}
  const char *getPassName() const override { return "DXIL Constant Color Mod"; }
  bool runOnModule(Module &M) override;
};

bool DxilRemoveDiscards::runOnModule(Module &M)
{
  // This pass removes all instances of the discard instruction within the shader.
  DxilModule &DM = M.GetOrCreateDxilModule();

  LLVMContext & Ctx = M.getContext();
  OP *HlslOP = DM.GetOP();
  Function * DiscardFunction = HlslOP->GetOpFunc(DXIL::OpCode::Discard, Type::getVoidTy(Ctx));
  auto DiscardFunctionUses = DiscardFunction->uses();

  bool Modified = false;

  for (auto FI = DiscardFunctionUses.begin(), FE = DiscardFunctionUses.end(); FI != FE; ) {
    auto & FunctionUse = *FI++;
    auto FunctionUser = FunctionUse.getUser();
    auto instruction = cast<Instruction>(FunctionUser);
    instruction->removeFromParent();
    delete instruction;
    Modified = true;
  }

  return Modified;
}

char DxilRemoveDiscards::ID = 0;

ModulePass *llvm::createDxilRemoveDiscardsPass() {
  return new DxilRemoveDiscards();
}

INITIALIZE_PASS(DxilRemoveDiscards, "hlsl-dxil-remove-discards", "HLSL DXIL Remove all discard instructions", false, false)
