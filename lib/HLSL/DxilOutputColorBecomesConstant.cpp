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
    float r = 0.2f;
    float g = 0.4f;
    float b = 0.6f;
    float a = 1.f;

    float color[4] = { r, g, b, a };

    DxilModule &DM = M.GetOrCreateDxilModule();

    const hlsl::DxilSignature & outputSignature = DM.GetOutputSignature();

    const std::vector<std::unique_ptr<DxilSignatureElement> > & outputSigElements = outputSignature.GetElements();

    int startRow = 0;
    int startColumn = 0;
    bool targetFound = false;
    for (const auto & el : outputSigElements)
    {
      if (el->GetKind() == hlsl::DXIL::SemanticKind::Target)
      {
        targetFound = true;
        startRow = el->GetStartRow();
        startColumn = el->GetStartCol();
      }
    }

    if (!targetFound)
    {
      return false;
    }

    auto pEntrypoint = DM.GetEntryFunction();

    BasicBlock * pBasicBlock = pEntrypoint->begin();

    IRBuilder<> Builder(pBasicBlock->begin());

    OP *hlslOP = DM.GetOP();
    Function *pOutputFunction = hlslOP->GetOpFunc(DXIL::OpCode::StoreOutput, Builder.getFloatTy());

    auto uses = pOutputFunction->uses();

    for (Use &use : uses) {
      iterator_range<Value::user_iterator> users = use->users();
      for (User * user : users) {
        if (isa<Instruction>(user)){
          auto instruction = cast<Instruction>(user);

          Value * pOutputRowOperand = instruction->getOperand(hlsl::DXIL::OperandIndex::kStoreOutputRowOpIdx);
          ConstantInt * pOutputRow = cast<ConstantInt>(pOutputRowOperand);
          APInt outputRow = pOutputRow->getValue();
          outputRow;

          Value * pOutputColumnOperand = instruction->getOperand(hlsl::DXIL::OperandIndex::kStoreOutputColOpIdx);
          ConstantInt * pOutputColumn = cast<ConstantInt>(pOutputColumnOperand);
          APInt outputColumn = pOutputColumn->getValue();

          Constant * pFloatConstant = hlslOP->GetFloatConst(color[*outputColumn.getRawData()]);

          instruction->setOperand(hlsl::DXIL::OperandIndex::kStoreOutputValOpIdx, pFloatConstant);
        }
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
