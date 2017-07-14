///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilAddPixelHitInstrumentation.cpp                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides a pass to add instrumentation to determine pixel hit count and   //
// cost. Used by PIX.                                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilSignatureElement.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "dxc/HLSL/DxilConstants.h"
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

class DxilAddPixelHitInstrumentation : public ModulePass {

  bool ForceEarlyZ = false;
  bool AddPixelCost = false;
  int RTWidth = 1024;
  int NumPixels = 128;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilAddPixelHitInstrumentation() : ModulePass(ID) {}
  const char *getPassName() const override { return "DXIL Constant Color Mod"; }
  void applyOptions(PassOptions O) override;
  bool runOnModule(Module &M) override;
};

void DxilAddPixelHitInstrumentation::applyOptions(PassOptions O)
{
  for (const auto & option : O)
  {
    if (0 == option.first.compare("force-early-z"))
    {
      ForceEarlyZ = atoi(option.second.data()) != 0;
    }
    else if (0 == option.first.compare("rt-width"))
    {
      RTWidth = atoi(option.second.data());
    }
    else if (0 == option.first.compare("num-pixels"))
    {
      NumPixels = atoi(option.second.data());
    }
    else if (0 == option.first.compare("add-pixel-cost"))
    {
      AddPixelCost = atoi(option.second.data()) != 0;
    }
  }
}

bool DxilAddPixelHitInstrumentation::runOnModule(Module &M)
{
  // This pass adds instrumentation for pixel hit counting and pixel cost

  DxilModule &DM = M.GetOrCreateDxilModule();
  //LLVMContext & Ctx = M.getContext();
  //OP *HlslOP = DM.GetOP();

  if (ForceEarlyZ)
  {
    DM.m_ShaderFlags.SetForceEarlyDepthStencil(true);
  }
  
  hlsl::DxilSignature & InputSignature = DM.GetInputSignature();

  auto & InputElements = InputSignature.GetElements();
  if (std::find_if(InputElements.begin(), InputElements.end(), [](const std::unique_ptr<DxilSignatureElement> & Element) {
    return Element->GetSemantic()->GetKind() == hlsl::DXIL::SemanticKind::Position; }) 
    == InputElements.end()) {

    auto SVPosition = std::make_unique<DxilSignatureElement>(DXIL::SigPointKind::PSIn);
    SVPosition->Initialize("Position", hlsl::CompType::getF32(), hlsl::DXIL::InterpolationMode::Linear, 1, 2);
    InputSignature.AppendElement(std::move(SVPosition));
  }

  DM.GetGlobalFlags();

  auto EntryPointFunction = DM.GetEntryFunction();

  auto & EntryBlock = EntryPointFunction->getEntryBlock();

  auto & Instructions = EntryBlock.getInstList();
  auto It = Instructions.begin();
  while(It != Instructions.end()) {
    auto ThisInstruction = It++;
    LlvmInst_Ret Ret(ThisInstruction);
    if (Ret) {

    }
  }

  bool Modified = false;

  return Modified;
}

char DxilAddPixelHitInstrumentation::ID = 0;

ModulePass *llvm::createDxilAddPixelHitInstrumentationPass() {
  return new DxilAddPixelHitInstrumentation();
}

INITIALIZE_PASS(DxilAddPixelHitInstrumentation, "hlsl-dxil-add-pixel-hit-instrmentation", "DXIL Count completed PS invocations and costs", false, false)
