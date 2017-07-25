///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilOutputColorBecomesConstant.cpp                                        //
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
#include <array>

using namespace llvm;
using namespace hlsl;

class DxilForceEarlyZ : public ModulePass {

  enum VisualizerInstrumentationMode
  {
    FromLiteralConstant,
    FromConstantBuffer
  };

  float Red = 1.f;
  float Green = 1.f;
  float Blue = 1.f;
  float Alpha = 1.f;
  VisualizerInstrumentationMode Mode = FromLiteralConstant;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilForceEarlyZ() : ModulePass(ID) {}
  const char *getPassName() const override { return "DXIL Constant Color Mod"; }
  bool runOnModule(Module &M) override;
};


bool DxilForceEarlyZ::runOnModule(Module &M)
{
  // This pass adds the force-early-z flag, if the shader has no uses of discard (which precludes early z)

  DxilModule &DM = M.GetOrCreateDxilModule();
  OP *HlslOP = DM.GetOP();
  LLVMContext & Ctx = M.getContext();

  bool Modified = false;

  if (HlslOP->GetOpFunc(DXIL::OpCode::Discard, Type::getVoidTy(Ctx))->user_empty()) {
    Modified = true;
    DM.m_ShaderFlags.SetForceEarlyDepthStencil(true);
  }

  return Modified;
}

char DxilForceEarlyZ::ID = 0;

ModulePass *llvm::createDxilForceEarlyZPass() {
  return new DxilForceEarlyZ();
}

INITIALIZE_PASS(DxilForceEarlyZ, "hlsl-dxil-force-early-z", "HLSL DXIL Force the early Z global flag, if shader has no discard calls", false, false)
