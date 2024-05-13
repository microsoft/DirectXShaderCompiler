///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPIXAddTidToAmplificationShaderPayload.cpp                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilUtil.h"

#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Transforms/Utils/Local.h"

#include "PixPassHelpers.h"

using namespace llvm;
using namespace hlsl;
using namespace PIXPassHelpers;

class DxilPIXAddTidToAmplificationShaderPayload : public ModulePass {
  uint32_t m_DispatchArgumentY = 1;
  uint32_t m_DispatchArgumentZ = 1;

public:
  static char ID; // Pass identification, replacement for typeid
  DxilPIXAddTidToAmplificationShaderPayload() : ModulePass(ID) {}
  StringRef getPassName() const override {
    return "DXIL Add flat thread id to payload from AS to MS";
  }
  bool runOnModule(Module &M) override;
  void applyOptions(PassOptions O) override;
};

void DxilPIXAddTidToAmplificationShaderPayload::applyOptions(PassOptions O) {
  GetPassOptionUInt32(O, "dispatchArgY", &m_DispatchArgumentY, 1);
  GetPassOptionUInt32(O, "dispatchArgZ", &m_DispatchArgumentZ, 1);
}

void AddValueToExpandedPayload(OP *HlslOP, llvm::IRBuilder<> &B,
                               AllocaInst *NewStructAlloca,
                               unsigned int expandedValueIndex, Value *value) {
  Constant *Zero32Arg = HlslOP->GetU32Const(0);
  SmallVector<Value *, 2> IndexToAppendedValue;
  IndexToAppendedValue.push_back(Zero32Arg);
  IndexToAppendedValue.push_back(HlslOP->GetU32Const(expandedValueIndex));
  auto *PointerToEmbeddedNewValue = B.CreateInBoundsGEP(
      NewStructAlloca, IndexToAppendedValue,
      "PointerToEmbeddedNewValue" + std::to_string(expandedValueIndex));
  B.CreateStore(value, PointerToEmbeddedNewValue);
}

void CopyStruct(IRBuilder<> &Builder, StructType *MyStructType, Value *Struct1,
                Value *Struct2) {
  for (unsigned i = 0; i < MyStructType->getNumElements(); ++i) {
    Value *Elem1 = Builder.CreateStructGEP(MyStructType, Struct1, i);
    Value *Elem2 = Builder.CreateStructGEP(MyStructType, Struct2, i);

    if (ArrayType *AT = dyn_cast<ArrayType>(MyStructType->getElementType(i))) {
      for (unsigned j = 0; j < AT->getNumElements(); ++j) {
        Value *ArrayElem1 = Builder.CreateGEP(Elem1, Builder.getInt32(j));
        Value *ArrayElem2 = Builder.CreateGEP(Elem2, Builder.getInt32(j));
        CopyStruct(Builder, cast<StructType>(AT->getElementType()), ArrayElem1,
                   ArrayElem2);
      }
    } else {
      Value *Val = Builder.CreateLoad(Elem1);
      Builder.CreateStore(Val, Elem2);
    }
  }
}
int limit = 3;
void CopyAggregate(int &counter, IRBuilder<> &B, Value *Source, Value *Dest,
                   ArrayRef< Value *> GEPIndices ) {
  if (counter > limit)
    return;
  // if (SourcePtr->getAddressSpace() == hlsl::DXIL::kTGSMAddrSpace) {
  auto *SourcePtr = dyn_cast<PointerType>(Source->getType());
  if (StructType *ST =
          dyn_cast<StructType>(SourcePtr->getPointerElementType())) {
    SmallVector<Value *, 16> StructIndices;
    StructIndices.append(GEPIndices.begin(), GEPIndices.end());
    StructIndices.push_back(nullptr);
    for (unsigned j = 0; j < ST->getNumElements(); ++j) {
      StructIndices.back() = B.getInt32(j);
      CopyAggregate(counter, B, Source, Dest, StructIndices);
      if (counter > limit)
        return;
    }
  } else if (ArrayType *AT =
                 dyn_cast<ArrayType>(SourcePtr->getPointerElementType())) {
    SmallVector<Value *, 16> StructIndices;
    StructIndices.append(GEPIndices.begin(), GEPIndices.end());
    for (unsigned j = 0; j < AT->getNumElements(); ++j) {
      StructIndices.back() = B.getInt32(j);
      CopyAggregate(counter, B, Source, Dest, StructIndices);
    }
  } else {
    auto *SourceGEP = B.CreateGEP(Source, GEPIndices,
                                    "CopyStructSourceGEP");
    Value *Val = B.CreateLoad(SourceGEP, "CopyStructLoad");
    auto *DestGEP = B.CreateGEP(Dest, GEPIndices, "CopyStructDestGEP");
    B.CreateStore(Val, DestGEP, "CopyStructStore");
    counter++;
  }
}

bool DxilPIXAddTidToAmplificationShaderPayload::runOnModule(Module &M) {
  DxilModule &DM = M.GetOrCreateDxilModule();
  LLVMContext &Ctx = M.getContext();
  OP *HlslOP = DM.GetOP();
  llvm::Function *entryFunction = PIXPassHelpers::GetEntryFunction(DM);
  for (inst_iterator I = inst_begin(entryFunction), E = inst_end(entryFunction);
       I != E; ++I) {
    if (hlsl::OP::IsDxilOpFuncCallInst(&*I, hlsl::OP::OpCode::DispatchMesh)) {
      DxilInst_DispatchMesh DispatchMesh(&*I);
      Type *OriginalPayloadStructPointerType =
          DispatchMesh.get_payload()->getType();
      Type *OriginalPayloadStructType =
          OriginalPayloadStructPointerType->getPointerElementType();
      ExpandedStruct expanded =
          ExpandStructType(Ctx, OriginalPayloadStructType);

      llvm::IRBuilder<> B(&*I);

      auto *NewStructAlloca =
          B.CreateAlloca(expanded.ExpandedPayloadStructType,
                         HlslOP->GetU32Const(1), "NewPayload");
      NewStructAlloca->setAlignment(4);
      // auto *NewPayloadPointer =
      //    B.CreateGEP(NewStructAlloca, HlslOP->GetU32Const(0));
      //
      //// llvm.memcpy cannot be used because the source struct might be in,
      ////e.g, / group-shared
      // auto *OldPayloadPointer =
      //    B.CreateGEP(DispatchMesh.get_payload(), HlslOP->GetU32Const(0));
      int counter = 0;
      SmallVector<Value *, 16> GEPIndices;
      CopyAggregate(counter, B, DispatchMesh.get_payload(), NewStructAlloca,
                    GEPIndices);

      auto ThreadIdFunc =
          HlslOP->GetOpFunc(DXIL::OpCode::ThreadId, Type::getInt32Ty(Ctx));
      Constant *Opcode = HlslOP->GetU32Const((unsigned)DXIL::OpCode::ThreadId);
      Constant *Zero32Arg = HlslOP->GetU32Const(0);
      Constant *One32Arg = HlslOP->GetU32Const(1);
      Constant *Two32Arg = HlslOP->GetU32Const(2);

      auto ThreadIdX =
          B.CreateCall(ThreadIdFunc, {Opcode, Zero32Arg}, "ThreadIdX");
      auto ThreadIdY =
          B.CreateCall(ThreadIdFunc, {Opcode, One32Arg}, "ThreadIdY");
      auto ThreadIdZ =
          B.CreateCall(ThreadIdFunc, {Opcode, Two32Arg}, "ThreadIdZ");

      auto *XxY =
          B.CreateMul(ThreadIdX, HlslOP->GetU32Const(m_DispatchArgumentY));
      auto *XplusY = B.CreateAdd(ThreadIdY, XxY);
      auto *XYxZ =
          B.CreateMul(XplusY, HlslOP->GetU32Const(m_DispatchArgumentZ));
      auto *XYZ = B.CreateAdd(ThreadIdZ, XYxZ);

      AddValueToExpandedPayload(
          HlslOP, B, NewStructAlloca,
          expanded.ExpandedPayloadStructType->getStructNumElements() - 3, XYZ);
      AddValueToExpandedPayload(
          HlslOP, B, NewStructAlloca,
          expanded.ExpandedPayloadStructType->getStructNumElements() - 2,
          DispatchMesh.get_threadGroupCountY());
      AddValueToExpandedPayload(
          HlslOP, B, NewStructAlloca,
          expanded.ExpandedPayloadStructType->getStructNumElements() - 1,
          DispatchMesh.get_threadGroupCountZ());

      auto DispatchMeshFn = HlslOP->GetOpFunc(
          DXIL::OpCode::DispatchMesh, expanded.ExpandedPayloadStructPtrType);
      Constant *DispatchMeshOpcode =
          HlslOP->GetU32Const((unsigned)DXIL::OpCode::DispatchMesh);
      B.CreateCall(DispatchMeshFn,
                   {DispatchMeshOpcode, DispatchMesh.get_threadGroupCountX(),
                    DispatchMesh.get_threadGroupCountY(),
                    DispatchMesh.get_threadGroupCountZ(), NewStructAlloca});
      I->removeFromParent();
      delete &*I;
      // Validation requires exactly one DispatchMesh in an AS, so we can exit
      // after the first one:
      DM.ReEmitDxilResources();
      return true;
    }
  }

  return false;
}

char DxilPIXAddTidToAmplificationShaderPayload::ID = 0;

ModulePass *llvm::createDxilPIXAddTidToAmplificationShaderPayloadPass() {
  return new DxilPIXAddTidToAmplificationShaderPayload();
}

INITIALIZE_PASS(DxilPIXAddTidToAmplificationShaderPayload,
                "hlsl-dxil-PIX-add-tid-to-as-payload",
                "HLSL DXIL Add flat thread id to payload from AS to MS", false,
                false)
