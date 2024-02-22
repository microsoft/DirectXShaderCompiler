///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPixEmitResources.cpp                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Emit Dxil resource meta data                                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilFunctionProps.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "dxc/DxilPIXPasses/DxilPIXVirtualRegisters.h"
#include "dxc/HLSL/DxilGenerationPass.h"

#include "llvm/IR/Module.h"

using namespace llvm;
using namespace hlsl;

class EmitDxilResourceToMeta : public ModulePass {
public:
  static char ID;
  explicit EmitDxilResourceToMeta() : ModulePass(ID) {}
  bool runOnModule(Module &M) override {
    DxilModule &DM = M.GetOrCreateDxilModule();
    DM.ReEmitDxilResources();
    return true;
  }
};

char EmitDxilResourceToMeta::ID = 0;

ModulePass *createEmitDxilResourceToMeta() {
  return new EmitDxilResourceToMeta();
}

INITIALIZE_PASS(EmitDxilResourceToMeta, "hlsl-dxil-emit-resources",
                "Emit Dxil resources to metadata", false, false)
