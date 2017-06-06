///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilOutputColorBecomesConstant.cpp                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides a pass to stomp a pixel shader's output color to a given         //
// constant value                                                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilSignatureElement.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "dxc/HLSL/DxilInstructions.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Local.h"
#include <memory>
#include <unordered_set>

using namespace llvm;
using namespace hlsl;

class DxilOutputColorBecomesConstant : public ModulePass {

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilOutputColorBecomesConstant() : ModulePass(ID) {}

  const char *getPassName() const override { return "DXIL Constant Color Mod"; }

  bool runOnModule(Module &M) override {

    //todo: make these parameters to the pass
    float r = 0.f;
    float g = 1.f;
    float b = 0.f;
    float a = 1.f;

    DxilModule &DM = M.GetOrCreateDxilModule();

    auto pEntrypoint = DM.GetEntryFunction();

    llvm::BasicBlock * pBasicBlock = pEntrypoint->begin();

    for (auto pInstruction = pBasicBlock->begin(); pInstruction != pBasicBlock->end(); pInstruction++) {

      unsigned llvmOpcode = pInstruction->getOpcode();

      if (llvmOpcode == llvm::Instruction::Ret) {
        // Emit a new output-constant-color that looks something like this:
        //
        //  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0.000000e+00), !dbg !142; StoreOutput(outputtSigId, rowIndex, colIndex, value)
        //  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 1.000000e+00), !dbg !142; StoreOutput(outputtSigId, rowIndex, colIndex, value)
        //  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float 0.000000e+00), !dbg !142; StoreOutput(outputtSigId, rowIndex, colIndex, value)
        //  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 1.000000e+00), !dbg !142; StoreOutput(outputtSigId, rowIndex, colIndex, value)

        llvm::IRBuilder<> Builder(pInstruction);

        // Create a new hlsl operation as a factory for creating a reference to the store-output "global function" and the required float constants
        hlsl::OP *hlslOP = DM.GetOP();

        llvm::Function *pOutputFunction = hlslOP->GetOpFunc(hlsl::DXIL::OpCode::StoreOutput, Builder.getVoidTy());

        std::unique_ptr<OP> hlslOp = std::make_unique<OP>(M.getContext(), & M);
        llvm::Constant *OpArg = hlslOp->GetU32Const((unsigned)hlsl::DXIL::OpCode::StoreOutput);

        // Prepare the argument array, of which the first two elements remain constant for each of the four pixel-elements we're going to set
        llvm::SmallVector<llvm::Value *, 4> Args;
        Args.push_back(OpArg);
        Args.push_back(Builder.getInt32(0)); //todo: this is probably an index into the output signature... so we'll have to look up the proper index

                                                // Emit the RGBA stores in that order, since each one is being sequentially inserted before the "return" instruction

        llvm::Constant * pFloatConstant = hlslOP->GetFloatConst(r);
        Args[2] = Builder.getInt32(3);
        Args[3] = pFloatConstant;
        llvm::CallInst * callStoreOutput = Builder.CreateCall(pOutputFunction, Args);
        callStoreOutput->insertBefore(pInstruction);

        pFloatConstant = hlslOP->GetFloatConst(g);
        Args[2] = Builder.getInt32(2);
        Args[3] = pFloatConstant;
        callStoreOutput = Builder.CreateCall(pOutputFunction, Args);
        callStoreOutput->insertBefore(pInstruction);

        pFloatConstant = hlslOP->GetFloatConst(b);
        Args[2] = Builder.getInt32(1);
        Args[3] = pFloatConstant;
        callStoreOutput = Builder.CreateCall(pOutputFunction, Args);
        callStoreOutput->insertBefore(pInstruction);

        pFloatConstant = hlslOP->GetFloatConst(a);
        Args[2] = Builder.getInt32(0);
        Args[4] = pFloatConstant;
        callStoreOutput = Builder.CreateCall(pOutputFunction, Args);
        callStoreOutput->insertBefore(pInstruction);
      }
    }
    return true;
  }
};

char DxilOutputColorBecomesConstant::ID = 0;

ModulePass *llvm::createDxilOutputColorBecomesConstantPass() {
  return new DxilOutputColorBecomesConstant();
}

INITIALIZE_PASS(DxilOutputColorBecomesConstant, "hlsl-dxil-constantColor", "DXIL Constant Color Mod", false, false)
