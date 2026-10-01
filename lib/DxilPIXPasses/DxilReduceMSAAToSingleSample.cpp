///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilReduceMSAAToSingleSample.cpp                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides a pass to reduce all MSAA writes to single-sample writes         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilOperations.h"

#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilResourceProperties.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "dxc/HLSL/DxilGenerationPass.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"

using namespace llvm;
using namespace hlsl;

class DxilReduceMSAAToSingleSample : public ModulePass {

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilReduceMSAAToSingleSample() : ModulePass(ID) {}
  StringRef getPassName() const override {
    return "HLSL DXIL Reduce all MSAA reads to single-sample reads";
  }
  bool runOnModule(Module &M) override;
};

static bool IsMultisampledSRVHandle(Value *TextureHandle, DxilModule &DM) {
  auto *TextureHandleInst = dyn_cast<CallInst>(TextureHandle);
  if (!TextureHandleInst)
    return false;

  if (OP::IsDxilOpFuncCallInst(TextureHandleInst, OP::OpCode::CreateHandle)) {
    DxilInst_CreateHandle CreateHandle(TextureHandleInst);
    if (!isa<ConstantInt>(CreateHandle.get_rangeId()))
      return false;

    if (static_cast<DXIL::ResourceClass>(
            CreateHandle.get_resourceClass_val()) != DXIL::ResourceClass::SRV)
      return false;

    unsigned RangeId =
        cast<ConstantInt>(CreateHandle.get_rangeId())->getLimitedValue();
    auto Resource = DM.GetSRV(RangeId);
    return Resource.GetKind() == DXIL::ResourceKind::Texture2DMS ||
           Resource.GetKind() == DXIL::ResourceKind::Texture2DMSArray;
  }

  // SM 6.6 handles carry the resource kind in the annotateHandle
  // properties operand.
  if (OP::IsDxilOpFuncCallInst(TextureHandleInst, OP::OpCode::AnnotateHandle)) {
    DxilInst_AnnotateHandle AnnotateHandle(TextureHandleInst);
    DxilResourceProperties ResourceProperties =
        resource_helper::loadPropsFromAnnotateHandle(AnnotateHandle,
                                                     *DM.GetShaderModel());
    return ResourceProperties.getResourceClass() == DXIL::ResourceClass::SRV &&
           (ResourceProperties.getResourceKind() ==
                DXIL::ResourceKind::Texture2DMS ||
            ResourceProperties.getResourceKind() ==
                DXIL::ResourceKind::Texture2DMSArray);
  }

  return false;
}

bool DxilReduceMSAAToSingleSample::runOnModule(Module &M) {
  DxilModule &DM = M.GetOrCreateDxilModule();

  OP *HlslOP = DM.GetOP();
  bool Modified = false;

  // Iterate every materialised TextureLoad overload; the 16-bit form
  // lowers Texture2DMS<half4>.Load.
  for (const auto &TextureLoadOverload :
       HlslOP->GetOpFuncList(DXIL::OpCode::TextureLoad)) {
    Function *TexLoadFunction = TextureLoadOverload.second;
    if (!TexLoadFunction)
      continue;

    for (auto FI = TexLoadFunction->use_begin();
         FI != TexLoadFunction->use_end();) {
      auto &FunctionUse = *FI++;
      auto *InstructionUser = dyn_cast<Instruction>(FunctionUse.getUser());
      if (!InstructionUser)
        continue;

      DxilInst_TextureLoad LoadInstruction(InstructionUser);
      if (!LoadInstruction)
        continue;

      if (IsMultisampledSRVHandle(LoadInstruction.get_srv(), DM)) {
        LoadInstruction.set_mipLevelOrSampleCount(HlslOP->GetI32Const(0));
        Modified = true;
      }
    }
  }

  return Modified;
}

char DxilReduceMSAAToSingleSample::ID = 0;

ModulePass *llvm::createDxilReduceMSAAToSingleSamplePass() {
  return new DxilReduceMSAAToSingleSample();
}

INITIALIZE_PASS(DxilReduceMSAAToSingleSample, "hlsl-dxil-reduce-msaa-to-single",
                "HLSL DXIL Reduce all MSAA writes to single-sample writes",
                false, false)
