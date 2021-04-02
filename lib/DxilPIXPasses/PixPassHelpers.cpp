///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PixPassHelpers.cpp														 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilResourceBinding.h"
#include "dxc/DXIL/DxilResourceProperties.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

using namespace llvm;
using namespace hlsl;

namespace PIXPassHelpers {
bool IsAllocateRayQueryInstruction(llvm::Value *Val) {
  if (Val != nullptr) {
    if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(Val)) {
      return hlsl::OP::IsDxilOpFuncCallInst(Inst,
                                            hlsl::OP::OpCode::AllocateRayQuery);
    }
  }
  return false;
}

static unsigned int
GetNextRegisterIdForClass(hlsl::DxilModule &DM,
                          DXIL::ResourceClass resourceClass) {
  switch (resourceClass) {
  case DXIL::ResourceClass::CBuffer:
    return static_cast<unsigned int>(DM.GetCBuffers().size());
  case DXIL::ResourceClass::UAV:
    return static_cast<unsigned int>(DM.GetUAVs().size());
  default:
    DXASSERT(false, "Unexpected resource class");
    return 0;
  }
}

static bool IsDynamicResourceShaderModel(DxilModule &DM) {
  return DM.GetShaderModel()->IsSMAtLeast(6, 6);
}

llvm::CallInst *CreateHandleForResource(hlsl::DxilModule &DM,
                                        llvm::IRBuilder<> &Builder,
                                        hlsl::DxilResourceBase *resource,
                                        const char *name) {

  OP *HlslOP = DM.GetOP();
  LLVMContext &Ctx = DM.GetModule()->getContext();

  DXIL::ResourceClass resourceClass = resource->GetClass();

  unsigned int resourceMetaDataId =
      GetNextRegisterIdForClass(DM, resourceClass);

  // Create handle for the newly-added resource
  if (IsDynamicResourceShaderModel(DM)) {
    Function *CreateHandleFromBindingOpFunc = HlslOP->GetOpFunc(
        DXIL::OpCode::CreateHandleFromBinding, Type::getVoidTy(Ctx));
    Constant *CreateHandleFromBindingOpcodeArg =
        HlslOP->GetU32Const((unsigned)DXIL::OpCode::CreateHandleFromBinding);
    DxilResourceBinding binding =
        resource_helper::loadBindingFromResourceBase(resource);
    Value *bindingV = resource_helper::getAsConstant(
        binding, HlslOP->GetResourceBindingType(), *DM.GetShaderModel());

    Value *registerIndex = HlslOP->GetU32Const(resourceMetaDataId);

    Value *isUniformRes = HlslOP->GetI1Const(0);

    Value *createHandleFromBindingArgs[] = {CreateHandleFromBindingOpcodeArg,
                                            bindingV, registerIndex,
                                            isUniformRes};

    auto *handle = Builder.CreateCall(CreateHandleFromBindingOpFunc,
                                      createHandleFromBindingArgs, name);

    Function *annotHandleFn =
        HlslOP->GetOpFunc(DXIL::OpCode::AnnotateHandle, Type::getVoidTy(Ctx));
    Value *annotHandleArg =
        HlslOP->GetI32Const((unsigned)DXIL::OpCode::AnnotateHandle);
    DxilResourceProperties RP =
        resource_helper::loadPropsFromResourceBase(resource);
    Type *resPropertyTy = HlslOP->GetResourcePropertiesType();
    Value *propertiesV =
        resource_helper::getAsConstant(RP, resPropertyTy, *DM.GetShaderModel());

    return Builder.CreateCall(annotHandleFn,
                              {annotHandleArg, handle, propertiesV});
  } else {
    Function *CreateHandleOpFunc =
        HlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(Ctx));
    Constant *CreateHandleOpcodeArg =
        HlslOP->GetU32Const((unsigned)DXIL::OpCode::CreateHandle);
    Constant *ClassArg = HlslOP->GetI8Const(
        static_cast<std::underlying_type<DxilResourceBase::Class>::type>(
            resourceClass));
    Constant *MetaDataArg = HlslOP->GetU32Const(
        resourceMetaDataId); // position of the metadata record in the
                             // corresponding metadata list
    Constant *IndexArg = HlslOP->GetU32Const(0); //
    Constant *FalseArg =
        HlslOP->GetI1Const(0); // non-uniform resource index: false
    return Builder.CreateCall(
        CreateHandleOpFunc,
        {CreateHandleOpcodeArg, ClassArg, MetaDataArg, IndexArg, FalseArg}, name);
  }
}

// Set up a UAV with structure of a single int
llvm::CallInst *CreateUAV(DxilModule &DM, IRBuilder<> &Builder,
                          unsigned int registerId, const char *name) {
  LLVMContext &Ctx = DM.GetModule()->getContext();

  SmallVector<llvm::Type *, 1> Elements{Type::getInt32Ty(Ctx)};
  llvm::StructType *UAVStructTy =
      llvm::StructType::create(Elements, "class.RWStructuredBuffer");
  std::unique_ptr<DxilResource> pUAV = llvm::make_unique<DxilResource>();
  pUAV->SetGlobalName(name);
  pUAV->SetGlobalSymbol(UndefValue::get(UAVStructTy->getPointerTo()));
  pUAV->SetID(GetNextRegisterIdForClass(DM, DXIL::ResourceClass::UAV));
  pUAV->SetRW(true); // sets UAV class
  pUAV->SetSpaceID(
      (unsigned int)-2); // This is the reserved-for-tools register space
  pUAV->SetSampleCount(1);
  pUAV->SetGloballyCoherent(false);
  pUAV->SetHasCounter(false);
  pUAV->SetCompType(CompType::getI32());
  pUAV->SetLowerBound(0);
  pUAV->SetRangeSize(1);
  pUAV->SetKind(DXIL::ResourceKind::RawBuffer);

  auto pAnnotation = DM.GetTypeSystem().GetStructAnnotation(UAVStructTy);
  if (pAnnotation == nullptr) {

    pAnnotation = DM.GetTypeSystem().AddStructAnnotation(UAVStructTy);
    pAnnotation->GetFieldAnnotation(0).SetCBufferOffset(0);
    pAnnotation->GetFieldAnnotation(0).SetCompType(
        hlsl::DXIL::ComponentType::I32);
    pAnnotation->GetFieldAnnotation(0).SetFieldName("count");
  }

  auto *handle = CreateHandleForResource(DM, Builder, pUAV.get(), name);

  DM.AddUAV(std::move(pUAV));

  return handle;
}
} // namespace PIXPassHelpers
