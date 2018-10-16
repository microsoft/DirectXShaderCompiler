///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// WaveSensitivityAnalysis.cpp                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// This file provides support for doing analysis that are aware of wave      //
// intrinsics.                                                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilValidation.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilShaderModel.h"
#include "dxc/DXIL/DxilContainer.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/DXIL/DxilInstructions.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/ADT/BitVector.h"
#ifdef _WIN32
#include <winerror.h>
#endif
#include "llvm/Support/raw_ostream.h"
#include <unordered_set>

using namespace llvm;
using namespace std;

namespace hlsl {

class WaveSensitivityAnalyzer : public WaveSensitivityAnalysis {
private:
  enum WaveSensitivity {
    KnownSensitive,
    KnownNotSensitive,
    Unknown
  };
  map<Instruction *, WaveSensitivity> InstState;
  map<BasicBlock *, WaveSensitivity> BBState;
  std::vector<Instruction *> InstWorkList;
  std::vector<BasicBlock *> BBWorkList;
  bool CheckBBState(BasicBlock *BB, WaveSensitivity WS);
  WaveSensitivity GetInstState(Instruction *I);
  void UpdateBlock(BasicBlock *BB, WaveSensitivity WS);
  void UpdateInst(Instruction *I, WaveSensitivity WS);
  void VisitInst(Instruction *I);
public:
  void Analyze(Function *F);
  bool IsWaveSensitive(Instruction *op);
};

WaveSensitivityAnalysis* WaveSensitivityAnalysis::create() {
  return new WaveSensitivityAnalyzer();
}

void WaveSensitivityAnalyzer::Analyze(Function *F) {
  UpdateBlock(&F->getEntryBlock(), KnownNotSensitive);
  while (!InstWorkList.empty() || !BBWorkList.empty()) {
    // Process the instruction work list.
    while (!InstWorkList.empty()) {
      Instruction *I = InstWorkList.back();
      InstWorkList.pop_back();

      // "I" got into the work list because it made a transition.
      for (User *U : I->users()) {
        Instruction *UI = cast<Instruction>(U);
        VisitInst(UI);
      }
    }

    // Process the basic block work list.
    while (!BBWorkList.empty()) {
      BasicBlock *BB = BBWorkList.back();
      BBWorkList.pop_back();

      // Notify all instructions in this basic block that they need to
      // be reevaluated (eg, a block previously though to be insensitive
      // is now sensitive).
      for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I)
        VisitInst(I);
    }
  }
}

bool WaveSensitivityAnalyzer::CheckBBState(BasicBlock *BB, WaveSensitivity WS) {
  auto c = BBState.find(BB);
  if (c == BBState.end()) {
    return WS == Unknown;
  }
  else {
    return (*c).second == WS;
  }
}

WaveSensitivityAnalyzer::WaveSensitivity
WaveSensitivityAnalyzer::GetInstState(Instruction *I) {
  auto c = InstState.find(I);
  if (c == InstState.end())
    return Unknown;
  return (*c).second;
}

void WaveSensitivityAnalyzer::UpdateBlock(BasicBlock *BB, WaveSensitivity WS) {
  auto c = BBState.find(BB);
  // Do not update if an entry is already found and it hasn't changed or
  // has already been marked as wave sensitive (an insensitive term might
  // try to mark it as such, but this effectively implements the 'any pred'
  // rule).
  if (c != BBState.end() && ((*c).second == WS || (*c).second == KnownSensitive))
    return;
  BBState[BB] = WS;
  BBWorkList.push_back(BB);
}

void WaveSensitivityAnalyzer::UpdateInst(Instruction *I, WaveSensitivity WS) {
  auto c = InstState.find(I);
  if (c == InstState.end() || (*c).second != WS) {
    InstState[I] = WS;
    InstWorkList.push_back(I);
    if (TerminatorInst * TI = dyn_cast<TerminatorInst>(I)) {
      for (unsigned i = 0; i < TI->getNumSuccessors(); ++i) {
        BasicBlock *BB = TI->getSuccessor(i);
        UpdateBlock(BB, WS);
      }
    }
  }
}

void WaveSensitivityAnalyzer::VisitInst(Instruction *I) {
  unsigned firstArg = 0;
  if (CallInst *CI = dyn_cast<CallInst>(I)) {
    if (OP::IsDxilOpFuncCallInst(CI)) {
      firstArg = 1;
      OP::OpCode opcode = OP::GetDxilOpFuncCallInst(CI);
      if (OP::IsDxilOpWave(opcode)) {
        UpdateInst(I, KnownSensitive);
        return;
      }
    }
  }

  if (CheckBBState(I->getParent(), KnownSensitive)) {
    UpdateInst(I, KnownSensitive);
    return;
  }

  bool allKnownNotSensitive = true;
  for (unsigned i = firstArg; i < I->getNumOperands(); ++i) {
    Value *V = I->getOperand(i);
    if (Instruction *IArg = dyn_cast<Instruction>(V)) {
      WaveSensitivity WS = GetInstState(IArg);
      if (WS == KnownSensitive) {
        UpdateInst(I, KnownSensitive);
        return;
      }
      if (WS == Unknown) {
        allKnownNotSensitive = false;
        return;
      }
    }
  }
  if (allKnownNotSensitive) {
    UpdateInst(I, KnownNotSensitive);
  }
}

bool WaveSensitivityAnalyzer::IsWaveSensitive(Instruction *op) {
  auto c = InstState.find(op);
  DXASSERT(c != InstState.end(), "else analysis didn't complete");
  DXASSERT((*c).second != Unknown, "else analysis is missing a case");
  return (*c).second == KnownSensitive;
}

} // namespace hlsl
