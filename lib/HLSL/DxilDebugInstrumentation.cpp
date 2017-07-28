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

using namespace llvm;
using namespace hlsl;

class DxilDebugInstrumentation : public ModulePass {

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilDebugInstrumentation() : ModulePass(ID) {}
  const char *getPassName() const override { return "DXIL Force Early Z"; }
  bool runOnModule(Module &M) override;
};


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
        auto OpIterator = i.op_begin();
        while(OpIterator != i.op_end())
        {
          Value const * operand = *OpIterator;

          Type::TypeID ID = operand->getType()->getTypeID();

          switch (ID)
          {
              case VoidTyID     : OutputDebugStringW("VoidTyID     "); break;
              case HalfTyID     : OutputDebugStringW("HalfTyID     "); break;
              case FloatTyID    : OutputDebugStringW("FloatTyID    "); break;
              case DoubleTyID   : OutputDebugStringW("DoubleTyID   "); break;
              case X86_FP80TyID : OutputDebugStringW("X86_FP80TyID "); break;
              case FP128TyID    : OutputDebugStringW("FP128TyID    "); break;
              case PPC_FP128TyID: OutputDebugStringW("PPC_FP128TyID"); break;
              case LabelTyID    : OutputDebugStringW("LabelTyID    "); break;
              case MetadataTyID : OutputDebugStringW("MetadataTyID "); break;
              case X86_MMXTyID  : OutputDebugStringW("X86_MMXTyID  "); break;
              case IntegerTyID  : OutputDebugStringW("IntegerTyID  "); break;
              case FunctionTyID : OutputDebugStringW("FunctionTyID "); break;
              case StructTyID   : OutputDebugStringW("StructTyID   "); break;
              case ArrayTyID    : OutputDebugStringW("ArrayTyID    "); break;
              case PointerTyID  : OutputDebugStringW("PointerTyID  "); break;
              case VectorTyID   : OutputDebugStringW("VectorTyID   "); break;
          }
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
