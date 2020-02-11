///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// NormalizeDxil.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Normalize DXIL transformation.                                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "llvm/Pass.h"


namespace llvm {
  class Function;
  class PassRegistry;
  class FunctionPass;


  llvm::FunctionPass *createNormalizeDxilPass();
  void initializeNormalizeDxilPassPass(llvm::PassRegistry&);

  // The legacy pass manager's analysis pass to normalize dxil ir.
  class NormalizeDxilPass : public FunctionPass {
  public:
    static char ID; // Pass identification, replacement for typeid

    NormalizeDxilPass() : FunctionPass(ID) {
      initializeNormalizeDxilPassPass(*PassRegistry::getPassRegistry());
    }

    // Normalize incoming dxil ir.
    bool runOnFunction(Function &F) override;

    virtual const char *getPassName() const override { return "Normalize Dxil"; }

    void getAnalysisUsage(AnalysisUsage &AU) const override;
  };
}
