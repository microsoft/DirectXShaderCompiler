///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPixReadOutputSig.cpp                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Read output sig and return output same as text                            //
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

class DxilPixReadOutputSig : public ModulePass {
public:
  static char ID;
  explicit DxilPixReadOutputSig() : ModulePass(ID) {}
  bool runOnModule(Module &M) override {
    if (OSOverride != nullptr) {
      DxilModule &DM = M.GetOrCreateDxilModule();
      auto const &Meta = DM.GetOutputSignature();
      auto const &Elements = Meta.GetElements();
      for (auto const &Element : Elements) {
        *OSOverride
            << "OutputSigElement:" << Element->GetName() << ":"
            << std::to_string(Element->GetSemanticStartIndex()) << ":"
            << (Element->IsArbitrary()
                    ? "Arbitrary"
                    : hlsl::Semantic::Get(Element->GetKind())->GetName())
            << "=" << std::to_string(Element->GetStartRow()) << "-"
            << std::to_string(Element->GetStartCol()) << "-"
            << std::to_string(Element->GetRows()) << "-"
            << std::to_string(Element->GetCols()) << "\n";
      }
    }
    return false;
  }
};

char DxilPixReadOutputSig::ID = 0;

ModulePass *createDxilPixReadOutputSigPass() {
  return new DxilPixReadOutputSig();
}

INITIALIZE_PASS(DxilPixReadOutputSig, "dxil-read-output-sig",
                "Read and print module output sig", false, false)
