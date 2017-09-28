///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilOutputColorBecomesConstant.cpp                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Adds instrumentation that enables shader debugging in PIX                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilModule.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "llvm/support/FormattedStream.h"

#include <sstream>

using namespace llvm;
using namespace hlsl;


enum DebugShaderModifierRecordType {
  DebugShaderModifierRecordTypeInvocationStartMarker,
  DebugShaderModifierRecordTypeStep,
  DebugShaderModifierRecordTypeEvent,
  DebugShaderModifierRecordTypeInputRegister,
  DebugShaderModifierRecordTypeReadRegister,
  DebugShaderModifierRecordTypeWrittenRegister,
  DebugShaderModifierRecordTypeRegisterRelativeIndex0,
  DebugShaderModifierRecordTypeRegisterRelativeIndex1,
  DebugShaderModifierRecordTypeRegisterRelativeIndex2,
  DebugShaderModifierRecordTypeDXILStepVoid = 252,
  DebugShaderModifierRecordTypeDXILStepFloat = 253,
  DebugShaderModifierRecordTypeDXILStepUint32 = 254,
  DebugShaderModifierRecordTypeDXILStepDouble = 255,
};

struct DebugShaderModifierBufferHeader {
  uint32_t DwordCount;
};

struct DebugShaderModifierRecordHeader {
  union  {
    struct {
      uint32_t SizeDwords : 4;
      uint32_t Flags : 4;
      uint32_t Type : 8;
      uint32_t HeaderPayload : 16;
    } Details;
    uint32_t u32Header;
  } Header;
  uint32_t UID;
};

struct DebugShaderModifierRecordDXILStepBase
{
  union {
    struct {
      uint32_t SizeDwords : 4;
      uint32_t Flags : 4;
      uint32_t Type : 8;
      uint32_t Opcode : 16;
    } Details;
    uint32_t u32Header;
  } Header;
  uint32_t UID;
  uint32_t InstructionOffset;
  uint32_t VirtualRegisterOrdinal;
};

template< typename ReturnType >
struct DebugShaderModifierRecordDXILStep : public DebugShaderModifierRecordDXILStepBase
{
  ReturnType ReturnValue;
};

template< >
struct DebugShaderModifierRecordDXILStep<void> : public DebugShaderModifierRecordDXILStepBase
{
};


uint32_t DebugShaderModifierRecordPayloadSizeDwords(size_t recordTotalSizeBytes) {
  return ((recordTotalSizeBytes - sizeof(DebugShaderModifierRecordHeader)) / sizeof(uint32_t));
}

class DxilDebugInstrumentation : public ModulePass {

private:
  union ParametersAllTogether
  {
    unsigned Parameters[3];
    struct PixelShaderParameters
    {
      unsigned X;
      unsigned Y;
    } PixelShader;
    struct VertexShaderParameters
    {
      unsigned VertexId;
      unsigned InstanceId;
    } VertexShader;
    struct ComputeShaderParameters
    {
      unsigned ThreadIdX;
      unsigned ThreadIdY;
      unsigned ThreadIdZ;
    } ComputeShader;
  } m_Parameters = { 0,0,0 };

  union SystemValueIndices
  {
    struct PixelShaderParameters
    {
      unsigned Position;
    } PixelShader;
    struct VertexShaderParameters
    {
      unsigned VertexId;
      unsigned InstanceId;
    } VertexShader;
  };

  uint64_t m_UAVSize = 1024*1024;

  CallInst * m_IndexVariable = nullptr;
  Value * m_SelectionCriterion = nullptr;
  CallInst * m_HandleForUAV = nullptr;
  Value * m_InvocationId = nullptr;

  // Together these two values allow branchless writing to the UAV. An invocation of the shader
  // is either of interest or not (e.g. it writes to the pixel the user selected for debugging
  // or it doesn't). If not of interest, debugging output will still occur, but it will be
  // relegated to the very top few bytes of the UAV. Invocations of interest, by contrast, will
  // be written to the UAV at sequentially increasing offsets.

  // This value will either be one or zero (one if the invocation is of interest, zero otherwise)
  Value * m_OffsetMultiplicand = nullptr;
  // This will either be zero (if the invocation is of interest) or (UAVSize)-(SmallValue) if not.
  Value * m_OffsetAddend = nullptr;

  std::map<uint32_t, Value *> m_IncrementInstructionBySize;

  std::vector<std::string> m_Variables;

  struct BuilderContext
  {
    Module &M;
    DxilModule &DM;
    LLVMContext & Ctx;
    OP * HlslOP;
    IRBuilder<> & Builder;
  };


public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilDebugInstrumentation() : ModulePass(ID) {}
  const char *getPassName() const override { return "DXIL Force Early Z"; }
  void applyOptions(PassOptions O) override;
  bool runOnModule(Module &M) override;

private:
  void OutputDebugStringW(Module &M, const char *p);
  SystemValueIndices addRequiredSystemValues(BuilderContext & BC);
  CallInst * addUAV(BuilderContext & BC);
  Value * addInvocationSelectionProlog(BuilderContext & BC, SystemValueIndices SVIndices);
  Value * addPixelShaderProlog(BuilderContext & BC, SystemValueIndices SVIndices);
  Value * addComputeShaderProlog(BuilderContext & BC);
  Value * addVertexShaderProlog(BuilderContext & BC, SystemValueIndices SVIndices);
  void addDebugEntryValue(BuilderContext & BC, Value * Index, Value * TheValue);
  void addInvocationStartMarker(BuilderContext & BC);
  Value * reserveDebugEntrySpace(BuilderContext & BC, uint32_t SpaceInDwords);
  Value * incrementUAVIndex(BuilderContext & BC, Value * CurrentValue);
  void addStepDebugEntry(BuilderContext & BC, unsigned int InstructionIndex, Instruction * Inst);
  uint32_t UAVDumpingGroundOffset();
  template<typename ReturnType>
  void addStepEntryForType(DebugShaderModifierRecordType RecordType, BuilderContext & BC, unsigned int InstructionIndex, Instruction * Inst);

};

void DxilDebugInstrumentation::applyOptions(PassOptions O)
{
  for (const auto & option : O)
  {
    if (0 == option.first.compare("parameter0"))
    {
      m_Parameters.Parameters[0] = atoi(option.second.data());
    }
    else if (0 == option.first.compare("parameter1"))
    {
      m_Parameters.Parameters[1] = atoi(option.second.data());
    }
    else if (0 == option.first.compare("parameter2"))
    {
      m_Parameters.Parameters[2] = atoi(option.second.data());
    }
    else if (0 == option.first.compare("UAVSize"))
    {
      m_UAVSize = std::stoull(option.second.data());
    }
  }
}

#define MAX_ROOM_NEEDED_FOR_DEBUG_FIELD 64
uint32_t DxilDebugInstrumentation::UAVDumpingGroundOffset()
{
  return static_cast<uint32_t>(m_UAVSize - MAX_ROOM_NEEDED_FOR_DEBUG_FIELD);
}


void DxilDebugInstrumentation::OutputDebugStringW(Module &M, const char *p)
{
  if (OSOverride != nullptr) {
    formatted_raw_ostream FOS(*OSOverride);
    FOS << p << "\n";
  }
}

DxilDebugInstrumentation::SystemValueIndices DxilDebugInstrumentation::addRequiredSystemValues(BuilderContext & BC)
{
  SystemValueIndices SVIndices{};

  hlsl::DxilSignature & InputSignature = BC.DM.GetInputSignature();

  auto & InputElements = InputSignature.GetElements();

  auto ShaderModel = BC.DM.GetShaderModel();
  switch (ShaderModel->GetKind())
  {
  case DXIL::ShaderKind::Pixel:
  {
    auto Existing_SV_Position = std::find_if(
      InputElements.begin(), InputElements.end(),
      [](const std::unique_ptr<DxilSignatureElement> & Element) {
      return Element->GetSemantic()->GetKind() == hlsl::DXIL::SemanticKind::Position; });

    // SV_Position, if present, has to have full mask, so we needn't worry 
    // about the shader having selected components that don't include x or y.
    // If not present, we add it.
    if (Existing_SV_Position == InputElements.end()) {
      auto Added_SV_Position = std::make_unique<DxilSignatureElement>(DXIL::SigPointKind::PSIn);
      Added_SV_Position->Initialize("Position", hlsl::CompType::getF32(), hlsl::DXIL::InterpolationMode::Linear, 1, 4);
      Added_SV_Position->AppendSemanticIndex(0);
      Added_SV_Position->SetSigPointKind(DXIL::SigPointKind::PSIn);
      Added_SV_Position->SetKind(hlsl::DXIL::SemanticKind::Position);

      auto index = InputSignature.AppendElement(std::move(Added_SV_Position));
      SVIndices.PixelShader.Position = InputElements[index]->GetID();
    }
    else {
      SVIndices.PixelShader.Position = Existing_SV_Position->get()->GetID();
    }
  }
  break;
  case DXIL::ShaderKind::Vertex:
  {
    {
      auto Existing_SV_VertexId = std::find_if(
        InputElements.begin(), InputElements.end(),
        [](const std::unique_ptr<DxilSignatureElement> & Element) {
        return Element->GetSemantic()->GetKind() == hlsl::DXIL::SemanticKind::VertexID; });

      if (Existing_SV_VertexId == InputElements.end()) {
        auto Added_SV_VertexId = std::make_unique<DxilSignatureElement>(DXIL::SigPointKind::VSIn);
        Added_SV_VertexId->Initialize("VertexId", hlsl::CompType::getF32(), hlsl::DXIL::InterpolationMode::Undefined, 1, 1);
        Added_SV_VertexId->AppendSemanticIndex(0);
        Added_SV_VertexId->SetSigPointKind(DXIL::SigPointKind::VSIn);
        Added_SV_VertexId->SetKind(hlsl::DXIL::SemanticKind::VertexID);

        auto index = InputSignature.AppendElement(std::move(Added_SV_VertexId));
        SVIndices.VertexShader.VertexId = InputElements[index]->GetID();
      }
      else {
        SVIndices.VertexShader.VertexId = Existing_SV_VertexId->get()->GetID();
      }
    }
    {
      auto Existing_SV_InstanceId = std::find_if(
        InputElements.begin(), InputElements.end(),
        [](const std::unique_ptr<DxilSignatureElement> & Element) {
        return Element->GetSemantic()->GetKind() == hlsl::DXIL::SemanticKind::InstanceID; });

      if (Existing_SV_InstanceId == InputElements.end()) {
        auto Added_SV_InstanceId = std::make_unique<DxilSignatureElement>(DXIL::SigPointKind::VSIn);
        Added_SV_InstanceId->Initialize("InstanceId", hlsl::CompType::getF32(), hlsl::DXIL::InterpolationMode::Undefined, 1, 1);
        Added_SV_InstanceId->AppendSemanticIndex(0);
        Added_SV_InstanceId->SetSigPointKind(DXIL::SigPointKind::VSIn);
        Added_SV_InstanceId->SetKind(hlsl::DXIL::SemanticKind::InstanceID);

        auto index = InputSignature.AppendElement(std::move(Added_SV_InstanceId));
        SVIndices.VertexShader.InstanceId = InputElements[index]->GetID();
      }
      else {
        SVIndices.VertexShader.InstanceId = Existing_SV_InstanceId->get()->GetID();
      }
    }
  }
  break;
  case DXIL::ShaderKind::Compute:
    // Compute thread Id is not in the input signature
  //{
  //  auto Existing_SV_ThreadId = std::find_if(
  //    InputElements.begin(), InputElements.end(),
  //    [](const std::unique_ptr<DxilSignatureElement> & Element) {
  //    return Element->GetSemantic()->GetKind() == hlsl::DXIL::SemanticKind::DispatchThreadID; });
  //
  //  if (Existing_SV_ThreadId == InputElements.end()) {
  //    auto Added_SV_Thread = std::make_unique<DxilSignatureElement>(DXIL::SigPointKind::CSIn);
  //    Added_SV_Thread->Initialize("ThreadId", hlsl::CompType::getF32(), hlsl::DXIL::InterpolationMode::Undefined, 1, 4);
  //    Added_SV_Thread->AppendSemanticIndex(0);
  //    Added_SV_Thread->SetSigPointKind(DXIL::SigPointKind::CSIn);
  //    Added_SV_Thread->SetKind(hlsl::DXIL::SemanticKind::DispatchThreadID);
  //
  //    auto index = InputSignature.AppendElement(std::move(Added_SV_Thread));
  //    SVIndices.ComputeShader.ThreadId = InputElements[index]->GetID();
  //  }
  //  else {
  //    SVIndices.ComputeShader.ThreadId = Existing_SV_ThreadId->get()->GetID();
  //  }
  //}
  break;
  default:
    assert(false);
  }

  return SVIndices;
}

Value * DxilDebugInstrumentation::addComputeShaderProlog(BuilderContext & BC)
{
  Constant* Zero32Arg = BC.HlslOP->GetU32Const(0);
  Constant* One32Arg = BC.HlslOP->GetU32Const(1);
  Constant* Two32Arg = BC.HlslOP->GetU32Const(2);

  auto ThreadIdFunc = BC.HlslOP->GetOpFunc(DXIL::OpCode::ThreadId, Type::getInt32Ty(BC.Ctx));
  Constant* Opcode = BC.HlslOP->GetU32Const((unsigned)DXIL::OpCode::ThreadId);
  auto ThreadIdX = BC.Builder.CreateCall(ThreadIdFunc, { Opcode, Zero32Arg }, "ThreadIdX");
  auto ThreadIdY = BC.Builder.CreateCall(ThreadIdFunc, { Opcode, One32Arg  }, "ThreadIdY");
  auto ThreadIdZ = BC.Builder.CreateCall(ThreadIdFunc, { Opcode, Two32Arg  }, "ThreadIdZ");

  // Compare to expected pixel position and primitive ID
  auto CompareToX = BC.Builder.CreateICmpEQ(ThreadIdX, BC.HlslOP->GetU32Const(m_Parameters.ComputeShader.ThreadIdX), "CompareToThreadIdX");
  auto CompareToY = BC.Builder.CreateICmpEQ(ThreadIdY, BC.HlslOP->GetU32Const(m_Parameters.ComputeShader.ThreadIdY), "CompareToThreadIdY");
  auto CompareToZ = BC.Builder.CreateICmpEQ(ThreadIdZ, BC.HlslOP->GetU32Const(m_Parameters.ComputeShader.ThreadIdZ), "CompareToThreadIdZ");

  auto CompareXAndY = BC.Builder.CreateAnd(CompareToX, CompareToY, "CompareXAndY");

  auto CompareAll = BC.Builder.CreateAnd(CompareXAndY, CompareToZ, "CompareAll");

  return CompareAll;
}

Value * DxilDebugInstrumentation::addVertexShaderProlog(BuilderContext & BC, SystemValueIndices SVIndices)
{
  Constant* Zero32Arg = BC.HlslOP->GetU32Const(0);
  Constant* Zero8Arg = BC.HlslOP->GetI8Const(0);
  UndefValue* UndefArg = UndefValue::get(Type::getInt32Ty(BC.Ctx));

  auto LoadInputOpFunc = BC.HlslOP->GetOpFunc(DXIL::OpCode::LoadInput, Type::getInt32Ty(BC.Ctx));
  Constant* LoadInputOpcode = BC.HlslOP->GetU32Const((unsigned)DXIL::OpCode::LoadInput);
  Constant*  SV_Vert_ID = BC.HlslOP->GetU32Const(SVIndices.VertexShader.VertexId);
  auto VertId = BC.Builder.CreateCall(LoadInputOpFunc,
  { LoadInputOpcode, SV_Vert_ID, Zero32Arg /*row*/, Zero8Arg /*column*/, UndefArg }, "VertId");

  Constant*  SV_Instance_ID = BC.HlslOP->GetU32Const(SVIndices.VertexShader.InstanceId);
  auto InstanceId = BC.Builder.CreateCall(LoadInputOpFunc,
  { LoadInputOpcode, SV_Instance_ID, Zero32Arg /*row*/, Zero8Arg /*column*/, UndefArg }, "InstanceId");

  // Compare to expected pixel position and primitive ID
  auto CompareToVert = BC.Builder.CreateICmpEQ(VertId, BC.HlslOP->GetU32Const(m_Parameters.VertexShader.VertexId), "CompareToVertId");
  auto CompareToInstance = BC.Builder.CreateICmpEQ(InstanceId, BC.HlslOP->GetU32Const(m_Parameters.VertexShader.InstanceId), "CompareToInstanceId");
  auto CompareBoth = BC.Builder.CreateAnd(CompareToVert, CompareToInstance, "CompareBoth");

  return CompareBoth;
}

Value * DxilDebugInstrumentation::addPixelShaderProlog(BuilderContext & BC, SystemValueIndices SVIndices)
{
  Constant* Zero32Arg = BC.HlslOP->GetU32Const(0);
  Constant* Zero8Arg = BC.HlslOP->GetI8Const(0);
  Constant* One8Arg = BC.HlslOP->GetI8Const(1);
  UndefValue* UndefArg = UndefValue::get(Type::getInt32Ty(BC.Ctx));

  // Convert SV_POSITION to UINT    
  Value * XAsInt;
  Value * YAsInt;
  {
    auto LoadInputOpFunc = BC.HlslOP->GetOpFunc(DXIL::OpCode::LoadInput, Type::getFloatTy(BC.Ctx));
    Constant* LoadInputOpcode = BC.HlslOP->GetU32Const((unsigned)DXIL::OpCode::LoadInput);
    Constant*  SV_Pos_ID = BC.HlslOP->GetU32Const(SVIndices.PixelShader.Position);
    auto XPos = BC.Builder.CreateCall(LoadInputOpFunc,
    { LoadInputOpcode, SV_Pos_ID, Zero32Arg /*row*/, Zero8Arg /*column*/, UndefArg }, "XPos");
    auto YPos = BC.Builder.CreateCall(LoadInputOpFunc,
    { LoadInputOpcode, SV_Pos_ID, Zero32Arg /*row*/, One8Arg /*column*/, UndefArg }, "YPos");

    XAsInt = BC.Builder.CreateCast(Instruction::CastOps::FPToUI, XPos, Type::getInt32Ty(BC.Ctx), "XIndex");
    YAsInt = BC.Builder.CreateCast(Instruction::CastOps::FPToUI, YPos, Type::getInt32Ty(BC.Ctx), "YIndex");
  }

  // Compare to expected pixel position and primitive ID
  auto CompareToX = BC.Builder.CreateICmpEQ(XAsInt, BC.HlslOP->GetU32Const(m_Parameters.PixelShader.X), "CompareToX");
  auto CompareToY = BC.Builder.CreateICmpEQ(YAsInt, BC.HlslOP->GetU32Const(m_Parameters.PixelShader.Y), "CompareToY");
  auto ComparePos = BC.Builder.CreateAnd(CompareToX, CompareToY, "ComparePos");

  return ComparePos;
}

CallInst * DxilDebugInstrumentation::addUAV(BuilderContext & BC)
{
  // Set up a UAV with structure of a single int
  unsigned int UAVResourceHandle = static_cast<unsigned int>(BC.DM.GetUAVs().size());
  SmallVector<llvm::Type*, 1> Elements{ Type::getInt32Ty(BC.Ctx) };
  llvm::StructType *UAVStructTy = llvm::StructType::create(Elements, "PIX_DebugUAV_Type");
  std::unique_ptr<DxilResource> pUAV = llvm::make_unique<DxilResource>();
  pUAV->SetGlobalName("PIX_DebugUAVName");
  pUAV->SetGlobalSymbol(UndefValue::get(UAVStructTy->getPointerTo()));
  pUAV->SetID(UAVResourceHandle);
  pUAV->SetSpaceID((unsigned int)-2); // This is the reserved-for-tools register space
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
  Function* CreateHandleOpFunc = BC.HlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(BC.Ctx));
  Constant* CreateHandleOpcodeArg = BC.HlslOP->GetU32Const((unsigned)DXIL::OpCode::CreateHandle);
  Constant* UAVVArg = BC.HlslOP->GetI8Const(static_cast<std::underlying_type<DxilResourceBase::Class>::type>(DXIL::ResourceClass::UAV));
  Constant* MetaDataArg = BC.HlslOP->GetU32Const(ID); // position of the metadata record in the corresponding metadata list
  Constant* IndexArg = BC.HlslOP->GetU32Const(0); // 
  Constant* FalseArg = BC.HlslOP->GetI1Const(0); // non-uniform resource index: false
  auto HandleForUAV = BC.Builder.CreateCall(CreateHandleOpFunc,
  { CreateHandleOpcodeArg, UAVVArg, MetaDataArg, IndexArg, FalseArg }, "PIX_DebugUAV_Handle");

  return HandleForUAV;
}

Value * DxilDebugInstrumentation::addInvocationSelectionProlog(BuilderContext & BC, SystemValueIndices SVIndices)
{
  auto ShaderModel = BC.DM.GetShaderModel();

  Value * ParameterTestResult;
  switch (ShaderModel->GetKind())
  {
  case DXIL::ShaderKind::Pixel:
    ParameterTestResult = addPixelShaderProlog(BC, SVIndices);
    break;
    case DXIL::ShaderKind::Vertex:
      ParameterTestResult = addVertexShaderProlog(BC, SVIndices);
    break;
    case DXIL::ShaderKind::Compute:
      ParameterTestResult = addComputeShaderProlog(BC);
    break;
  default:
    assert(false);
  }

  m_OffsetMultiplicand = BC.Builder.CreateCast(Instruction::CastOps::ZExt, ParameterTestResult, Type::getInt32Ty(BC.Ctx), "OffsetMultiplicand");
  auto InverseOffsetMultiplicand = BC.Builder.CreateSub(BC.HlslOP->GetU32Const(1), m_OffsetMultiplicand, "ComplementOfMultiplicand");
  m_OffsetAddend = BC.Builder.CreateMul(BC.HlslOP->GetU32Const(UAVDumpingGroundOffset()), InverseOffsetMultiplicand, "OffsetAddend");

  return ParameterTestResult;
}

Value * DxilDebugInstrumentation::reserveDebugEntrySpace(BuilderContext & BC, uint32_t SpaceInBytes)
{
  // Insert the UAV increment instruction:
  Function* AtomicOpFunc = BC.HlslOP->GetOpFunc(OP::OpCode::AtomicBinOp, Type::getInt32Ty(BC.Ctx));
  Constant* AtomicBinOpcode = BC.HlslOP->GetU32Const((unsigned)OP::OpCode::AtomicBinOp);
  Constant* AtomicAdd = BC.HlslOP->GetU32Const((unsigned)DXIL::AtomicBinOpCode::Add);
  Constant* Zero32Arg = BC.HlslOP->GetU32Const(0);
  UndefValue* UndefArg = UndefValue::get(Type::getInt32Ty(BC.Ctx));

  // so inc will be zero for uninteresting invocations:
  Value * IncrementForThisInvocation;
  auto findIncrementInstruction = m_IncrementInstructionBySize.find(SpaceInBytes);
  if (findIncrementInstruction == m_IncrementInstructionBySize.end())
  {
    Constant* Increment = BC.HlslOP->GetU32Const(SpaceInBytes);
    auto it = m_IncrementInstructionBySize.emplace(
      SpaceInBytes, BC.Builder.CreateMul(Increment, m_OffsetMultiplicand, "IncrementForThisInvocation"));
    findIncrementInstruction = it.first;
  }
  IncrementForThisInvocation = findIncrementInstruction->second;

  auto PreviousValue = BC.Builder.CreateCall(AtomicOpFunc, {
    AtomicBinOpcode,// i32, ; opcode
    m_HandleForUAV, // %dx.types.Handle, ; resource handle
    AtomicAdd,      // i32, ; binary operation code : EXCHANGE, IADD, AND, OR, XOR, IMIN, IMAX, UMIN, UMAX
    Zero32Arg,      // i32, ; coordinate c0: index in bytes
    UndefArg,       // i32, ; coordinate c1 (unused)
    UndefArg,       // i32, ; coordinate c2 (unused)
    IncrementForThisInvocation,      // i32); increment value
  }, "UAVIncResult");

  if (m_InvocationId == nullptr)
  {
      m_InvocationId = PreviousValue;
  }

  // The return value will either end up being itself (multiplied by one and added with zero)
  // or the "dump uninteresting things here" value of (UAVSize - a bit).
  auto MultipliedForInterest = BC.Builder.CreateMul(PreviousValue, m_OffsetMultiplicand);
  auto AddedForInterest = BC.Builder.CreateAdd(MultipliedForInterest, m_OffsetAddend);
  return AddedForInterest;
}

Value * DxilDebugInstrumentation::incrementUAVIndex(BuilderContext & BC, Value * CurrentValue)
{
  return BC.Builder.CreateAdd(CurrentValue, BC.HlslOP->GetU32Const(4));;
}

void DxilDebugInstrumentation::addDebugEntryValue(BuilderContext & BC, Value * Index, Value * TheValue)
{
  Function* StoreValue = BC.HlslOP->GetOpFunc(OP::OpCode::BufferStore, TheValue->getType()); // Type::getInt32Ty(BC.Ctx));
  Constant* StoreValueOpcode = BC.HlslOP->GetU32Const((unsigned)DXIL::OpCode::BufferStore);
  Constant* Zero32Arg = BC.HlslOP->GetU32Const(0);
  Constant* ZeroArg;
  auto TheValueTypeID = TheValue->getType()->getTypeID();
  if (TheValueTypeID  == Type::TypeID::IntegerTyID)
  {
      ZeroArg = BC.HlslOP->GetU32Const(0);
  }
  else if (TheValueTypeID == Type::TypeID::FloatTyID)
  {
      ZeroArg = BC.HlslOP->GetFloatConst(0.f);
  }
  else
  {
      __debugbreak();
  }
  Constant* WriteMask_X = BC.HlslOP->GetI8Const(1);
  (void)BC.Builder.CreateCall(StoreValue, {
    StoreValueOpcode, // i32 opcode
    m_HandleForUAV,     // %dx.types.Handle, ; resource handle
    Index,            // i32 c0: index in bytes into UAV
    Zero32Arg,        // i32 c1: unused
    TheValue,
    ZeroArg,        // unused values
    ZeroArg,        // unused values
    ZeroArg,        // unused values
    WriteMask_X
  });
}

void DxilDebugInstrumentation::addInvocationStartMarker(BuilderContext & BC)
{
  DebugShaderModifierRecordHeader marker{ 0 };
  auto RecordStart = reserveDebugEntrySpace(BC, sizeof(marker));

  marker.Header.Details.SizeDwords = DebugShaderModifierRecordPayloadSizeDwords(sizeof(marker));;
  marker.Header.Details.Flags = 0;
  marker.Header.Details.Type = DebugShaderModifierRecordTypeInvocationStartMarker;
  addDebugEntryValue(BC, RecordStart, BC.HlslOP->GetU32Const(marker.Header.u32Header));
  auto NextIndex = incrementUAVIndex(BC, RecordStart);
  addDebugEntryValue(BC, NextIndex, m_InvocationId);
}

template<typename ReturnType>
void DxilDebugInstrumentation::addStepEntryForType(DebugShaderModifierRecordType RecordType, BuilderContext & BC, unsigned int InstructionIndex, Instruction * Inst)
{
  DebugShaderModifierRecordDXILStep<ReturnType> step = {};
  auto RecordStart = reserveDebugEntrySpace(BC, sizeof(step));

  step.Header.Details.SizeDwords = DebugShaderModifierRecordPayloadSizeDwords(sizeof(step));
  step.Header.Details.Type = static_cast<uint8_t>(RecordType);
  addDebugEntryValue(BC, RecordStart, BC.HlslOP->GetU32Const(step.Header.u32Header));
  auto SecondIndex = incrementUAVIndex(BC, RecordStart);
  addDebugEntryValue(BC, SecondIndex, m_InvocationId);
  auto ThirdIndex = incrementUAVIndex(BC, SecondIndex);
  addDebugEntryValue(BC, ThirdIndex, BC.HlslOP->GetU32Const(InstructionIndex));
  auto FourthIndex = incrementUAVIndex(BC, ThirdIndex);

  auto pName = std::find(m_Variables.begin(), m_Variables.end(), Inst->getName());
  auto RegisterIndex = static_cast<uint32_t>(pName - m_Variables.begin());

  addDebugEntryValue(BC, FourthIndex, BC.HlslOP->GetU32Const(RegisterIndex));

  if (RecordType != DebugShaderModifierRecordTypeDXILStepVoid)
  {
    auto FifthIndex = incrementUAVIndex(BC, FourthIndex);
    addDebugEntryValue(BC, FifthIndex, Inst);
  }
}

void DxilDebugInstrumentation::addStepDebugEntry(BuilderContext & BC, unsigned int InstructionIndex, Instruction * Inst)
{
  Type::TypeID ID = Inst->getType()->getTypeID();

  switch (ID)
  {
  case Type::TypeID::StructTyID:
  case Type::TypeID::VoidTyID:
    addStepEntryForType<void>(DebugShaderModifierRecordTypeDXILStepVoid, BC, InstructionIndex, Inst);
    break;
  case Type::TypeID::FloatTyID:
    addStepEntryForType<float>(DebugShaderModifierRecordTypeDXILStepFloat, BC, InstructionIndex, Inst);
    break;
  case Type::TypeID::IntegerTyID:
    addStepEntryForType<uint32_t>(DebugShaderModifierRecordTypeDXILStepUint32, BC, InstructionIndex, Inst);
    break;
  case Type::TypeID::DoubleTyID:
    addStepEntryForType<double>(DebugShaderModifierRecordTypeDXILStepDouble, BC, InstructionIndex, Inst);
    break;
  case Type::TypeID::FP128TyID:
  case Type::TypeID::HalfTyID:
  case Type::TypeID::LabelTyID:
  case Type::TypeID::MetadataTyID:
  case Type::TypeID::FunctionTyID:
  case Type::TypeID::ArrayTyID:
  case Type::TypeID::PointerTyID:
  case Type::TypeID::VectorTyID:
    assert(false);
  }

}

bool DxilDebugInstrumentation::runOnModule(Module &M)
{
  DxilModule &DM = M.GetOrCreateDxilModule();
  LLVMContext & Ctx = M.getContext();
  OP *HlslOP = DM.GetOP();

  // First record pointers to all instructions in the function:
  std::vector<Instruction*> AllInstrucitons;
  auto & Blocks = DM.GetEntryFunction()->getBasicBlockList();
  for (auto & b : Blocks)
  {
    auto & Instructions = b.getInstList();

    for (
      Instruction * InstructionIterator = Instructions.begin();
      InstructionIterator != Instructions.end();
      InstructionIterator = InstructionIterator->getNextNode())
    {
      AllInstrucitons.push_back(InstructionIterator);
    }
  }

  // Branchless instrumentation requires taking care of a few things:
  // -Each invocation of the shader will be either of interest or not of interest
  //    -If of interest, the offset into the output UAV will be as expected
  //    -If not, the offset is forced to (UAVsize) - (Small Amount), and that output is ignored by the CPU-side code.
  // -The invocation of interest may overflow the UAV. This is handled by taking the modulus of the
  //  output index. Overflow is then detected on the CPU side by checking for the presence of a canary
  //  value at (UAVSize) - (Small Amount) * 2 (which is actually a conservative definition of overflow).
  //

  Instruction* firstInsertionPt = DM.GetEntryFunction()->getEntryBlock().getFirstInsertionPt();
  IRBuilder<> Builder(firstInsertionPt);

  BuilderContext BC{ M, DM, Ctx, HlslOP, Builder };

  m_HandleForUAV = addUAV(BC);
  auto SystemValues = addRequiredSystemValues(BC);
  m_SelectionCriterion = addInvocationSelectionProlog(BC, SystemValues);
  addInvocationStartMarker(BC);

  // Instrument original instructions:
  {
    unsigned int InstructionIndex = 0;
    unsigned int UnnamedVariableCounter = 0;
    for (auto & Inst : AllInstrucitons)
    {
      if (OSOverride != nullptr) {
        formatted_raw_ostream FOS(*OSOverride);
        Inst->print(FOS);
        FOS << "\n<--Instruction\n";
      }

      if (!Inst->getType()->isVoidTy())
      {
        if (Inst->getName().empty())
        {
          std::ostringstream s;
          s << UnnamedVariableCounter++;
          Inst->setName(s.str().c_str());
        }
        m_Variables.emplace_back(Inst->getName().data());
      }

      if (Inst != AllInstrucitons.back() && Inst->getNextNode()) //Inst->getOpcode() != Instruction::Ret)
      {
        IRBuilder<> Builder(Inst->getNextNode());
        BuilderContext BC2{ BC.M, BC.DM, BC.Ctx, BC.HlslOP, Builder };
        addStepDebugEntry(BC2, InstructionIndex++, Inst);
      }
    }
  }

  DM.ReEmitDxilResources();

  return true;
}

char DxilDebugInstrumentation::ID = 0;

ModulePass *llvm::createDxilDebugInstrumentationPass() {
  return new DxilDebugInstrumentation();
}

INITIALIZE_PASS(DxilDebugInstrumentation, "hlsl-dxil-debug-instrumentation", "HLSL DXIL debug instrumentation for PIX", false, false)
