///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilStripDebugSensitiveInfo.cpp                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Pass to strip debug-sensitive information from DXIL modules.              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilModule.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;
using namespace hlsl;

namespace {

class DxilStripDebugSensitiveInfo : public ModulePass {
public:
  static char ID;
  explicit DxilStripDebugSensitiveInfo() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return "DXIL Strip Debug-Sensitive Information";
  }

  bool runOnModule(Module &M) override {
    if (!M.HasDxilModule())
      return false;
    return M.GetOrCreateDxilModule().StripNamesSensitiveToDebug();
  }
};

char DxilStripDebugSensitiveInfo::ID = 0;

} // namespace

ModulePass *llvm::createDxilStripDebugSensitiveInfoPass() {
  return new DxilStripDebugSensitiveInfo();
}

INITIALIZE_PASS(DxilStripDebugSensitiveInfo, "hlsl-dxil-strip-debug-info",
                "HLSL DXIL Strip Debug-Sensitive Information", false, false)
