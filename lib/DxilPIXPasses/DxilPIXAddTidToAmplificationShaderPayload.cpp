///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilOutputColorBecomesConstant.cpp                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides a pass to turn on the early-z flag                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilUtil.h"

#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Transforms/Utils/Local.h"

#include "PixPassHelpers.h"

using namespace llvm;
using namespace hlsl;

class DxilPIXAddTidToAmplificationShaderPayload : public ModulePass {

public:
  static char ID; // Pass identification, replacement for typeid
  DxilPIXAddTidToAmplificationShaderPayload() : ModulePass(ID) {}
  const char *getPassName() const override { return "DXIL Add flat thread id to payload from AS to MS"; }
  bool runOnModule(Module &M) override;
};

bool DxilPIXAddTidToAmplificationShaderPayload::runOnModule(Module &M) {

  DxilModule &DM = M.GetOrCreateDxilModule();
  LLVMContext &Ctx = M.getContext();
  OP *HlslOP = DM.GetOP();

  Type* OriginalPayloadStructPointerType = nullptr;
  Type* OriginalPayloadStructType = nullptr;
  Type* ExpandedPayloadStructType = nullptr;
  for (inst_iterator I = inst_begin(PIXPassHelpers::GetEntryFunction(DM)),
                     E = inst_end(PIXPassHelpers::GetEntryFunction(DM));
       I != E; ++I) {
      if (auto* Instr = llvm::cast<Instruction>(&*I)) {
          if (hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::DispatchMesh))
          {
              DxilInst_DispatchMesh DispatchMesh(Instr);
              OriginalPayloadStructPointerType = DispatchMesh.get_payload()->getType();
              OriginalPayloadStructType = OriginalPayloadStructPointerType->getPointerElementType();
              auto* PayloadTypeAsStruct = llvm::cast<llvm::StructType>(OriginalPayloadStructType);
              SmallVector<Type*, 16> Elements;
              for (unsigned int e = 0; e < PayloadTypeAsStruct->getNumElements();
                  ++e) {
                  Elements.push_back(PayloadTypeAsStruct->getElementType(e));
              }
              // This adds an int432 in order to pass flat thread id to the mesh shader:
              Elements.push_back(Type::getInt32Ty(Ctx));
              ExpandedPayloadStructType = StructType::create(Ctx, Elements, "PIX_AS2MS_Expanded_Type");
          }
      }
  }

    for (inst_iterator I = inst_begin(PIXPassHelpers::GetEntryFunction(DM)),
                     E = inst_end(PIXPassHelpers::GetEntryFunction(DM));
       I != E; ++I) {
        auto* Inst = &*I;
      if (llvm::isa<AllocaInst>(Inst)) {
          auto* Alloca = llvm::cast<AllocaInst>(Inst);
          if (Alloca->getType() == OriginalPayloadStructType)
            {
          llvm::IRBuilder<> B(Alloca->getContext());

              auto* NewAlloca = B.CreateAlloca(ExpandedPayloadStructType, HlslOP->GetU32Const(1) ,"NewPayload");
              (void)NewAlloca;
              NewAlloca->insertAfter(Alloca);
              break;
            }
      }
  }
#if 0
  auto F = HlslOP->GetOpFunc(DXIL::OpCode::DispatchMesh, OriginalPayloadStructType);
  auto FunctionUses = F->uses();
  for (auto FI = FunctionUses.begin(); FI != FunctionUses.end();) {
      auto& FunctionUse = *FI++;
      auto FunctionUser = FunctionUse.getUser();

      DxilInst_DispatchMesh DispatchMesh(llvm::cast<Instruction>(FunctionUser));

      auto * PayloadType = DispatchMesh.get_payload()->getType();
      auto* PayloadTypeAsStruct = llvm::cast<llvm::StructType>(PayloadType);
      SmallVector<Type*, 16> Elements;
      for (unsigned int e = 0; e < PayloadTypeAsStruct->getNumElements(); ++e)
      {
          Elements.push_back(PayloadTypeAsStruct->getElementType(e));
      }
      Elements.push_back(Type::getInt32Ty(Ctx));
      PayloadTypeAsStruct->setBody(Elements);
  }
#endif
  DM.ReEmitDxilResources();

  return true;
}

char DxilPIXAddTidToAmplificationShaderPayload::ID = 0;

ModulePass *llvm::createDxilPIXAddTidToAmplificationShaderPayloadPass() { return new DxilPIXAddTidToAmplificationShaderPayload(); }

INITIALIZE_PASS(
    DxilPIXAddTidToAmplificationShaderPayload, "hlsl-dxil-PIX-add-tid-to-as-payload",
    "HLSL DXIL Add flat thread id to payload from AS to MS",
    false, false)
