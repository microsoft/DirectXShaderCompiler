///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSignature.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// DxilLegalizeSampleOffsetPass implementation.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilOperations.h"

#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"

#include <unordered_set>

using std::vector;
using std::unique_ptr;
using namespace llvm;
using namespace hlsl;

///////////////////////////////////////////////////////////////////////////////
// Legalize Sample offset.

namespace {
// When optimizations are disabled, try to legalize sample offset.
class DxilLegalizeSampleOffsetPass : public ModulePass {

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilLegalizeSampleOffsetPass() : ModulePass(ID) {}

  const char *getPassName() const override {
    return "DXIL legalize sample offset";
  }

  bool runOnModule(Module &M) override {
    std::vector<Instruction *> illegalOffsets;

    CollectIllegalSamples(illegalOffsets, M);

    if (illegalOffsets.empty())
      return false;

    TryUnrollLoop(illegalOffsets);

    std::vector<Instruction *> ssaIllegalOffsets;
    CollectIllegalSamples(ssaIllegalOffsets, M);
    LegalizeOffsets(ssaIllegalOffsets);

    legacy::PassManager PM;
    PM.add(createPromoteMemoryToRegisterPass());
    PM.run(M);

    return true;
  }

private:
  void TryUnrollLoop(std::vector<Instruction *> &illegalOffsets);
  void CollectIllegalSamples(std::vector<Instruction *> &illegalOffsets,
                             Module &M);
  void CollectIllegalSamples(std::vector<Instruction *> &illegalOffsets,
                             Function *F, DXIL::OpCode);
  void LegalizeOffsets(std::vector<Instruction *> &illegalOffsets);
};

char DxilLegalizeSampleOffsetPass::ID = 0;
}

void DxilLegalizeSampleOffsetPass::TryUnrollLoop(
    std::vector<Instruction *> &illegalOffsets) {
  std::unordered_set<Function *> funcSet;
  for (Instruction *I : illegalOffsets) {
    BasicBlock *BB = I->getParent();
    funcSet.insert(BB->getParent());
  }

  for (Function *F : funcSet) {
    DominatorTreeAnalysis DTA;
    DominatorTree DT = DTA.run(*F);
    LoopInfo LI;
    LI.Analyze(DT);

    bool findOffset = false;

    for (auto loopIt = LI.begin(); loopIt != LI.end(); loopIt++) {
      Loop *loop = *loopIt;
      for (Instruction *I : illegalOffsets) {
        BasicBlock *BB = I->getParent();
        if (loop->contains(BB)) {
          findOffset = true;
          break;
        }
      }
      if (findOffset)
        break;
    }

    legacy::FunctionPassManager PM(F->getParent());
    PM.add(createPromoteMemoryToRegisterPass());

    if (findOffset) {
      PM.add(createCFGSimplificationPass());
      PM.add(createLCSSAPass());
      PM.add(createLoopSimplifyPass());
      PM.add(createLoopRotatePass());
      PM.add(createSimpleLoopUnrollPass());
    }
    PM.run(*F);
  }
}

void DxilLegalizeSampleOffsetPass::CollectIllegalSamples(
    std::vector<Instruction *> &illegalOffsets, Module &M) {
  for (Function &F : M.functions()) {
    if (hlsl::OP::IsDxilOpFunc(&F)) {
      if (F.user_empty())
        continue;
      CallInst *CI = cast<CallInst>(*F.user_begin());
      DXIL::OpCode opcode = hlsl::OP::GetDxilOpFuncCallInst(CI);
      switch (opcode) {
      case DXIL::OpCode::Sample:
      case DXIL::OpCode::SampleBias:
      case DXIL::OpCode::SampleCmp:
      case DXIL::OpCode::SampleCmpLevelZero:
      case DXIL::OpCode::SampleGrad:
      case DXIL::OpCode::SampleLevel:
        CollectIllegalSamples(illegalOffsets, &F, opcode);
        break;
      default:
        continue;
      }
    }
  }
}

void DxilLegalizeSampleOffsetPass::CollectIllegalSamples(
    std::vector<Instruction *> &illegalOffsets, Function *F, DXIL::OpCode) {
  for (User *U : F->users()) {
    CallInst *CI = cast<CallInst>(U);
    Value *offset0 =
        CI->getArgOperand(DXIL::OperandIndex::kTextureSampleOffset0OpIdx);
    // No offset.
    if (isa<UndefValue>(offset0))
      continue;

    if (Instruction *I = dyn_cast<Instruction>(offset0))
      illegalOffsets.emplace_back(I);
    Value *offset1 =
        CI->getArgOperand(DXIL::OperandIndex::kTextureSampleOffset1OpIdx);
    if (Instruction *I = dyn_cast<Instruction>(offset1))
      illegalOffsets.emplace_back(I);
    Value *offset2 =
        CI->getArgOperand(DXIL::OperandIndex::kTextureSampleOffset2OpIdx);
    if (Instruction *I = dyn_cast<Instruction>(offset2))
      illegalOffsets.emplace_back(I);
  }
}

void DxilLegalizeSampleOffsetPass::LegalizeOffsets(
    std::vector<Instruction *> &illegalOffsets) {
  for (Instruction *I : illegalOffsets)
    llvm::recursivelySimplifyInstruction(I);
}

ModulePass *llvm::createDxilLegalizeSampleOffsetPass() {
  return new DxilLegalizeSampleOffsetPass();
}

INITIALIZE_PASS(DxilLegalizeSampleOffsetPass, "dxil-legalize-sample-offset",
                "DXIL legailze sample offset", false, false)
