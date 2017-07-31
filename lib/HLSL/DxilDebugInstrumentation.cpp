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
#include "dxc/HLSL/DxilModule.h"
#include "llvm/IR/Module.h"

#include "llvm/support/FormattedStream.h"

using namespace llvm;
using namespace hlsl;

class DxilDebugInstrumentation : public ModulePass {

private:
  union ParametersAllTogether
  {
    uint32_t Parameters[3];
    struct PixelShaderParameters
    {
      uint32_t X;
      uint32_t Y;
      uint32_t PrimitiveId;
    } PixelShader;
    struct VertexShaderParameters
    {
      uint32_t VertexId;
      uint32_t InstanceId;
      uint32_t Unused;
    } VertexShader;
    struct ComputeShaderParameters
    {
      uint32_t ThreadIdX;
      uint32_t ThreadIdY;
      uint32_t ThreadIdZ;
    };
  } Parameters = { 0,0,0 };

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilDebugInstrumentation() : ModulePass(ID) {}
  const char *getPassName() const override { return "DXIL Force Early Z"; }
  void applyOptions(PassOptions O) override;
  bool runOnModule(Module &M) override;

private:
  void OutputDebugStringW(Module &M, const char *p);
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

bool DxilDebugInstrumentation::runOnModule(Module &M)
{
  DxilModule &DM = M.GetOrCreateDxilModule();

  auto & Functions = M.getFunctionList();
  for (auto & f : Functions)
  {
    auto & Blocks = f.getBasicBlockList();
    for (auto & b : Blocks)
    {
      auto & Instructions = b.getInstList();
      for (auto & i : Instructions)
      {
        OutputDebugStringW(M, "Beginning of instruction\n");
        auto OpIterator = i.op_begin();
        while(OpIterator != i.op_end())
        {
          Value const * operand = *OpIterator;

          Type::TypeID ID = operand->getType()->getTypeID();

          switch (ID)
          {
              case Type::TypeID::VoidTyID     : OutputDebugStringW(M, "VoidTyID     "); break;
              case Type::TypeID::HalfTyID     : OutputDebugStringW(M, "HalfTyID     "); break;
              case Type::TypeID::FloatTyID    : OutputDebugStringW(M, "FloatTyID    "); break;
              case Type::TypeID::DoubleTyID   : OutputDebugStringW(M, "DoubleTyID   "); break;
              case Type::TypeID::X86_FP80TyID : OutputDebugStringW(M, "X86_FP80TyID "); break;
              case Type::TypeID::FP128TyID    : OutputDebugStringW(M, "FP128TyID    "); break;
              case Type::TypeID::PPC_FP128TyID: OutputDebugStringW(M, "PPC_FP128TyID"); break;
              case Type::TypeID::LabelTyID    : OutputDebugStringW(M, "LabelTyID    "); break;
              case Type::TypeID::MetadataTyID : OutputDebugStringW(M, "MetadataTyID "); break;
              case Type::TypeID::X86_MMXTyID  : OutputDebugStringW(M, "X86_MMXTyID  "); break;
              case Type::TypeID::IntegerTyID  : OutputDebugStringW(M, "IntegerTyID  "); break;
              case Type::TypeID::FunctionTyID : OutputDebugStringW(M, "FunctionTyID "); break;
              case Type::TypeID::StructTyID   : OutputDebugStringW(M, "StructTyID   "); break;
              case Type::TypeID::ArrayTyID    : OutputDebugStringW(M, "ArrayTyID    "); break;
              case Type::TypeID::PointerTyID  : OutputDebugStringW(M, "PointerTyID  "); break;
              case Type::TypeID::VectorTyID   : OutputDebugStringW(M, "VectorTyID   "); break;
          }
          OutputDebugStringW(M, "End of operands\n");
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
