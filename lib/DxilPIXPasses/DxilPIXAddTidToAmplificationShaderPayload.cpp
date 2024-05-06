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
                               ExpandedStruct &expanded,
                               AllocaInst *NewStructAlloca,
                               unsigned int expandedValueIndex, Value *value) {
  Constant *Zero32Arg = HlslOP->GetU32Const(0);
  SmallVector<Value *, 2> IndexToAppendedValue;
  IndexToAppendedValue.push_back(Zero32Arg);
  IndexToAppendedValue.push_back(HlslOP->GetU32Const(expandedValueIndex));
  auto *PointerToEmbeddedNewValue = B.CreateInBoundsGEP(
      expanded.ExpandedPayloadStructType, NewStructAlloca, IndexToAppendedValue,
      "PointerToEmbeddedNewValue" + std::to_string(expandedValueIndex));
  B.CreateStore(value, PointerToEmbeddedNewValue);
}

bool DxilPIXAddTidToAmplificationShaderPayload::runOnModule(Module &M) {

  DxilModule &DM = M.GetOrCreateDxilModule();
  LLVMContext &Ctx = M.getContext();
  OP *HlslOP = DM.GetOP();

  Type *OriginalPayloadStructPointerType = nullptr;
  Type *OriginalPayloadStructType = nullptr;
  ExpandedStruct expanded;
  llvm::Function *entryFunction = PIXPassHelpers::GetEntryFunction(DM);
  for (inst_iterator I = inst_begin(entryFunction), E = inst_end(entryFunction);
       I != E; ++I) {
    if (auto *Instr = llvm::cast<Instruction>(&*I)) {
      if (hlsl::OP::IsDxilOpFuncCallInst(Instr,
                                         hlsl::OP::OpCode::DispatchMesh)) {
        DxilInst_DispatchMesh DispatchMesh(Instr);
        OriginalPayloadStructPointerType =
            DispatchMesh.get_payload()->getType();
        OriginalPayloadStructType =
            OriginalPayloadStructPointerType->getPointerElementType();
        expanded = ExpandStructType(Ctx, OriginalPayloadStructType);
      }
    }
  }

  AllocaInst *OldStructAlloca = nullptr;
  AllocaInst *NewStructAlloca = nullptr;
  std::vector<AllocaInst *> allocasOfPayloadType;
  for (inst_iterator I = inst_begin(entryFunction), E = inst_end(entryFunction);
       I != E; ++I) {
    auto *Inst = &*I;
    if (llvm::isa<AllocaInst>(Inst)) {
      auto *Alloca = llvm::cast<AllocaInst>(Inst);
      if (Alloca->getType() == OriginalPayloadStructPointerType) {
        allocasOfPayloadType.push_back(Alloca);
      }
    }
  }
  for (auto &Alloca : allocasOfPayloadType) {
    OldStructAlloca = Alloca;
    llvm::IRBuilder<> B(Alloca->getContext());
    NewStructAlloca = B.CreateAlloca(expanded.ExpandedPayloadStructType,
                                     HlslOP->GetU32Const(1), "NewPayload");
    NewStructAlloca->setAlignment(Alloca->getAlignment());
    NewStructAlloca->insertAfter(Alloca);

    ReplaceAllUsesOfInstructionWithNewValueAndDeleteInstruction(
        Alloca, NewStructAlloca, expanded.ExpandedPayloadStructType);
  }

  auto F = HlslOP->GetOpFunc(DXIL::OpCode::DispatchMesh,
                             expanded.ExpandedPayloadStructPtrType);
  for (auto FI = F->user_begin(); FI != F->user_end();) {
    auto *FunctionUser = *FI++;
    auto *UserInstruction = llvm::cast<Instruction>(FunctionUser);
    DxilInst_DispatchMesh DispatchMesh(UserInstruction);

    llvm::IRBuilder<> B(UserInstruction);

    Constant *Zero32Arg = HlslOP->GetU32Const(0);
    Constant *One32Arg = HlslOP->GetU32Const(1);
    Constant *Two32Arg = HlslOP->GetU32Const(2);

    auto GroupIdFunc =
        HlslOP->GetOpFunc(DXIL::OpCode::GroupId, Type::getInt32Ty(Ctx));
    Constant *GroupIdOpcode =
        HlslOP->GetU32Const((unsigned)DXIL::OpCode::GroupId);
    auto *GroupIdX =
        B.CreateCall(GroupIdFunc, {GroupIdOpcode, Zero32Arg}, "GroupIdX");
    auto *GroupIdY =
        B.CreateCall(GroupIdFunc, {GroupIdOpcode, One32Arg}, "GroupIdY");
    auto *GroupIdZ =
        B.CreateCall(GroupIdFunc, {GroupIdOpcode, Two32Arg}, "GroupIdZ");

    auto *GroupYxNumZ = B.CreateMul(
        GroupIdY, HlslOP->GetU32Const(m_DispatchArgumentZ), "GroupYxNumZ");
    auto *FlatGroupNumZY = B.CreateAdd(GroupIdZ, GroupYxNumZ, "FlatGroupNumZY");
    auto *GroupXxNumYZ = B.CreateMul(
        GroupIdX,
        HlslOP->GetU32Const(m_DispatchArgumentY * m_DispatchArgumentZ),
        "GroupXxNumYZ");
    auto *FlatGroupNum =
        B.CreateAdd(GroupXxNumYZ, FlatGroupNumZY, "FlatGroupNum");

    auto *FlatGroupNumWithSpaceForThreadInGroupId = B.CreateMul(
        FlatGroupNum,
        HlslOP->GetU32Const(DM.GetNumThreads(0) * DM.GetNumThreads(1) *
                            DM.GetNumThreads(2)),
        "FlatGroupNumWithSpaceForThreadInGroupId");

    auto *FlattenedThreadIdInGroupFunc = HlslOP->GetOpFunc(
        DXIL::OpCode::FlattenedThreadIdInGroup, Type::getInt32Ty(Ctx));
    Constant *FlattenedThreadIdInGroupOpcode =
        HlslOP->GetU32Const((unsigned)DXIL::OpCode::FlattenedThreadIdInGroup);
    auto FlatThreadIdInGroup = B.CreateCall(FlattenedThreadIdInGroupFunc,
                                            {FlattenedThreadIdInGroupOpcode},
                                            "FlattenedThreadIdInGroup");

    auto *FlatId = B.CreateAdd(FlatGroupNumWithSpaceForThreadInGroupId,
                               FlatThreadIdInGroup, "FlatId");

    AddValueToExpandedPayload(HlslOP, B, expanded, NewStructAlloca,
                              OriginalPayloadStructType->getStructNumElements(),
                              FlatId);
    AddValueToExpandedPayload(
        HlslOP, B, expanded, NewStructAlloca,
        OriginalPayloadStructType->getStructNumElements() + 1,
        DispatchMesh.get_threadGroupCountY());
    AddValueToExpandedPayload(
        HlslOP, B, expanded, NewStructAlloca,
        OriginalPayloadStructType->getStructNumElements() + 2,
        DispatchMesh.get_threadGroupCountZ());
  }

  DM.ReEmitDxilResources();

  return true;
}

char DxilPIXAddTidToAmplificationShaderPayload::ID = 0;

ModulePass *llvm::createDxilPIXAddTidToAmplificationShaderPayloadPass() {
  return new DxilPIXAddTidToAmplificationShaderPayload();
}

INITIALIZE_PASS(DxilPIXAddTidToAmplificationShaderPayload,
                "hlsl-dxil-PIX-add-tid-to-as-payload",
                "HLSL DXIL Add flat thread id to payload from AS to MS", false,
                false)
