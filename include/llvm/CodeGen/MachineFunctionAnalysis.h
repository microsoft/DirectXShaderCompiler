//===-- MachineFunctionAnalysis.h - Owner of MachineFunctions ----*-C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MachineFunctionAnalysis.h                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file declares the MachineFunctionAnalysis class.                     //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CODEGEN_MACHINEFUNCTIONANALYSIS_H
#define LLVM_CODEGEN_MACHINEFUNCTIONANALYSIS_H

#include "llvm/Pass.h"

namespace llvm {

class MachineFunction;
class MachineFunctionInitializer;
class TargetMachine;

/// MachineFunctionAnalysis - This class is a Pass that manages a
/// MachineFunction object.
struct MachineFunctionAnalysis : public FunctionPass {
private:
  const TargetMachine &TM;
  MachineFunction *MF;
  unsigned NextFnNum;
  MachineFunctionInitializer *MFInitializer;

public:
  static char ID;
  explicit MachineFunctionAnalysis(const TargetMachine &tm,
                                   MachineFunctionInitializer *MFInitializer);
  ~MachineFunctionAnalysis() override;

  MachineFunction &getMF() const { return *MF; }

  const char* getPassName() const override {
    return "Machine Function Analysis";
  }

private:
  bool doInitialization(Module &M) override;
  bool runOnFunction(Function &F) override;
  void releaseMemory() override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};

} // End llvm namespace

#endif
