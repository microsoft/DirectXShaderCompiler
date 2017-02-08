//===-- DxilValidationRaceCondCheck.h - Dxil race condition check pass -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
/// Check race condition for Dxil Validation.
///
//===----------------------------------------------------------------------===//

#pragma once

#include <unordered_set>
#include <functional>

namespace llvm {

class Function;
class Instruction;
class StoreInst;

class RaceConditionCheckPass : public FunctionPass {

public:
  using ErrorEmitter = std::function<void(Instruction*)>;
  static char ID;

  RaceConditionCheckPass();

  void getAnalysisUsage(AnalysisUsage &AU) const override;

  bool runOnFunction(Function &F) override;

private:
  std::unordered_set<Function *> *m_pFixAddrTGSMFuncSet;
  std::vector<StoreInst *> *m_pAcessList;
  ErrorEmitter *m_pEmitError;
  friend Pass *createRaceConditionCheckPass(std::vector<StoreInst *> *TGSMList,
                         std::unordered_set<Function *> *FuncSet,
                         ErrorEmitter *EE);
};

Pass *createRaceConditionCheckPass(std::vector<StoreInst *> *TGSMList,
                         std::unordered_set<Function *> *FuncSet,
                         RaceConditionCheckPass::ErrorEmitter *EE);

} // end namespace llvm
