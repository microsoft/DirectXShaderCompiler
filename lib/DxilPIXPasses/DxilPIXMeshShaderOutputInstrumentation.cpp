///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilAddPixelHitInstrumentation.cpp                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides a pass to add instrumentation to retrieve mesh shader output.    //
// Used by PIX.                                                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilUtil.h"

#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Transforms/Utils/Local.h"
#include <deque>

#ifdef _WIN32
#include <winerror.h>
#endif

#include "PixPassHelpers.h"

// Keep these in sync with the same-named value in the debugger application's
// WinPixShaderUtils.h

constexpr uint64_t DebugBufferDumpingGroundSize = 64 * 1024;
// The actual max size per record is much smaller than this, but it never
// hurts to be generous.
constexpr size_t CounterOffsetBeyondUsefulData =
    DebugBufferDumpingGroundSize / 2;

// Keep these in sync with the same-named values in PIX's MeshShaderOutput.cpp
constexpr uint32_t triangleIndexIndicator = 0x1;
constexpr uint32_t int32ValueIndicator = 0x2;
constexpr uint32_t floatValueIndicator = 0x3;
constexpr uint32_t int16ValueIndicator = 0x4;
constexpr uint32_t float16ValueIndicator = 0x5;

using namespace llvm;
using namespace hlsl;
using namespace PIXPassHelpers;

class DxilPIXMeshShaderOutputInstrumentation : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilPIXMeshShaderOutputInstrumentation() : ModulePass(ID) {}
  StringRef getPassName() const override {
    return "DXIL mesh shader output instrumentation";
  }
  void applyOptions(PassOptions O) override;
  bool runOnModule(Module &M) override;

private:
  CallInst *m_OutputUAV = nullptr;
  int m_RemainingReservedSpaceInBytes = 0;
  Constant *m_OffsetMask = nullptr;
  SmallVector<Value *, 2> m_threadUniquifier;

  uint64_t m_UAVSize = 1024 * 1024;
  bool m_ExpandPayload = false;
  uint32_t m_DispatchArgumentY = 1;
  uint32_t m_DispatchArgumentZ = 1;
  uint32_t m_ExpandedPayloadSize = 0;
  uint32_t m_ExpandedPayloadAppendedFieldsOffset = 0;

  struct BuilderContext {
    Module &M;
    DxilModule &DM;
    LLVMContext &Ctx;
    OP *HlslOP;
    IRBuilder<> &Builder;
  };

  SmallVector<Value *, 2> insertInstructionsToCreateDisambiguationValue(
      IRBuilder<> &Builder, OP *HlslOP, LLVMContext &Ctx,
      unsigned appendedFieldsElementIndex, Instruction *firstGetPayload);
  Value *reserveDebugEntrySpace(BuilderContext &BC, uint32_t SpaceInBytes);
  uint32_t UAVDumpingGroundOffset();
  Value *writeDwordAndReturnNewOffset(BuilderContext &BC, Value *TheOffset,
                                      Value *TheValue);
  template <typename... T> void Instrument(BuilderContext &BC, T... values);
};

void DxilPIXMeshShaderOutputInstrumentation::applyOptions(PassOptions O) {
  GetPassOptionUInt64(O, "UAVSize", &m_UAVSize, 1024 * 1024);
  GetPassOptionBool(O, "expand-payload", &m_ExpandPayload, 0);
  GetPassOptionUInt32(O, "dispatchArgY", &m_DispatchArgumentY, 1);
  GetPassOptionUInt32(O, "dispatchArgZ", &m_DispatchArgumentZ, 1);
  GetPassOptionUInt32(O, "expanded-payload-size", &m_ExpandedPayloadSize, 0);
  GetPassOptionUInt32(O, "expanded-payload-offset",
                      &m_ExpandedPayloadAppendedFieldsOffset, 0);
}

uint32_t DxilPIXMeshShaderOutputInstrumentation::UAVDumpingGroundOffset() {
  return static_cast<uint32_t>(m_UAVSize - DebugBufferDumpingGroundSize);
}

Value *DxilPIXMeshShaderOutputInstrumentation::reserveDebugEntrySpace(
    BuilderContext &BC, uint32_t SpaceInBytes) {
  // Check the previous caller didn't reserve too much space:
  assert(m_RemainingReservedSpaceInBytes == 0);

  // Check that the caller didn't ask for so much memory that it will
  // overwrite the offset counter:
  assert(SpaceInBytes < CounterOffsetBeyondUsefulData);

  m_RemainingReservedSpaceInBytes = SpaceInBytes;

  // Insert the UAV increment instruction:
  Function *AtomicOpFunc =
      BC.HlslOP->GetOpFunc(OP::OpCode::AtomicBinOp, Type::getInt32Ty(BC.Ctx));
  Constant *AtomicBinOpcode =
      BC.HlslOP->GetU32Const((unsigned)OP::OpCode::AtomicBinOp);
  Constant *AtomicAdd =
      BC.HlslOP->GetU32Const((unsigned)DXIL::AtomicBinOpCode::Add);
  Constant *OffsetArg = BC.HlslOP->GetU32Const(UAVDumpingGroundOffset() +
                                               CounterOffsetBeyondUsefulData);
  UndefValue *UndefArg = UndefValue::get(Type::getInt32Ty(BC.Ctx));

  Constant *Increment = BC.HlslOP->GetU32Const(SpaceInBytes);

  auto *PreviousValue = BC.Builder.CreateCall(
      AtomicOpFunc,
      {
          AtomicBinOpcode, // i32, ; opcode
          m_OutputUAV,     // %dx.types.Handle, ; resource handle
          AtomicAdd, // i32, ; binary operation code : EXCHANGE, IADD, AND, OR,
                     // XOR, IMIN, IMAX, UMIN, UMAX
          OffsetArg, // i32, ; coordinate c0: index in bytes
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
       m_OutputUAV,      // %dx.types.Handle, ; resource handle
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

template <typename... T>
void DxilPIXMeshShaderOutputInstrumentation::Instrument(BuilderContext &BC,
                                                        T... values) {
  llvm::SmallVector<llvm::Value *, 10> Values(
      {static_cast<llvm::Value *>(values)...});
  const uint32_t DwordCount = Values.size();
  llvm::Value *byteOffset =
      reserveDebugEntrySpace(BC, DwordCount * sizeof(uint32_t));
  for (llvm::Value *V : Values) {
    byteOffset = writeDwordAndReturnNewOffset(BC, byteOffset, V);
  }
}

Value *GetValueFromExpandedPayload(IRBuilder<> &Builder,
                                   Instruction *firstGetPayload,
                                   unsigned int offset, const char *name) {
  auto *DerefPointer = Builder.getInt32(0);
  auto *OffsetToExpandedData = Builder.getInt32(offset);
  auto *GEP = Builder.CreateGEP(
      cast<PointerType>(firstGetPayload->getType()->getScalarType())
          ->getElementType(),
      firstGetPayload, {DerefPointer, OffsetToExpandedData});
  return Builder.CreateLoad(GEP, name);
}

SmallVector<Value *, 2> DxilPIXMeshShaderOutputInstrumentation::
    insertInstructionsToCreateDisambiguationValue(
        IRBuilder<> &Builder, OP *HlslOP, LLVMContext &Ctx,
        unsigned appendedFieldsElementIndex, Instruction *firstGetPayload) {

  // When a mesh shader is called from an amplification shader, all of the
  // thread id values are relative to the DispatchMesh call made by
  // that amplification shader. Data about what thread counts were passed
  // by the CPU to *CommandList::DispatchMesh are not available, but we
  // will have added that value to the AS->MS payload...

  SmallVector<Value *, 2> ret;
  Constant *Zero32Arg = HlslOP->GetU32Const(0);

  bool AmplificationShaderIsActive = firstGetPayload != nullptr;

  llvm::Value *ASDispatchMeshYCount = nullptr;
  llvm::Value *ASDispatchMeshZCount = nullptr;
  if (AmplificationShaderIsActive) {

    auto *ASThreadId = GetValueFromExpandedPayload(
        Builder, firstGetPayload, appendedFieldsElementIndex, "ASThreadId");
    ret.push_back(ASThreadId);
    ASDispatchMeshYCount = GetValueFromExpandedPayload(
        Builder, firstGetPayload, appendedFieldsElementIndex + 1,
        "ASDispatchMeshYCount");
    ASDispatchMeshZCount = GetValueFromExpandedPayload(
        Builder, firstGetPayload, appendedFieldsElementIndex + 2,
        "ASDispatchMeshZCount");
  } else {
    ret.push_back(Zero32Arg);
  }

  Constant *One32Arg = HlslOP->GetU32Const(1);
  Constant *Two32Arg = HlslOP->GetU32Const(2);

  auto GroupIdFunc =
      HlslOP->GetOpFunc(DXIL::OpCode::GroupId, Type::getInt32Ty(Ctx));
  Constant *Opcode = HlslOP->GetU32Const((unsigned)DXIL::OpCode::GroupId);
  auto *GroupIdX =
      Builder.CreateCall(GroupIdFunc, {Opcode, Zero32Arg}, "GroupIdX");
  auto *GroupIdY =
      Builder.CreateCall(GroupIdFunc, {Opcode, One32Arg}, "GroupIdY");
  auto *GroupIdZ =
      Builder.CreateCall(GroupIdFunc, {Opcode, Two32Arg}, "GroupIdZ");

  // flattend group number = z + y*numZ + x*numY*numZ
  if (AmplificationShaderIsActive) {
    auto *GroupYxNumZ = Builder.CreateMul(GroupIdY, ASDispatchMeshZCount);
    auto *FlatGroupNumZY = Builder.CreateAdd(GroupIdZ, GroupYxNumZ);
    auto *GroupXxNumZ = Builder.CreateMul(GroupIdX, ASDispatchMeshZCount);
    auto *GroupXxNumYZ = Builder.CreateMul(GroupXxNumZ, ASDispatchMeshYCount);
    auto *FlatGroupNum = Builder.CreateAdd(GroupXxNumYZ, FlatGroupNumZY);
    ret.push_back(FlatGroupNum);
  } else {
    auto *GroupYxNumZ =
        Builder.CreateMul(GroupIdY, HlslOP->GetU32Const(m_DispatchArgumentZ));
    auto *FlatGroupNumZY = Builder.CreateAdd(GroupIdZ, GroupYxNumZ);
    auto *GroupXxNumYZ =
        Builder.CreateMul(GroupIdX, HlslOP->GetU32Const(m_DispatchArgumentY *
                                                        m_DispatchArgumentZ));
    auto *FlatGroupNum = Builder.CreateAdd(GroupXxNumYZ, FlatGroupNumZY);
    ret.push_back(FlatGroupNum);
  }

  return ret;
}

static bool IsValidExpandedPayloadLayout(uint32_t ExpandedSizeInBytes,
                                         uint32_t AppendedFieldsOffsetInBytes) {
  constexpr uint32_t AppendedFieldsSizeInBytes = 3 * sizeof(uint32_t);
  return AppendedFieldsOffsetInBytes % sizeof(uint32_t) == 0 &&
         AppendedFieldsOffsetInBytes <= DXIL::kMaxMSASPayloadBytes &&
         ExpandedSizeInBytes % sizeof(uint32_t) == 0 &&
         ExpandedSizeInBytes >=
             AppendedFieldsOffsetInBytes + AppendedFieldsSizeInBytes &&
         ExpandedSizeInBytes <= DXIL::kMaxMSASPayloadBytes;
}

static ExpandedStruct BuildExpandedPayloadTypeMatchingAmplificationShader(
    Module &M, LLVMContext &Ctx, Type *OriginalPayloadStructType,
    uint32_t ExpandedSizeInBytes, uint32_t AppendedFieldsOffsetInBytes,
    unsigned *AppendedFieldsElementIndex) {
  ExpandedStruct ret = {};
  auto *OriginalStructType = dyn_cast<StructType>(OriginalPayloadStructType);
  if (OriginalStructType == nullptr || OriginalStructType->isOpaque() ||
      !IsValidExpandedPayloadLayout(ExpandedSizeInBytes,
                                    AppendedFieldsOffsetInBytes)) {
    return ret;
  }

  constexpr uint32_t AppendedFieldsSizeInBytes = 3 * sizeof(uint32_t);
  const DataLayout &DL = M.getDataLayout();
  const StructLayout *OriginalLayout = DL.getStructLayout(OriginalStructType);
  auto *Int32Type = Type::getInt32Ty(Ctx);
  const unsigned OriginalElementCount = OriginalStructType->getNumElements();

  // Try the natural layout before a packed fallback.
  const bool PackedCandidates[] = {false, true};
  for (bool Packed : PackedCandidates) {
    SmallVector<Type *, 16> Elements;
    for (unsigned i = 0; i < OriginalElementCount; ++i) {
      Elements.push_back(OriginalStructType->getElementType(i));
    }

    Elements.push_back(Int32Type);
    Elements.push_back(Int32Type);
    Elements.push_back(Int32Type);
    uint64_t UnpaddedOffsetInBytes =
        DL.getStructLayout(StructType::get(Ctx, Elements, Packed))
            ->getElementOffset(OriginalElementCount);
    Elements.resize(OriginalElementCount);
    if (UnpaddedOffsetInBytes > AppendedFieldsOffsetInBytes) {
      continue;
    }

    unsigned AppendedIndex = OriginalElementCount;
    uint64_t MidPaddingInBytes =
        AppendedFieldsOffsetInBytes - UnpaddedOffsetInBytes;
    if (MidPaddingInBytes != 0) {
      Elements.push_back(
          ArrayType::get(Int32Type, MidPaddingInBytes / sizeof(uint32_t)));
      ++AppendedIndex;
    }
    Elements.push_back(Int32Type);
    Elements.push_back(Int32Type);
    Elements.push_back(Int32Type);
    uint32_t TailPaddingInBytes = ExpandedSizeInBytes -
                                  AppendedFieldsOffsetInBytes -
                                  AppendedFieldsSizeInBytes;
    if (TailPaddingInBytes != 0) {
      Elements.push_back(
          ArrayType::get(Int32Type, TailPaddingInBytes / sizeof(uint32_t)));
    }

    StructType *Candidate = StructType::get(Ctx, Elements, Packed);
    const StructLayout *CandidateLayout = DL.getStructLayout(Candidate);
    // Verify the candidate preserves both payload layouts.
    if (DL.getTypeAllocSize(Candidate) != ExpandedSizeInBytes ||
        CandidateLayout->getElementOffset(AppendedIndex) !=
            AppendedFieldsOffsetInBytes) {
      continue;
    }

    bool OriginalFieldsUnmoved = true;
    for (unsigned i = 0; i < OriginalElementCount; ++i) {
      if (CandidateLayout->getElementOffset(i) !=
          OriginalLayout->getElementOffset(i)) {
        OriginalFieldsUnmoved = false;
        break;
      }
    }
    if (!OriginalFieldsUnmoved) {
      continue;
    }

    *AppendedFieldsElementIndex = AppendedIndex;
    ret.ExpandedPayloadStructType =
        StructType::create(Ctx, Elements, "PIX_AS2MS_Expanded_Type", Packed);
    ret.ExpandedPayloadStructPtrType =
        ret.ExpandedPayloadStructType->getPointerTo();
    return ret;
  }

  return ret;
}

static ExpandedStruct
SynthesizeExpandedPayloadType(LLVMContext &Ctx, uint32_t ExpandedSizeInBytes,
                              uint32_t AppendedFieldsOffsetInBytes) {
  ExpandedStruct ret = {};
  if (!IsValidExpandedPayloadLayout(ExpandedSizeInBytes,
                                    AppendedFieldsOffsetInBytes)) {
    return ret;
  }

  constexpr uint32_t AppendedFieldsSizeInBytes = 3 * sizeof(uint32_t);
  auto *Int32Type = Type::getInt32Ty(Ctx);
  auto *OpaqueOriginalPayloadType =
      ArrayType::get(Int32Type, AppendedFieldsOffsetInBytes / sizeof(uint32_t));
  SmallVector<Type *, 5> Elements{OpaqueOriginalPayloadType, Int32Type,
                                  Int32Type, Int32Type};
  uint32_t TailPaddingInBytes = ExpandedSizeInBytes -
                                AppendedFieldsOffsetInBytes -
                                AppendedFieldsSizeInBytes;
  if (TailPaddingInBytes != 0) {
    Elements.push_back(
        ArrayType::get(Int32Type, TailPaddingInBytes / sizeof(uint32_t)));
  }

  ret.ExpandedPayloadStructType =
      StructType::create(Ctx, Elements, "PIX_AS2MS_Expanded_Type");
  ret.ExpandedPayloadStructPtrType =
      ret.ExpandedPayloadStructType->getPointerTo();
  return ret;
}

static bool OutputSignatureElementIsSigned(DxilModule &DM, Value *OutputSigId) {
  auto *SigIdConstant = dyn_cast<ConstantInt>(OutputSigId);
  if (SigIdConstant == nullptr) {
    return false;
  }
  const DxilSignature &OutputSignature = DM.GetOutputSignature();
  uint64_t SigId = SigIdConstant->getLimitedValue();
  if (SigId >= OutputSignature.GetElements().size()) {
    return false;
  }
  return OutputSignature.GetElement(static_cast<unsigned>(SigId))
      .GetCompType()
      .IsSIntTy();
}

bool DxilPIXMeshShaderOutputInstrumentation::runOnModule(Module &M) {
  DxilModule &DM = M.GetOrCreateDxilModule();
  LLVMContext &Ctx = M.getContext();
  OP *HlslOP = DM.GetOP();

  Type *OriginalPayloadStructType = nullptr;
  ExpandedStruct expanded = {};
  unsigned AppendedFieldsElementIndex = 0;
  Instruction *FirstNewStructGetMeshPayload = nullptr;
  if (m_ExpandPayload) {
    Instruction *getMeshPayloadInstructions = nullptr;
    llvm::Function *entryFunction = PIXPassHelpers::GetEntryFunction(DM);
    for (inst_iterator I = inst_begin(entryFunction),
                       E = inst_end(entryFunction);
         I != E; ++I) {
      if (auto *Instr = llvm::cast<Instruction>(&*I)) {
        if (hlsl::OP::IsDxilOpFuncCallInst(Instr,
                                           hlsl::OP::OpCode::GetMeshPayload)) {
          getMeshPayloadInstructions = Instr;
          Type *OriginalPayloadStructPointerType = Instr->getType();
          OriginalPayloadStructType =
              OriginalPayloadStructPointerType->getPointerElementType();
          // The validator assures that there is only one call to
          // GetMeshPayload...
          break;
        }
      }
    }

    if (OriginalPayloadStructType != nullptr) {
      if (m_ExpandedPayloadSize != 0) {
        expanded = BuildExpandedPayloadTypeMatchingAmplificationShader(
            M, Ctx, OriginalPayloadStructType, m_ExpandedPayloadSize,
            m_ExpandedPayloadAppendedFieldsOffset, &AppendedFieldsElementIndex);
        if (expanded.ExpandedPayloadStructPtrType == nullptr &&
            OSOverride != nullptr) {
          *OSOverride << "MeshPayloadExpansionFailed\n";
        }
      } else {
        expanded = ExpandStructType(Ctx, OriginalPayloadStructType);
        AppendedFieldsElementIndex =
            OriginalPayloadStructType->getStructNumElements();
        unsigned expandedPayloadSizeInBytes =
            (unsigned)M.getDataLayout().getTypeAllocSize(
                expanded.ExpandedPayloadStructType);
        if (expandedPayloadSizeInBytes > DXIL::kMaxMSASPayloadBytes) {
          if (OSOverride != nullptr) {
            *OSOverride << "MeshPayloadExpansionFailed\n";
          }
          expanded = {};
        }
      }

      if (expanded.ExpandedPayloadStructPtrType != nullptr) {
        llvm::Function *OriginalGetMeshPayloadFunction =
            cast<CallInst>(getMeshPayloadInstructions)->getCalledFunction();

        Function *DxilFunc = HlslOP->GetOpFunc(
            OP::OpCode::GetMeshPayload, expanded.ExpandedPayloadStructPtrType);
        Constant *opArg =
            HlslOP->GetU32Const((unsigned)OP::OpCode::GetMeshPayload);
        IRBuilder<> Builder(getMeshPayloadInstructions);
        Value *args[] = {opArg};
        Instruction *payload = Builder.CreateCall(DxilFunc, args);

        FirstNewStructGetMeshPayload = payload;
        ReplaceAllUsesOfInstructionWithNewValueAndDeleteInstruction(
            getMeshPayloadInstructions, payload,
            expanded.ExpandedPayloadStructType);
        PIXPassHelpers::EraseIfUnused(DM, OriginalGetMeshPayloadFunction);
      }
    } else if (m_ExpandedPayloadSize != 0) {
      expanded = SynthesizeExpandedPayloadType(
          Ctx, m_ExpandedPayloadSize, m_ExpandedPayloadAppendedFieldsOffset);
      if (expanded.ExpandedPayloadStructPtrType != nullptr) {
        AppendedFieldsElementIndex = 1;
        IRBuilder<> Builder(dxilutil::FirstNonAllocaInsertionPt(
            PIXPassHelpers::GetEntryFunction(DM)));
        Function *DxilFunc = HlslOP->GetOpFunc(
            OP::OpCode::GetMeshPayload, expanded.ExpandedPayloadStructPtrType);
        Constant *opArg =
            HlslOP->GetU32Const((unsigned)OP::OpCode::GetMeshPayload);
        Value *args[] = {opArg};
        FirstNewStructGetMeshPayload = Builder.CreateCall(DxilFunc, args);
      } else if (OSOverride != nullptr) {
        *OSOverride << "MeshPayloadExpansionFailed\n";
      }
    }
  }

  Instruction *firstInsertionPt =
      dxilutil::FirstNonAllocaInsertionPt(GetEntryFunction(DM));
  IRBuilder<> Builder(firstInsertionPt);

  BuilderContext BC{M, DM, Ctx, HlslOP, Builder};

  m_OffsetMask = BC.HlslOP->GetU32Const(UAVDumpingGroundOffset() - 1);

  m_OutputUAV = CreateUAVOnceForModule(DM, Builder, 0, "PIX_DebugUAV_Handle");

  if (FirstNewStructGetMeshPayload == nullptr) {
    Instruction *firstInsertionPt = dxilutil::FirstNonAllocaInsertionPt(
        PIXPassHelpers::GetEntryFunction(DM));
    IRBuilder<> Builder(firstInsertionPt);
    m_threadUniquifier = insertInstructionsToCreateDisambiguationValue(
        Builder, HlslOP, Ctx, 0, nullptr);
  } else {
    IRBuilder<> Builder(FirstNewStructGetMeshPayload->getNextNode());
    m_threadUniquifier = insertInstructionsToCreateDisambiguationValue(
        Builder, HlslOP, Ctx, AppendedFieldsElementIndex,
        FirstNewStructGetMeshPayload);
  }

  auto F = HlslOP->GetOpFunc(DXIL::OpCode::EmitIndices, Type::getVoidTy(Ctx));
  auto FunctionUses = F->uses();
  for (auto FI = FunctionUses.begin(); FI != FunctionUses.end();) {
    auto &FunctionUse = *FI++;
    auto FunctionUser = FunctionUse.getUser();

    auto Call = cast<CallInst>(FunctionUser);

    IRBuilder<> Builder2(Call);
    BuilderContext BC2{M, DM, Ctx, HlslOP, Builder2};

    Instrument(BC2, BC2.HlslOP->GetI32Const(triangleIndexIndicator),
               m_threadUniquifier[0], m_threadUniquifier[1],
               Call->getOperand(1), Call->getOperand(2), Call->getOperand(3),
               Call->getOperand(4));
  }
  PIXPassHelpers::EraseIfUnused(DM, F);

  struct OutputType {
    Type *type;
    uint32_t tag;
  };
  SmallVector<OutputType, 4> StoreVertexOutputOverloads{
      {Type::getInt32Ty(Ctx), int32ValueIndicator},
      {Type::getInt16Ty(Ctx), int16ValueIndicator},
      {Type::getFloatTy(Ctx), floatValueIndicator},
      {Type::getHalfTy(Ctx), float16ValueIndicator}};
  SmallVector<Function *, 4> StoreVertexOutputFunctions;

  for (auto const &Overload : StoreVertexOutputOverloads) {
    F = HlslOP->GetOpFunc(DXIL::OpCode::StoreVertexOutput, Overload.type);
    StoreVertexOutputFunctions.push_back(F);
    FunctionUses = F->uses();
    for (auto FI = FunctionUses.begin(); FI != FunctionUses.end();) {
      auto &FunctionUse = *FI++;
      auto FunctionUser = FunctionUse.getUser();

      auto Call = cast<CallInst>(FunctionUser);

      IRBuilder<> Builder2(Call);
      BuilderContext BC2{M, DM, Ctx, HlslOP, Builder2};

      // Expand column index to 32 bits:
      auto ColumnIndex = BC2.Builder.CreateCast(
          Instruction::ZExt, Call->getOperand(3), Type::getInt32Ty(Ctx));

      // Coerce actual value to int32
      Value *CoercedValue = Call->getOperand(4);

      if (Overload.tag == floatValueIndicator) {
        CoercedValue = BC2.Builder.CreateCast(
            Instruction::BitCast, CoercedValue, Type::getInt32Ty(Ctx));
      } else if (Overload.tag == float16ValueIndicator) {
        auto *HalfInt = BC2.Builder.CreateCast(
            Instruction::BitCast, CoercedValue, Type::getInt16Ty(Ctx));

        CoercedValue = BC2.Builder.CreateCast(Instruction::ZExt, HalfInt,
                                              Type::getInt32Ty(Ctx));
      } else if (Overload.tag == int16ValueIndicator) {
        Instruction::CastOps ExtensionKind =
            OutputSignatureElementIsSigned(DM, Call->getOperand(1))
                ? Instruction::SExt
                : Instruction::ZExt;
        CoercedValue = BC2.Builder.CreateCast(ExtensionKind, CoercedValue,
                                              Type::getInt32Ty(Ctx));
      }

      Instrument(BC2, BC2.HlslOP->GetI32Const(Overload.tag),
                 m_threadUniquifier[0], m_threadUniquifier[1],
                 Call->getOperand(1), Call->getOperand(2), ColumnIndex,
                 CoercedValue, Call->getOperand(5));
    }
  }

  for (Function *StoreVertexOutputFunction : StoreVertexOutputFunctions) {
    PIXPassHelpers::EraseIfUnused(DM, StoreVertexOutputFunction);
  }

  if (expanded.ExpandedPayloadStructType != nullptr) {
    DM.GetDxilFunctionProps(PIXPassHelpers::GetEntryFunction(DM))
        .ShaderProps.MS.payloadSizeInBytes =
        (unsigned)M.getDataLayout().getTypeAllocSize(
            expanded.ExpandedPayloadStructType);
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
