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

#include "llvm/support/FormattedStream.h"

using namespace llvm;
using namespace hlsl;

class DxilDebugInstrumentation : public ModulePass {

private:
  union ParametersAllTogether
  {
    unsigned Parameters[3];
    struct PixelShaderParameters
    {
      unsigned X;
      unsigned Y;
      unsigned PrimitiveId;
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
  } Parameters = { 0,0,0 };

  union SystemValueIndices
  {
    struct PixelShaderParameters
    {
      unsigned Position;
      unsigned PrimitiveId;
    } PixelShader;
    struct VertexShaderParameters
    {
      unsigned VertexId;
      unsigned InstanceId;
    } VertexShader;
    struct ComputeShaderParameters
    {
      unsigned ThreadId;
    } ComputeShader;
  };

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
  void recordInputValue(BuilderContext & BC, CallInst * HandleForUAV, Value const * operand);
};

void DxilDebugInstrumentation::applyOptions(PassOptions O)
{
  for (const auto & option : O)
  {
    if (0 == option.first.compare("parameter0"))
    {
      Parameters.Parameters[0] = atoi(option.second.data());
    }
    else if (0 == option.first.compare("parameter1"))
    {
      Parameters.Parameters[1] = atoi(option.second.data());
    }
    else if (0 == option.first.compare("parameter2"))
    {
      Parameters.Parameters[2] = atoi(option.second.data());
    }
  }
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
  SystemValueIndices SVIndices;

  hlsl::DxilSignature & InputSignature = BC.DM.GetInputSignature();

  auto & InputElements = InputSignature.GetElements();

  auto ShaderModel = BC.DM.GetShaderModel();
  switch (ShaderModel->GetKind())
  {
  case DXIL::ShaderKind::Pixel:
  {
    auto Existing_SV_Position = std::find_if(InputElements.begin(), InputElements.end(), [](const std::unique_ptr<DxilSignatureElement> & Element) {
      return Element->GetSemantic()->GetKind() == hlsl::DXIL::SemanticKind::Position; });

    // SV_Position, if present, has to have full mask, so we needn't worry 
    // about the shader having selected components that don't include x or y.
    // If not present, we add it.
    if (Existing_SV_Position == InputElements.end()) {
      auto Added_SV_Position = std::make_unique<DxilSignatureElement>(DXIL::SigPointKind::PSIn);
      Added_SV_Position->Initialize("Position", hlsl::CompType::getF32(), hlsl::DXIL::InterpolationMode::Linear, 1, 4, 0, 0);
      Added_SV_Position->AppendSemanticIndex(0);
      Added_SV_Position->SetSigPointKind(DXIL::SigPointKind::PSIn);
      Added_SV_Position->SetKind(hlsl::DXIL::SemanticKind::Position);

      auto index = InputSignature.AppendElement(std::move(Added_SV_Position));
      SVIndices.PixelShader.Position = InputElements[index]->GetID();
    }
    else {
      SVIndices.PixelShader.Position = Existing_SV_Position->get()->GetID();
    }

    auto Existing_SV_PrimId = std::find_if(InputElements.begin(), InputElements.end(), [](const std::unique_ptr<DxilSignatureElement> & Element) {
      return Element->GetSemantic()->GetKind() == hlsl::DXIL::SemanticKind::PrimitiveID; });

    if (Existing_SV_PrimId == InputElements.end()) {
      auto Added_SV_PrimId = std::make_unique<DxilSignatureElement>(DXIL::SigPointKind::PSIn);
      Added_SV_PrimId->Initialize("PrimitiveId", hlsl::CompType::getF32(), hlsl::DXIL::InterpolationMode::Linear, 1, 4, 0, 0);
      Added_SV_PrimId->AppendSemanticIndex(0);
      Added_SV_PrimId->SetSigPointKind(DXIL::SigPointKind::PSIn);
      Added_SV_PrimId->SetKind(hlsl::DXIL::SemanticKind::PrimitiveID);

      auto index = InputSignature.AppendElement(std::move(Added_SV_PrimId));
      SVIndices.PixelShader.PrimitiveId = InputElements[index]->GetID();
    }
    else {
      SVIndices.PixelShader.PrimitiveId = Existing_SV_PrimId->get()->GetID();
    }
  }
  break;
  default:
    assert(false);
  }

  return SVIndices;
}

Value * DxilDebugInstrumentation::addPixelShaderProlog(BuilderContext & BC, SystemValueIndices SVIndices)
{
  Constant* Zero32Arg = BC.HlslOP->GetU32Const(0);
  Constant* Zero8Arg  = BC.HlslOP->GetI8Const(0);
  Constant* One8Arg   = BC.HlslOP->GetI8Const(1);
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
  auto CompareToX = BC.Builder.CreateICmpEQ(XAsInt, BC.HlslOP->GetU32Const(Parameters.PixelShader.X), "CompareToX");
  auto CompareToY = BC.Builder.CreateICmpEQ(YAsInt, BC.HlslOP->GetU32Const(Parameters.PixelShader.Y), "CompareToY");
  auto ComparePos = BC.Builder.CreateAnd(CompareToX, CompareToY, "ComparePos");

  // Compare primitve ID to expected primitive ID
  CallInst * PrimitiveId;
  {
    auto LoadInputOpFunc = BC.HlslOP->GetOpFunc(DXIL::OpCode::LoadInput, Type::getInt32Ty(BC.Ctx));
    Constant* LoadInputOpcode = BC.HlslOP->GetU32Const((unsigned)DXIL::OpCode::LoadInput);
    Constant*  SV_Prim_ID = BC.HlslOP->GetU32Const(SVIndices.PixelShader.PrimitiveId);
    PrimitiveId = BC.Builder.CreateCall(LoadInputOpFunc,
      { LoadInputOpcode, SV_Prim_ID, Zero32Arg /*row*/, Zero8Arg /*column*/, UndefArg }, "PrimitiveId");
  }

  auto CompareToExpectedPrimId = BC.Builder.CreateICmpEQ(PrimitiveId, BC.HlslOP->GetU32Const(Parameters.PixelShader.PrimitiveId), "CompareToPrimId");

  // Merge comparisons into one:
  auto CompareAll = BC.Builder.CreateAnd(ComparePos, CompareToExpectedPrimId, "ComparePosAndPrimId");

  return CompareAll;
}

CallInst * DxilDebugInstrumentation::addUAV(BuilderContext & BC)
{

  // Set up a UAV with structure of a single int
  SmallVector<llvm::Type*, 1> Elements{ Type::getInt32Ty(BC.Ctx) };
  llvm::StructType *UAVStructTy = llvm::StructType::create(Elements, "PIX_CountUAV_Type");
  std::unique_ptr<DxilResource> pUAV = llvm::make_unique<DxilResource>();
  pUAV->SetGlobalName("PIX_CountUAVName");
  pUAV->SetGlobalSymbol(UndefValue::get(UAVStructTy->getPointerTo()));
  pUAV->SetID(0);
  pUAV->SetSpaceID((unsigned int)-2); // This is the reserved-for-tools register space
  pUAV->SetSampleCount(1);
  pUAV->SetGloballyCoherent(false);
  pUAV->SetHasCounter(false);
  pUAV->SetCompType(CompType::getI32());
  pUAV->SetLowerBound(0);
  pUAV->SetRangeSize(1);
  pUAV->SetKind(DXIL::ResourceKind::StructuredBuffer);
  pUAV->SetElementStride(4);

  auto ID = BC.DM.AddUAV(std::move(pUAV));

  // Create handle for the newly-added UAV
  Function* CreateHandleOpFunc = BC.HlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(BC.Ctx));
  Constant* CreateHandleOpcodeArg = BC.HlslOP->GetU32Const((unsigned)DXIL::OpCode::CreateHandle);
  Constant* UAVVArg = BC.HlslOP->GetI8Const(static_cast<std::underlying_type<DxilResourceBase::Class>::type>(DXIL::ResourceClass::UAV));
  Constant* MetaDataArg = BC.HlslOP->GetU32Const(ID); // position of the metadata record in the corresponding metadata list
  Constant* IndexArg = BC.HlslOP->GetU32Const(0); // 
  Constant* FalseArg = BC.HlslOP->GetI1Const(0); // non-uniform resource index: false
  auto HandleForUAV = BC.Builder.CreateCall(CreateHandleOpFunc,
  { CreateHandleOpcodeArg, UAVVArg, MetaDataArg, IndexArg, FalseArg }, "PIX_CountUAV_Handle");

  return HandleForUAV;
}

void DxilDebugInstrumentation::recordInputValue(BuilderContext & BC, CallInst * HandleForUAV, Value const * operand)
{

  Type::TypeID ID = operand->getType()->getTypeID();

  switch (ID)
  {
      case Type::TypeID::VoidTyID     : OutputDebugStringW(BC.M, "VoidTyID     "); break;
      case Type::TypeID::HalfTyID     : OutputDebugStringW(BC.M, "HalfTyID     "); break;
      case Type::TypeID::FloatTyID    : OutputDebugStringW(BC.M, "FloatTyID    "); break;
      case Type::TypeID::DoubleTyID   : OutputDebugStringW(BC.M, "DoubleTyID   "); break;
      case Type::TypeID::X86_FP80TyID : OutputDebugStringW(BC.M, "X86_FP80TyID "); break;
      case Type::TypeID::FP128TyID    : OutputDebugStringW(BC.M, "FP128TyID    "); break;
      case Type::TypeID::PPC_FP128TyID: OutputDebugStringW(BC.M, "PPC_FP128TyID"); break;
      case Type::TypeID::LabelTyID    : OutputDebugStringW(BC.M, "LabelTyID    "); break;
      case Type::TypeID::MetadataTyID : OutputDebugStringW(BC.M, "MetadataTyID "); break;
      case Type::TypeID::X86_MMXTyID  : OutputDebugStringW(BC.M, "X86_MMXTyID  "); break;
      case Type::TypeID::IntegerTyID  : OutputDebugStringW(BC.M, "IntegerTyID  "); break;
      case Type::TypeID::FunctionTyID : OutputDebugStringW(BC.M, "FunctionTyID "); break;
      case Type::TypeID::StructTyID   : OutputDebugStringW(BC.M, "StructTyID   "); break;
      case Type::TypeID::ArrayTyID    : OutputDebugStringW(BC.M, "ArrayTyID    "); break;
      case Type::TypeID::PointerTyID  : OutputDebugStringW(BC.M, "PointerTyID  "); break;
      case Type::TypeID::VectorTyID   : OutputDebugStringW(BC.M, "VectorTyID   "); break;
  }
  OutputDebugStringW(BC.M, "End of operands\n");
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
    //  case DXIL::ShaderKind::Vertex:
    //    addVertexShaderProlog(Builder);
    //    break;
    //  case DXIL::ShaderKind::Compute:
    //    addComputeShaderProlog(Builder);
    //    break;
  default:
    assert(false);
  }

  return ParameterTestResult;
}

bool DxilDebugInstrumentation::runOnModule(Module &M)
{
  DxilModule &DM = M.GetOrCreateDxilModule();
  LLVMContext & Ctx = M.getContext();
  OP *HlslOP = DM.GetOP();
  Instruction* firstInsertionPt = DM.GetEntryFunction()->getEntryBlock().getFirstInsertionPt();
  IRBuilder<> Builder(firstInsertionPt);

  BuilderContext BC{ M, DM, Ctx, HlslOP, Builder };

  auto SystemValues = addRequiredSystemValues(BC);
  auto HandleForUAV = addUAV(BC);
  /*auto SelectionCriterion = */addInvocationSelectionProlog(BC, SystemValues);

  auto & Functions = M.getFunctionList();
  for (auto & f : Functions)
  {
    auto & Blocks = f.getBasicBlockList();
    for (auto & b : Blocks)
    {
      auto & Instructions = b.getInstList();

      Instruction * InstructionIterator = Instructions.begin();

      if (&f == DM.GetEntryFunction())
      {
        // Skip over the instructions we added at the beginning: Don't instrument them.
        while (InstructionIterator != firstInsertionPt)
        {
          InstructionIterator = InstructionIterator->getNextNode();
          assert(InstructionIterator != nullptr);
        }
      }

      for (; InstructionIterator != Instructions.end(); InstructionIterator = InstructionIterator->getNextNode())
      {
        OutputDebugStringW(M, "Beginning of instruction\n");
        auto OpIterator = InstructionIterator->op_begin();
        while(OpIterator != InstructionIterator->op_end())
        {
          Value const * operand = *OpIterator;

          recordInputValue(BC, HandleForUAV, operand);

          OpIterator++;
        }
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
