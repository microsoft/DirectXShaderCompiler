///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPIXMeshShaderOutputInstrumentation.cpp // Copyright (C) Microsoft
// Corporation. All rights reserved.                 // This file is distributed
// under the University of Illinois Open Source     // License. See LICENSE.TXT
// for details.                                     //
//                                                                           //
// Provides a pass to add instrumentation to determine pixel hit count and   //
// cost. Used by PIX.                                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilUtil.h"

#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Transforms/Utils/Local.h"
#include <deque>

#ifdef _WIN32
#include <winerror.h>
#endif

// Keep this in sync with the same-named value in the debugger application's
// WinPixShaderUtils.h
constexpr uint64_t DebugBufferDumpingGroundSize = 64 * 1024;

// Keep these in sync with the same-named values in PIX's MeshShaderOutput.cpp
constexpr uint32_t triangleIndexIndicator = 1;
constexpr uint32_t int32ValueIndicator = 2;
constexpr uint32_t floatValueIndicator = 3;

using namespace llvm;
using namespace hlsl;

class DxilPIXMeshShaderOutputInstrumentation : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilPIXMeshShaderOutputInstrumentation() : ModulePass(ID) {}
  const char *getPassName() const override {
    return "DXIL mesh shader output instrumentation";
  }
  void applyOptions(PassOptions O) override;
  bool runOnModule(Module &M) override;

private:
  CallInst *m_OutputUAV = nullptr;
  int m_RemainingReservedSpaceInBytes = 0;
  Constant *m_OffsetMask = nullptr;

  uint32_t m_GroupCountX;
  uint32_t m_GroupCountY;
  uint32_t m_GroupCountZ;
  uint64_t m_UAVSize = 1024 * 1024;

  struct BuilderContext {
    Module &M;
    DxilModule &DM;
    LLVMContext &Ctx;
    OP *HlslOP;
    IRBuilder<> &Builder;
  };

  CallInst *addUAV(BuilderContext &BC);
  Value *insertInstructionsToCalculateFlattenedGroupId(BuilderContext &BC);
  Value *insertInstructionsToCalculateFlattenedThreadId(BuilderContext &BC);
  Value *reserveDebugEntrySpace(BuilderContext &BC, uint32_t SpaceInBytes);
  uint32_t UAVDumpingGroundOffset();
  Value *writeDwordAndReturnNewOffset(BuilderContext &BC, Value *TheOffset,
                                      Value *TheValue);
};

void DxilPIXMeshShaderOutputInstrumentation::applyOptions(PassOptions O) {
  GetPassOptionUnsigned(O, "GroupCountX", &m_GroupCountX, 0);
  GetPassOptionUnsigned(O, "GroupCountY", &m_GroupCountY, 0);
  GetPassOptionUnsigned(O, "GroupCountZ", &m_GroupCountZ, 0);
  GetPassOptionUInt64(O, "UAVSize", &m_UAVSize, 1024 * 1024);
}

uint32_t DxilPIXMeshShaderOutputInstrumentation::UAVDumpingGroundOffset() {
  return static_cast<uint32_t>(m_UAVSize - DebugBufferDumpingGroundSize);
}

CallInst *DxilPIXMeshShaderOutputInstrumentation::addUAV(BuilderContext &BC) {
  // Set up a UAV with structure of a single int
  unsigned int UAVResourceHandle =
      static_cast<unsigned int>(BC.DM.GetUAVs().size());
  SmallVector<llvm::Type *, 1> Elements{Type::getInt32Ty(BC.Ctx)};
  llvm::StructType *UAVStructTy =
      llvm::StructType::create(Elements, "PIX_DebugUAV_Type");
  std::unique_ptr<DxilResource> pUAV = llvm::make_unique<DxilResource>();
  pUAV->SetGlobalName("PIX_DebugUAVName");
  pUAV->SetGlobalSymbol(UndefValue::get(UAVStructTy->getPointerTo()));
  pUAV->SetID(UAVResourceHandle);
  pUAV->SetSpaceID(
      (unsigned int)-2); // This is the reserved-for-tools register space
  pUAV->SetSampleCount(1);
  pUAV->SetGloballyCoherent(false);
  pUAV->SetHasCounter(false);
  pUAV->SetCompType(CompType::getI32());
  pUAV->SetLowerBound(0);
  pUAV->SetRangeSize(1);
  pUAV->SetKind(DXIL::ResourceKind::RawBuffer);
  pUAV->SetRW(true);

  auto ID = BC.DM.AddUAV(std::move(pUAV));
  assert(ID == UAVResourceHandle);

  BC.DM.m_ShaderFlags.SetEnableRawAndStructuredBuffers(true);

  // Create handle for the newly-added UAV
  Function *CreateHandleOpFunc =
      BC.HlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(BC.Ctx));
  Constant *CreateHandleOpcodeArg =
      BC.HlslOP->GetU32Const((unsigned)DXIL::OpCode::CreateHandle);
  Constant *UAVVArg = BC.HlslOP->GetI8Const(
      static_cast<std::underlying_type<DxilResourceBase::Class>::type>(
          DXIL::ResourceClass::UAV));
  Constant *MetaDataArg = BC.HlslOP->GetU32Const(
      ID); // position of the metadata record in the corresponding metadata list
  Constant *IndexArg = BC.HlslOP->GetU32Const(0); //
  Constant *FalseArg =
      BC.HlslOP->GetI1Const(0); // non-uniform resource index: false
  return BC.Builder.CreateCall(
      CreateHandleOpFunc,
      {CreateHandleOpcodeArg, UAVVArg, MetaDataArg, IndexArg, FalseArg},
      "PIX_DebugUAV_Handle");
}

Value *DxilPIXMeshShaderOutputInstrumentation::
    insertInstructionsToCalculateFlattenedGroupId(BuilderContext &BC) {
  Constant *Zero32Arg = BC.HlslOP->GetU32Const(0);
  Constant *One32Arg = BC.HlslOP->GetU32Const(1);
  Constant *Two32Arg = BC.HlslOP->GetU32Const(2);

  auto GroupIdFunc =
      BC.HlslOP->GetOpFunc(DXIL::OpCode::GroupId, Type::getInt32Ty(BC.Ctx));
  Constant *Opcode = BC.HlslOP->GetU32Const((unsigned)DXIL::OpCode::GroupId);
  auto GroupIdX =
      BC.Builder.CreateCall(GroupIdFunc, {Opcode, Zero32Arg}, "GroupIdX");
  auto GroupIdY =
      BC.Builder.CreateCall(GroupIdFunc, {Opcode, One32Arg}, "GroupIdY");
  auto GroupIdZ =
      BC.Builder.CreateCall(GroupIdFunc, {Opcode, Two32Arg}, "GroupIdZ");

  auto Z = BC.Builder.CreateMul(
      GroupIdZ, BC.HlslOP->GetU32Const(m_GroupCountY * m_GroupCountX));
  auto Y =
      BC.Builder.CreateMul(GroupIdY, BC.HlslOP->GetU32Const(m_GroupCountX));
  auto ZY = BC.Builder.CreateMul(Z, Y);
  return BC.Builder.CreateAdd(ZY, GroupIdX);
}

Value *DxilPIXMeshShaderOutputInstrumentation::
    insertInstructionsToCalculateFlattenedThreadId(BuilderContext &BC) {

  auto ThreadIdFunc = BC.HlslOP->GetOpFunc(
      DXIL::OpCode::FlattenedThreadIdInGroup, Type::getInt32Ty(BC.Ctx));
  Constant *Opcode =
      BC.HlslOP->GetU32Const((unsigned)DXIL::OpCode::FlattenedThreadIdInGroup);
  return BC.Builder.CreateCall(ThreadIdFunc, {Opcode},
                               "FlattenedThreadId");
}

Value *DxilPIXMeshShaderOutputInstrumentation::reserveDebugEntrySpace(
    BuilderContext &BC, uint32_t SpaceInBytes) {
  assert(m_RemainingReservedSpaceInBytes ==
         0); // or else the previous caller reserved too much space

  m_RemainingReservedSpaceInBytes = SpaceInBytes;

  // Insert the UAV increment instruction:
  Function *AtomicOpFunc =
      BC.HlslOP->GetOpFunc(OP::OpCode::AtomicBinOp, Type::getInt32Ty(BC.Ctx));
  Constant *AtomicBinOpcode =
      BC.HlslOP->GetU32Const((unsigned)OP::OpCode::AtomicBinOp);
  Constant *AtomicAdd =
      BC.HlslOP->GetU32Const((unsigned)DXIL::AtomicBinOpCode::Add);
  Constant *Zero32Arg = BC.HlslOP->GetU32Const(0);
  UndefValue *UndefArg = UndefValue::get(Type::getInt32Ty(BC.Ctx));

  Constant *Increment = BC.HlslOP->GetU32Const(SpaceInBytes);

  auto *PreviousValue = BC.Builder.CreateCall(
      AtomicOpFunc,
      {
          AtomicBinOpcode, // i32, ; opcode
          m_OutputUAV,      // %dx.types.Handle, ; resource handle
          AtomicAdd, // i32, ; binary operation code : EXCHANGE, IADD, AND, OR,
                     // XOR, IMIN, IMAX, UMIN, UMAX
          Zero32Arg, // i32, ; coordinate c0: index in bytes
          UndefArg,  // i32, ; coordinate c1 (unused)
          UndefArg,  // i32, ; coordinate c2 (unused)
          Increment, // i32); increment value
      },
      "UAVIncResult");

  return BC.Builder.CreateAnd(PreviousValue, m_OffsetMask, "MaskedForUAVLimit");
}

Value *DxilPIXMeshShaderOutputInstrumentation::writeDwordAndReturnNewOffset(
    BuilderContext &BC, Value *TheOffset, Value *TheValue) {

  Function *StoreValue =
      BC.HlslOP->GetOpFunc(OP::OpCode::BufferStore, Type::getInt32Ty(BC.Ctx));
  Constant *StoreValueOpcode =
      BC.HlslOP->GetU32Const((unsigned)DXIL::OpCode::BufferStore);
  UndefValue *Undef32Arg = UndefValue::get(Type::getInt32Ty(BC.Ctx));
  Constant *WriteMask_X = BC.HlslOP->GetI8Const(1);

  (void)BC.Builder.CreateCall(
      StoreValue,
      {StoreValueOpcode, // i32 opcode
       m_OutputUAV,       // %dx.types.Handle, ; resource handle
       TheOffset,        // i32 c0: index in bytes into UAV
       Undef32Arg,       // i32 c1: unused
       TheValue,
       Undef32Arg, // unused values
       Undef32Arg, // unused values
       Undef32Arg, // unused values
       WriteMask_X});

  m_RemainingReservedSpaceInBytes -= sizeof(uint32_t);
  assert(m_RemainingReservedSpaceInBytes >=
         0); // or else the caller didn't reserve enough space

  return BC.Builder.CreateAdd(
      TheOffset,
      BC.HlslOP->GetU32Const(static_cast<unsigned int>(sizeof(uint32_t))));
}

bool DxilPIXMeshShaderOutputInstrumentation::runOnModule(Module &M) {
  DxilModule &DM = M.GetOrCreateDxilModule();
  LLVMContext &Ctx = M.getContext();
  OP *HlslOP = DM.GetOP();

  Instruction *firstInsertionPt =
      dxilutil::FirstNonAllocaInsertionPt(DM.GetEntryFunction());
  IRBuilder<> Builder(firstInsertionPt);

  BuilderContext BC{M, DM, Ctx, HlslOP, Builder};

  m_OffsetMask = BC.HlslOP->GetU32Const(UAVDumpingGroundOffset() - 1);

  m_OutputUAV = addUAV(BC);

  auto FlattenedGroupId = insertInstructionsToCalculateFlattenedGroupId(BC);
  auto FlattenedThreadId = insertInstructionsToCalculateFlattenedThreadId(BC);

  auto F = HlslOP->GetOpFunc(DXIL::OpCode::EmitIndices, Type::getVoidTy(Ctx));
  auto FunctionUses = F->uses();
  for (auto FI = FunctionUses.begin(); FI != FunctionUses.end();) {
    auto &FunctionUse = *FI++;
    auto FunctionUser = FunctionUse.getUser();

    auto Call = cast<CallInst>(FunctionUser);

    IRBuilder<> Builder2(Call);
    BuilderContext BC2{M, DM, Ctx, HlslOP, Builder2};

    {
      constexpr uint32_t DwordCount = 1   // index indicator
                                      + 1 // flattened group id
                                      + 1 // flattened thread id
                                      + 1 // PrimitiveIndex
                                      + 3 // vertex indices
          ;

      Value *byteOffset = reserveDebugEntrySpace(BC2, DwordCount * static_cast<uint32_t>(sizeof(uint32_t)));

      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, BC.HlslOP->GetI32Const(triangleIndexIndicator));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, FlattenedGroupId);
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, FlattenedThreadId);
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, Call->getOperand(1));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, Call->getOperand(2));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, Call->getOperand(3));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, Call->getOperand(4));
    }
  }

  F = HlslOP->GetOpFunc(DXIL::OpCode::StoreVertexOutput, Type::getInt32Ty(Ctx));
  FunctionUses = F->uses();
  for (auto FI = FunctionUses.begin(); FI != FunctionUses.end();) {
    auto &FunctionUse = *FI++;
    auto FunctionUser = FunctionUse.getUser();

    auto Call = cast<CallInst>(FunctionUser);

    IRBuilder<> Builder2(Call);
    BuilderContext BC2{M, DM, Ctx, HlslOP, Builder2};

    {
      constexpr uint32_t DwordCount = 1   // integer vertex value indicator
                                      + 1 // flattened group id
                                      + 1 // flattened thread id
                                      + 1 // Output sig id
                                      + 1 // row
                                      + 1 // column
                                      + 1 // value
                                      + 1 // vertex index
          ;

      Value *byteOffset = reserveDebugEntrySpace(BC2, DwordCount * static_cast<uint32_t>(sizeof(uint32_t)));

      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, BC.HlslOP->GetI32Const(int32ValueIndicator));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, FlattenedGroupId);
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, FlattenedThreadId);
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, Call->getOperand(1));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, Call->getOperand(2));
      auto expandBits = BC2.Builder.CreateCast(Instruction::ZExt, Call->getOperand(3), Type::getInt32Ty(Ctx));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, expandBits);
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, Call->getOperand(4));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, Call->getOperand(5));
    }
  }

  F = HlslOP->GetOpFunc(DXIL::OpCode::StoreVertexOutput, Type::getFloatTy(Ctx));
  FunctionUses = F->uses();
  for (auto FI = FunctionUses.begin(); FI != FunctionUses.end();) {
    auto &FunctionUse = *FI++;
    auto FunctionUser = FunctionUse.getUser();

    auto Call = cast<CallInst>(FunctionUser);

    IRBuilder<> Builder2(Call);
    BuilderContext BC2{M, DM, Ctx, HlslOP, Builder2};

    {
      constexpr uint32_t DwordCount = 1   // integer vertex value indicator
                                      + 1 // flattened group id
                                      + 1 // flattened thread id
                                      + 1 // Output sig id
                                      + 1 // row
                                      + 1 // column
                                      + 1 // value
                                      + 1 // vertex index
          ;

      Value *byteOffset = reserveDebugEntrySpace(BC2, DwordCount * static_cast<uint32_t>(sizeof(uint32_t)));

      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, BC.HlslOP->GetI32Const(floatValueIndicator));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, FlattenedGroupId);
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, FlattenedThreadId);
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, Call->getOperand(1));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, Call->getOperand(2));
      auto expandBits = BC2.Builder.CreateCast(Instruction::ZExt, Call->getOperand(3), Type::getInt32Ty(Ctx));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, expandBits);
      auto reinterpretFloatToInt = BC2.Builder.CreateCast(Instruction::BitCast, Call->getOperand(4), Type::getInt32Ty(Ctx));
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, reinterpretFloatToInt);
      byteOffset = writeDwordAndReturnNewOffset(BC2, byteOffset, Call->getOperand(5));
    }
  }

  DM.ReEmitDxilResources();

  return true;
}

char DxilPIXMeshShaderOutputInstrumentation::ID = 0;

ModulePass *llvm::createDxilDxilPIXMeshShaderOutputInstrumentation() {
  return new DxilPIXMeshShaderOutputInstrumentation();
}

INITIALIZE_PASS(DxilPIXMeshShaderOutputInstrumentation,
                "hlsl-dxil-pix-meshshader-output-instrumentation",
                "DXIL mesh shader output instrumentation for PIX", false, false)
