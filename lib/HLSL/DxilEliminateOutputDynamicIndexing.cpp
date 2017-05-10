///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilCondenseResources.cpp                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Eliminate dynamic indexing on output.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilSignatureElement.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilInstructions.h"

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include <llvm/ADT/DenseSet.h>

using namespace llvm;
using namespace hlsl;

namespace {
class DxilEliminateOutputDynamicIndexing : public ModulePass {
private:

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilEliminateOutputDynamicIndexing() : ModulePass(ID) {}

  const char *getPassName() const override {
    return "DXIL eliminate ouptut dynamic indexing";
  }

  bool runOnModule(Module &M) override {
    DxilModule &DM = M.GetOrCreateDxilModule();
    // Skip pass thru entry.
    if (!DM.GetEntryFunction())
      return false;

    hlsl::OP *hlslOP = DM.GetOP();

    ArrayRef<llvm::Function *> storeOutputs = hlslOP->GetOpFuncList(DXIL::OpCode::StoreOutput);
    DenseMap<Value *, Type *> dynamicSigSet;
    for (Function *F : storeOutputs) {
      // Skip overload not used.
      if (!F)
        continue;
      for (User *U : F->users()) {
        CallInst *CI = cast<CallInst>(U);
        DxilInst_StoreOutput store(CI);
        // Save dynamic indeed sigID.
        if (!isa<ConstantInt>(store.get_rowIndex())) {
          Value * sigID = store.get_outputtSigId();
          dynamicSigSet[sigID] = store.get_value()->getType();
        }
      }
    }

    if (dynamicSigSet.empty())
      return false;

    Function *Entry = DM.GetEntryFunction();
    IRBuilder<> Builder(Entry->getEntryBlock().getFirstInsertionPt());

    DxilSignature &outputSig = DM.GetOutputSignature();
    Value *opcode =
        Builder.getInt32(static_cast<unsigned>(DXIL::OpCode::StoreOutput));
    Value *zero = Builder.getInt32(0);

    for (auto sig : dynamicSigSet) {
      Value *sigID = sig.first;
      Type *EltTy = sig.second;
      unsigned ID = cast<ConstantInt>(sigID)->getLimitedValue();
      DxilSignatureElement &sigElt = outputSig.GetElement(ID);
      unsigned row = sigElt.GetRows();
      unsigned col = sigElt.GetCols();
      Type *AT = ArrayType::get(EltTy, row);

      std::vector<Value *> tmpSigElts(col);
      for (unsigned c = 0; c < col; c++) {
        Value *newCol = Builder.CreateAlloca(AT);
        tmpSigElts[c] = newCol;
      }

      Function *F = hlslOP->GetOpFunc(DXIL::OpCode::StoreOutput, EltTy);
      // Change store output to store tmpSigElts.
      ReplaceDynamicOutput(tmpSigElts, sigID, zero, F);
      // Store tmpSigElts to Output before return.
      StoreTmpSigToOutput(tmpSigElts, row, opcode, sigID, F, Entry);
    }

    return true;
  }

private:
  void ReplaceDynamicOutput(ArrayRef<Value *> tmpSigElts, Value * sigID, Value *zero, Function *F);
  void StoreTmpSigToOutput(ArrayRef<Value *> tmpSigElts, unsigned row,
                           Value *opcode, Value *sigID, Function *StoreOutput,
                           Function *Entry);
};

void DxilEliminateOutputDynamicIndexing::ReplaceDynamicOutput(
    ArrayRef<Value *> tmpSigElts, Value *sigID, Value *zero, Function *F) {
  for (auto it = F->user_begin(); it != F->user_end();) {
    CallInst *CI = cast<CallInst>(*(it++));
    DxilInst_StoreOutput store(CI);
    if (sigID == store.get_outputtSigId()) {
      Value *col = store.get_colIndex();
      unsigned c = cast<ConstantInt>(col)->getLimitedValue();
      Value *tmpSigElt = tmpSigElts[c];
      IRBuilder<> Builder(CI);
      Value *r = store.get_rowIndex();
      // Store to tmpSigElt.
      Value *GEP = Builder.CreateInBoundsGEP(tmpSigElt, {zero, r});
      Builder.CreateStore(store.get_value(), GEP);
      // Remove store output.
      CI->eraseFromParent();
    }
  }
}

void DxilEliminateOutputDynamicIndexing::StoreTmpSigToOutput(
    ArrayRef<Value *> tmpSigElts, unsigned row, Value *opcode, Value *sigID,
    Function *StoreOutput, Function *Entry) {
  Value *args[] = {opcode, sigID, /*row*/ nullptr, /*col*/ nullptr,
                   /*val*/ nullptr};
  // Store the tmpSigElts to Output before every return.
  for (auto &BB : Entry->getBasicBlockList()) {
    if (ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
      IRBuilder<> Builder(RI);
      Value *zero = Builder.getInt32(0);
      for (unsigned c = 0; c<tmpSigElts.size(); c++) {
        Value *col = tmpSigElts[c];
        args[DXIL::OperandIndex::kStoreOutputColOpIdx] = Builder.getInt8(c);
        for (unsigned r = 0; r < row; r++) {
          Value *GEP =
              Builder.CreateInBoundsGEP(col, {zero, Builder.getInt32(r)});
          Value *V = Builder.CreateLoad(GEP);
          args[DXIL::OperandIndex::kStoreOutputRowOpIdx] = Builder.getInt32(r);
          args[DXIL::OperandIndex::kStoreOutputValOpIdx] = V;
          Builder.CreateCall(StoreOutput, args);
        }
      }
    }
  }
}

}

char DxilEliminateOutputDynamicIndexing::ID = 0;

ModulePass *llvm::createDxilEliminateOutputDynamicIndexingPass() {
  return new DxilEliminateOutputDynamicIndexing();
}

INITIALIZE_PASS(DxilEliminateOutputDynamicIndexing,
                "hlsl-dxil-eliminate-output-dynamic",
                "DXIL eliminate ouptut dynamic indexing", false, false)
