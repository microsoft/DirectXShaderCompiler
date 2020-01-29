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
#include "dxc/HLSL/DxilValueCache.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"

#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
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
class DxilLegalizeSampleOffsetPass : public FunctionPass {

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilLegalizeSampleOffsetPass() : FunctionPass(ID) {}

  const char *getPassName() const override {
    return "DXIL legalize sample offset";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<DxilValueCache>();
    AU.setPreservesAll();
  }

  bool runOnFunction(Function &F) override {
    DxilModule &DM = F.getParent()->GetOrCreateDxilModule();
    hlsl::OP *hlslOP = DM.GetOP();

    std::vector<Instruction *> illegalOffsets;

    CollectIllegalOffsets(illegalOffsets, F, hlslOP);

    if (illegalOffsets.empty())
      return false;

    // Loop unroll if has offset inside loop.
    TryUnrollLoop(illegalOffsets, F);

    // Collect offset again after mem2reg.
    std::vector<Instruction *> ssaIllegalOffsets;
    CollectIllegalOffsets(ssaIllegalOffsets, F, hlslOP);

    // Run simple optimization to legalize offsets.
    LegalizeOffsets(ssaIllegalOffsets);

    // Remove PHINodes to keep code shape.
    legacy::FunctionPassManager PM(F.getParent());
    PM.add(createDemoteRegisterToMemoryHlslPass());
    PM.run(F);

    FinalCheck(illegalOffsets, F, hlslOP);

    return true;
  }

private:
  void TryUnrollLoop(std::vector<Instruction *> &illegalOffsets, Function &F);
  void CollectIllegalOffsets(std::vector<Instruction *> &illegalOffsets,
                             Function &F, hlsl::OP *hlslOP);
  void CollectIllegalOffsets(std::vector<Instruction *> &illegalOffsets,
                             Function &F, DXIL::OpCode opcode,
                             hlsl::OP *hlslOP);
  void LegalizeOffsets(const std::vector<Instruction *> &illegalOffsets);
  void FinalCheck(std::vector<Instruction *> &illegalOffsets, Function &F,
                  hlsl::OP *hlslOP);
};

char DxilLegalizeSampleOffsetPass::ID = 0;

bool HasIllegalOffsetInLoop(std::vector<Instruction *> &illegalOffsets,
                            Function &F) {
  DominatorTreeAnalysis DTA;
  DominatorTree DT = DTA.run(F);
  LoopInfo LI;
  LI.Analyze(DT);

  bool findOffset = false;

  for (Instruction *I : illegalOffsets) {
    BasicBlock *BB = I->getParent();
    if (LI.getLoopFor(BB)) {
      findOffset = true;
      break;
    }
  }
  return findOffset;
}

void CollectIllegalOffset(CallInst *CI,
                          std::vector<Instruction *> &illegalOffsets) {
  Value *offset0 =
      CI->getArgOperand(DXIL::OperandIndex::kTextureSampleOffset0OpIdx);
  // No offset.
  if (isa<UndefValue>(offset0))
    return;

  for (unsigned i = DXIL::OperandIndex::kTextureSampleOffset0OpIdx;
       i <= DXIL::OperandIndex::kTextureSampleOffset2OpIdx; i++) {
    Value *offset = CI->getArgOperand(i);
    if (Instruction *I = dyn_cast<Instruction>(offset))
      illegalOffsets.emplace_back(I);
  }
}
}

void DxilLegalizeSampleOffsetPass::FinalCheck(
    std::vector<Instruction *> &illegalOffsets, Function &F, hlsl::OP *hlslOP) {
  // Collect offset to make sure no illegal offsets.
  std::vector<Instruction *> finalIllegalOffsets;
  CollectIllegalOffsets(finalIllegalOffsets, F, hlslOP);

  if (!finalIllegalOffsets.empty()) {
    const StringRef kIllegalOffsetError =
        "Offsets for Sample* must be immediated value. "
        "Consider unroll the loop manually and use O3, it may help in some "
        "cases\n";
    std::string errorMsg;
    raw_string_ostream errorStr(errorMsg);
    for (Instruction *offset : finalIllegalOffsets) {
      if (const DebugLoc &L = offset->getDebugLoc())
        L.print(errorStr);
      errorStr << " " << kIllegalOffsetError;
    }
    errorStr.flush();
    F.getContext().emitError(errorMsg);
  }
}

void DxilLegalizeSampleOffsetPass::TryUnrollLoop(
    std::vector<Instruction *> &illegalOffsets, Function &F) {
  legacy::FunctionPassManager PM(F.getParent());
  // Scalarize aggregates as mem2reg only applies on scalars.
  PM.add(createSROAPass());
  // Always need mem2reg for simplify illegal offsets.
  PM.add(createPromoteMemoryToRegisterPass());

  bool UnrollLoop = HasIllegalOffsetInLoop(illegalOffsets, F);
  if (UnrollLoop) {
    PM.add(createCFGSimplificationPass());
    PM.add(createLCSSAPass());
    PM.add(createLoopSimplifyPass());
    PM.add(createLoopRotatePass());
    PM.add(createLoopUnrollPass(-2, -1, 0, 0));
  }
  PM.run(F);

  if (UnrollLoop) {
    DxilValueCache *DVC = &getAnalysis<DxilValueCache>();
    DVC->ResetUnknowns();
  }
}

void DxilLegalizeSampleOffsetPass::CollectIllegalOffsets(
    std::vector<Instruction *> &illegalOffsets, Function &CurF,
    hlsl::OP *hlslOP) {
  CollectIllegalOffsets(illegalOffsets, CurF, DXIL::OpCode::Sample, hlslOP);
  CollectIllegalOffsets(illegalOffsets, CurF, DXIL::OpCode::SampleBias, hlslOP);
  CollectIllegalOffsets(illegalOffsets, CurF, DXIL::OpCode::SampleCmp, hlslOP);
  CollectIllegalOffsets(illegalOffsets, CurF, DXIL::OpCode::SampleCmpLevelZero,
                        hlslOP);
  CollectIllegalOffsets(illegalOffsets, CurF, DXIL::OpCode::SampleGrad, hlslOP);
  CollectIllegalOffsets(illegalOffsets, CurF, DXIL::OpCode::SampleLevel,
                        hlslOP);
}

void DxilLegalizeSampleOffsetPass::CollectIllegalOffsets(
    std::vector<Instruction *> &illegalOffsets, Function &CurF,
    DXIL::OpCode opcode, hlsl::OP *hlslOP) {
  auto &intrFuncList = hlslOP->GetOpFuncList(opcode);
  for (auto it : intrFuncList) {
    Function *intrFunc = it.second;
    if (!intrFunc)
      continue;
    for (User *U : intrFunc->users()) {
      CallInst *CI = cast<CallInst>(U);
      // Skip inst not in current function.
      if (CI->getParent()->getParent() != &CurF)
        continue;

      CollectIllegalOffset(CI, illegalOffsets);
    }
  }
}

void DxilLegalizeSampleOffsetPass::LegalizeOffsets(
    const std::vector<Instruction *> &illegalOffsets) {
  if (illegalOffsets.size()) {
    DxilValueCache *DVC = &getAnalysis<DxilValueCache>();
    for (Instruction *I : illegalOffsets) {
      if (Value *V = DVC->GetValue(I)) {
        I->replaceAllUsesWith(V);
      }
    }
  }
}

FunctionPass *llvm::createDxilLegalizeSampleOffsetPass() {
  return new DxilLegalizeSampleOffsetPass();
}

INITIALIZE_PASS_BEGIN(DxilLegalizeSampleOffsetPass, "dxil-legalize-sample-offset",
                "DXIL legalize sample offset", false, false)
INITIALIZE_PASS_DEPENDENCY(DxilValueCache)
INITIALIZE_PASS_END(DxilLegalizeSampleOffsetPass, "dxil-legalize-sample-offset",
                "DXIL legalize sample offset", false, false)
